#pragma once

#include "core/api/framework/Export.h"
#include "core/api/framework/IModule.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"
#include "foundation/config/ConfigValue.h"

#include <string>

namespace module_context {
namespace framework {

class IContext;

class MC_FRAMEWORK_API IModuleManager {
public:
    virtual ~IModuleManager() {}

    virtual foundation::base::Result<void> LoadModules(
        const std::string& config_file_path) = 0;
    virtual foundation::base::Result<void> LoadModule(
        const std::string& name,
        const std::string& library_path) = 0;

    virtual foundation::base::Result<void> Init(IContext& ctx) = 0;
    virtual foundation::base::Result<void> Start() = 0;
    virtual foundation::base::Result<void> Stop() = 0;
    virtual foundation::base::Result<void> Fini() = 0;

    virtual foundation::base::Result<foundation::config::ConfigValue> ModuleConfig(
        const std::string& name) = 0;

    template <typename T>
    foundation::base::Result<T*> Module(const std::string& name) {
        foundation::base::Result<IModule*> module = LookupModuleRaw(name);
        if (!module.IsOk()) {
            return foundation::base::Result<T*>(
                module.GetError(),
                module.GetMessage());
        }

        T* typed_module = dynamic_cast<T*>(module.Value());
        if (typed_module == NULL) {
            return foundation::base::Result<T*>(
                foundation::base::ErrorCode::kInvalidState,
                "Module type cast failed");
        }

        return foundation::base::Result<T*>(typed_module);
    }

private:
    virtual foundation::base::Result<IModule*> LookupModuleRaw(
        const std::string& name) = 0;
};

}  // namespace framework
}  // namespace module_context
