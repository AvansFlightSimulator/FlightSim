cmake_minimum_required(VERSION 3.8.0)

project(flight_simulator_main VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(Sources 
    main.cpp
)

add_executable(flight_simulator_main ${Sources})

target_link_libraries(flight_simulator_main PRIVATE
    flight_simulator_lib
    # MSFS::SimConnect
)
