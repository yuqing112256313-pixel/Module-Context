#include "core/framework/module_base.h"

#include "core/api/framework/icontext.h"

#include <cassert>

namespace module_context {
namespace framework {

ModuleBase::ModuleBase()
    : ctx_(nullptr)
    , state_(ModuleState::Created)
{
}

ModuleBase::~ModuleBase()
{
}

std::string ModuleBase::ModuleName() const
{
    return "unknown";
}

std::string ModuleBase::ModuleVersion() const
{
    return "unknown";
}

ModuleState ModuleBase::State() const
{
    return state_;
}

IContext& ModuleBase::Context() const
{
    assert(ctx_ != nullptr && "module context is not initialized");
    return *ctx_;
}

bool ModuleBase::HasContext() const
{
    return ctx_ != nullptr;
}

void ModuleBase::OnInit()
{
}

void ModuleBase::OnStart()
{
}

void ModuleBase::OnStop()
{
}

void ModuleBase::OnFini()
{
}

bool ModuleBase::IsValidTransition(ModuleState from, ModuleState to)
{
    if (from == to) {
        return false;
    }

    switch (to) {
    case ModuleState::Inited:
        return from == ModuleState::Created || from == ModuleState::Fini;
    case ModuleState::Started:
        return from == ModuleState::Inited || from == ModuleState::Stopped;
    case ModuleState::Stopped:
        return from == ModuleState::Started;
    case ModuleState::Fini:
        return from == ModuleState::Created
               || from == ModuleState::Inited
               || from == ModuleState::Stopped
               || from == ModuleState::Started;
    case ModuleState::Created:
        return false;
    default:
        return false;
    }
}

void ModuleBase::Init(IContext& ctx)
{
    if (!IsValidTransition(state_, ModuleState::Inited)) {
        return;
    }

    ctx_ = &ctx;
    OnInit();
    state_ = ModuleState::Inited;
}

void ModuleBase::Start()
{
    if (!IsValidTransition(state_, ModuleState::Started)) {
        return;
    }

    OnStart();
    state_ = ModuleState::Started;
}

void ModuleBase::Stop()
{
    if (!IsValidTransition(state_, ModuleState::Stopped)) {
        return;
    }

    OnStop();
    state_ = ModuleState::Stopped;
}

void ModuleBase::Fini()
{
    if (!IsValidTransition(state_, ModuleState::Fini)) {
        return;
    }

    if (state_ == ModuleState::Started) {
        OnStop();
    }

    if (state_ == ModuleState::Started
        || state_ == ModuleState::Stopped
        || state_ == ModuleState::Inited) {
        OnFini();
    }

    ctx_ = nullptr;
    state_ = ModuleState::Fini;
}

} // namespace framework
} // namespace module_context
