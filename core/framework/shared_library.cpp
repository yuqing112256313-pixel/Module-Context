#include "core/framework/shared_library.h"

#if defined(_WIN32)
    #include <windows.h>
#else
    #include <dlfcn.h>
#endif

namespace mc {

SharedLibrary::SharedLibrary(const std::string& path)
    : path_(path)
    , handle_(nullptr)
{
}

SharedLibrary::~SharedLibrary()
{
    close();
}

bool SharedLibrary::open()
{
    if (isOpen()) {
        return true;
    }

    if (path_.empty()) {
        return false;
    }

#if defined(_WIN32)
    handle_ = reinterpret_cast<void*>(::LoadLibraryA(path_.c_str()));
#else
    handle_ = ::dlopen(path_.c_str(), RTLD_NOW | RTLD_LOCAL);
#endif

    return handle_ != nullptr;
}

void SharedLibrary::close()
{
    if (!handle_) {
        return;
    }

#if defined(_WIN32)
    ::FreeLibrary(reinterpret_cast<HMODULE>(handle_));
#else
    ::dlclose(handle_);
#endif

    handle_ = nullptr;
}

bool SharedLibrary::isOpen() const
{
    return handle_ != nullptr;
}

void* SharedLibrary::resolveRaw(const char* symbolName) const
{
    if (!handle_ || !symbolName || !symbolName[0]) {
        return nullptr;
    }

#if defined(_WIN32)
    FARPROC proc = ::GetProcAddress(reinterpret_cast<HMODULE>(handle_), symbolName);
    return reinterpret_cast<void*>(proc);
#else
    ::dlerror();
    void* proc = ::dlsym(handle_, symbolName);
    ::dlerror();
    return proc;
#endif
}

} // namespace mc