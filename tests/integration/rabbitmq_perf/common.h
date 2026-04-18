#pragma once

#include "RabbitMqBusModule.h"

#include "module_context/framework/IContext.h"
#include "module_context/framework/IModuleManager.h"
#include "module_context/messaging/IMessageBusService.h"

#include "foundation/base/Result.h"
#include "foundation/config/ConfigValue.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace module_context {
namespace tests {
namespace rabbitmq_perf {

struct TaskMessage {
    std::string task_id;
    std::string image_path;
    std::size_t image_bytes;
    std::uint64_t publish_ts_ms;
};

struct ResultMessage {
    std::string task_id;
    std::string worker_id;
    std::string image_path;
    std::string output_path;
    std::string status;
    std::string error_message;
    std::size_t image_bytes;
    std::uint64_t publish_ts_ms;
    std::uint64_t worker_receive_ts_ms;
    std::uint64_t read_done_ts_ms;
    std::uint64_t process_done_ts_ms;
    std::uint64_t output_write_done_ts_ms;
    std::uint64_t cleanup_done_ts_ms;
    std::uint64_t result_publish_ts_ms;
    bool output_deleted;
};

class StaticConfigModuleManager : public module_context::framework::IModuleManager {
public:
    explicit StaticConfigModuleManager(const foundation::config::ConfigValue& module_config);
    ~StaticConfigModuleManager() override;

    foundation::base::Result<void> LoadModules(const std::string& path) override;
    foundation::base::Result<void> LoadModule(
        const std::string& name,
        const std::string& library_path) override;
    foundation::base::Result<void> Init(module_context::framework::IContext& ctx) override;
    foundation::base::Result<void> Start() override;
    foundation::base::Result<void> Stop() override;
    foundation::base::Result<void> Fini() override;
    foundation::base::Result<foundation::config::ConfigValue> ModuleConfig(
        const std::string& name) override;

private:
    foundation::base::Result<module_context::framework::IModule*> LookupModuleRaw(
        const std::string& name) override;

private:
    foundation::config::ConfigValue module_config_;
};

class StaticContext : public module_context::framework::IContext {
public:
    explicit StaticContext(module_context::framework::IModuleManager* manager);
    ~StaticContext() override;

    foundation::base::Result<void> Init() override;
    foundation::base::Result<void> Start() override;
    foundation::base::Result<void> Stop() override;
    foundation::base::Result<void> Fini() override;
    module_context::framework::IModuleManager* ModuleManager() override;

private:
    foundation::base::Result<module_context::framework::IModule*> LookupServiceRaw(
        const char* service_key,
        const std::string& name) override;
    foundation::base::Result<module_context::framework::IModule*> LookupUniqueServiceRaw(
        const char* service_key) override;

private:
    module_context::framework::IModuleManager* manager_;
};

class RabbitMqBusHarness {
public:
    explicit RabbitMqBusHarness(const foundation::config::ConfigValue& module_config);
    ~RabbitMqBusHarness();

    foundation::base::Result<void> Init();
    foundation::base::Result<void> Start();
    foundation::base::Result<void> Stop();
    foundation::base::Result<void> Fini();
    module_context::messaging::IMessageBusService* Service();

private:
    StaticConfigModuleManager manager_;
    StaticContext context_;
    module_context::messaging::RabbitMqBusModule module_;
};

foundation::config::ConfigValue MakeMasterBusConfig(
    const std::string& uri,
    int result_consumer_threads,
    int result_consumer_prefetch);
foundation::config::ConfigValue MakeMasterPublisherBusConfig(
    const std::string& uri,
    int publisher_bus_threads);
foundation::config::ConfigValue MakeWorkerBusConfig(
    const std::string& uri,
    int worker_bus_threads,
    int consumer_prefetch);

foundation::base::Result<void> WaitForConnected(
    module_context::messaging::IMessageBusService* bus,
    int timeout_ms);

foundation::base::Result<void> EnsureDirectory(const std::string& path);
std::string ParentPath(const std::string& path);
std::string JoinPath(const std::string& left, const std::string& right);
foundation::base::Result<void> WriteTextFile(
    const std::string& path,
    const std::string& content);
foundation::base::Result<std::vector<char> > ReadBinaryFile(const std::string& path);
foundation::base::Result<void> WriteBinaryFile(
    const std::string& path,
    const std::vector<char>& data);
foundation::base::Result<void> WriteBinaryFileRecoverable(
    const std::string& path,
    const std::vector<char>& data,
    bool use_mmap);
foundation::base::Result<void> RemoveFileIfExists(const std::string& path);
foundation::base::Result<void> WriteLargePseudoImage(
    const std::string& path,
    std::size_t target_bytes,
    int seed,
    bool use_mmap);
foundation::base::Result<void> ReadMappedFileForProcessing(
    const std::string& path,
    std::size_t* image_bytes,
    std::uint64_t* rolling_checksum);
std::uint64_t NowMs();

std::string SerializeTaskMessage(const TaskMessage& message);
foundation::base::Result<TaskMessage> ParseTaskMessage(const std::string& payload);
std::string SerializeResultMessage(const ResultMessage& message);
foundation::base::Result<ResultMessage> ParseResultMessage(const std::string& payload);

foundation::base::Result<std::string> RequireArgument(
    const std::map<std::string, std::string>& args,
    const std::string& key);
int ParseOptionalInt(
    const std::map<std::string, std::string>& args,
    const std::string& key,
    int fallback_value);
std::size_t ParseOptionalSize(
    const std::map<std::string, std::string>& args,
    const std::string& key,
    std::size_t fallback_value);
bool ParseOptionalBool(
    const std::map<std::string, std::string>& args,
    const std::string& key,
    bool fallback_value);
std::map<std::string, std::string> ParseArguments(int argc, char** argv);

}  // namespace rabbitmq_perf
}  // namespace tests
}  // namespace module_context
