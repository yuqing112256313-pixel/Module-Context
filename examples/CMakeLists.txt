add_executable(mc_context_lifecycle_example
    basic_context_lifecycle.cpp
)

target_link_libraries(mc_context_lifecycle_example
    PRIVATE
        mc_core_framework
)
