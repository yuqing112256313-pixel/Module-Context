#pragma once

#include "core/api/framework/IContext.h"

#include "foundation/base/NonCopyable.h"
#include "foundation/base/Result.h"

#include <memory>

namespace module_context {
namespace framework {

class Context final
    : public IContext,
      private foundation::base::NonCopyable {
public:
    /// 默认上下文实现，内部持有 ModuleManager。
    Context();
    ~Context() override;

    foundation::base::Result<void> Init() override;
    foundation::base::Result<void> Start() override;
    foundation::base::Result<void> Stop() override;
    foundation::base::Result<void> Fini() override;

    IModuleManager* ModuleManager() override;

private:
    /// 模块管理器的唯一所有权，由 Context 生命周期托管。
    std::unique_ptr<IModuleManager> module_manager_;
};

}  // namespace framework
}  // namespace module_context
