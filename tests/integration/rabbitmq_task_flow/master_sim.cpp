#include "common.h"

#include "foundation/base/ErrorCode.h"

#include <chrono>
#include <future>
#include <iostream>
#include <mutex>
#include <vector>

namespace {

using foundation::base::Result;
using module_context::messaging::ConsumeAction;
using module_context::messaging::IncomingMessage;
using module_context::messaging::PublishRequest;
using module_context::tests::rabbitmq_task_flow::ParseArguments;
using module_context::tests::rabbitmq_task_flow::ParseOptionalInt;
using module_context::tests::rabbitmq_task_flow::ParseResultMessage;
using module_context::tests::rabbitmq_task_flow::RabbitMqBusHarness;
using module_context::tests::rabbitmq_task_flow::RequireArgument;
using module_context::tests::rabbitmq_task_flow::ResultMessage;
using module_context::tests::rabbitmq_task_flow::SerializeTaskMessage;
using module_context::tests::rabbitmq_task_flow::TaskMessage;
using module_context::tests::rabbitmq_task_flow::WaitForConnected;
using module_context::tests::rabbitmq_task_flow::WriteSamplePpmImage;

const char kTaskExchange[] = "mc.task.exchange";
const char kTaskRoutingKey[] = "task.dispatch";

}  // namespace

int main(int argc, char** argv) {
    const std::map<std::string, std::string> args = ParseArguments(argc, argv);

    Result<std::string> uri = RequireArgument(args, "rabbitmq-uri");
    Result<std::string> image_path = RequireArgument(args, "image-path");
    Result<std::string> task_id = RequireArgument(args, "task-id");
    if (!uri.IsOk() || !image_path.IsOk() || !task_id.IsOk()) {
        std::cerr << (uri.IsOk() ? (image_path.IsOk() ? task_id.GetMessage() : image_path.GetMessage())
                                 : uri.GetMessage())
                  << std::endl;
        return 2;
    }

    const int timeout_ms = ParseOptionalInt(args, "timeout-ms", 20000);

    RabbitMqBusHarness harness(
        module_context::tests::rabbitmq_task_flow::MakeMasterBusConfig(uri.Value()));

    Result<void> init_result = harness.Init();
    if (!init_result.IsOk()) {
        std::cerr << "master init failed: " << init_result.GetMessage() << std::endl;
        return 1;
    }

    module_context::messaging::IMessageBusService* bus = harness.Service();
    if (bus == NULL) {
        std::cerr << "master bus service unavailable" << std::endl;
        return 1;
    }

    std::promise<ResultMessage> result_promise;
    std::future<ResultMessage> result_future = result_promise.get_future();
    bool result_set = false;
    std::mutex result_mutex;
    const std::string expected_task_id = task_id.Value();

    std::cout << "[master] waiting for result on mc.result.queue" << std::endl;

    Result<void> register_result = bus->RegisterConsumerHandler(
        "result_consumer",
        [&result_promise, &result_set, &result_mutex, &expected_task_id](
            const IncomingMessage& incoming) {
            const std::string payload(incoming.payload.begin(), incoming.payload.end());
            Result<ResultMessage> parsed = ParseResultMessage(payload);
            if (!parsed.IsOk()) {
                std::cerr << "master failed to parse result payload: "
                          << parsed.GetMessage() << std::endl;
                return ConsumeAction::Reject;
            }

            if (parsed.Value().task_id != expected_task_id) {
                std::cerr << "master ignored unexpected task result for '"
                          << parsed.Value().task_id << "'" << std::endl;
                return ConsumeAction::Ack;
            }

            std::lock_guard<std::mutex> lock(result_mutex);
            if (!result_set) {
                result_promise.set_value(parsed.Value());
                result_set = true;
            }
            return ConsumeAction::Ack;
        });
    if (!register_result.IsOk()) {
        std::cerr << "master failed to register consumer handler: "
                  << register_result.GetMessage() << std::endl;
        (void)harness.Fini();
        return 1;
    }

    Result<void> start_result = harness.Start();
    if (!start_result.IsOk()) {
        std::cerr << "master start failed: " << start_result.GetMessage() << std::endl;
        (void)harness.Fini();
        return 1;
    }

    Result<void> wait_result = WaitForConnected(bus, timeout_ms);
    if (!wait_result.IsOk()) {
        std::cerr << "master failed to connect RabbitMQ: " << wait_result.GetMessage() << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    std::cout << "[master] connected to RabbitMQ, creating sample image: "
              << image_path.Value() << std::endl;

    Result<void> image_write_result = WriteSamplePpmImage(image_path.Value());
    if (!image_write_result.IsOk()) {
        std::cerr << "master failed to create sample image: "
                  << image_write_result.GetMessage() << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    TaskMessage task_message;
    task_message.task_id = task_id.Value();
    task_message.image_path = image_path.Value();

    const std::string payload_text = SerializeTaskMessage(task_message);
    PublishRequest request;
    request.exchange = kTaskExchange;
    request.routing_key = kTaskRoutingKey;
    request.content_type = "text/plain";
    request.correlation_id = task_id.Value();
    request.persistent = true;
    request.payload.assign(payload_text.begin(), payload_text.end());

    std::cout << "[master] publishing task task_id=" << task_id.Value()
              << ", image_path=" << image_path.Value() << std::endl;

    Result<void> publish_result = bus->Publish(request);
    if (!publish_result.IsOk()) {
        std::cerr << "master failed to publish task: "
                  << publish_result.GetMessage() << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    if (result_future.wait_for(std::chrono::milliseconds(timeout_ms)) != std::future_status::ready) {
        std::cerr << "master timed out waiting for result" << std::endl;
        (void)harness.Stop();
        (void)harness.Fini();
        return 1;
    }

    std::cout << "[master] task published, waiting for result" << std::endl;

    const ResultMessage result_message = result_future.get();
    std::cout << "master received result:" << std::endl;
    std::cout << "  task_id=" << result_message.task_id << std::endl;
    std::cout << "  status=" << result_message.status << std::endl;
    std::cout << "  output_path=" << result_message.output_path << std::endl;
    std::cout << "  report_path=" << result_message.report_path << std::endl;
    std::cout << "  image_bytes=" << result_message.image_bytes << std::endl;

    Result<void> stop_result = harness.Stop();
    Result<void> fini_result = harness.Fini();
    if (!stop_result.IsOk()) {
        std::cerr << "master stop failed: " << stop_result.GetMessage() << std::endl;
        return 1;
    }
    if (!fini_result.IsOk()) {
        std::cerr << "master fini failed: " << fini_result.GetMessage() << std::endl;
        return 1;
    }

    return result_message.status == "processed" ? 0 : 1;
}
