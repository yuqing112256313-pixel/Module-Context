cmake_minimum_required(VERSION 3.16)
project(module_context_core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_library(mc_core_framework SHARED
    core/framework/context.cpp
    core/framework/module_base.cpp
    core/framework/module_manager.cpp
    core/framework/shared_library.cpp
)

target_compile_definitions(mc_core_framework PRIVATE MC_FRAMEWORK_EXPORTS)

target_include_directories(mc_core_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

if(UNIX AND NOT APPLE)
    target_link_libraries(mc_core_framework PRIVATE ${CMAKE_DL_LIBS})
elseif(APPLE)
    target_link_libraries(mc_core_framework PRIVATE ${CMAKE_DL_LIBS})
endif()
