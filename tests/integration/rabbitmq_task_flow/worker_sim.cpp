#include "common.h"

#include "foundation/base/ErrorCode.h"

#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace {

using foundation::base::Result;
using module_context::messaging::ConsumeAction;
using module_context::messaging::IncomingMessage;
using module_context::messaging::PublishRequest;
using module_context::tests::rabbitmq_task_flow::EnsureDirectory;
using module_context::tests::rabbitmq_task_flow::JoinPath;
using module_context::tests::rabbitmq_task_flow::ParseArguments;
using module_context::tests::rabbitmq_task_flow::ParseOptionalInt;
using module_context::tests::rabbitmq_task_flow::ParseTaskMessage;
using module_context::tests::rabbitmq_task_flow::RabbitMqBusHarness;
using module_context::tests::rabbitmq_task_flow::ReadBinaryFile;
using module_context::tests::rabbitmq_task_flow::RequireArgument;
using module_context::tests::rabbitmq_task_flow::ResultMessage;
using module_context::tests::rabbitmq_task_flow::SerializeResultMessage;
using module_context::tests::rabbitmq_task_flow::WaitForConnected;
using module_context::tests::rabbitmq_task_flow::WriteBinaryFile;
using module_context::tests::rabbitmq_task_flow::WriteTextFile;

const char kResultExchange[] = "mc.result.exchange";
const char kResultRoutingKey[] = "result.ready";

}  // namespace

int main(int argc, char** argv) {
    const std::map<std::string, std::string> args = ParseArguments(argc, argv);

    Result<std::string> uri = RequireArgument(args, "rabbitmq-uri");
    Result<std::string> output_dir = RequireArgument(args, "output-dir");
    if (!uri.IsOk() || !output_dir.IsOk()) {
        std::cerr << (uri.IsOk() ? output_dir.GetMessage() : uri.GetMessage())
                  << std::endl;
        return 2;
    }

    const int timeout_ms = ParseOptionalInt(args, "timeout-ms", 30000);

    Result<void> ensure_output_result = EnsureDirectory(output_dir.Value());
    if (!ensure_output_result.IsOk()) {
        std::cerr << "worker failed to create output directory: "
                  << ensure_output_result.GetMessage() << std::endl;
        return 1;
    }

    RabbitMqBusHarness harness(
        module_context::tests::rabbitmq_task_flow::MakeWorkerBusConfig(uri.Value()));

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

    std::promise<std::string> completion_promise;
    std::future<std::string> completion_future = completion_promise.get_future();
    bool completion_set = false;
    std::mutex completion_mutex;
    const std::string output_dir_value = output_dir.Value();

    std::cout << "[worker] waiting for task on mc.task.queue" << std::endl;

    Result<void> register_result = bus->RegisterConsumerHandler(
        "task_consumer",
        [bus, &completion_promise, &completion_set, &completion_mutex, &output_dir_value](
            const IncomingMessage& incoming) {
            const std::string payload(incoming.payload.begin(), incoming.payload.end());
            Result<module_context::tests::rabbitmq_task_flow::TaskMessage> task =
                ParseTaskMessage(payload);
            if (!task.IsOk()) {
                std::cerr << "worker failed to parse task payload: "
                          << task.GetMessage() << std::endl;
                return ConsumeAction::Reject;
            }

            std::cout << "[worker] received task payload" << std::endl;

            ResultMessage result_message;
            result_message.task_id = task.Value().task_id;
            result_message.image_path = task.Value().image_path;
            result_message.status = "processed";
            result_message.image_bytes = 0;
            result_message.output_path =
                JoinPath(output_dir_value, task.Value().task_id + "_processed.ppm");
            result_message.report_path =
                JoinPath(output_dir_value, task.Value().task_id + "_result.txt");

            std::cout << "[worker] task_id=" << task.Value().task_id
                      << ", reading image: " << task.Value().image_path << std::endl;

            Result<std::vector<char> > image_data = ReadBinaryFile(task.Value().image_path);
            if (!image_data.IsOk()) {
                result_message.status = "image_read_failed";
            } else {
                result_message.image_bytes = image_data.Value().size();
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                Result<void> image_write_result =
                    WriteBinaryFile(result_message.output_path, image_data.Value());
                if (!image_write_result.IsOk()) {
                    result_message.status = "image_write_failed";
                }
            }

            std::ostringstream report;
            report << "task_id=" << result_message.task_id << "\n";
            report << "image_path=" << result_message.image_path << "\n";
            report << "output_path=" << result_message.output_path << "\n";
            report << "status=" << result_message.status << "\n";
            report << "image_bytes=" << result_message.image_bytes << "\n";
            Result<void> report_write_result =
                WriteTextFile(result_message.report_path, report.str());
            if (!report_write_result.IsOk()) {
                result_message.status = "report_write_failed";
            }

            std::cout << "[worker] writing output image: " << result_message.output_path << std::endl;
            std::cout << "[worker] writing result report: " << result_message.report_path << std::endl;

            const std::string result_payload = SerializeResultMessage(result_message);
            PublishRequest publish_request;
            publish_request.exchange = kResultExchange;
            publish_request.routing_key = kResultRoutingKey;
            publish_request.content_type = "text/plain";
            publish_request.correlation_id = result_message.task_id;
            publish_request.persistent = true;
            publish_request.payload.assign(result_payload.begin(), result_payload.end());

            std::cout << "[worker] publishing result for task_id=" << result_message.task_id
                      << ", status=" << result_message.status << std::endl;

            Result<void> publish_result = bus->Publish(publish_request);
            if (!publish_result.IsOk()) {
                std::cerr << "worker failed to publish result: "
                          << publish_result.GetMessage() << std::endl;
                return ConsumeAction::Requeue;
            }

            std::lock_guard<std::mutex> lock(completion_mutex);
            if (!completion_set) {
                completion_promise.set_value(result_message.status);
                completion_set = true;
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

    if (completion_future.wait_for(std::chrono::milliseconds(timeout_ms)) != std::future_status::ready) {
        std::cerr << "worker timed out waiting for task" << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    const std::string status = completion_future.get();
    std::cout << "worker completed task with status=" << status << std::endl;

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

    return status == "processed" ? 0 : 1;
}
