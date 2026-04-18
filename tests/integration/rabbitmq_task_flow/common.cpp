#include "common.h"

#include "foundation/base/ErrorCode.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <sstream>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#endif
#include <thread>

namespace module_context {
namespace tests {
namespace rabbitmq_task_flow {

namespace {

using foundation::base::ErrorCode;
using foundation::base::Result;
using foundation::config::ConfigValue;
using module_context::messaging::ConnectionState;
using module_context::messaging::ConsumerSpec;
using module_context::messaging::ExchangeSpec;
using module_context::messaging::ExchangeType;
using module_context::messaging::QueueSpec;

const char kTaskExchange[] = "mc.task.exchange";
const char kTaskQueue[] = "mc.task.queue";
const char kTaskRoutingKey[] = "task.dispatch";
const char kResultExchange[] = "mc.result.exchange";
const char kResultQueue[] = "mc.result.queue";
const char kResultRoutingKey[] = "result.ready";

Result<void> MakeError(ErrorCode code, const std::string& message) {
    return Result<void>(code, message);
}

Result<std::string> MakeStringError(ErrorCode code, const std::string& message) {
    return Result<std::string>(code, message);
}

Result<TaskMessage> MakeTaskError(ErrorCode code, const std::string& message) {
    return Result<TaskMessage>(code, message);
}

Result<ResultMessage> MakeResultError(ErrorCode code, const std::string& message) {
    return Result<ResultMessage>(code, message);
}

void SetField(ConfigValue* object,
              const std::string& key,
              const ConfigValue& value) {
    (void)object->Set(key, value);
}

void AppendValue(ConfigValue* array, const ConfigValue& value) {
    (void)array->Append(value);
}

ConfigValue MakeConnectionConfig(const std::string& uri) {
    ConfigValue connection = ConfigValue::MakeObject();
    SetField(&connection, "uri", ConfigValue(uri));
    SetField(&connection, "heartbeat_seconds", ConfigValue(10));
    SetField(&connection, "socket_timeout_ms", ConfigValue(200));

    ConfigValue reconnect = ConfigValue::MakeObject();
    SetField(&reconnect, "enabled", ConfigValue(true));
    SetField(&reconnect, "initial_delay_ms", ConfigValue(200));
    SetField(&reconnect, "max_delay_ms", ConfigValue(2000));
    SetField(&connection, "reconnect", reconnect);
    return connection;
}

ConfigValue MakeWorkerPoolConfig() {
    ConfigValue worker_pool = ConfigValue::MakeObject();
    SetField(&worker_pool, "thread_count", ConfigValue(2));
    return worker_pool;
}

ConfigValue MakeExchange(const std::string& name) {
    ConfigValue exchange = ConfigValue::MakeObject();
    SetField(&exchange, "name", ConfigValue(name));
    SetField(&exchange, "type", ConfigValue("direct"));
    SetField(&exchange, "durable", ConfigValue(true));
    SetField(&exchange, "passive", ConfigValue(true));
    return exchange;
}

ConfigValue MakeQueue(const std::string& name) {
    ConfigValue queue = ConfigValue::MakeObject();
    SetField(&queue, "name", ConfigValue(name));
    SetField(&queue, "durable", ConfigValue(true));
    SetField(&queue, "passive", ConfigValue(true));
    return queue;
}

ConfigValue MakePublisher(const std::string& name,
                          const std::string& exchange,
                          const std::string& routing_key) {
    ConfigValue publisher = ConfigValue::MakeObject();
    SetField(&publisher, "name", ConfigValue(name));
    SetField(&publisher, "exchange", ConfigValue(exchange));
    SetField(&publisher, "routing_key", ConfigValue(routing_key));
    SetField(&publisher, "persistent", ConfigValue(true));
    SetField(&publisher, "content_type", ConfigValue("text/plain"));
    return publisher;
}

ConfigValue MakeConsumer(const std::string& name,
                         const std::string& queue,
                         int prefetch_count) {
    ConfigValue consumer = ConfigValue::MakeObject();
    SetField(&consumer, "name", ConfigValue(name));
    SetField(&consumer, "queue", ConfigValue(queue));
    SetField(&consumer, "prefetch_count", ConfigValue(prefetch_count));
    SetField(&consumer, "auto_ack", ConfigValue(false));
    return consumer;
}

bool DirectoryExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }

