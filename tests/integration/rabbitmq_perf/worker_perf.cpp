#include "common.h"

#include "foundation/base/ErrorCode.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
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
using module_context::tests::rabbitmq_perf::MakeWorkerBusConfig;
using module_context::tests::rabbitmq_perf::NowMs;
using module_context::tests::rabbitmq_perf::ParseArguments;
using module_context::tests::rabbitmq_perf::ParseOptionalBool;
using module_context::tests::rabbitmq_perf::ParseOptionalInt;
using module_context::tests::rabbitmq_perf::ParseTaskMessage;
using module_context::tests::rabbitmq_perf::RabbitMqBusHarness;
using module_context::tests::rabbitmq_perf::ReadBinaryFile;
using module_context::tests::rabbitmq_perf::ReadMappedFileForProcessing;
using module_context::tests::rabbitmq_perf::RemoveFileIfExists;
using module_context::tests::rabbitmq_perf::RequireArgument;
using module_context::tests::rabbitmq_perf::ResultMessage;
using module_context::tests::rabbitmq_perf::SerializeResultMessage;
using module_context::tests::rabbitmq_perf::WaitForConnected;
using module_context::tests::rabbitmq_perf::WriteBinaryFileRecoverable;

const char kResultExchange[] = "mc.perf.result.exchange";
const char kResultRoutingKey[] = "result.ready";

std::atomic<bool> g_stop_requested(false);

