#pragma once

#include "core/api/framework/module_traits.h"

#include <functional>

namespace mc {

class IThreadPool
{
public:
    virtual ~IThreadPool() {}

    virtual void post(const std::function<void()>& fn) = 0;
    virtual void postDelayed(int ms, const std::function<void()>& fn) = 0;
    virtual int activeThreadCount() const = 0;
    virtual int maxThreadCount() const = 0;
    virtual void setMaxThreadCount(int n) = 0;
};

MC_DECLARE_MODULE_TRAITS(IThreadPool, "ThreadPool");

} // namespace mc
