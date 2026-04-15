#pragma once

#include "core/api/framework/module_state.h"

namespace mc {

class IModule
{
public:
    virtual ~IModule() {}

    virtual void init() = 0;
    virtual void start() = 0;
    virtual void stop() = 0;
    virtual void fini() = 0;

    virtual ModuleState state() const = 0;
};

} // namespace mc