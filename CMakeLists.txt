cmake_minimum_required(VERSION 3.16)
project(FlightSim LANGUAGES CXX)

if(NOT WIN32)
    message(WARNING "FlightSim requires Windows, Winsock, and the MSFS SimConnect SDK")
endif()

add_executable(FlightSim
    BridgeTypes.h
    MotionCalculator.h
    ControllerProtocol.h
    MotionCalculator.cpp
    ControllerProtocol.cpp
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

# Optional local dependency path, shared by the application and protocol tests.
# Supply -DNLOHMANN_JSON_INCLUDE_DIR=... when the header is not on the search path.
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
if(NLOHMANN_JSON_INCLUDE_DIR)
    target_include_directories(FlightSim PRIVATE "${NLOHMANN_JSON_INCLUDE_DIR}")
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

    add_executable(motion_calculator_tests
        MotionCalculator.cpp
        calculate_legs.cpp
        tests/motion_calculator_tests.cpp
    )
    target_compile_features(motion_calculator_tests PRIVATE cxx_std_14)
    target_include_directories(motion_calculator_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME motion_calculator_tests COMMAND motion_calculator_tests)

    if(NLOHMANN_JSON_INCLUDE_DIR)
        add_executable(controller_protocol_tests
            ControllerProtocol.cpp
            tests/controller_protocol_tests.cpp
        )
        target_compile_features(controller_protocol_tests PRIVATE cxx_std_14)
        target_include_directories(controller_protocol_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}" "${NLOHMANN_JSON_INCLUDE_DIR}")
        add_test(NAME controller_protocol_tests COMMAND controller_protocol_tests)
    else()
        message(STATUS "Protocol tests skipped: set NLOHMANN_JSON_INCLUDE_DIR to the folder containing nlohmann/json.hpp")
    endif()
endif()
