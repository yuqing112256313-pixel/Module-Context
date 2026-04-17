#pragma once

#include "core/api/framework/IModule.h"
#include "core/api/framework/IModuleManager.h"

#include "foundation/base/NonCopyable.h"
#include "foundation/base/Result.h"
#include "foundation/config/ConfigValue.h"
#include "foundation/plugin/PluginLoader.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace module_context {
namespace framework {

class ModuleManager final
    : public IModuleManager,
      private foundation::base::NonCopyable {
public:
    /// 默认实现：负责插件加载、生命周期转发与模块查询。
    ModuleManager();
    ~ModuleManager() override;

    foundation::base::Result<void> LoadModules(
        const std::string& config_file_path) override;
    foundation::base::Result<void> LoadModule(
        const std::string& name,
        const std::string& library_path) override;

    foundation::base::Result<void> Init(IContext& ctx) override;
    foundation::base::Result<void> Start() override;
    foundation::base::Result<void> Stop() override;
    foundation::base::Result<void> Fini() override;

    foundation::base::Result<IModule*> Module(
        const std::string& name) override;
    foundation::base::Result<foundation::config::ConfigValue> ModuleConfig(
        const std::string& name) override;

private:
    typedef foundation::plugin::PluginLoader<IModule> ModuleLoader;
    typedef ModuleLoader::PluginHandle ModuleHandle;

    /// 打开动态库并创建模块句柄（已校验插件 API 版本）。
    foundation::base::Result<ModuleHandle> CreateModuleHandle(
        const std::string& normalized_library_path);
    /// 将模块保存到映射表，并记录生命周期执行顺序。
    void StoreLoadedModule(
        const std::string& name,
        ModuleHandle module);

private:
    typedef std::unordered_map<std::string, ModuleHandle> ModuleMap;
    typedef std::unordered_map<std::string, foundation::config::ConfigValue> ModuleConfigMap;
    typedef std::vector<std::string> ModuleOrder;

    ModuleMap modules_by_name_;
    ModuleConfigMap configs_by_name_;
    ModuleOrder module_order_;
};

}  // namespace framework
}  // namespace module_context
