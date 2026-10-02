cmake_minimum_required(VERSION 3.21)
project(RaceMenuBufferTests LANGUAGES CXX)
enable_testing()
add_executable(dynamic-data-transaction adjusted-dynamic-data.cpp)
target_compile_features(dynamic-data-transaction PRIVATE cxx_std_23)
target_include_directories(dynamic-data-transaction PRIVATE ../skee64)
add_test(NAME dynamic-data-transaction COMMAND dynamic-data-transaction)
# Standalone, opt-in test project. It does not build/load the plugin, execute
# automatically during production compilation, or require game dependencies.