void HandleSignal(int) {
    g_stop_requested.store(true);
}

}  // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, HandleSignal);
    std::signal(SIGTERM, HandleSignal);

    const std::map<std::string, std::string> args = ParseArguments(argc, argv);

    Result<std::string> uri = RequireArgument(args, "rabbitmq-uri");
    Result<std::string> worker_id = RequireArgument(args, "worker-id");
    Result<std::string> output_dir = RequireArgument(args, "output-dir");
    if (!uri.IsOk() || !worker_id.IsOk() || !output_dir.IsOk()) {
        std::cerr << (uri.IsOk() ? (worker_id.IsOk() ? output_dir.GetMessage() : worker_id.GetMessage())
                                 : uri.GetMessage())
                  << std::endl;
        return 2;
    }

    const int timeout_ms = ParseOptionalInt(args, "timeout-ms", 600000);
    const int simulate_process_ms = ParseOptionalInt(args, "simulate-process-ms", 80);
    const int idle_timeout_ms = ParseOptionalInt(args, "idle-timeout-ms", 5000);
    const bool cleanup_inputs = ParseOptionalBool(args, "cleanup-inputs", true);
    const std::string stop_file =
        args.find("stop-file") == args.end() ? std::string() : args.find("stop-file")->second;
    const std::string io_mode =
        args.find("io-mode") == args.end() ? std::string("stream") : args.find("io-mode")->second;
    const bool use_mmap = io_mode == "mmap";
    const bool materialize_output =
        args.find("materialize-output") == args.end() ? false :
            (args.find("materialize-output")->second == "1" || args.find("materialize-output")->second == "true");

    Result<void> ensure_output_dir = EnsureDirectory(output_dir.Value());
    if (!ensure_output_dir.IsOk()) {
        std::cerr << "worker failed to create output directory: "
                  << ensure_output_dir.GetMessage() << std::endl;
        return 1;
    }

    RabbitMqBusHarness harness(MakeWorkerBusConfig(uri.Value()));
    Result<void> init_result = harness.Init();
    if (!init_result.IsOk()) {
        std::cerr << "worker init failed: " << init_result.GetMessage() << std::endl;
        return 1;
    }

    module_context::messaging::IMessageBusService* bus = harness.Service();
    if (bus == NULL) {
        std::cerr << "worker bus service unavailable" << std::endl;
        return 1;
    }

    std::mutex state_mutex;
    std::size_t handled_count = 0;
    std::size_t inflight_count = 0;
    std::uint64_t last_activity_ts_ms = NowMs();
    const std::string output_dir_value = output_dir.Value();
    const std::string worker_id_value = worker_id.Value();

    Result<void> register_result = bus->RegisterConsumerHandler(
        "perf_task_consumer",
        [bus,
         &state_mutex,
         &handled_count,
         &inflight_count,
         &last_activity_ts_ms,
         cleanup_inputs,
         simulate_process_ms,
         use_mmap,
         materialize_output,
         &output_dir_value,
         &worker_id_value](const IncomingMessage& incoming) {
            {
                std::lock_guard<std::mutex> lock(state_mutex);
                ++inflight_count;
                last_activity_ts_ms = NowMs();
            }

            Result<module_context::tests::rabbitmq_perf::TaskMessage> task =
                ParseTaskMessage(std::string(incoming.payload.begin(), incoming.payload.end()));
            if (!task.IsOk()) {
                std::cerr << "[worker " << worker_id_value << "] failed to parse task payload: "
                          << task.GetMessage() << std::endl;
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    if (inflight_count > 0) {
                        --inflight_count;
                    }
                    last_activity_ts_ms = NowMs();
                }
                return ConsumeAction::Reject;
            }

            ResultMessage result;
            result.task_id = task.Value().task_id;
            result.worker_id = worker_id_value;
            result.image_path = task.Value().image_path;
            result.output_path = JoinPath(output_dir_value, task.Value().task_id + "_processed.ppm");
            result.status = "processed";
            result.error_message.clear();
            result.image_bytes = task.Value().image_bytes;
            result.publish_ts_ms = task.Value().publish_ts_ms;
            result.worker_receive_ts_ms = NowMs();
            result.read_done_ts_ms = result.worker_receive_ts_ms;
            result.process_done_ts_ms = result.worker_receive_ts_ms;
            result.output_write_done_ts_ms = result.worker_receive_ts_ms;
            result.cleanup_done_ts_ms = result.worker_receive_ts_ms;
            result.result_publish_ts_ms = result.worker_receive_ts_ms;
            result.output_deleted = false;

            std::vector<char> image_data;
            std::uint64_t rolling_checksum = 0;
            if (use_mmap) {
                Result<void> mapped_read_result = ReadMappedFileForProcessing(
                    task.Value().image_path,
                    &result.image_bytes,
                    &rolling_checksum);
                result.read_done_ts_ms = NowMs();
                if (!mapped_read_result.IsOk()) {
                    result.status = "image_read_failed";
                    result.error_message = mapped_read_result.GetMessage();
                }
            } else {
                Result<std::vector<char> > read_result = ReadBinaryFile(task.Value().image_path);
                result.read_done_ts_ms = NowMs();
                if (!read_result.IsOk()) {
                    result.status = "image_read_failed";
                    result.error_message = read_result.GetMessage();
                } else {
                    image_data = read_result.Value();
                    result.image_bytes = image_data.size();
                    for (std::size_t index = 0; index < image_data.size(); index += 4096) {
                        rolling_checksum += static_cast<unsigned char>(image_data[index]);
                    }
                }
            }

            if (result.status == "processed") {
                std::this_thread::sleep_for(std::chrono::milliseconds(simulate_process_ms));
                result.process_done_ts_ms = NowMs();

                if (materialize_output) {
                    if (use_mmap) {
                        std::vector<char> manifest;
                        std::ostringstream stream;
                        stream << "mapped_checksum=" << rolling_checksum << "\n";
                        const std::string manifest_text = stream.str();
                        manifest.assign(manifest_text.begin(), manifest_text.end());
                        Result<void> write_result = WriteBinaryFileRecoverable(result.output_path, manifest, true);
                        result.output_write_done_ts_ms = NowMs();
                        if (!write_result.IsOk()) {
                            result.status = "image_write_failed";
                            result.error_message = write_result.GetMessage();
                        }
                    } else {
                        Result<void> write_result = WriteBinaryFileRecoverable(result.output_path, image_data, false);
                        result.output_write_done_ts_ms = NowMs();
                        if (!write_result.IsOk()) {
                            result.status = "image_write_failed";
                            result.error_message = write_result.GetMessage();
                        }
                    }
                } else {
                    result.output_path.clear();
                    result.output_write_done_ts_ms = result.process_done_ts_ms;
                }
            } else {
                result.process_done_ts_ms = result.read_done_ts_ms;
                result.output_write_done_ts_ms = result.read_done_ts_ms;
            }

            if (cleanup_inputs) {
                Result<void> remove_output_result = foundation::base::MakeSuccess();
                if (result.status == "processed" && !result.output_path.empty()) {
                    remove_output_result = RemoveFileIfExists(result.output_path);
                }
                Result<void> remove_input_result = RemoveFileIfExists(task.Value().image_path);
                result.cleanup_done_ts_ms = NowMs();
                if (result.status == "processed" && !remove_output_result.IsOk()) {
                    result.status = "cleanup_failed";
                    result.error_message = remove_output_result.GetMessage();
                } else if (!remove_input_result.IsOk()) {
                    result.status = "cleanup_failed";
                    result.error_message = remove_input_result.GetMessage();
                } else {
                    result.output_deleted = true;
                }
            } else {
                result.cleanup_done_ts_ms = result.output_write_done_ts_ms;
            }

            PublishRequest publish_request;
            publish_request.exchange = kResultExchange;
            publish_request.routing_key = kResultRoutingKey;
            publish_request.content_type = "text/plain";
            publish_request.correlation_id = result.task_id;
            publish_request.persistent = true;
            result.result_publish_ts_ms = NowMs();
            const std::string payload = SerializeResultMessage(result);
            publish_request.payload.assign(payload.begin(), payload.end());

            Result<void> publish_result = bus->Publish(publish_request);
            if (!publish_result.IsOk()) {
                std::cerr << "[worker " << worker_id_value << "] failed to publish result for task_id="
                          << result.task_id << ": " << publish_result.GetMessage() << std::endl;
                if (cleanup_inputs && !image_data.empty()) {
                    (void)WriteBinaryFileRecoverable(task.Value().image_path, image_data, false);
                }
                {
                    std::lock_guard<std::mutex> lock(state_mutex);
                    if (inflight_count > 0) {
                        --inflight_count;
                    }
                    last_activity_ts_ms = NowMs();
                }
                return ConsumeAction::Requeue;
            }

            {
                std::lock_guard<std::mutex> lock(state_mutex);
                ++handled_count;
                if (inflight_count > 0) {
                    --inflight_count;
                }
                last_activity_ts_ms = NowMs();
                if (handled_count <= 3 || handled_count % 20 == 0) {
                    const std::uint64_t total_ms =
                        result.result_publish_ts_ms >= result.worker_receive_ts_ms
                            ? result.result_publish_ts_ms - result.worker_receive_ts_ms
                            : 0;
                    std::cout << "[worker " << worker_id_value << "] handled=" << handled_count
                              << " task_id=" << result.task_id
                              << " status=" << result.status
                              << " bytes=" << result.image_bytes
                              << " total_ms=" << total_ms << std::endl;
                }
            }

            return ConsumeAction::Ack;
        });
    if (!register_result.IsOk()) {
        std::cerr << "worker failed to register consumer handler: "
                  << register_result.GetMessage() << std::endl;
        (void)harness.Fini();
        return 1;
    }

    Result<void> start_result = harness.Start();
    if (!start_result.IsOk()) {
        std::cerr << "worker start failed: " << start_result.GetMessage() << std::endl;
        (void)harness.Fini();
        return 1;
    }

    Result<void> wait_result = WaitForConnected(bus, timeout_ms);
    if (!wait_result.IsOk()) {
        std::cerr << "worker failed to connect RabbitMQ: " << wait_result.GetMessage() << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    std::cout << "[worker " << worker_id.Value() << "] connected, simulate_process_ms="
              << simulate_process_ms << ", cleanup_inputs=" << (cleanup_inputs ? 1 : 0)
              << ", io_mode=" << io_mode
              << ", materialize_output=" << (materialize_output ? 1 : 0)
              << std::endl;

    const std::uint64_t loop_started_ts_ms = NowMs();
    while (!g_stop_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        const std::uint64_t now_ms = NowMs();
        std::size_t snapshot_handled = 0;
        std::size_t snapshot_inflight = 0;
        std::uint64_t snapshot_last_activity_ms = loop_started_ts_ms;
        {
            std::lock_guard<std::mutex> lock(state_mutex);
            snapshot_handled = handled_count;
            snapshot_inflight = inflight_count;
            snapshot_last_activity_ms = last_activity_ts_ms;
        }

        if (!stop_file.empty()) {
            FILE* stop_handle = std::fopen(stop_file.c_str(), "r");
            if (stop_handle != NULL) {
                std::fclose(stop_handle);
                if (snapshot_inflight == 0 && now_ms >= snapshot_last_activity_ms + 1000) {
                    break;
                }
            }
        }

        if (snapshot_inflight == 0 && snapshot_handled > 0 &&
            now_ms >= snapshot_last_activity_ms + static_cast<std::uint64_t>(idle_timeout_ms)) {
            break;
        }

        if (now_ms >= loop_started_ts_ms + static_cast<std::uint64_t>(timeout_ms)) {
            std::cerr << "[worker " << worker_id.Value() << "] timed out waiting for more work" << std::endl;
            break;
        }
    }

    Result<void> stop_result = harness.Stop();
    Result<void> fini_result = harness.Fini();
    if (!stop_result.IsOk()) {
        std::cerr << "worker stop failed: " << stop_result.GetMessage() << std::endl;
        return 1;
    }
    if (!fini_result.IsOk()) {
        std::cerr << "worker fini failed: " << fini_result.GetMessage() << std::endl;
        return 1;
    }

    std::cout << "[worker " << worker_id.Value() << "] exiting" << std::endl;
    return 0;
}
