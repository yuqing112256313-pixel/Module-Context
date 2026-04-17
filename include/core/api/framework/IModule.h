#pragma once

#include "core/api/framework/Export.h"
#include "core/api/framework/ModuleState.h"

#include "foundation/base/Result.h"

#include <string>

namespace module_context {
namespace framework {

class IContext;

class MC_FRAMEWORK_API IModule {
public:
    virtual ~IModule() {}

    // Returns the configured instance name for this module.
    virtual std::string ModuleName() const = 0;
    // Returns the stable logical type name for this module.
    virtual std::string ModuleType() const = 0;
    // Returns the module version string, for example "1.0.0".
    virtual std::string ModuleVersion() const = 0;
    // Injects the instance name before the module enters its lifecycle.
    virtual foundation::base::Result<void> SetModuleName(
        const std::string& name) = 0;

    virtual foundation::base::Result<void> Init(IContext& ctx) = 0;
    virtual foundation::base::Result<void> Start() = 0;
    virtual foundation::base::Result<void> Stop() = 0;
    virtual foundation::base::Result<void> Fini() = 0;

    virtual ModuleState State() const = 0;
};

}  // namespace framework
}  // namespace module_context
