cmake_minimum_required(VERSION 3.16)
project(module_context_core LANGUAGES CXX)

include(FetchContent)
include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(MC_BUILD_EXAMPLES "Build examples targets" ON)
option(MC_BUILD_TESTS "Build test targets" ON)

set(MC_FOUNDATION_SOURCE_DIR
    ""
    CACHE PATH
    "Optional local foundation source directory. Can point to the repo root or the directory that contains CMakeLists.txt.")
set(MC_FOUNDATION_GIT_REPOSITORY
    "https://github.com/yuqing112256313-pixel/Foundation.git"
    CACHE STRING
    "Git repository used when foundation must be fetched automatically.")
set(MC_FOUNDATION_GIT_TAG
    "main"
    CACHE STRING
    "Git ref used when foundation must be fetched automatically.")

function(mc_prepare_foundation_build_options)
    set(FOUNDATION_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(FOUNDATION_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(FOUNDATION_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
endfunction()

function(mc_normalize_foundation_source_dir input_dir output_var)
    if(NOT input_dir)
        set(${output_var} "" PARENT_SCOPE)
        return()
    endif()

    if(EXISTS "${input_dir}/CMakeLists.txt")
        set(${output_var} "${input_dir}" PARENT_SCOPE)
        return()
    endif()

    if(EXISTS "${input_dir}/foundation/CMakeLists.txt")
        set(${output_var} "${input_dir}/foundation" PARENT_SCOPE)
        return()
    endif()

    message(FATAL_ERROR
        "foundation source directory not found under '${input_dir}'. "
        "Expected either '${input_dir}/CMakeLists.txt' or "
        "'${input_dir}/foundation/CMakeLists.txt'.")
endfunction()

function(mc_ensure_foundation_dependency)
    if(TARGET foundation::foundation)
        return()
    endif()

    set(_foundation_source_dir "")

    if(MC_FOUNDATION_SOURCE_DIR)
        mc_normalize_foundation_source_dir(
            "${MC_FOUNDATION_SOURCE_DIR}"
            _foundation_source_dir)
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../foundation_0415_v1/foundation/CMakeLists.txt")
        set(_foundation_source_dir
            "${CMAKE_CURRENT_SOURCE_DIR}/../foundation_0415_v1/foundation")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Foundation/CMakeLists.txt")
        set(_foundation_source_dir
            "${CMAKE_CURRENT_SOURCE_DIR}/../Foundation")
    endif()

    if(_foundation_source_dir)
        message(STATUS
            "Using foundation from local source: ${_foundation_source_dir}")
        mc_prepare_foundation_build_options()
        add_subdirectory(
            "${_foundation_source_dir}"
            "${CMAKE_CURRENT_BINARY_DIR}/_deps/foundation-build")
        return()
    endif()

    find_package(foundation CONFIG QUIET)
    if(TARGET foundation::foundation)
        message(STATUS "Using installed foundation package")
        return()
    endif()

    message(STATUS
        "Fetching foundation from ${MC_FOUNDATION_GIT_REPOSITORY} "
        "(ref: ${MC_FOUNDATION_GIT_TAG})")
    mc_prepare_foundation_build_options()
    FetchContent_Declare(
        foundation
        GIT_REPOSITORY "${MC_FOUNDATION_GIT_REPOSITORY}"
        GIT_TAG "${MC_FOUNDATION_GIT_TAG}"
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(foundation)
endfunction()

function(mc_patch_foundation_for_gcc_template_lookup foundation_source_dir)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        return()
    endif()

    set(_plugin_loader_header
        "${foundation_source_dir}/include/foundation/plugin/PluginLoader.h")
    if(NOT EXISTS "${_plugin_loader_header}")
        return()
    endif()

    file(READ "${_plugin_loader_header}" _plugin_loader_content)
    string(REPLACE
        "new_state->library.GetSymbol<"
        "new_state->library.template GetSymbol<"
        _patched_plugin_loader_content
        "${_plugin_loader_content}")

    if(NOT _patched_plugin_loader_content STREQUAL _plugin_loader_content)
        file(WRITE
            "${_plugin_loader_header}"
            "${_patched_plugin_loader_content}")
        message(STATUS
            "Patched foundation PluginLoader.h for GCC dependent-template lookup")
    endif()
endfunction()

mc_ensure_foundation_dependency()
if(DEFINED foundation_SOURCE_DIR)
    mc_patch_foundation_for_gcc_template_lookup("${foundation_SOURCE_DIR}")
endif()

if(NOT TARGET foundation::foundation)
    message(FATAL_ERROR
        "foundation dependency could not be resolved. "
        "Set MC_FOUNDATION_SOURCE_DIR to a local checkout or allow CMake "
        "to fetch ${MC_FOUNDATION_GIT_REPOSITORY}.")
endif()

add_library(mc_core_framework SHARED
    src/framework/Context.cpp
    src/framework/ModuleBase.cpp
    src/framework/ModuleManager.cpp
)

set(MC_CORE_FRAMEWORK_PUBLIC_HEADERS
    include/core/api/framework/Export.h
    include/core/api/framework/IContext.h
    include/core/api/framework/IModule.h
    include/core/api/framework/IModuleManager.h
    include/core/api/framework/ModuleFactory.h
    include/core/api/framework/ModuleState.h
)

target_compile_definitions(mc_core_framework
    PUBLIC
        MC_FRAMEWORK_SHARED_LIBRARY
    PRIVATE
        MC_FRAMEWORK_BUILDING_LIBRARY
)

target_include_directories(mc_core_framework
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(mc_core_framework
    PUBLIC
        foundation::foundation
)

if(UNIX AND NOT APPLE)
    target_link_libraries(mc_core_framework PRIVATE ${CMAKE_DL_LIBS})
elseif(APPLE)
    target_link_libraries(mc_core_framework PRIVATE ${CMAKE_DL_LIBS})
endif()

install(TARGETS mc_core_framework
    EXPORT module_context_core_targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(
    FILES
        ${MC_CORE_FRAMEWORK_PUBLIC_HEADERS}
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/core/api/framework
)

if(MC_BUILD_EXAMPLES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/CMakeLists.txt")
    add_subdirectory(examples)
endif()

if(MC_BUILD_TESTS AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt")
    include(CTest)
    if(BUILD_TESTING)
        add_subdirectory(tests)
    endif()
endif()
