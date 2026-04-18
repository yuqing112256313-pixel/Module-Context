function(mc_configure_test_target target)
    target_include_directories(${target}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
    )

    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    if(WIN32 AND TARGET mc_core_framework)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:mc_core_framework>
                $<TARGET_FILE_DIR:${target}>
        )
    endif()
endfunction()

add_executable(mc_module_base_lifecycle_test
    module_base_lifecycle_test.cpp
    ../src/framework/ModuleBase.cpp
)

mc_configure_test_target(mc_module_base_lifecycle_test)

target_link_libraries(mc_module_base_lifecycle_test
    PRIVATE
        foundation::foundation
)

add_library(mc_test_config_plugin SHARED
    module_manager_config_test_plugin.cpp
)

mc_configure_test_target(mc_test_config_plugin)

target_link_libraries(mc_test_config_plugin
    PRIVATE
        mc_core_framework
        foundation::foundation
)

add_executable(mc_module_manager_config_test
    module_manager_config_test.cpp
)

mc_configure_test_target(mc_module_manager_config_test)

target_link_libraries(mc_module_manager_config_test
    PRIVATE
        mc_core_framework
        foundation::foundation
)

target_compile_definitions(mc_module_manager_config_test
    PRIVATE
        MC_TEST_BINARY_DIR="${CMAKE_CURRENT_BINARY_DIR}"
        MC_TEST_CONFIG_PLUGIN_PATH="$<TARGET_FILE:mc_test_config_plugin>"
)

add_test(
    NAME mc_module_base_lifecycle_test
    COMMAND mc_module_base_lifecycle_test
)

add_test(
    NAME mc_module_manager_config_test
    COMMAND mc_module_manager_config_test
)