    return (info.st_mode & S_IFDIR) != 0;
}

int MakeSingleDirectory(const std::string& path) {
#if defined(_WIN32)
    return _mkdir(path.c_str());
#else
    return mkdir(path.c_str(), 0755);
#endif
}

std::string Trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() &&
           (value[begin] == ' ' || value[begin] == '\t' || value[begin] == '\r' || value[begin] == '\n')) {
        ++begin;
    }

    std::size_t end = value.size();
    while (end > begin &&
           (value[end - 1] == ' ' || value[end - 1] == '\t' || value[end - 1] == '\r' || value[end - 1] == '\n')) {
        --end;
    }

    return value.substr(begin, end - begin);
}

std::string GetRequiredField(const std::map<std::string, std::string>& fields,
                             const std::string& key,
                             std::string* missing_key) {
    std::map<std::string, std::string>::const_iterator it = fields.find(key);
    if (it == fields.end()) {
        if (missing_key != NULL) {
            *missing_key = key;
        }
        return std::string();
    }

    return it->second;
}

std::map<std::string, std::string> ParseKeyValuePayload(const std::string& payload) {
    std::map<std::string, std::string> fields;
    std::istringstream stream(payload);
    std::string line;
    while (std::getline(stream, line)) {
        const std::size_t delimiter = line.find('=');
        if (delimiter == std::string::npos) {
            continue;
        }

        const std::string key = Trim(line.substr(0, delimiter));
        const std::string value = Trim(line.substr(delimiter + 1));
        if (!key.empty()) {
            fields[key] = value;
        }
    }

    return fields;
}

}  // namespace

StaticConfigModuleManager::StaticConfigModuleManager(
    const ConfigValue& module_config)
    : module_config_(module_config) {
}

StaticConfigModuleManager::~StaticConfigModuleManager() {
}

Result<void> StaticConfigModuleManager::LoadModules(const std::string&) {
    return foundation::base::MakeSuccess();
}

Result<void> StaticConfigModuleManager::LoadModule(
    const std::string&,
    const std::string&) {
    return foundation::base::MakeSuccess();
}

Result<void> StaticConfigModuleManager::Init(module_context::framework::IContext&) {
    return foundation::base::MakeSuccess();
}

Result<void> StaticConfigModuleManager::Start() {
    return foundation::base::MakeSuccess();
}

Result<void> StaticConfigModuleManager::Stop() {
    return foundation::base::MakeSuccess();
}

Result<void> StaticConfigModuleManager::Fini() {
    return foundation::base::MakeSuccess();
}

Result<ConfigValue> StaticConfigModuleManager::ModuleConfig(const std::string&) {
    return Result<ConfigValue>(module_config_);
}

Result<module_context::framework::IModule*> StaticConfigModuleManager::LookupModuleRaw(
    const std::string& name) {
    return Result<module_context::framework::IModule*>(
        ErrorCode::kNotFound,
        "No module registered for '" + name + "'");
}

StaticContext::StaticContext(module_context::framework::IModuleManager* manager)
    : manager_(manager) {
}

StaticContext::~StaticContext() {
}

Result<void> StaticContext::Init() {
    return foundation::base::MakeSuccess();
}

Result<void> StaticContext::Start() {
    return foundation::base::MakeSuccess();
}

Result<void> StaticContext::Stop() {
    return foundation::base::MakeSuccess();
}

Result<void> StaticContext::Fini() {
    return foundation::base::MakeSuccess();
}

