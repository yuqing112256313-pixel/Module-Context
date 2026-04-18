#include "common.h"

#include "foundation/base/ErrorCode.h"

#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <sys/stat.h>
#if defined(_WIN32)
#include <direct.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <thread>

namespace module_context {
namespace tests {
namespace rabbitmq_perf {

namespace {

using foundation::base::ErrorCode;
using foundation::base::Result;
using foundation::config::ConfigValue;
using module_context::messaging::ConnectionState;

const char kTaskExchange[] = "mc.perf.task.exchange";
const char kTaskQueue[] = "mc.perf.task.queue";
const char kTaskRoutingKey[] = "task.dispatch";
const char kResultExchange[] = "mc.perf.result.exchange";
const char kResultQueue[] = "mc.perf.result.queue";
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

void SetField(ConfigValue* object, const std::string& key, const ConfigValue& value) {
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

std::string Field(const std::map<std::string, std::string>& fields,
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

std::uint64_t ParseUint64(const std::string& value) {
    return static_cast<std::uint64_t>(std::strtoull(value.c_str(), NULL, 10));
}

std::size_t ParseSizeValue(const std::string& value) {
    return static_cast<std::size_t>(std::strtoull(value.c_str(), NULL, 10));
}

bool ParseBoolValue(const std::string& value) {
    return value == "1" || value == "true" || value == "TRUE" || value == "yes";
}

std::string BoolString(bool value) {
    return value ? "1" : "0";
}

std::string TempPathForRecoverableWrite(const std::string& path) {
    return path + ".tmp";
}

Result<void> RenameReplaceFile(const std::string& from, const std::string& to) {
#if defined(_WIN32)
    if (!MoveFileExA(from.c_str(), to.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return MakeError(
            ErrorCode::kInvalidState,
            "Failed to rename temp file from '" + from + "' to '" + to + "'");
    }
#else
    if (rename(from.c_str(), to.c_str()) != 0) {
        return MakeError(
            ErrorCode::kInvalidState,
            "Failed to rename temp file from '" + from + "' to '" + to + "', errno=" +
                std::to_string(errno));
    }
#endif
    return foundation::base::MakeSuccess();
}

#if !defined(_WIN32)
struct PosixMappedRegion {
    int fd;
    void* address;
    std::size_t size;

    PosixMappedRegion() : fd(-1), address(MAP_FAILED), size(0) {}
};

Result<void> OpenAndMapFile(const std::string& path,
                            bool writable,
                            std::size_t size,
                            PosixMappedRegion* region) {
    const int flags = writable ? (O_RDWR | O_CREAT | O_TRUNC) : O_RDONLY;
    const mode_t mode = 0644;
    region->fd = open(path.c_str(), flags, mode);
    if (region->fd < 0) {
        return MakeError(ErrorCode::kInvalidState, "Failed to open file '" + path + "'");
    }

    if (writable) {
        if (ftruncate(region->fd, static_cast<off_t>(size)) != 0) {
            close(region->fd);
            region->fd = -1;
            return MakeError(ErrorCode::kInvalidState, "Failed to resize file '" + path + "'");
        }
    } else {
        struct stat info;
        if (fstat(region->fd, &info) != 0) {
            close(region->fd);
            region->fd = -1;
            return MakeError(ErrorCode::kInvalidState, "Failed to stat file '" + path + "'");
        }
        size = static_cast<std::size_t>(info.st_size);
    }

    if (size == 0) {
        region->size = 0;
        return foundation::base::MakeSuccess();
    }

    region->address = mmap(
        NULL,
        size,
        writable ? (PROT_READ | PROT_WRITE) : PROT_READ,
        MAP_SHARED,
        region->fd,
        0);
    if (region->address == MAP_FAILED) {
        close(region->fd);
        region->fd = -1;
        return MakeError(ErrorCode::kInvalidState, "Failed to mmap file '" + path + "'");
    }
    region->size = size;
    return foundation::base::MakeSuccess();
}

void CloseMappedRegion(PosixMappedRegion* region, bool flush) {
    if (region->address != MAP_FAILED && region->address != NULL && region->size > 0) {
        if (flush) {
            (void)msync(region->address, region->size, MS_SYNC);
        }
        (void)munmap(region->address, region->size);
    }
    if (region->fd >= 0) {
        if (flush) {
            (void)fsync(region->fd);
        }
        (void)close(region->fd);
    }
    region->fd = -1;
    region->address = MAP_FAILED;
    region->size = 0;
}
#endif

}  // namespace

StaticConfigModuleManager::StaticConfigModuleManager(const ConfigValue& module_config)
    : module_config_(module_config) {
}

StaticConfigModuleManager::~StaticConfigModuleManager() {
}

Result<void> StaticConfigModuleManager::LoadModules(const std::string&) {
    return foundation::base::MakeSuccess();
}

Result<void> StaticConfigModuleManager::LoadModule(const std::string&, const std::string&) {
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
        "No service registered in static perf context");
}

Result<module_context::framework::IModule*> StaticContext::LookupUniqueServiceRaw(
    const char*) {
    return Result<module_context::framework::IModule*>(
        ErrorCode::kNotFound,
        "No service registered in static perf context");
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
    AppendValue(&publishers, MakePublisher("perf_task_producer", kTaskExchange, kTaskRoutingKey));
    SetField(&root, "publishers", publishers);

    ConfigValue consumers = ConfigValue::MakeArray();
    AppendValue(&consumers, MakeConsumer("perf_result_consumer", kResultQueue, 32));
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
    AppendValue(&publishers, MakePublisher("perf_result_producer", kResultExchange, kResultRoutingKey));
    SetField(&root, "publishers", publishers);

    ConfigValue consumers = ConfigValue::MakeArray();
    AppendValue(&consumers, MakeConsumer("perf_task_consumer", kTaskQueue, 1));
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

Result<void> WriteTextFile(const std::string& path, const std::string& content) {
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

Result<std::vector<char> > ReadBinaryFile(const std::string& path) {
    std::ifstream input(path.c_str(), std::ios::in | std::ios::binary | std::ios::ate);
    if (!input.is_open()) {
        return Result<std::vector<char> >(
            ErrorCode::kInvalidState,
            "Failed to open input file '" + path + "'");
    }

    const std::ifstream::pos_type end_position = input.tellg();
    if (end_position < 0) {
        return Result<std::vector<char> >(
            ErrorCode::kInvalidState,
            "Failed to determine input file size for '" + path + "'");
    }

    const std::size_t size = static_cast<std::size_t>(end_position);
    input.seekg(0, std::ios::beg);

    std::vector<char> data(size);
    if (size > 0) {
        input.read(&data[0], static_cast<std::streamsize>(size));
    }
    if (!input.good() && !input.eof()) {
        return Result<std::vector<char> >(
            ErrorCode::kInvalidState,
            "Failed while reading input file '" + path + "'");
    }

    return Result<std::vector<char> >(data);
}

Result<void> WriteBinaryFile(const std::string& path, const std::vector<char>& data) {
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

Result<void> WriteBinaryFileRecoverable(const std::string& path,
                                        const std::vector<char>& data,
                                        bool use_mmap) {
    Result<void> ensure_result = EnsureDirectory(ParentPath(path));
    if (!ensure_result.IsOk()) {
        return ensure_result;
    }

    const std::string temp_path = TempPathForRecoverableWrite(path);
    (void)RemoveFileIfExists(temp_path);

#if !defined(_WIN32)
    if (use_mmap) {
        PosixMappedRegion region;
        Result<void> map_result = OpenAndMapFile(temp_path, true, data.size(), &region);
        if (!map_result.IsOk()) {
            return map_result;
        }
        if (region.size > 0) {
            std::memcpy(region.address, &data[0], region.size);
        }
        CloseMappedRegion(&region, true);
        return RenameReplaceFile(temp_path, path);
    }
#endif

    Result<void> write_result = WriteBinaryFile(temp_path, data);
    if (!write_result.IsOk()) {
        return write_result;
    }
    return RenameReplaceFile(temp_path, path);
}

Result<void> RemoveFileIfExists(const std::string& path) {
    if (path.empty()) {
        return foundation::base::MakeSuccess();
    }
    if (std::remove(path.c_str()) != 0 && errno != ENOENT) {
        return MakeError(ErrorCode::kInvalidState, "Failed to remove file '" + path + "'");
    }
    return foundation::base::MakeSuccess();
}

Result<void> WriteLargePseudoImage(const std::string& path,
                                   std::size_t target_bytes,
                                   int seed,
                                   bool use_mmap) {
    Result<void> ensure_result = EnsureDirectory(ParentPath(path));
    if (!ensure_result.IsOk()) {
        return ensure_result;
    }

    const std::string header = "P6\n4096 1706\n255\n";
    const std::size_t final_size = target_bytes < header.size() ? header.size() : target_bytes;
    const std::string temp_path = TempPathForRecoverableWrite(path);
    (void)RemoveFileIfExists(temp_path);

#if !defined(_WIN32)
    if (use_mmap) {
        PosixMappedRegion region;
        Result<void> map_result = OpenAndMapFile(temp_path, true, final_size, &region);
        if (!map_result.IsOk()) {
            return map_result;
        }

        char* output = static_cast<char*>(region.address);
        if (region.size > 0) {
            std::memcpy(output, header.data(), header.size());
            for (std::size_t index = header.size(); index < final_size; ++index) {
                output[index] = static_cast<char>((seed + static_cast<int>(index)) % 251);
            }
        }
        CloseMappedRegion(&region, true);
        return RenameReplaceFile(temp_path, path);
    }
#endif

    std::ofstream output(temp_path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!output.is_open()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to open image file '" + path + "'");
    }

    output.write(header.data(), static_cast<std::streamsize>(header.size()));
    if (!output.good()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to write image header to '" + path + "'");
    }

    if (final_size > header.size()) {
        const std::size_t payload_size = final_size - header.size();
        const std::size_t block_size = 1024 * 1024;
        std::vector<char> block(block_size);
        for (std::size_t index = 0; index < block.size(); ++index) {
            block[index] = static_cast<char>((seed + static_cast<int>(index)) % 251);
        }

        std::size_t written = 0;
        while (written < payload_size) {
            const std::size_t remaining = payload_size - written;
            const std::size_t chunk = remaining < block.size() ? remaining : block.size();
            output.write(&block[0], static_cast<std::streamsize>(chunk));
            if (!output.good()) {
                return MakeError(ErrorCode::kInvalidState, "Failed while writing pseudo image '" + path + "'");
            }
            written += chunk;
        }
    }

    output.close();
    if (!output.good()) {
        return MakeError(ErrorCode::kInvalidState, "Failed to finalize pseudo image '" + path + "'");
    }

    return RenameReplaceFile(temp_path, path);
}

Result<void> ReadMappedFileForProcessing(const std::string& path,
                                         std::size_t* image_bytes,
                                         std::uint64_t* rolling_checksum) {
#if defined(_WIN32)
    Result<std::vector<char> > data = ReadBinaryFile(path);
    if (!data.IsOk()) {
        return Result<void>(data.GetCode(), data.GetMessage());
    }
    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < data.Value().size(); index += 4096) {
        checksum += static_cast<unsigned char>(data.Value()[index]);
    }
    if (image_bytes != NULL) {
        *image_bytes = data.Value().size();
    }
    if (rolling_checksum != NULL) {
        *rolling_checksum = checksum;
    }
    return foundation::base::MakeSuccess();
#else
    PosixMappedRegion region;
    Result<void> map_result = OpenAndMapFile(path, false, 0, &region);
    if (!map_result.IsOk()) {
        return map_result;
    }

    const unsigned char* bytes = static_cast<const unsigned char*>(region.address);
    std::uint64_t checksum = 0;
    for (std::size_t index = 0; index < region.size; index += 4096) {
        checksum += bytes[index];
    }
    if (region.size > 0) {
        checksum += bytes[region.size - 1];
    }

    if (image_bytes != NULL) {
        *image_bytes = region.size;
    }
    if (rolling_checksum != NULL) {
        *rolling_checksum = checksum;
    }
    CloseMappedRegion(&region, false);
    return foundation::base::MakeSuccess();
#endif
}

std::uint64_t NowMs() {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
}

std::string SerializeTaskMessage(const TaskMessage& message) {
    std::ostringstream output;
    output << "task_id=" << message.task_id << "\n";
    output << "image_path=" << message.image_path << "\n";
    output << "image_bytes=" << message.image_bytes << "\n";
    output << "publish_ts_ms=" << message.publish_ts_ms << "\n";
    return output.str();
}

Result<TaskMessage> ParseTaskMessage(const std::string& payload) {
    const std::map<std::string, std::string> fields = ParseKeyValuePayload(payload);
    std::string missing_key;
    TaskMessage message;
    message.task_id = Field(fields, "task_id", &missing_key);
    if (message.task_id.empty()) {
        return MakeTaskError(ErrorCode::kParseError, "Task payload missing field '" + missing_key + "'");
    }
    message.image_path = Field(fields, "image_path", &missing_key);
    if (message.image_path.empty()) {
        return MakeTaskError(ErrorCode::kParseError, "Task payload missing field '" + missing_key + "'");
    }
    const std::string image_bytes = Field(fields, "image_bytes", &missing_key);
    if (image_bytes.empty()) {
        return MakeTaskError(ErrorCode::kParseError, "Task payload missing field '" + missing_key + "'");
    }
    const std::string publish_ts_ms = Field(fields, "publish_ts_ms", &missing_key);
    if (publish_ts_ms.empty()) {
        return MakeTaskError(ErrorCode::kParseError, "Task payload missing field '" + missing_key + "'");
    }
    message.image_bytes = ParseSizeValue(image_bytes);
    message.publish_ts_ms = ParseUint64(publish_ts_ms);
    return Result<TaskMessage>(message);
}

std::string SerializeResultMessage(const ResultMessage& message) {
    std::ostringstream output;
    output << "task_id=" << message.task_id << "\n";
    output << "worker_id=" << message.worker_id << "\n";
    output << "image_path=" << message.image_path << "\n";
    output << "output_path=" << message.output_path << "\n";
    output << "status=" << message.status << "\n";
    output << "error_message=" << message.error_message << "\n";
    output << "image_bytes=" << message.image_bytes << "\n";
    output << "publish_ts_ms=" << message.publish_ts_ms << "\n";
    output << "worker_receive_ts_ms=" << message.worker_receive_ts_ms << "\n";
    output << "read_done_ts_ms=" << message.read_done_ts_ms << "\n";
    output << "process_done_ts_ms=" << message.process_done_ts_ms << "\n";
    output << "output_write_done_ts_ms=" << message.output_write_done_ts_ms << "\n";
    output << "cleanup_done_ts_ms=" << message.cleanup_done_ts_ms << "\n";
    output << "result_publish_ts_ms=" << message.result_publish_ts_ms << "\n";
    output << "output_deleted=" << BoolString(message.output_deleted) << "\n";
    return output.str();
}

Result<ResultMessage> ParseResultMessage(const std::string& payload) {
    const std::map<std::string, std::string> fields = ParseKeyValuePayload(payload);
    std::string missing_key;
    ResultMessage message;
    message.task_id = Field(fields, "task_id", &missing_key);
    if (message.task_id.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.worker_id = Field(fields, "worker_id", &missing_key);
    if (message.worker_id.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.image_path = Field(fields, "image_path", &missing_key);
    message.output_path = Field(fields, "output_path", &missing_key);
    message.status = Field(fields, "status", &missing_key);
    if (message.status.empty()) {
        return MakeResultError(ErrorCode::kParseError, "Result payload missing field '" + missing_key + "'");
    }
    message.error_message = Field(fields, "error_message", NULL);
    message.image_bytes = ParseSizeValue(Field(fields, "image_bytes", &missing_key));
    message.publish_ts_ms = ParseUint64(Field(fields, "publish_ts_ms", &missing_key));
    message.worker_receive_ts_ms = ParseUint64(Field(fields, "worker_receive_ts_ms", &missing_key));
    message.read_done_ts_ms = ParseUint64(Field(fields, "read_done_ts_ms", &missing_key));
    message.process_done_ts_ms = ParseUint64(Field(fields, "process_done_ts_ms", &missing_key));
    message.output_write_done_ts_ms = ParseUint64(Field(fields, "output_write_done_ts_ms", &missing_key));
    message.cleanup_done_ts_ms = ParseUint64(Field(fields, "cleanup_done_ts_ms", &missing_key));
    message.result_publish_ts_ms = ParseUint64(Field(fields, "result_publish_ts_ms", &missing_key));
    message.output_deleted = ParseBoolValue(Field(fields, "output_deleted", &missing_key));
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

std::size_t ParseOptionalSize(const std::map<std::string, std::string>& args,
                              const std::string& key,
                              std::size_t fallback_value) {
    std::map<std::string, std::string>::const_iterator it = args.find(key);
    if (it == args.end() || it->second.empty()) {
        return fallback_value;
    }
    return ParseSizeValue(it->second);
}

bool ParseOptionalBool(const std::map<std::string, std::string>& args,
                       const std::string& key,
                       bool fallback_value) {
    std::map<std::string, std::string>::const_iterator it = args.find(key);
    if (it == args.end() || it->second.empty()) {
        return fallback_value;
    }
    return ParseBoolValue(it->second);
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

}  // namespace rabbitmq_perf
}  // namespace tests
}  // namespace module_context
