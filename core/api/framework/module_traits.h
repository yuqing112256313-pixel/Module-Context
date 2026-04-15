#pragma once

namespace mc {

template<typename T>
struct ModuleTraits;

#define MC_DECLARE_MODULE_TRAITS(InterfaceType, ModuleNameLiteral) \
    template<> \
    struct ModuleTraits<InterfaceType> \
    { \
        static const char* moduleName() \
        { \
            return ModuleNameLiteral; \
        } \
    }

} // namespace mc