module_context::framework::IModuleManager* StaticContext::ModuleManager() {
    return manager_;
}

Result<module_context::framework::IModule*> StaticContext::LookupServiceRaw(
    const char*,
    const std::string&) {
    return Result<module_context::framework::IModule*>(
        ErrorCode::kNotFound,
        "No service registered in static test context");
}

Result<module_context::framework::IModule*> StaticContext::LookupUniqueServiceRaw(
    const char*) {
    return Result<module_context::framework::IModule*>(
        ErrorCode::kNotFound,
        "No service registered in static test context");
}

RabbitMqBusHarness::RabbitMqBusHarness(const ConfigValue& module_config)
    : manager_(module_config),
      context_(&manager_),
      module_() {
}

RabbitMqBusHarness::~RabbitMqBusHarness() {
}

Result<void> RabbitMqBusHarness::Init() {
    return module_.Init(context_);
}

Result<void> RabbitMqBusHarness::Start() {
    return module_.Start();
}

Result<void> RabbitMqBusHarness::Stop() {
    return module_.Stop();
}

Result<void> RabbitMqBusHarness::Fini() {
    return module_.Fini();
}

module_context::messaging::IMessageBusService* RabbitMqBusHarness::Service() {
    return &module_;
}

module_context::messaging::RabbitMqBusModule* RabbitMqBusHarness::Module() {
    return &module_;
}

ConfigValue MakeMasterBusConfig(const std::string& uri) {
    ConfigValue root = ConfigValue::MakeObject();
    SetField(&root, "connection", MakeConnectionConfig(uri));
    SetField(&root, "worker_pool", MakeWorkerPoolConfig());

    ConfigValue topology = ConfigValue::MakeObject();
    ConfigValue exchanges = ConfigValue::MakeArray();
    AppendValue(&exchanges, MakeExchange(kTaskExchange));
    SetField(&topology, "exchanges", exchanges);

    ConfigValue queues = ConfigValue::MakeArray();
    AppendValue(&queues, MakeQueue(kResultQueue));
    SetField(&topology, "queues", queues);
    SetField(&root, "topology", topology);

    ConfigValue publishers = ConfigValue::MakeArray();
    AppendValue(&publishers, MakePublisher("task_producer", kTaskExchange, kTaskRoutingKey));
    SetField(&root, "publishers", publishers);

    ConfigValue consumers = ConfigValue::MakeArray();
    AppendValue(&consumers, MakeConsumer("result_consumer", kResultQueue, 1));
    SetField(&root, "consumers", consumers);

    return root;
}

ConfigValue MakeWorkerBusConfig(const std::string& uri) {
    ConfigValue root = ConfigValue::MakeObject();
    SetField(&root, "connection", MakeConnectionConfig(uri));
    SetField(&root, "worker_pool", MakeWorkerPoolConfig());

    ConfigValue topology = ConfigValue::MakeObject();
    ConfigValue exchanges = ConfigValue::MakeArray();
    AppendValue(&exchanges, MakeExchange(kResultExchange));
    SetField(&topology, "exchanges", exchanges);

    ConfigValue queues = ConfigValue::MakeArray();
    AppendValue(&queues, MakeQueue(kTaskQueue));
    SetField(&topology, "queues", queues);
    SetField(&root, "topology", topology);

    ConfigValue publishers = ConfigValue::MakeArray();
    AppendValue(&publishers, MakePublisher("result_producer", kResultExchange, kResultRoutingKey));
    SetField(&root, "publishers", publishers);

    ConfigValue consumers = ConfigValue::MakeArray();
    AppendValue(&consumers, MakeConsumer("task_consumer", kTaskQueue, 1));
    SetField(&root, "consumers", consumers);

    return root;
}

