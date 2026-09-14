# Flight Simulator Bridge

This Windows application connects Microsoft Flight Simulator to either a PLC or a Unity client. It reads aircraft orientation and rudder data through the MSFS SimConnect SDK, translates that motion into target lengths and speeds for six platform actuators, and exchanges newline-delimited JSON over TCP.

## Runtime flow

1. `main.cpp` opens the HMI and starts the TCP server for the target selected in `BuildMode.h`. The HMI remains responsive while it waits for external systems.
2. `SimConnectHandler` connects to MSFS and subscribes to pitch, bank, and rudder updates.
3. At most 20 times per second, the handler clamps the requested attitude to 30 degrees on each axis and calls the Stewart-platform geometry functions for all six actuators.
4. Each actuator target is limited to the configured maximum step and speed, then published as the latest PLC or Unity payload.
5. The output worker sends the latest payload at 20 Hz while the feedback worker receives `currentPositions` messages.
6. If the client disconnects, the feedback worker keeps the listening socket open and waits for a replacement client.

```text
MSFS 2020 -> SimConnectHandler -> six-actuator calculation -> latest command
                                                           |
PLC / Unity <- newline JSON <- TCPServer <- 20 Hz output ---+
PLC / Unity -> position feedback -> TCPServer -> actuator calculation
```

## File map

- `BuildMode.h`: selects exactly one output target and defines its TCP bind address, port, and PLC number format. The current selection is PLC mode.
- `main.cpp`: application entry point, HMI/TCP startup, SimConnect dispatch loop and reconnect attempts, Windows message handling, and worker-thread lifetime.
- `DashboardModel.h/.cpp`: synchronized HMI telemetry, connection states, actuator values, message counts, and recent system events.
- `HmiWindow.h/.cpp`: dependency-free Win32/GDI dashboard for the six-actuator platform, system statuses, aircraft attitude, and activity feed.
- `SimConnectHandler.h/.cpp`: SimConnect data definitions and callback, 20 Hz calculation limit, attitude conversion, actuator command calculation, and target-specific JSON serialization.
- `calculate_legs.h/.cpp`: vector operations, Euler-angle rotation matrix, and Stewart-platform actuator-length calculations.
- `TCPServer.h/.cpp`: Winsock server setup, reconnection handling, complete sends, buffered feedback parsing, keep-alive configuration, synchronized position state, and connection cleanup.
- `ProtocolLogger.h/.cpp`: thread-safe timestamped logging for sent commands and received feedback.
- `tests/calculate_legs_tests.cpp`: platform-independent neutral and boundary-pose checks for all six actuators.
- `tests/dashboard_model_tests.cpp`: platform-independent telemetry state and disconnect-reset checks.
- `StuartServer.sln`: Visual Studio solution containing the server project.
- `StuartClient.vcxproj`: primary Visual C++ build definition. Despite its filename, its project name is `StuartServer` and it builds the server sources.
- `StuartClient.vcxproj.filters`: Visual Studio Solution Explorer grouping for the server source and header files.
- `CMakeLists.txt`: auxiliary CMake source list. The application still requires Windows, Winsock, SimConnect, and nlohmann/json to build.
- `.gitmodules`: declares optional googletest and nlohmann/json submodules; they are not currently checked out in this working tree.
- `.gitignore`: excludes C++, Visual Studio, build, and log artifacts.
- `LICENSE`: Apache License 2.0 terms for the project.

## Protocols

PLC mode sends `positions` and `speeds`, each containing exactly six values. By default they are zero-padded three-character strings. Unity mode additionally sends `orientation` and `legs`, and uses JSON numbers. Both modes append a newline to every outbound JSON object.

Incoming feedback must contain exactly six numeric values:

```json
{"currentPositions":[200,200,200,200,200,200]}
```

Terminate incoming messages with a newline. For compatibility, a complete single JSON object without a newline is also accepted. Invalid types and array lengths are rejected without changing the last valid positions.

## HMI

The HMI opens as soon as the executable starts; a PLC connection is not required for the window to appear. It shows:

- Microsoft Flight Simulator/SimConnect and PLC/Unity connection state.
- Live receive/transmit counters and feedback freshness.
- A six-leg Stewart-platform visualization driven by pitch, roll, yaw, and actuator feedback.
- Current position, commanded target, and commanded speed for all six actuators.
- Recent startup, connection, limit, and shutdown messages that also provide the operational context previously available only in the console.

Closing the HMI performs the same orderly worker, socket, and SimConnect shutdown as closing the application. If MSFS is not running at startup, the bridge keeps the HMI available and retries SimConnect every five seconds.

## Safety and review notes

- The six actuator values and JSON keys are protocol contracts. Coordinate changes with the PLC and Unity consumers.
- Output remains disabled until the connected client supplies its first valid six-value position message. A reconnected client must supply fresh feedback before commands resume.
- Motion constants such as attitude limits, maximum step, minimum speed, and maximum speed are grouped at the top of `SimConnectHandler.cpp` for review and tuning.
- The geometry implementation in `calculate_legs.cpp`
- Protocol logs are appended to `logs/FlightSim_StewartServer_Log.txt`; the directory is created automatically.

## Building

Use Visual Studio 2022 and build `StuartServer.sln` for x64 on Windows. A complete build requires the Microsoft Flight Simulator SimConnect SDK, Winsock, and nlohmann/json headers. The Debug x64 project configuration currently contains developer-specific include and library paths; replace those locally rather than committing another developer's absolute paths.

The auxiliary CMake definition contains the same active source list as the Visual Studio project, but it is not a portable build: the application still uses Windows and SimConnect APIs. A CMake configure on macOS or Linux is therefore not proof that the application builds.

The calculation and dashboard-model checks do not require SimConnect and can run on any platform with CMake and a C++14 compiler:

```shell
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build --target calculate_legs_tests dashboard_model_tests
ctest --test-dir build --output-on-failure
```

## Manual validation checklist

- Build Debug x64 and Release x64 on Windows.
- Start and stop MSFS/SimConnect cleanly.
- Connect, disconnect, and reconnect the selected PLC or Unity client.
- Confirm an exact 20 Hz newline-delimited command stream.
- Send fragmented and combined feedback messages and verify all six positions.
- Exercise pitch, roll, and rudder limits with the physical platform in a safe state.
- Repeat the protocol checks in both `TARGET_PLC` and `TARGET_UNITY` modes.

See the [project wiki](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/wikis/home) for additional information.
