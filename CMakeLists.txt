cmake_minimum_required(VERSION 3.8.0)

set(PROJECT_NAME flight_simulator_lib)


project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

enable_testing()

add_subdirectory(external/googletest)
add_subdirectory(external/nlohmann-json)

# add_library(SimConnect STATIC IMPORTED)
# add_library(MSFS::SimConnect ALIAS SimConnect)
# set_property(TARGET SimConnect PROPERTY
#              IMPORTED_LOCATION "c:/MSFS SDK/SimConnect SDK/lib/SimConnect.dll")
# set_property(TARGET SimConnect PROPERTY
#              IMPORTED_IMPLIB "c:/MSFS SDK/SimConnect SDK/lib/SimConnect.lib")
# set_property(TARGET SimConnect PROPERTY
#             INTERFACE_INCLUDE_DIRECTORIES "c:/MSFS SDK/SimConnect SDK/include")

set(Headers
    include/calculations.h
    include/controller.h
    include/cout_output_strategy.h
    include/csv_input_strategy.h
    include/flight_data.h
    include/input_strategy.h
    include/output_strategy.h
    include/math_func.h
    include/max_piston_strategy.h
    include/mfs_input_strategy.h
    include/smooth_step_piston_strategy.h
    include/tcp_client.h
    include/tcp_output_strategy.h
)

set(Sources
    src/calculations.cpp
    src/controller.cpp
    src/cout_output_strategy.cpp
    src/csv_input_strategy.cpp
    src/flight_data.cpp
    src/input_strategy.cpp
    src/output_strategy.cpp
    src/math_func.cpp
    src/mfs_input_strategy.cpp
    src/smooth_step_piston_strategy.cpp
    src/tcp_client.cpp
    src/tcp_output_strategy.cpp
)

add_library(${PROJECT_NAME} STATIC ${Sources} ${Headers})

add_subdirectory(tests)
add_subdirectory(main)

target_link_libraries(${PROJECT_NAME}
    PRIVATE 
    nlohmann_json
    # SimConnect
)
