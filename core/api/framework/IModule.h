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

    virtual std::string ModuleName() const = 0;
    virtual std::string ModuleVersion() const = 0;

    virtual foundation::base::Result<void> Init(IContext& ctx) = 0;
    virtual foundation::base::Result<void> Start() = 0;
    virtual foundation::base::Result<void> Stop() = 0;
    virtual foundation::base::Result<void> Fini() = 0;

    virtual ModuleState State() const = 0;
};

}  // namespace framework
}  // namespace module_context
