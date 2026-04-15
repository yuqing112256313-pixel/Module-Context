#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/module_state.h"

#include <string>

namespace module_context {
namespace framework {

class IContext;

/// @brief 模块生命周期基础接口。
/// @details 统一约定生命周期顺序：Created -> Inited -> Started -> Stopped -> Fini。
class MC_FRAMEWORK_API IModule
{
public:
    virtual ~IModule() {}

    /// @brief 获取模块名称。
    virtual std::string ModuleName() const = 0;

    /// @brief 获取模块版本号。
    virtual std::string ModuleVersion() const = 0;

    /// @brief 初始化模块。
    virtual void Init(IContext& ctx) = 0;

    /// @brief 启动模块。
    virtual void Start() = 0;

    /// @brief 停止模块。
    virtual void Stop() = 0;

    /// @brief 释放模块资源。
    virtual void Fini() = 0;

    /// @brief 查询模块当前生命周期状态。
    virtual ModuleState State() const = 0;
};

} // namespace framework
} // namespace module_context
