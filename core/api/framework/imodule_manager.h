#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/imodule.h"

#include <string>

namespace module_context {
namespace framework {

class IContext;

/// @brief 模块管理器接口。
class MC_FRAMEWORK_API IModuleManager
{
public:
    virtual ~IModuleManager() {}

    /// @brief 从配置文件批量加载模块。
    /// @param configFilePath 配置文件路径，每行格式为 `name=library_path`。
    /// @return 全部加载成功返回 true，遇到失败返回 false。
    virtual bool LoadModules(const std::string& configFilePath) = 0;

    /// @brief 加载单个模块。
    /// @param name 模块名称。
    /// @param libraryPath 模块动态库路径。
    /// @return 加载成功返回 true，失败返回 false。
    virtual bool LoadModule(const std::string& name,
                           const std::string& libraryPath) = 0;

    /// @brief 初始化所有已加载模块。
    /// @param ctx 传入应用上下文。
    virtual void Init(IContext& ctx) = 0;

    /// @brief 启动所有已加载模块。
    virtual void Start() = 0;

    /// @brief 停止所有已加载模块。
    virtual void Stop() = 0;

    /// @brief 按顺序销毁所有模块并清理状态。
    virtual void Fini() = 0;

    /// @brief 根据模块名称获取模块实例。
    /// @param name 模块名称。
    /// @return 找不到返回 nullptr。
    virtual IModule* Module(const std::string& name) = 0;

    template<typename T>
    T* ModuleAs(const std::string& name)
    {
        return dynamic_cast<T*>(Module(name));
    }
};

} // namespace framework
} // namespace module_context
