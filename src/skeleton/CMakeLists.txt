add_executable(skeleton_app
    main.cpp
)

target_include_directories(skeleton_app
    PRIVATE
        ${PROJECT_SOURCE_DIR}/include
)

if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(skeleton_app PRIVATE -Wall -Wextra -Wpedantic)
endif()

