#pragma once

#include "core/api/framework/module_state.h"

#include <string>

namespace mc {

class IModuleManager;

class IContext
{
public:
    virtual ~IContext() {}

    virtual bool loadModules(const std::string& configFilePath) = 0;

    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fini() = 0;

    virtual ModuleState state() const = 0;

    virtual IModuleManager* moduleMgr() = 0;
};

} // namespace mc