Result<void> WaitForConnected(module_context::messaging::IMessageBusService* bus,
                              int timeout_ms) {
    if (bus == NULL) {
        return MakeError(ErrorCode::kInvalidArgument, "IMessageBusService is null");
    }

    const std::chrono::steady_clock::time_point deadline =
        std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);

    while (std::chrono::steady_clock::now() < deadline) {
        if (bus->GetConnectionState() == ConnectionState::Connected) {
            return foundation::base::MakeSuccess();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::ostringstream message;
    message << "Timed out waiting for RabbitMQ connection, last state="
            << static_cast<int>(bus->GetConnectionState());
    return MakeError(ErrorCode::kInvalidState, message.str());
}

Result<void> EnsureDirectory(const std::string& path) {
    if (path.empty() || path == ".") {
        return foundation::base::MakeSuccess();
    }

    if (DirectoryExists(path)) {
        return foundation::base::MakeSuccess();
    }

    const std::string parent = ParentPath(path);
    if (!parent.empty() && parent != path) {
        Result<void> parent_result = EnsureDirectory(parent);
        if (!parent_result.IsOk()) {
            return parent_result;
        }
    }

    if (MakeSingleDirectory(path) != 0 && errno != EEXIST) {
        return MakeError(
            ErrorCode::kInvalidState,
            "Failed to create directory '" + path + "', errno=" + std::to_string(errno));
    }

    return foundation::base::MakeSuccess();
}

std::string ParentPath(const std::string& path) {
    const std::size_t delimiter = path.find_last_of("/\\");
    if (delimiter == std::string::npos) {
        return std::string();
    }
    if (delimiter == 0) {
        return path.substr(0, 1);
    }
    return path.substr(0, delimiter);
}

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (right.empty()) {
        return left;
    }
    if (left[left.size() - 1] == '/' || left[left.size() - 1] == '\\') {
        return left + right;
    }
    return left + "/" + right;
}

Result<void> WriteSamplePpmImage(const std::string& path) {
    Result<void> ensure_result = EnsureDirectory(ParentPath(path));
    if (!ensure_result.IsOk()) {
        return ensure_result;
    }

    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to open image file '" + path + "'");
    }

    output << "P3\n";
    output << "4 4\n";
    output << "255\n";
    output << "255 0 0   255 255 0   0 255 0   0 255 255\n";
    output << "255 0 255 0 0 255     255 255 255 128 128 128\n";
    output << "32 64 128  64 128 32  200 80 40   40 80 200\n";
    output << "10 20 30   30 20 10   220 180 40  60 160 220\n";
    output.close();

    if (!output.good()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to write image file '" + path + "'");
    }

    return foundation::base::MakeSuccess();
}

Result<std::vector<char> > ReadBinaryFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        return Result<std::vector<char> >(
            ErrorCode::kInvalidState,
            "Failed to open input file '" + path + "'");
    }

    std::vector<char> data((std::istreambuf_iterator<char>(input)),
                           std::istreambuf_iterator<char>());
    if (input.bad()) {
        return Result<std::vector<char> >(
            ErrorCode::kInvalidState,
            "Failed while reading input file '" + path + "'");
    }

    return Result<std::vector<char> >(data);
}

Result<void> WriteBinaryFile(const std::string& path,
                             const std::vector<char>& data) {
    Result<void> ensure_result = EnsureDirectory(ParentPath(path));
    if (!ensure_result.IsOk()) {
        return ensure_result;
    }

    std::ofstream output(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to open output file '" + path + "'");
    }

    if (!data.empty()) {
        output.write(&data[0], static_cast<std::streamsize>(data.size()));
    }
    output.close();

    if (!output.good()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to write output file '" + path + "'");
    }

    return foundation::base::MakeSuccess();
}

Result<void> WriteTextFile(const std::string& path,
                           const std::string& content) {
    Result<void> ensure_result = EnsureDirectory(ParentPath(path));
    if (!ensure_result.IsOk()) {
        return ensure_result;
    }

    std::ofstream output(path.c_str(), std::ios::out | std::ios::trunc);
    if (!output.is_open()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to open text file '" + path + "'");
    }

    output << content;
    output.close();

    if (!output.good()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to write text file '" + path + "'");
    }

    return foundation::base::MakeSuccess();
}

