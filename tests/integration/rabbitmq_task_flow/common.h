#pragma once

#include "RabbitMqBusModule.h"

#include "module_context/framework/IContext.h"
#include "module_context/framework/IModuleManager.h"
#include "module_context/messaging/IMessageBusService.h"

#include "foundation/base/Result.h"
#include "foundation/config/ConfigValue.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace module_context {
namespace tests {
namespace rabbitmq_task_flow {

struct TaskMessage {
    std::string task_id;
    std::string image_path;
};

struct ResultMessage {
    std::string task_id;
    std::string image_path;
    std::string output_path;
    std::string report_path;
    std::string status;
    std::size_t image_bytes;
};

class StaticConfigModuleManager : public module_context::framework::IModuleManager {
public:
    explicit StaticConfigModuleManager(
        const foundation::config::ConfigValue& module_config);
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
    module_context::messaging::RabbitMqBusModule* Module();

private:
    StaticConfigModuleManager manager_;
    StaticContext context_;
    module_context::messaging::RabbitMqBusModule module_;
};

foundation::config::ConfigValue MakeMasterBusConfig(const std::string& uri);
foundation::config::ConfigValue MakeWorkerBusConfig(const std::string& uri);

foundation::base::Result<void> WaitForConnected(
    module_context::messaging::IMessageBusService* bus,
    int timeout_ms);

foundation::base::Result<void> EnsureDirectory(const std::string& path);
std::string ParentPath(const std::string& path);
std::string JoinPath(const std::string& left, const std::string& right);

foundation::base::Result<void> WriteSamplePpmImage(const std::string& path);
foundation::base::Result<std::vector<char> > ReadBinaryFile(const std::string& path);
foundation::base::Result<void> WriteBinaryFile(
    const std::string& path,
    const std::vector<char>& data);
foundation::base::Result<void> WriteTextFile(
    const std::string& path,
    const std::string& content);

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
std::map<std::string, std::string> ParseArguments(int argc, char** argv);

}  // namespace rabbitmq_task_flow
}  // namespace tests
}  // namespace module_context
