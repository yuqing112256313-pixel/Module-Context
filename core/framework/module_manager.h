#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/imodule.h"
#include "core/api/framework/imodule_manager.h"
#include "core/framework/shared_library.h"

#include <functional>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>

namespace module_context {
namespace framework {

/// @brief 管理动态库模块的加载与生命周期调度。
class MC_FRAMEWORK_API ModuleManager final : public IModuleManager
{
public:
    ModuleManager();
    ~ModuleManager() override;

    /// @brief 读取配置文件批量加载模块。
    bool LoadModules(const std::string& configFilePath) override;

    /// @brief 加载单个模块（通过动态库路径）。
    bool LoadModule(const std::string& name,
                    const std::string& libraryPath) override;

    /// @brief 初始化全部已加载模块。
    void Init(IContext& ctx) override;

    /// @brief 启动全部模块。
    void Start() override;

    /// @brief 反向停止全部模块。
    void Stop() override;

    /// @brief 反向销毁全部模块。
    void Fini() override;

    /// @brief 按名称获取模块实例。
    IModule* Module(const std::string& name) override;

private:
    using ModuleDeleter = std::function<void(IModule*)>;
    using ModulePtr = std::unique_ptr<IModule, ModuleDeleter>;

    struct ModuleRecord
    {
        std::string name;
        std::string libraryPath;
        std::unique_ptr<SharedLibrary> library;
        ModulePtr module;

        ModuleRecord()
            : module(nullptr, ModuleDeleter())
        {
        }
    };

private:
    using ModuleList = std::vector<ModuleRecord>;
    using ModuleIndex = std::unordered_map<std::string, std::size_t>;

    ModuleList modules_;
    ModuleIndex moduleIndexByName_;
};

} // namespace framework
} // namespace module_context
