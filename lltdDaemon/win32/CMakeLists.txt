cmake_minimum_required(VERSION 3.16)

project(lltd_win32 C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(lltd_win32
    ../lltdAutomata.c
    ../windows/windows-backend-legacy.c
    ../windows/windows-backend-ndis.c
    ../windows/windows-main.c
    ../windows/windows-ops.c
    ../windows/windows-shared.c
    lltd_win32_stubs.c
)

target_include_directories(lltd_win32 PRIVATE ..)

target_compile_definitions(lltd_win32 PRIVATE LLTD_TARGET_WIN32_USERMODE)

target_link_libraries(lltd_win32 PRIVATE ws2_32 iphlpapi)
