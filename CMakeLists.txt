cmake_minimum_required(VERSION 3.16)
project(FlightSim LANGUAGES CXX)

if(NOT WIN32)
    message(WARNING "FlightSim requires Windows, Winsock, and the MSFS SimConnect SDK")
endif()

add_executable(FlightSim
    BuildMode.h
    DashboardModel.cpp
    DashboardModel.h
    HmiWindow.cpp
    HmiWindow.h
    ProtocolLogger.cpp
    ProtocolLogger.h
    SimConnectHandler.cpp
    SimConnectHandler.h
    TCPServer.cpp
    TCPServer.h
    calculate_legs.cpp
    calculate_legs.h
    main.cpp
)

target_compile_features(FlightSim PRIVATE cxx_std_14)
target_include_directories(FlightSim PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(WIN32)
    target_link_libraries(FlightSim PRIVATE gdi32 user32 ws2_32)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(calculate_legs_tests
        calculate_legs.cpp
        calculate_legs.h
        tests/calculate_legs_tests.cpp
    )
    target_compile_features(calculate_legs_tests PRIVATE cxx_std_14)
    target_include_directories(calculate_legs_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME calculate_legs_tests COMMAND calculate_legs_tests)

    add_executable(dashboard_model_tests
        DashboardModel.cpp
        DashboardModel.h
        tests/dashboard_model_tests.cpp
    )
    target_compile_features(dashboard_model_tests PRIVATE cxx_std_14)
    target_include_directories(dashboard_model_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME dashboard_model_tests COMMAND dashboard_model_tests)
endif()
