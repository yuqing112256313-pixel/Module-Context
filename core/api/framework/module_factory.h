#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/imodule.h"

namespace module_context {
namespace framework {

/// @brief 模块创建函数别名。
using CreateModuleFn = IModule* (*)();

/// @brief 模块销毁函数别名。
using DestroyModuleFn = void (*)(IModule*);

/// @brief 约定导出的函数名常量。
static const char* const kCreateModuleSymbol = "mcCreateModule";
static const char* const kDestroyModuleSymbol = "mcDestroyModule";

/// @brief 声明插件导出工厂函数。
/// 约定：
///   MC_DECLARE_MODULE_FACTORY(MyModule)
/// 会生成：
///   mcCreateModule() -> 返回 new MyModule
///   mcDestroyModule(IModule*) -> 调用 delete
///
/// 宿主通过这两个函数动态加载并管理模块。
#define MC_DECLARE_MODULE_FACTORY(ModuleType)                               \
    extern "C" MC_PLUGIN_EXPORT module_context::framework::IModule* mcCreateModule() \
    {                                                                      \
        return new ModuleType();                                            \
    }                                                                      \
    extern "C" MC_PLUGIN_EXPORT void mcDestroyModule(module_context::framework::IModule* module)  \
    {                                                                      \
        delete module;                                                      \
    }

} // namespace framework
} // namespace module_context
