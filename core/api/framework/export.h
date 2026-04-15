#pragma once

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(MC_FRAMEWORK_EXPORTS)
        #define MC_FRAMEWORK_API __declspec(dllexport)
    #else
        #define MC_FRAMEWORK_API __declspec(dllimport)
    #endif
    #define MC_PLUGIN_EXPORT extern "C" __declspec(dllexport)
#else
    #if __GNUC__ >= 4
        #define MC_FRAMEWORK_API __attribute__((visibility("default")))
        #define MC_PLUGIN_EXPORT extern "C" __attribute__((visibility("default")))
    #else
        #define MC_FRAMEWORK_API
        #define MC_PLUGIN_EXPORT extern "C"
    #endif
#endif

// 模块框架统一命名空间：module_context::framework
namespace module_context {
namespace framework {
}
} // namespace framework
} // namespace module_context
