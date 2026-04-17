#pragma once

#include "core/api/framework/Export.h"
#include "core/api/framework/IModule.h"

namespace module_context {
namespace framework {

/// 插件 API 版本号。宿主加载时会校验版本一致性。
static const int kModulePluginApiVersion = 2;

// This header intentionally remains a tiny C-ABI export shim instead of
// reusing foundation::patterns::Factory. PluginLoader discovers modules across
// a shared-library boundary through fixed exported symbols, while Factory is an
// in-process registry of C++ creators.

/**
 * @brief 声明插件工厂导出符号（带自定义 API 版本）。
 *
 * @param ModuleType 具体模块类型（需实现 IModule）。
 * @param ApiVersion 插件 API 版本号。
 */
#define MC_DECLARE_MODULE_FACTORY_WITH_API_VERSION(ModuleType, ApiVersion)          \
    extern "C" MC_PLUGIN_EXPORT int GetPluginApiVersion() {                         \
        return (ApiVersion);                                                        \
    }                                                                               \
    extern "C" MC_PLUGIN_EXPORT ::module_context::framework::IModule*               \
        CreatePlugin() {                                                            \
        return new ModuleType();                                                    \
    }                                                                               \
    extern "C" MC_PLUGIN_EXPORT void DestroyPlugin(                                 \
        ::module_context::framework::IModule* module) {                             \
        delete module;                                                              \
    }

/**
 * @brief 声明插件工厂导出符号（使用默认 API 版本）。
 */
#define MC_DECLARE_MODULE_FACTORY(ModuleType)                                       \
    MC_DECLARE_MODULE_FACTORY_WITH_API_VERSION(                                     \
        ModuleType,                                                                 \
        ::module_context::framework::kModulePluginApiVersion)

}  // namespace framework
}  // namespace module_context