if(TARGET rabbitmq_bus)
    add_executable(mc_rabbitmq_bus_module_test
        rabbitmq_bus_module_test.cpp
        ../modules/rabbitmq_bus/src/internal/MessageBusServiceProxy.cpp
        ../modules/rabbitmq_bus/src/RabbitMqBusModule.cpp
    )

    mc_configure_test_target(mc_rabbitmq_bus_module_test)

    target_include_directories(mc_rabbitmq_bus_module_test
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/../modules/rabbitmq_bus/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../modules/rabbitmq_bus/src/internal
    )

    target_link_libraries(mc_rabbitmq_bus_module_test
        PRIVATE
            amqpcpp
            mc_core_framework
            foundation::foundation
    )

    if(WIN32)
        target_link_libraries(mc_rabbitmq_bus_module_test PRIVATE ws2_32)
    endif()

    add_executable(mc_rabbitmq_bus_plugin_integration_test
        rabbitmq_bus_plugin_integration_test.cpp
    )

    mc_configure_test_target(mc_rabbitmq_bus_plugin_integration_test)

    target_link_libraries(mc_rabbitmq_bus_plugin_integration_test
        PRIVATE
            mc_core_framework
            foundation::foundation
    )

    target_compile_definitions(mc_rabbitmq_bus_plugin_integration_test
        PRIVATE
            MC_TEST_RABBITMQ_BUS_PLUGIN_PATH="$<TARGET_FILE:rabbitmq_bus>"
    )

    add_test(
        NAME mc_rabbitmq_bus_module_test
        COMMAND mc_rabbitmq_bus_module_test
    )

    add_test(
        NAME mc_rabbitmq_bus_plugin_integration_test
        COMMAND mc_rabbitmq_bus_plugin_integration_test
    )

    add_library(mc_rabbitmq_task_flow_support STATIC
        integration/rabbitmq_task_flow/common.cpp
        ../modules/rabbitmq_bus/src/internal/MessageBusServiceProxy.cpp
        ../modules/rabbitmq_bus/src/RabbitMqBusModule.cpp
    )

    mc_configure_test_target(mc_rabbitmq_task_flow_support)

    target_include_directories(mc_rabbitmq_task_flow_support
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/integration/rabbitmq_task_flow
            ${CMAKE_CURRENT_SOURCE_DIR}/../modules/rabbitmq_bus/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../modules/rabbitmq_bus/src/internal
    )

    target_link_libraries(mc_rabbitmq_task_flow_support
        PUBLIC
            amqpcpp
            mc_core_framework
            foundation::foundation
    )

    if(WIN32)
        target_link_libraries(mc_rabbitmq_task_flow_support PUBLIC ws2_32)
    endif()

    add_executable(mc_rabbitmq_task_master_sim
        integration/rabbitmq_task_flow/master_sim.cpp
    )

    mc_configure_test_target(mc_rabbitmq_task_master_sim)

    target_link_libraries(mc_rabbitmq_task_master_sim
        PRIVATE
            mc_rabbitmq_task_flow_support
    )

    add_executable(mc_rabbitmq_task_worker_sim
        integration/rabbitmq_task_flow/worker_sim.cpp
    )

    mc_configure_test_target(mc_rabbitmq_task_worker_sim)

    target_link_libraries(mc_rabbitmq_task_worker_sim
        PRIVATE
            mc_rabbitmq_task_flow_support
    )

    add_library(mc_rabbitmq_perf_support STATIC
        integration/rabbitmq_perf/common.cpp
        ../modules/rabbitmq_bus/src/internal/MessageBusServiceProxy.cpp
        ../modules/rabbitmq_bus/src/RabbitMqBusModule.cpp
    )

    mc_configure_test_target(mc_rabbitmq_perf_support)

    target_include_directories(mc_rabbitmq_perf_support
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/../include
            ${CMAKE_CURRENT_SOURCE_DIR}/../src
            ${CMAKE_CURRENT_SOURCE_DIR}/integration/rabbitmq_perf
            ${CMAKE_CURRENT_SOURCE_DIR}/../modules/rabbitmq_bus/src
            ${CMAKE_CURRENT_SOURCE_DIR}/../modules/rabbitmq_bus/src/internal
    )

    target_link_libraries(mc_rabbitmq_perf_support
        PUBLIC
            amqpcpp
            mc_core_framework
            foundation::foundation
    )

    if(WIN32)
        target_link_libraries(mc_rabbitmq_perf_support PUBLIC ws2_32)
    endif()

    add_executable(mc_rabbitmq_perf_master
        integration/rabbitmq_perf/master_perf.cpp
    )

    mc_configure_test_target(mc_rabbitmq_perf_master)

    target_link_libraries(mc_rabbitmq_perf_master
        PRIVATE
            mc_rabbitmq_perf_support
    )

    add_executable(mc_rabbitmq_perf_worker
        integration/rabbitmq_perf/worker_perf.cpp
    )

    mc_configure_test_target(mc_rabbitmq_perf_worker)

    target_link_libraries(mc_rabbitmq_perf_worker
        PRIVATE
            mc_rabbitmq_perf_support
    )

    if(UNIX AND MC_BUILD_REAL_RABBITMQ_TESTS)
        find_program(MC_DOCKER_EXECUTABLE docker)
        if(MC_DOCKER_EXECUTABLE)
            add_test(
                NAME mc_rabbitmq_task_flow_real_test
                COMMAND /bin/bash
                    ${CMAKE_CURRENT_SOURCE_DIR}/integration/rabbitmq_task_flow/run_task_flow_test.sh
                    $<TARGET_FILE:mc_rabbitmq_task_master_sim>
                    $<TARGET_FILE:mc_rabbitmq_task_worker_sim>
                    ${CMAKE_CURRENT_BINARY_DIR}/rabbitmq_task_flow_runtime
            )

            add_test(
                NAME mc_rabbitmq_perf_real_test
                COMMAND /bin/bash
                    ${CMAKE_CURRENT_SOURCE_DIR}/integration/rabbitmq_perf/run_perf_test.sh
                    $<TARGET_FILE:mc_rabbitmq_perf_master>
                    $<TARGET_FILE:mc_rabbitmq_perf_worker>
                    ${CMAKE_CURRENT_BINARY_DIR}/rabbitmq_perf_runtime
            )
        endif()
    endif()
endif()
