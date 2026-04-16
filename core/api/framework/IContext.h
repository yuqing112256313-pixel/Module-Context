#pragma once

#include "core/api/framework/Export.h"

#include "foundation/base/Result.h"

namespace module_context {
namespace framework {

class IModuleManager;

class MC_FRAMEWORK_API IContext {
public:
    static IContext& Instance();

    virtual ~IContext() {}

    virtual foundation::base::Result<void> Init() = 0;
    virtual foundation::base::Result<void> Start() = 0;
    virtual foundation::base::Result<void> Stop() = 0;
    virtual foundation::base::Result<void> Fini() = 0;

    virtual IModuleManager* ModuleManager() = 0;
};

}  // namespace framework
}  // namespace module_context
