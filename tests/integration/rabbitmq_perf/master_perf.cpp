#include "common.h"

#include "foundation/base/ErrorCode.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

using foundation::base::ErrorCode;
using foundation::base::Result;
using module_context::messaging::ConsumeAction;
using module_context::messaging::IncomingMessage;
using module_context::messaging::PublishRequest;
using module_context::tests::rabbitmq_perf::EnsureDirectory;
using module_context::tests::rabbitmq_perf::JoinPath;
using module_context::tests::rabbitmq_perf::MakeMasterBusConfig;
using module_context::tests::rabbitmq_perf::NowMs;
using module_context::tests::rabbitmq_perf::ParseArguments;
using module_context::tests::rabbitmq_perf::ParseOptionalInt;
using module_context::tests::rabbitmq_perf::ParseOptionalSize;
using module_context::tests::rabbitmq_perf::ParseResultMessage;
using module_context::tests::rabbitmq_perf::RabbitMqBusHarness;
using module_context::tests::rabbitmq_perf::RequireArgument;
using module_context::tests::rabbitmq_perf::ResultMessage;
using module_context::tests::rabbitmq_perf::SerializeTaskMessage;
using module_context::tests::rabbitmq_perf::TaskMessage;
using module_context::tests::rabbitmq_perf::WaitForConnected;
using module_context::tests::rabbitmq_perf::WriteLargePseudoImage;
using module_context::tests::rabbitmq_perf::WriteTextFile;

const char kTaskExchange[] = "mc.perf.task.exchange";
const char kTaskRoutingKey[] = "task.dispatch";

struct PublishMetrics {
    std::string task_id;
    std::string image_path;
    std::size_t image_bytes;
    std::uint64_t image_write_start_ts_ms;
    std::uint64_t image_write_done_ts_ms;
    std::uint64_t publish_ts_ms;
    std::uint64_t publish_done_ts_ms;
};

struct ReceivedMetrics {
    ResultMessage result;
    std::uint64_t master_receive_ts_ms;
};

std::string CsvEscape(const std::string& value) {
    bool needs_quote = value.find(',') != std::string::npos ||
                       value.find('"') != std::string::npos ||
                       value.find('\n') != std::string::npos;
    if (!needs_quote) {
        return value;
    }

    std::string escaped;
    escaped.reserve(value.size() + 4);
    escaped.push_back('"');
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(value[index]);
        }
    }
    escaped.push_back('"');
    return escaped;
}

std::string DefaultScenarioId() {
    const std::uint64_t now_ms = NowMs();
    std::ostringstream stream;
    stream << "perf-" << now_ms;
    return stream.str();
}

Result<void> WriteScenarioSummary(
    const std::string& path,
    const std::string& scenario_id,
    int worker_count,
    int task_count,
    int target_rate,
    int simulate_process_ms,
    std::size_t image_bytes,
    const std::string& io_mode,
    bool materialize_output,
    const std::string& image_dir,
    const std::string& result_dir,
    const std::string& notes) {
    std::ostringstream output;
    output << "scenario_id=" << scenario_id << "\n";
    output << "worker_count=" << worker_count << "\n";
    output << "task_count=" << task_count << "\n";
    output << "target_rate=" << target_rate << "\n";
    output << "simulate_process_ms=" << simulate_process_ms << "\n";
    output << "image_bytes=" << image_bytes << "\n";
    output << "io_mode=" << io_mode << "\n";
    output << "materialize_output=" << (materialize_output ? 1 : 0) << "\n";
    output << "image_dir=" << image_dir << "\n";
    output << "result_dir=" << result_dir << "\n";
    output << "notes=" << notes << "\n";
    output << "generated_at_ms=" << NowMs() << "\n";
    return WriteTextFile(path, output.str());
}

