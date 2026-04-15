#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/icontext.h"

#include <memory>

namespace module_context {
namespace framework {

/// @brief 框架上下文实现，统一管理模块生命周期。
class Context final : public IContext
{
public:
    Context();
    ~Context() override;

    /// @brief 初始化上下文并进入模块初始化阶段。
    void Init() override;
    /// @brief 启动上下文并驱动模块启动。
    void Start() override;
    /// @brief 停止上下文并驱动模块停止。
    void Stop() override;
    /// @brief 关闭上下文并清理资源。
    void Fini() override;

    /// @brief 获取模块管理器。
    IModuleManager* ModuleManager() override;

private:
    std::unique_ptr<IModuleManager> moduleManager_;
};

} // namespace framework
} // namespace module_context
