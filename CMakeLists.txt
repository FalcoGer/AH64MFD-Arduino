cmake_minimum_required(VERSION 3.0.0)

add_compile_definitions(ARDUINO=10820)
add_compile_definitions(USBCON=1)

set(CMAKE_TOOLCHAIN_FILE "/home/paul/repositories/arduino/Arduino-CMake-Toolchain/Arduino-toolchain.cmake")
add_compile_options("-D_LIBCPP_HAS_NO_THREADS")

set(ARDUINO_INSTALL_PATH "/home/paul/repositories/arduino/arduino-ide/build")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -pedantic")

project (AH64MFDController CXX)

add_executable(AH64MFDController lnk/AH64MFD-Arduino.cpp)

target_link_arduino_libraries(AH64MFDController PRIVATE HID HID-Project core)

set_target_properties(AH64MFDController PROPERTIES LINKER_LANGUAGE CXX)

target_enable_arduino_upload(AH64MFDController)
