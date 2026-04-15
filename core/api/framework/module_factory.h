#pragma once

#include "core/api/framework/export.h"
#include "core/api/framework/imodule.h"

namespace mc {

using CreateModuleFn = IModule* (*)();
using DestroyModuleFn = void (*)(IModule*);

static const char* const kCreateModuleSymbol = "mcCreateModule";
static const char* const kDestroyModuleSymbol = "mcDestroyModule";

} // namespace mc

#define MC_DECLARE_MODULE_FACTORY(ModuleType) \
    extern "C" MC_PLUGIN_EXPORT mc::IModule* mcCreateModule() \
    { \
        return new ModuleType(); \
    } \
    extern "C" MC_PLUGIN_EXPORT void mcDestroyModule(mc::IModule* module) \
    { \
        delete module; \
    }