Result<void> WritePublishCsv(
    const std::string& path,
    const std::vector<PublishMetrics>& rows) {
    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return Result<void>(ErrorCode::kInvalidState, "Failed to open publish csv '" + path + "'");
    }

    output << "task_id,image_path,image_bytes,image_write_start_ts_ms,image_write_done_ts_ms,image_write_ms,publish_ts_ms,publish_done_ts_ms,publish_call_ms\n";
    for (std::size_t index = 0; index < rows.size(); ++index) {
        const PublishMetrics& row = rows[index];
        output << CsvEscape(row.task_id) << ","
               << CsvEscape(row.image_path) << ","
               << row.image_bytes << ","
               << row.image_write_start_ts_ms << ","
               << row.image_write_done_ts_ms << ","
               << (row.image_write_done_ts_ms - row.image_write_start_ts_ms) << ","
               << row.publish_ts_ms << ","
               << row.publish_done_ts_ms << ","
               << (row.publish_done_ts_ms - row.publish_ts_ms) << "\n";
    }

    output.close();
    if (!output.good()) {
        return Result<void>(ErrorCode::kInvalidState, "Failed to write publish csv '" + path + "'");
    }
    return foundation::base::MakeSuccess();
}

Result<void> WriteResultCsv(
    const std::string& path,
    const std::vector<PublishMetrics>& publishes,
    const std::map<std::string, ReceivedMetrics>& results) {
    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return Result<void>(ErrorCode::kInvalidState, "Failed to open result csv '" + path + "'");
    }

    output << "task_id,worker_id,status,error_message,image_path,output_path,image_bytes,publish_ts_ms,worker_receive_ts_ms,read_done_ts_ms,process_done_ts_ms,output_write_done_ts_ms,cleanup_done_ts_ms,result_publish_ts_ms,master_receive_ts_ms,queue_wait_ms,read_ms,process_ms,write_ms,cleanup_ms,worker_total_ms,result_delivery_ms,end_to_end_ms,output_deleted\n";
    for (std::size_t index = 0; index < publishes.size(); ++index) {
        const PublishMetrics& publish = publishes[index];
        std::map<std::string, ReceivedMetrics>::const_iterator it = results.find(publish.task_id);
        if (it == results.end()) {
            output << CsvEscape(publish.task_id) << ",,,"
                   << CsvEscape(publish.image_path) << ",,"
                   << publish.image_bytes << ","
                   << publish.publish_ts_ms
                   << ",,,,,,,,,,,,,,,\n";
            continue;
        }

        const ResultMessage& result = it->second.result;
        const std::uint64_t queue_wait_ms =
            result.worker_receive_ts_ms >= result.publish_ts_ms
                ? result.worker_receive_ts_ms - result.publish_ts_ms
                : 0;
        const std::uint64_t read_ms =
            result.read_done_ts_ms >= result.worker_receive_ts_ms
                ? result.read_done_ts_ms - result.worker_receive_ts_ms
                : 0;
        const std::uint64_t process_ms =
            result.process_done_ts_ms >= result.read_done_ts_ms
                ? result.process_done_ts_ms - result.read_done_ts_ms
                : 0;
        const std::uint64_t write_ms =
            result.output_write_done_ts_ms >= result.process_done_ts_ms
                ? result.output_write_done_ts_ms - result.process_done_ts_ms
                : 0;
        const std::uint64_t cleanup_ms =
            result.cleanup_done_ts_ms >= result.output_write_done_ts_ms
                ? result.cleanup_done_ts_ms - result.output_write_done_ts_ms
                : 0;
        const std::uint64_t worker_total_ms =
            result.result_publish_ts_ms >= result.worker_receive_ts_ms
                ? result.result_publish_ts_ms - result.worker_receive_ts_ms
                : 0;
        const std::uint64_t result_delivery_ms =
            it->second.master_receive_ts_ms >= result.result_publish_ts_ms
                ? it->second.master_receive_ts_ms - result.result_publish_ts_ms
                : 0;
        const std::uint64_t end_to_end_ms =
            it->second.master_receive_ts_ms >= result.publish_ts_ms
                ? it->second.master_receive_ts_ms - result.publish_ts_ms
                : 0;

        output << CsvEscape(result.task_id) << ","
               << CsvEscape(result.worker_id) << ","
               << CsvEscape(result.status) << ","
               << CsvEscape(result.error_message) << ","
               << CsvEscape(result.image_path) << ","
               << CsvEscape(result.output_path) << ","
               << result.image_bytes << ","
               << result.publish_ts_ms << ","
               << result.worker_receive_ts_ms << ","
               << result.read_done_ts_ms << ","
               << result.process_done_ts_ms << ","
               << result.output_write_done_ts_ms << ","
               << result.cleanup_done_ts_ms << ","
               << result.result_publish_ts_ms << ","
               << it->second.master_receive_ts_ms << ","
               << queue_wait_ms << ","
               << read_ms << ","
               << process_ms << ","
               << write_ms << ","
               << cleanup_ms << ","
               << worker_total_ms << ","
               << result_delivery_ms << ","
               << end_to_end_ms << ","
               << (result.output_deleted ? 1 : 0) << "\n";
    }

    output.close();
    if (!output.good()) {
        return Result<void>(ErrorCode::kInvalidState, "Failed to write result csv '" + path + "'");
    }
    return foundation::base::MakeSuccess();
}

}  // namespace

