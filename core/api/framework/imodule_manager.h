#pragma once

#include "core/api/framework/module_state.h"

#include <string>
#include <vector>

namespace mc {

class IModule;

class IModuleManager
{
public:
    virtual ~IModuleManager() {}

    virtual bool loadModule(const std::string& name,
                            const std::string& libraryPath) = 0;

    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fini() = 0;

    virtual ModuleState state() const = 0;

    virtual IModule* module(const std::string& name) = 0;
    virtual std::vector<std::string> moduleNames() const = 0;

    template<typename T>
    T* moduleAs(const std::string& name)
    {
        return dynamic_cast<T*>(module(name));
    }
};

} // namespace mc