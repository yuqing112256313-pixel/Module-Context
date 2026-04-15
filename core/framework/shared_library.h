#pragma once

#include "core/api/framework/export.h"

#include <string>

namespace mc {

class MC_FRAMEWORK_API SharedLibrary
{
public:
    explicit SharedLibrary(const std::string& path);
    ~SharedLibrary();

    bool open();
    void close();

    bool isOpen() const;

    template<typename T>
    T resolve(const char* symbolName) const
    {
        return reinterpret_cast<T>(resolveRaw(symbolName));
    }

private:
    void* resolveRaw(const char* symbolName) const;

private:
    std::string path_;
    void* handle_;
};

} // namespace mc