int main(int argc, char** argv) {
    const std::map<std::string, std::string> args = ParseArguments(argc, argv);

    Result<std::string> uri = RequireArgument(args, "rabbitmq-uri");
    Result<std::string> image_dir = RequireArgument(args, "image-dir");
    Result<std::string> report_dir = RequireArgument(args, "report-dir");
    if (!uri.IsOk() || !image_dir.IsOk() || !report_dir.IsOk()) {
        std::cerr << (uri.IsOk() ? (image_dir.IsOk() ? report_dir.GetMessage() : image_dir.GetMessage())
                                 : uri.GetMessage())
                  << std::endl;
        return 2;
    }

    const int timeout_ms = ParseOptionalInt(args, "timeout-ms", 600000);
    const int task_count = ParseOptionalInt(args, "task-count", 120);
    const int target_rate = ParseOptionalInt(args, "target-rate", 60);
    const int worker_count = ParseOptionalInt(args, "worker-count", 5);
    const int simulate_process_ms = ParseOptionalInt(args, "simulate-process-ms", 80);
    const std::size_t image_bytes = ParseOptionalSize(args, "image-bytes", 20u * 1024u * 1024u);
    const std::string scenario_id =
        args.find("scenario-id") == args.end() ? DefaultScenarioId() : args.find("scenario-id")->second;
    const std::string notes =
        args.find("notes") == args.end() ? std::string() : args.find("notes")->second;
    const std::string io_mode =
        args.find("io-mode") == args.end() ? std::string("stream") : args.find("io-mode")->second;
    const bool use_mmap = io_mode == "mmap";
    const bool materialize_output =
        args.find("materialize-output") == args.end() ? false : (args.find("materialize-output")->second == "1" || args.find("materialize-output")->second == "true");

    Result<void> ensure_image_dir = EnsureDirectory(image_dir.Value());
    Result<void> ensure_report_dir = EnsureDirectory(report_dir.Value());
    if (!ensure_image_dir.IsOk() || !ensure_report_dir.IsOk()) {
        std::cerr << (!ensure_image_dir.IsOk() ? ensure_image_dir.GetMessage() : ensure_report_dir.GetMessage())
                  << std::endl;
        return 1;
    }

    const std::string scenario_path = JoinPath(report_dir.Value(), "scenario.txt");
    const std::string publish_csv_path = JoinPath(report_dir.Value(), "publish_metrics.csv");
    const std::string result_csv_path = JoinPath(report_dir.Value(), "result_metrics.csv");

    RabbitMqBusHarness harness(MakeMasterBusConfig(uri.Value()));

    Result<void> init_result = harness.Init();
    if (!init_result.IsOk()) {
        std::cerr << "perf master init failed: " << init_result.GetMessage() << std::endl;
        return 1;
    }

    module_context::messaging::IMessageBusService* bus = harness.Service();
    if (bus == NULL) {
        std::cerr << "perf master bus service unavailable" << std::endl;
        return 1;
    }

    std::mutex result_mutex;
    std::condition_variable result_cv;
    std::map<std::string, ReceivedMetrics> received;

    Result<void> register_result = bus->RegisterConsumerHandler(
        "perf_result_consumer",
        [&result_mutex, &result_cv, &received](const IncomingMessage& incoming) {
            const std::uint64_t master_receive_ts_ms = NowMs();
            const std::string payload(incoming.payload.begin(), incoming.payload.end());
            Result<ResultMessage> parsed = ParseResultMessage(payload);
            if (!parsed.IsOk()) {
                std::cerr << "perf master failed to parse result payload: "
                          << parsed.GetMessage() << std::endl;
                return ConsumeAction::Reject;
            }

            std::lock_guard<std::mutex> lock(result_mutex);
            if (received.find(parsed.Value().task_id) == received.end()) {
                ReceivedMetrics metrics;
                metrics.result = parsed.Value();
                metrics.master_receive_ts_ms = master_receive_ts_ms;
                received[parsed.Value().task_id] = metrics;
                if (received.size() <= 5 || received.size() % 20 == 0) {
                    std::cout << "[master] received result " << received.size()
                              << " task_id=" << parsed.Value().task_id
                              << " worker=" << parsed.Value().worker_id
                              << " status=" << parsed.Value().status << std::endl;
                }
                result_cv.notify_all();
            }
            return ConsumeAction::Ack;
        });
    if (!register_result.IsOk()) {
        std::cerr << "perf master failed to register consumer: "
                  << register_result.GetMessage() << std::endl;
        (void)harness.Fini();
        return 1;
    }

    Result<void> start_result = harness.Start();
    if (!start_result.IsOk()) {
        std::cerr << "perf master start failed: " << start_result.GetMessage() << std::endl;
        (void)harness.Fini();
        return 1;
    }

    Result<void> wait_connected = WaitForConnected(bus, timeout_ms);
    if (!wait_connected.IsOk()) {
        std::cerr << "perf master failed to connect RabbitMQ: "
                  << wait_connected.GetMessage() << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    Result<void> scenario_result = WriteScenarioSummary(
        scenario_path,
        scenario_id,
        worker_count,
        task_count,
        target_rate,
        simulate_process_ms,
        image_bytes,
        io_mode,
        materialize_output,
        image_dir.Value(),
        report_dir.Value(),
        notes);
    if (!scenario_result.IsOk()) {
        std::cerr << "perf master failed to write scenario summary: "
                  << scenario_result.GetMessage() << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    std::cout << "[master] scenario_id=" << scenario_id
              << ", task_count=" << task_count
              << ", workers=" << worker_count
              << ", target_rate=" << target_rate << "/s"
              << ", image_bytes=" << image_bytes
              << ", io_mode=" << io_mode
              << ", materialize_output=" << (materialize_output ? 1 : 0) << std::endl;

    const std::chrono::steady_clock::time_point publish_anchor = std::chrono::steady_clock::now();
    const double interval_ms = target_rate > 0 ? (1000.0 / static_cast<double>(target_rate)) : 0.0;

    std::vector<PublishMetrics> publishes;
    publishes.reserve(static_cast<std::size_t>(task_count));

    for (int index = 0; index < task_count; ++index) {
        if (interval_ms > 0.0) {
            const std::chrono::steady_clock::time_point target_time =
                publish_anchor + std::chrono::milliseconds(static_cast<long long>(interval_ms * index));
            const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            if (target_time > now) {
                std::this_thread::sleep_until(target_time);
            }
        }

        std::ostringstream task_id_stream;
        task_id_stream << scenario_id << "-task-" << std::setw(4) << std::setfill('0') << (index + 1);
        const std::string task_id = task_id_stream.str();
        const std::string image_path = JoinPath(image_dir.Value(), task_id + ".ppm");

        PublishMetrics metrics;
        metrics.task_id = task_id;
        metrics.image_path = image_path;
        metrics.image_bytes = image_bytes;
        metrics.image_write_start_ts_ms = NowMs();
        Result<void> image_result = WriteLargePseudoImage(image_path, image_bytes, index + 17, use_mmap);
        metrics.image_write_done_ts_ms = NowMs();
        if (!image_result.IsOk()) {
            std::cerr << "perf master failed to write image '" << image_path
                      << "': " << image_result.GetMessage() << std::endl;
            (void)WritePublishCsv(publish_csv_path, publishes);
            (void)harness.Stop();
            (void)harness.Fini();
            return 1;
        }

        TaskMessage task_message;
        task_message.task_id = task_id;
        task_message.image_path = image_path;
        task_message.image_bytes = image_bytes;
        task_message.publish_ts_ms = NowMs();
        metrics.publish_ts_ms = task_message.publish_ts_ms;

        PublishRequest request;
        request.exchange = kTaskExchange;
        request.routing_key = kTaskRoutingKey;
        request.content_type = "text/plain";
        request.correlation_id = task_id;
        request.persistent = true;
        const std::string payload = SerializeTaskMessage(task_message);
        request.payload.assign(payload.begin(), payload.end());

        Result<void> publish_result = bus->Publish(request);
        metrics.publish_done_ts_ms = NowMs();
        publishes.push_back(metrics);
        if (!publish_result.IsOk()) {
            std::cerr << "perf master failed to publish task '" << task_id
                      << "': " << publish_result.GetMessage() << std::endl;
            (void)WritePublishCsv(publish_csv_path, publishes);
            (void)harness.Stop();
            (void)harness.Fini();
            return 1;
        }

        if (index < 5 || (index + 1) % 20 == 0) {
            std::cout << "[master] published " << (index + 1) << "/" << task_count
                      << " task_id=" << task_id << std::endl;
        }
    }

    const std::chrono::steady_clock::time_point wait_deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
    {
        std::unique_lock<std::mutex> lock(result_mutex);
        while (received.size() < static_cast<std::size_t>(task_count)) {
            const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            if (now >= wait_deadline) {
                break;
            }
            result_cv.wait_until(lock, wait_deadline);
        }
    }

    Result<void> publish_csv_result = WritePublishCsv(publish_csv_path, publishes);
    Result<void> result_csv_result = WriteResultCsv(result_csv_path, publishes, received);

    if (!publish_csv_result.IsOk() || !result_csv_result.IsOk()) {
        std::cerr << (!publish_csv_result.IsOk() ? publish_csv_result.GetMessage() : result_csv_result.GetMessage())
                  << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    const std::size_t completed = received.size();
    std::size_t success_count = 0;
    for (std::map<std::string, ReceivedMetrics>::const_iterator it = received.begin();
         it != received.end();
         ++it) {
        if (it->second.result.status == "processed") {
            ++success_count;
        }
    }

    std::cout << "[master] completed=" << completed << "/" << task_count
              << ", success=" << success_count << std::endl;
    std::cout << "[master] publish csv: " << publish_csv_path << std::endl;
    std::cout << "[master] result csv: " << result_csv_path << std::endl;

    // NOTE:
    // In the high-volume perf harness, RabbitMqBusModule::Stop/Fini may block for
    // a long time after all result data has already been flushed to disk. For this
    // benchmark tool we prefer deterministic artifact generation and shell return,
    // so let process teardown reclaim the in-process bus resources on normal exit.
    if (completed != static_cast<std::size_t>(task_count)) {
        std::cerr << "perf master timed out before receiving all task results" << std::endl;
        return 1;
    }

    if (success_count != static_cast<std::size_t>(task_count)) {
        std::cerr << "perf master detected non-processed task results" << std::endl;
        return 1;
    }

    return 0;
}
