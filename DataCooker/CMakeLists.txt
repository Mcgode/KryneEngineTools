cmake_minimum_required(VERSION 3.9)
project(KryneEngineDataCooker VERSION 1.0.0)


add_executable(DataCooker src/main.cpp src/DataCooker.cpp src/DataCooker.hpp)

target_link_libraries(DataCooker KryneEngine)