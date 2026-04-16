#pragma once

#include "core/api/framework/IContext.h"
#include "core/api/framework/IModule.h"

#include "foundation/base/NonCopyable.h"
#include "foundation/base/Result.h"

namespace module_context {
namespace framework {

class ModuleBase
    : public virtual IModule,
      private foundation::base::NonCopyable {
public:
    /**
     * @brief 默认模块基类。
     *
     * 面向开发人员：
     * - 对外固定生命周期入口（Init/Start/Stop/Fini）在该类中封装状态机；
     * - 子类只需重写 OnInit/OnStart/OnStop/OnFini 实现业务逻辑；
     * - 如需上下文，初始化后可通过 Context() 获取。
     */
    ModuleBase();
    ~ModuleBase() override;

    std::string ModuleName() const override;
    std::string ModuleVersion() const override;
    ModuleState State() const override;

    foundation::base::Result<void> Init(IContext& ctx) override final;
    foundation::base::Result<void> Start() override final;
    foundation::base::Result<void> Stop() override final;
    foundation::base::Result<void> Fini() override final;

protected:
    /// 访问初始化时注入的上下文；若未初始化会触发断言。
    IContext& Context() const;
    /// 判断当前是否已持有上下文。
    bool HasContext() const;

    /// 子类初始化扩展点。
    virtual foundation::base::Result<void> OnInit();
    /// 子类启动扩展点。
    virtual foundation::base::Result<void> OnStart();
    /// 子类停止扩展点。
    virtual foundation::base::Result<void> OnStop();
    /// 子类反初始化扩展点。
    virtual foundation::base::Result<void> OnFini();

private:
    /// 生命周期状态转移校验函数。
    static bool IsValidTransition(ModuleState from, ModuleState to);

private:
    IContext* ctx_;
    ModuleState state_;
};

}  // namespace framework
}  // namespace module_context
