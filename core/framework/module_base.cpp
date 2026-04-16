#include "core/framework/module_base.h"

#include "foundation/base/Assert.h"
#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"

namespace module_context {
namespace framework {

namespace {

foundation::base::Result<void> MakeStateError(
    const char* operation,
    const std::string& module_name,
    ModuleState state) {
    return foundation::base::Result<void>(
        foundation::base::ErrorCode::kInvalidState,
        std::string("ModuleBase::") + operation + " failed for module '" +
            module_name + "' from state " +
            std::to_string(static_cast<int>(state)));
}

foundation::base::Result<void> PrefixHookError(
    const char* operation,
    const std::string& module_name,
    const foundation::base::Result<void>& result) {
    if (result.IsOk()) {
        return result;
    }

    return foundation::base::Result<void>(
        result.GetError(),
        std::string("ModuleBase::") + operation + " failed for module '" +
            module_name + "': " + result.GetMessage());
}

}  // namespace

ModuleBase::ModuleBase()
    : ctx_(NULL),
      state_(ModuleState::Created) {
}

ModuleBase::~ModuleBase() {
}

std::string ModuleBase::ModuleName() const {
    return "unknown";
}

std::string ModuleBase::ModuleVersion() const {
    return "unknown";
}

ModuleState ModuleBase::State() const {
    return state_;
}

IContext& ModuleBase::Context() const {
    FOUNDATION_ASSERT_MSG(
        ctx_ != NULL,
        "ModuleBase::Context() called before module initialization");
    return *ctx_;
}

bool ModuleBase::HasContext() const {
    return ctx_ != NULL;
}

foundation::base::Result<void> ModuleBase::OnInit() {
    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleBase::OnStart() {
    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleBase::OnStop() {
    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleBase::OnFini() {
    return foundation::base::MakeSuccess();
}

bool ModuleBase::IsValidTransition(ModuleState from, ModuleState to) {
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
            return from == ModuleState::Created ||
                   from == ModuleState::Inited ||
                   from == ModuleState::Started ||
                   from == ModuleState::Stopped;
        case ModuleState::Created:
            return false;
        default:
            return false;
    }
}

foundation::base::Result<void> ModuleBase::Init(IContext& ctx) {
    if (!IsValidTransition(state_, ModuleState::Inited)) {
        return MakeStateError("Init", ModuleName(), state_);
    }

    ctx_ = &ctx;
    foundation::base::Result<void> init_result = OnInit();
    if (!init_result.IsOk()) {
        ctx_ = NULL;
        return PrefixHookError("Init", ModuleName(), init_result);
    }

    state_ = ModuleState::Inited;
    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleBase::Start() {
    if (!IsValidTransition(state_, ModuleState::Started)) {
        return MakeStateError("Start", ModuleName(), state_);
    }

    foundation::base::Result<void> start_result = OnStart();
    if (!start_result.IsOk()) {
        return PrefixHookError("Start", ModuleName(), start_result);
    }

    state_ = ModuleState::Started;
    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleBase::Stop() {
    if (!IsValidTransition(state_, ModuleState::Stopped)) {
        return MakeStateError("Stop", ModuleName(), state_);
    }

    foundation::base::Result<void> stop_result = OnStop();
    if (!stop_result.IsOk()) {
        return PrefixHookError("Stop", ModuleName(), stop_result);
    }

    state_ = ModuleState::Stopped;
    return foundation::base::MakeSuccess();
}

foundation::base::Result<void> ModuleBase::Fini() {
    if (!IsValidTransition(state_, ModuleState::Fini)) {
        return MakeStateError("Fini", ModuleName(), state_);
    }

    foundation::base::Result<void> first_error =
        foundation::base::MakeSuccess();

    if (state_ == ModuleState::Started) {
        foundation::base::Result<void> stop_result = OnStop();
        if (!stop_result.IsOk()) {
            first_error = PrefixHookError("Fini", ModuleName(), stop_result);
        }
    }

    if (state_ == ModuleState::Inited ||
        state_ == ModuleState::Stopped ||
        state_ == ModuleState::Started) {
        foundation::base::Result<void> fini_result = OnFini();
        if (first_error.IsOk() && !fini_result.IsOk()) {
            first_error = PrefixHookError("Fini", ModuleName(), fini_result);
        }
    }

    ctx_ = NULL;
    state_ = ModuleState::Fini;
    return first_error;
}

}  // namespace framework
}  // namespace module_context
