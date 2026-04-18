cmake_minimum_required(VERSION 3.16)
project(SharePointSyncSpeedup LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build as a Windows GUI subsystem app (no console window)
add_executable(SharePointSyncSpeedup WIN32 src/main.cpp)

# Strip debug info in Release for smallest binary
if(MSVC)
    target_compile_options(SharePointSyncSpeedup PRIVATE /W4)
    target_link_options(SharePointSyncSpeedup PRIVATE /SUBSYSTEM:WINDOWS)
else()
    target_compile_options(SharePointSyncSpeedup PRIVATE -Wall -Wextra)
    target_link_options(SharePointSyncSpeedup PRIVATE -mwindows)
endif()