std::string SerializeTaskMessage(const TaskMessage& message) {
    std::ostringstream output;
    output << "task_id=" << message.task_id << "\n";
    output << "image_path=" << message.image_path << "\n";
    return output.str();
}

Result<TaskMessage> ParseTaskMessage(const std::string& payload) {
    const std::map<std::string, std::string> fields = ParseKeyValuePayload(payload);
    std::string missing_key;
    TaskMessage message;
    message.task_id = GetRequiredField(fields, "task_id", &missing_key);
    if (message.task_id.empty()) {
        return MakeTaskError(ErrorCode::kParseError, "Task payload missing field '" + missing_key + "'");
    }
    message.image_path = GetRequiredField(fields, "image_path", &missing_key);
    if (message.image_path.empty()) {
        return MakeTaskError(ErrorCode::kParseError, "Task payload missing field '" + missing_key + "'");
    }
    return Result<TaskMessage>(message);
}

std::string SerializeResultMessage(const ResultMessage& message) {
    std::ostringstream output;
    output << "task_id=" << message.task_id << "\n";
    output << "image_path=" << message.image_path << "\n";
    output << "output_path=" << message.output_path << "\n";
    output << "report_path=" << message.report_path << "\n";
    output << "status=" << message.status << "\n";
    output << "image_bytes=" << message.image_bytes << "\n";
    return output.str();
}

Result<ResultMessage> ParseResultMessage(const std::string& payload) {
    const std::map<std::string, std::string> fields = ParseKeyValuePayload(payload);
    std::string missing_key;
    ResultMessage message;
    message.task_id = GetRequiredField(fields, "task_id", &missing_key);
    if (message.task_id.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.image_path = GetRequiredField(fields, "image_path", &missing_key);
    if (message.image_path.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.output_path = GetRequiredField(fields, "output_path", &missing_key);
    if (message.output_path.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.report_path = GetRequiredField(fields, "report_path", &missing_key);
    if (message.report_path.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.status = GetRequiredField(fields, "status", &missing_key);
    if (message.status.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }

    const std::string bytes_string = GetRequiredField(fields, "image_bytes", &missing_key);
    if (bytes_string.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.image_bytes = static_cast<std::size_t>(std::strtoul(bytes_string.c_str(), NULL, 10));
    return Result<ResultMessage>(message);
}

Result<std::string> RequireArgument(const std::map<std::string, std::string>& args,
                                    const std::string& key) {
    std::map<std::string, std::string>::const_iterator it = args.find(key);
    if (it == args.end() || it->second.empty()) {
        return MakeStringError(ErrorCode::kInvalidArgument, "Missing required argument '--" + key + "'");
    }
    return Result<std::string>(it->second);
}

int ParseOptionalInt(const std::map<std::string, std::string>& args,
                     const std::string& key,
                     int fallback_value) {
    std::map<std::string, std::string>::const_iterator it = args.find(key);
    if (it == args.end() || it->second.empty()) {
        return fallback_value;
    }
    return std::atoi(it->second.c_str());
}

std::map<std::string, std::string> ParseArguments(int argc, char** argv) {
    std::map<std::string, std::string> args;
    for (int index = 1; index < argc; ++index) {
        const std::string token(argv[index]);
        if (token.size() <= 2 || token[0] != '-' || token[1] != '-') {
            continue;
        }

        const std::string key = token.substr(2);
        std::string value = "1";
        if (index + 1 < argc) {
            const std::string next(argv[index + 1]);
            if (!(next.size() > 1 && next[0] == '-' && next[1] == '-')) {
                value = next;
                ++index;
            }
        }
        args[key] = value;
    }
    return args;
}

}  // namespace rabbitmq_task_flow
}  // namespace tests
}  // namespace module_context
