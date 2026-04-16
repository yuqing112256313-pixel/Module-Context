#include "framework/Context.h"

#include "framework/ModuleManager.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"

namespace module_context {
namespace framework {

IContext& IContext::Instance() {
    // 函数内静态对象：线程安全懒初始化，进程内全局唯一。
    static Context context;
    return context;
}

Context::Context()
    : module_manager_(new module_context::framework::ModuleManager()) {
}

Context::~Context() {
    // 析构兜底：确保即使调用方遗漏，也会尝试释放模块资源。
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
        // 已无管理器时视为幂等成功。
        return foundation::base::MakeSuccess();
    }

    return module_manager_->Fini();
}

IModuleManager* Context::ModuleManager() {
    return module_manager_.get();
}

}  // namespace framework
}  // namespace module_context
