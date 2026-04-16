#include "core/framework/context.h"

#include "core/framework/module_manager.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"

namespace module_context {
namespace framework {

IContext& IContext::Instance() {
    static Context context;
    return context;
}

Context::Context()
    : module_manager_(new ModuleManager()) {
}

Context::~Context() {
    (void)Fini();
}

foundation::base::Result<void> Context::Init() {
    if (!module_manager_) {
        return foundation::base::Result<void>(
            foundation::base::ErrorCode::kInvalidState,
            "Context::Init failed: module manager is unavailable");
    }

    return module_manager_->Init(*this);
}

foundation::base::Result<void> Context::Start() {
    if (!module_manager_) {
        return foundation::base::Result<void>(
            foundation::base::ErrorCode::kInvalidState,
            "Context::Start failed: module manager is unavailable");
    }

    return module_manager_->Start();
}

foundation::base::Result<void> Context::Stop() {
    if (!module_manager_) {
        return foundation::base::Result<void>(
            foundation::base::ErrorCode::kInvalidState,
            "Context::Stop failed: module manager is unavailable");
    }

    return module_manager_->Stop();
}

foundation::base::Result<void> Context::Fini() {
    if (!module_manager_) {
        return foundation::base::MakeSuccess();
    }

    return module_manager_->Fini();
}

IModuleManager* Context::ModuleManager() {
    return module_manager_.get();
}

}  // namespace framework
}  // namespace module_context
