cmake_minimum_required(VERSION 3.10)
project(eagle-cli)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Qt5
find_package(Qt5 REQUIRED COMPONENTS Core)

# Add executable
add_executable(eagle-cli main.cpp)

# Link libraries
target_link_libraries(eagle-cli Qt5::Core)

# Set output directory
set_target_properties(eagle-cli PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install
install(TARGETS eagle-cli DESTINATION bin)
