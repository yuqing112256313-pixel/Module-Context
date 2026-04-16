#pragma once

#include "core/api/framework/Export.h"

#include "foundation/base/Result.h"

namespace module_context {
namespace framework {

class IModuleManager;

/**
 * @brief 框架上下文接口。
 *
 * 对接入方而言，IContext 是框架入口：
 * - 通过 Instance() 可获取全局上下文；
 * - 通过 Init/Start/Stop/Fini 驱动整个模块系统生命周期；
 * - 通过 ModuleManager() 访问模块加载与查询能力。
 */
class MC_FRAMEWORK_API IContext {
public:
    /**
     * @brief 获取全局单例上下文实例。
     */
    static IContext& Instance();

    virtual ~IContext() {}

    /** @brief 初始化框架。 */
    virtual foundation::base::Result<void> Init() = 0;
    /** @brief 启动框架。 */
    virtual foundation::base::Result<void> Start() = 0;
    /** @brief 停止框架。 */
    virtual foundation::base::Result<void> Stop() = 0;
    /** @brief 反初始化框架并释放资源。 */
    virtual foundation::base::Result<void> Fini() = 0;

    /**
     * @brief 获取模块管理器。
     * @return 若上下文已正确构造，返回非空指针。
     */
    virtual IModuleManager* ModuleManager() = 0;
};

}  // namespace framework
}  // namespace module_context
