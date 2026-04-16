#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/imodule.h"
#include "core/api/framework/imodule_manager.h"

#include "foundation/base/NonCopyable.h"
#include "foundation/base/Result.h"
#include "foundation/plugin/PluginLoader.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace module_context {
namespace framework {

class MC_FRAMEWORK_API ModuleManager final
    : public IModuleManager,
      private foundation::base::NonCopyable {
public:
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

private:
    typedef foundation::plugin::PluginLoader<IModule> ModuleLoader;
    typedef ModuleLoader::PluginHandle ModuleHandle;

    foundation::base::Result<ModuleHandle> CreateModuleHandle(
        const std::string& normalized_library_path);
    void StoreLoadedModule(
        const std::string& name,
        ModuleHandle module);

private:
    typedef std::unordered_map<std::string, ModuleHandle> ModuleMap;
    typedef std::vector<std::string> ModuleOrder;

    ModuleMap modules_by_name_;
    ModuleOrder module_order_;
};

}  // namespace framework
}  // namespace module_context
