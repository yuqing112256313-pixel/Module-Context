#pragma once

#include "core/api/framework/module_state.h"

#include <string>
#include <vector>

namespace mc {

class IContext;
using StringList = std::vector<std::string>;

/**
 * @brief 模块统一生命周期与元信息接口。
 */
class IModule
{
public:
    virtual ~IModule() {}

    /** @brief 模块唯一名称。 */
    virtual std::string moduleName() const = 0;
    /** @brief 模块版本号。 */
    virtual std::string moduleVersion() const = 0;
    /** @brief 模块依赖列表（按模块名）。 */
    virtual StringList dependencies() const = 0;

    /** @brief 初始化并绑定上下文。 */
    virtual void init(IContext& ctx) = 0;
    /** @brief 启动模块。 */
    virtual void start() = 0;
    /** @brief 停止模块。 */
    virtual void stop() = 0;
    /** @brief 反初始化模块。 */
    virtual void fini() = 0;

    /** @brief 当前模块生命周期状态。 */
    virtual ModuleState state() const = 0;
};

} // namespace mc
