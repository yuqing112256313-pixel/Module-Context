#include "core/framework/module_base.h"

#include "core/api/framework/exception.h"
#include "core/api/framework/icontext.h"

namespace mc {

ModuleBase::ModuleBase()
    : ctx_(0)
    , state_(ModuleState::Created)
{
}

ModuleBase::~ModuleBase()
{
}

std::string ModuleBase::moduleName() const
{
    return "unknown";
}

std::string ModuleBase::moduleVersion() const
{
    return "unknown";
}

StringList ModuleBase::dependencies() const
{
    return StringList();
}

ModuleState ModuleBase::state() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return state_;
}

void ModuleBase::init(IContext& ctx)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    doInitLocked(ctx);
}

void ModuleBase::start()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    doStartLocked();
}

void ModuleBase::stop()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    doStopLocked();
}

void ModuleBase::fini()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    doFiniLocked();
}

IContext& ModuleBase::context() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (!ctx_) {
        throw InvalidStateError("module context is not bound");
    }
    return *ctx_;
}

bool ModuleBase::hasContext() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return ctx_ != 0;
}

void ModuleBase::onInit()
{
}

void ModuleBase::onStart()
{
}

void ModuleBase::onStop()
{
}

void ModuleBase::onFini()
{
}

void ModuleBase::doInitLocked(IContext& ctx)
{
    // 已完成初始化，视为幂等
    if (state_ == ModuleState::Inited
        || state_ == ModuleState::Started
        || state_ == ModuleState::Stopped) {
        return;
    }

    if (!(state_ == ModuleState::Created || state_ == ModuleState::Fini)) {
        throw InvalidStateError("invalid module state for init: " + moduleName());
    }

    const ModuleState previousState = state_;
    IContext* const previousCtx = ctx_;

    ctx_ = &ctx;

    try {
        onInit();
        state_ = ModuleState::Inited;
    } catch (...) {
        ctx_ = previousCtx;
        state_ = previousState;
        throw;
    }
}

void ModuleBase::doStartLocked()
{
    if (state_ == ModuleState::Started) {
        return;
    }

    if (!(state_ == ModuleState::Inited || state_ == ModuleState::Stopped)) {
        throw InvalidStateError("invalid module state for start: " + moduleName());
    }

    onStart();
    state_ = ModuleState::Started;
}

void ModuleBase::doStopLocked()
{
    if (state_ != ModuleState::Started) {
        // 非 Started 状态下 stop 视为幂等
        return;
    }

    onStop();
    state_ = ModuleState::Stopped;
}

void ModuleBase::doFiniLocked()
{
    if (state_ == ModuleState::Fini) {
        return;
    }

    if (state_ == ModuleState::Started) {
        doStopLocked();
    }

    if (state_ == ModuleState::Created) {
        ctx_ = 0;
        state_ = ModuleState::Fini;
        return;
    }

    if (!(state_ == ModuleState::Inited || state_ == ModuleState::Stopped)) {
        throw InvalidStateError("invalid module state for fini: " + moduleName());
    }

    onFini();

    ctx_ = 0;
    state_ = ModuleState::Fini;
}

} // namespace mc