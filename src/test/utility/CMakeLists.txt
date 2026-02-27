# src/test/utility/CMakeLists.txt

# Enable Address Sanitizer
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS "${CMAKE_LINKER_FLAGS} -fsanitize=address")

# Find Boost
find_package(Boost REQUIRED COMPONENTS unit_test_framework)

set(TEST_SOURCES
    Utility.cpp
    TestAllocator.cpp
    TestCounter.cpp
    TestFastHashTable.cpp
    TestSwissTableMT.cpp
    OrderBurstControlTest.cpp
    HashTableTest.cpp
    TestHashArray.cpp
    TestHashVarray.cpp
    TestKeyBuilder.cpp
    TestPriorityQueue.cpp
    TestEPoller.cpp
    HashTableTrivialTest.cpp
)

add_executable(utility_tests ${TEST_SOURCES})

target_include_directories(utility_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# Link against Boost and necessary threading libs for Clock/Allocator
target_link_libraries(utility_tests PRIVATE
    Boost::unit_test_framework
    pthread
)

# Add as a ctest entry
add_test(NAME utility_tests COMMAND utility_tests)
