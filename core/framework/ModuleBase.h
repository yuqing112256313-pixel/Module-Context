#pragma once

#include "core/api/framework/Export.h"
#include "core/api/framework/IContext.h"
#include "core/api/framework/IModule.h"

#include "foundation/base/NonCopyable.h"
#include "foundation/base/Result.h"

namespace module_context {
namespace framework {

class MC_FRAMEWORK_API ModuleBase
    : public virtual IModule,
      private foundation::base::NonCopyable {
public:
    ModuleBase();
    ~ModuleBase() override;

    std::string ModuleName() const override;
    std::string ModuleVersion() const override;
    ModuleState State() const override;

    foundation::base::Result<void> Init(IContext& ctx) override final;
    foundation::base::Result<void> Start() override final;
    foundation::base::Result<void> Stop() override final;
    foundation::base::Result<void> Fini() override final;

protected:
    IContext& Context() const;
    bool HasContext() const;

    virtual foundation::base::Result<void> OnInit();
    virtual foundation::base::Result<void> OnStart();
    virtual foundation::base::Result<void> OnStop();
    virtual foundation::base::Result<void> OnFini();

private:
    static bool IsValidTransition(ModuleState from, ModuleState to);

private:
    IContext* ctx_;
    ModuleState state_;
};

}  // namespace framework
}  // namespace module_context
