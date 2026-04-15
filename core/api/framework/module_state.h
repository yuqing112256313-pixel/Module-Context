#pragma once

#include "core/api/framework/export.h"

namespace module_context {
namespace framework {

/// @brief 模块生命周期状态枚举。
enum class MC_FRAMEWORK_API ModuleState
{
    /// @brief 已创建，尚未初始化。
    Created = 0,
    /// @brief 已完成初始化。
    Inited,
    /// @brief 正在运行中。
    Started,
    /// @brief 已停止。
    Stopped,
    /// @brief 已销毁，生命周期结束。
    Fini
};

} // namespace framework
} // namespace module_context
