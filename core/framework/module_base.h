#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/icontext.h"
#include "core/api/framework/imodule.h"

#include <mutex>
#include <string>

namespace mc {

class MC_FRAMEWORK_API ModuleBase : public virtual IModule
{
public:
    ModuleBase();
    ~ModuleBase() override;

    std::string moduleName() const override;
    std::string moduleVersion() const override;
    StringList dependencies() const override;
    ModuleState state() const override;

    void init(IContext& ctx) override final;
    void start() override final;
    void stop() override final;
    void fini() override final;

protected:
    IContext& context() const;

    bool hasContext() const;

    virtual void onInit();
    virtual void onStart();
    virtual void onStop();
    virtual void onFini();

private:
    void doInitLocked(IContext& ctx);
    void doStartLocked();
    void doStopLocked();
    void doFiniLocked();

private:
    mutable std::recursive_mutex mutex_;
    IContext* ctx_;
    ModuleState state_;
};

} // namespace mc
