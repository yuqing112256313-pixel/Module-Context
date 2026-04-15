#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/icontext.h"
#include "core/api/framework/imodule.h"

namespace module_context {
namespace framework {

/// @brief 所有业务模块可以继承的基类。
/// 提供统一上下文管理和默认生命周期骨架。
class MC_FRAMEWORK_API ModuleBase : public virtual IModule
{
public:
    ModuleBase();
    ~ModuleBase() override;

    /// @brief 默认返回未知模块名。
    std::string ModuleName() const override;

    /// @brief 默认返回未知版本。
    std::string ModuleVersion() const override;

    /// @brief 查询当前状态。
    ModuleState State() const override;

    /// @brief 初始化模块并绑定上下文。
    void Init(IContext& ctx) override final;

    /// @brief 启动前置 hook。
    void Start() override final;

    /// @brief 停止后置 hook。
    void Stop() override final;

    /// @brief 销毁前置 hook。
    void Fini() override final;

protected:
    /// @brief 获取已设置的上下文引用。
    IContext& Context() const;

    /// @brief 检查是否已绑定上下文。
    bool HasContext() const;

    /// @brief 模块初始化默认实现，子类可覆盖。
    virtual void OnInit();

    /// @brief 模块启动默认实现，子类可覆盖。
    virtual void OnStart();

    /// @brief 模块停止默认实现，子类可覆盖。
    virtual void OnStop();

    /// @brief 模块销毁默认实现，子类可覆盖。
    virtual void OnFini();

private:
    static bool IsValidTransition(ModuleState from, ModuleState to);

private:
    IContext* ctx_;
    ModuleState state_;
};

} // namespace framework
} // namespace module_context
