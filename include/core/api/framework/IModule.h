#pragma once

#include "core/api/framework/Export.h"
#include "core/api/framework/ModuleState.h"

#include "foundation/base/Result.h"

#include <string>

namespace module_context {
namespace framework {

class IContext;

/**
 * @brief 模块接口定义。
 *
 * 使用人员实现该接口（通常可继承 ModuleBase）后，即可被 ModuleManager
 * 统一管理生命周期。生命周期调用顺序约定为：
 * Init -> Start -> Stop -> Fini。
 */
class MC_FRAMEWORK_API IModule {
public:
    virtual ~IModule() {}

    /**
     * @brief 返回模块名（建议全局唯一，便于日志与定位）。
     */
    virtual std::string ModuleName() const = 0;
    /**
     * @brief 返回模块版本字符串（例如 "1.0.0"）。
     */
    virtual std::string ModuleVersion() const = 0;

    /**
     * @brief 初始化模块。
     * @param ctx 框架上下文，可用于访问 ModuleManager 等全局能力。
     * @return 成功返回 Result<void>::Ok，失败返回具体错误码与信息。
     */
    virtual foundation::base::Result<void> Init(IContext& ctx) = 0;
    /**
     * @brief 启动模块业务逻辑（通常在依赖初始化完成后调用）。
     */
    virtual foundation::base::Result<void> Start() = 0;
    /**
     * @brief 停止模块业务逻辑（通常应保证可重复安全调用）。
     */
    virtual foundation::base::Result<void> Stop() = 0;
    /**
     * @brief 释放模块资源。
     */
    virtual foundation::base::Result<void> Fini() = 0;

    /**
     * @brief 查询模块当前生命周期状态。
     */
    virtual ModuleState State() const = 0;
};

}  // namespace framework
}  // namespace module_context
