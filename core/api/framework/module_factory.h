#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/imodule.h"

namespace module_context {
namespace framework {

static const int kModulePluginApiVersion = 1;

// This header intentionally remains a tiny C-ABI export shim instead of
// reusing foundation::patterns::Factory. PluginLoader discovers modules across
// a shared-library boundary through fixed exported symbols, while Factory is an
// in-process registry of C++ creators.

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

#define MC_DECLARE_MODULE_FACTORY(ModuleType)                                       \
    MC_DECLARE_MODULE_FACTORY_WITH_API_VERSION(                                     \
        ModuleType,                                                                 \
        ::module_context::framework::kModulePluginApiVersion)

}  // namespace framework
}  // namespace module_context
