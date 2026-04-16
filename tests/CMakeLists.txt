add_executable(mc_module_base_lifecycle_test
    module_base_lifecycle_test.cpp
)

target_include_directories(mc_module_base_lifecycle_test
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../src
)

target_link_libraries(mc_module_base_lifecycle_test
    PRIVATE
        mc_core_framework
)

add_test(
    NAME mc_module_base_lifecycle_test
    COMMAND mc_module_base_lifecycle_test
)
