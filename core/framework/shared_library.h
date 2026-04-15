#pragma once

#include "core/api/framework/export.h"

#include <string>

namespace module_context {
namespace framework {

/// @brief 跨平台动态库封装，负责打开/关闭/查找符号。
class MC_FRAMEWORK_API SharedLibrary
{
public:
    explicit SharedLibrary(const std::string& path);
    ~SharedLibrary();

    /// @brief 打开动态库。
    bool open();

    /// @brief 关闭动态库。
    void close();

    /// @brief 判断是否已打开。
    bool isOpen() const;

    /// @brief 根据符号名查找导出函数。
    /// @param symbolName 符号名。
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

} // namespace framework
} // namespace module_context
