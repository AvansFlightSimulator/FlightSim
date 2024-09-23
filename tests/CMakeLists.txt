cmake_minimum_required(VERSION 3.8.0)

set(This flight_simulator_tests)

set(Sources 
    math_func.cpp
    tcp_output_strategy.cpp
    smooth_step_piston_strategy.cpp
)

add_executable(${This} ${Sources})

target_link_libraries(${This} PUBLIC
    gtest_main
    flight_simulator_lib
)

add_test(
    NAME ${This}
    COMMAND ${This}
)