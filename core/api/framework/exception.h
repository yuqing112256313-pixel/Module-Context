#pragma once

#include <stdexcept>
#include <string>

namespace mc {

class McException : public std::runtime_error
{
public:
    explicit McException(const std::string& message)
        : std::runtime_error(message)
    {
    }
};

class InvalidArgumentError : public McException
{
public:
    explicit InvalidArgumentError(const std::string& message)
        : McException(message)
    {
    }
};

class InvalidStateError : public McException
{
public:
    explicit InvalidStateError(const std::string& message)
        : McException(message)
    {
    }
};

class DuplicateModuleError : public McException
{
public:
    explicit DuplicateModuleError(const std::string& message)
        : McException(message)
    {
    }
};

class ModuleNotFoundError : public McException
{
public:
    explicit ModuleNotFoundError(const std::string& message)
        : McException(message)
    {
    }
};

class ModuleTypeMismatchError : public McException
{
public:
    explicit ModuleTypeMismatchError(const std::string& message)
        : McException(message)
    {
    }
};

class DependencyError : public McException
{
public:
    explicit DependencyError(const std::string& message)
        : McException(message)
    {
    }
};

class DynamicLoadError : public McException
{
public:
    explicit DynamicLoadError(const std::string& message)
        : McException(message)
    {
    }
};

} // namespace mc
