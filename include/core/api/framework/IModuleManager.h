#pragma once

#include "core/api/framework/Export.h"
#include "core/api/framework/IModule.h"

#include "foundation/base/ErrorCode.h"
#include "foundation/base/Result.h"
#include "foundation/config/ConfigValue.h"

#include <string>

namespace module_context {
namespace framework {

class IContext;

/**
 * @brief 模块管理器接口。
 *
 * 对使用者而言，该接口负责“加载模块 + 驱动生命周期 + 按名称查询模块”。
 * 一般通过 IContext::ModuleManager() 获取实例。
 */
class MC_FRAMEWORK_API IModuleManager {
public:
    virtual ~IModuleManager() {}

    /**
     * @brief 从 JSON 配置文件批量加载模块。
     * @param config_file_path 配置文件路径。
     * @return 成功/失败结果；失败时包含详细错误信息（例如解析失败、重复模块名等）。
     */
    virtual foundation::base::Result<void> LoadModules(
        const std::string& config_file_path) = 0;
    /**
     * @brief 按名称加载单个模块。
     * @param name 模块逻辑名称（用于后续查询）。
     * @param library_path 模块动态库路径（支持相对或绝对路径）。
     */
    virtual foundation::base::Result<void> LoadModule(
        const std::string& name,
        const std::string& library_path) = 0;

    /** @brief 顺序调用所有已加载模块的 Init。 */
    virtual foundation::base::Result<void> Init(IContext& ctx) = 0;
    /** @brief 顺序调用所有已加载模块的 Start。 */
    virtual foundation::base::Result<void> Start() = 0;
    /** @brief 逆序调用所有已加载模块的 Stop。 */
    virtual foundation::base::Result<void> Stop() = 0;
    /** @brief 逆序调用所有已加载模块的 Fini，并清理管理器内缓存。 */
    virtual foundation::base::Result<void> Fini() = 0;

    /**
     * @brief 按名称获取模块原始接口。
     * @return 成功时返回 IModule*；找不到或状态异常时返回失败结果。
     */
    virtual foundation::base::Result<IModule*> Module(
        const std::string& name) = 0;
    /**
     * @brief 按名称获取模块私有配置对象。
     *
     * 配置来自模块清单中的 `config` 字段；若未提供则返回空对象。
     */
    virtual foundation::base::Result<foundation::config::ConfigValue> ModuleConfig(
        const std::string& name) = 0;

    template <typename T>
    /**
     * @brief 按名称获取并转换为指定类型模块指针。
     *
     * @note 内部使用 dynamic_cast。若名称存在但类型不匹配，将返回
     *       kInvalidState 错误。
     */
    foundation::base::Result<T*> ModuleAs(const std::string& name) {
        foundation::base::Result<IModule*> module = Module(name);
        if (!module.IsOk()) {
            return foundation::base::Result<T*>(
                module.GetError(),
                module.GetMessage());
        }

        T* typed_module = dynamic_cast<T*>(module.Value());
        if (typed_module == NULL) {
            return foundation::base::Result<T*>(
                foundation::base::ErrorCode::kInvalidState,
                "Module type cast failed");
        }

        return foundation::base::Result<T*>(typed_module);
    }
};

}  // namespace framework
}  // namespace module_context
