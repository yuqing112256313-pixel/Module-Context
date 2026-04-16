#pragma once

#include "foundation/base/Platform.h"

#if defined(MC_FRAMEWORK_SHARED_LIBRARY)
    #if FOUNDATION_PLATFORM_WINDOWS
        #if defined(MC_FRAMEWORK_BUILDING_LIBRARY)
            #define MC_FRAMEWORK_API __declspec(dllexport)
        #else
            #define MC_FRAMEWORK_API __declspec(dllimport)
        #endif
    #else
        #if defined(MC_FRAMEWORK_BUILDING_LIBRARY)
            #define MC_FRAMEWORK_API __attribute__((visibility("default")))
        #else
            #define MC_FRAMEWORK_API
        #endif
    #endif
#else
    #define MC_FRAMEWORK_API
#endif

#if FOUNDATION_PLATFORM_WINDOWS
    #define MC_PLUGIN_EXPORT __declspec(dllexport)
#else
    #define MC_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif
