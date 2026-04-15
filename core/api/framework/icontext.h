#pragma once

#include "core/api/framework/export.h"

namespace module_context {
namespace framework {

class IModuleManager;

/// @brief 上下文统一入口。
/// 使用方式：
/// 1) 通过 ModuleManager() 获取模块管理器；
/// 2) 通过 LoadModules()/LoadModule() 加载插件；
/// 3) 调用 Init()/Start()/Stop()/Fini() 管理生命周期。
class MC_FRAMEWORK_API IContext
{
public:
    /// @brief 获取全局上下文实例。
    static IContext& Instance();

    virtual ~IContext() {}

    /// @brief 初始化上下文，并准备模块环境。
    virtual void Init() = 0;

    /// @brief 启动上下文并带动模块启动。
    virtual void Start() = 0;

    /// @brief 停止上下文并触发模块停止。
    virtual void Stop() = 0;

    /// @brief 关闭上下文并释放全局资源。
    virtual void Fini() = 0;

    /// @brief 获取模块管理器。
    /// @return 返回模块管理器指针，不存在时返回 nullptr。
    virtual IModuleManager* ModuleManager() = 0;
};

} // namespace framework
} // namespace module_context
