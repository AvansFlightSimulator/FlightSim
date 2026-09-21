# Flight Simulator Bridge

This Windows application connects Microsoft Flight Simulator to either a PLC or a Unity client. It reads aircraft orientation and rudder data through the MSFS SimConnect SDK, translates that motion into target lengths and speeds for six platform actuators, and exchanges newline-delimited JSON over TCP.

## Runtime flow

1. `main.cpp` opens the HMI and starts the TCP server for the target selected in `BuildMode.h`. The HMI remains responsive while it waits for external systems.
2. `SimConnectHandler` connects to MSFS and subscribes to pitch, bank, and rudder updates.
3. At most 20 times per second, `MotionCalculator` converts simulator units, applies the existing sign changes, halves each angle, and clamps it to +/-30 degrees. It calculates one rotation matrix shared by all six actuators.
4. With valid controller feedback available, each actuator target is limited to the configured maximum step and speed. `ControllerProtocol` encodes the command as PLC or Unity JSON, which the handler publishes as the latest payload.
5. The output worker sends the latest payload at 20 Hz while the feedback worker receives `currentPositions` messages.
6. If the client disconnects, the feedback worker keeps the listening socket open and waits for a replacement client.

```text
MSFS -> SimConnectHandler -> MotionCalculator -> ControllerProtocol -> latest JSON
                                                                         |
PLC / Unity <- TCPServer <- main.cpp output worker (20 Hz) <---------------+
PLC / Unity -> TCPServer -> ControllerProtocol -> position feedback
                                                     |
                                              MotionCalculator
```

## File map

- `BuildMode.h`: selects exactly one output target and defines its TCP bind address, port, and PLC number format. The current selection is PLC mode.
- `main.cpp`: application entry point, HMI/TCP startup, SimConnect dispatch loop and reconnect attempts, Windows message handling, and worker-thread lifetime.
- `DashboardModel.h/.cpp`: synchronized HMI telemetry, connection states, actuator values, message counts, and recent system events.
- `HmiWindow.h/.cpp`: dependency-free Win32/GDI dashboard for the six-actuator platform, system statuses, aircraft attitude, and activity feed.
- `SimConnectHandler.h/.cpp`: owns the SimConnect session, registers telemetry, dispatches callbacks, limits calculation frequency, and publishes the latest command.
- `BridgeTypes.h`: shared six-actuator array, platform attitude, and command types without Windows dependencies.
- `MotionCalculator.h/.cpp`: simulator-to-platform angle mapping, platform mounting coordinates, calibration constants, actuator step/speed limits, and shared control interval. No SDK or networking dependencies.
- `ControllerProtocol.h/.cpp`: PLC/Unity JSON encoding, six-value feedback validation, and buffered extraction of complete messages from TCP chunks. No socket or SDK dependencies.
- `calculate_legs.h/.cpp`: vector operations, Euler-angle rotation matrix, and Stewart-platform actuator-length calculations.
- `TCPServer.h/.cpp`: Winsock server setup, reconnection handling, complete sends, handoff to the protocol parser, keep-alive configuration, synchronized position state, and connection cleanup.
- `ProtocolLogger.h/.cpp`: thread-safe timestamped logging for sent commands and received feedback.
- `tests/calculate_legs_tests.cpp`: platform-independent neutral and boundary-pose checks for all six actuators.
- `tests/dashboard_model_tests.cpp`: platform-independent telemetry state and disconnect-reset checks.
- `tests/motion_calculator_tests.cpp`: existing angle mapping, calibrated targets, actuator order, feedback-relative step limits, and speed regression checks.
- `tests/controller_protocol_tests.cpp`: both output formats, feedback rejection, fragmented/combined messages, legacy framing, and stream reset checks; requires nlohmann/json.
- `StuartServer.sln`: Visual Studio solution containing the server project.
- `StuartClient.vcxproj`: primary Visual C++ build definition. Despite its filename, its project name is `StuartServer` and it builds the server sources.
- `StuartClient.vcxproj.filters`: Visual Studio Solution Explorer grouping for the server source and header files.
- `CMakeLists.txt`: auxiliary CMake source list. The application still requires Windows, Winsock, SimConnect, and nlohmann/json to build.
- `.gitmodules`: declares optional googletest and nlohmann/json submodules; they are not currently checked out in this working tree.
- `.gitignore`: excludes C++, Visual Studio, build, and log artifacts.
- `LICENSE`: Apache License 2.0 terms for the project.

## Student reading guide

Start with `main.cpp` to see the application's lifetime, then follow one sample:

1. **Receive simulator data:** `SimConnectHandler::HandleDispatch` receives two SDK message types. Pitch and bank are radians; rudder is degrees. The structs at the top of the file must match the subscription field order.
2. **Map the attitude:** `HandleOrientation` passes those values to `CalculatePlatformAttitude`. Raw HMI values keep simulator signs. Platform pitch and rudder are negated, all three angles are halved, and each is limited to +/-30 degrees. Rudder produces platform yaw; aircraft heading is not subscribed.
3. **Calculate the legs:** `CalculateMotion` takes the platform attitude and a copy of the latest feedback. The geometry applies translation plus a rotated platform mounting point minus a base mounting point. The vector's length becomes a target in the existing controller reference system. Geometry uses yaw about Z, negative platform roll about Y, and platform pitch about X; preserve this established mapping.
4. **Limit the move:** each target is at most 20 position units away from its feedback position per calculation. Speed is the limited distance divided by 0.05 seconds, bounded to 2..500. A level pose currently rounds to 201 on all six actuators, although the reference constant is 200. These are existing calibration results, not new physical-unit claims.
5. **Publish and send:** `BuildCommandPayload` selects the configured JSON format. The handler replaces its cached payload under a mutex. The output worker copies it, and `TCPServer::sendData` adds a newline and handles partial socket sends.
6. **Receive controller feedback:** `TCPServer::receiveData` reads bytes. `FeedbackStream` assembles complete messages. `TryParseFeedback` validates all six values before the server replaces its synchronized position array. The next calculation reads a copy of that array.

The three threads have distinct jobs:

| Thread | Owns/does | Shared data access |
| --- | --- | --- |
| Main | HMI message loop, SimConnect session, attitude mapping and motion calculation | Copies feedback; publishes the latest payload; updates dashboard |
| Feedback worker | Blocking accept/receive, partial-message buffer, client reconnects | Replaces feedback under a mutex; updates dashboard |
| Output worker | Nominal 50 ms send schedule and protocol send logging | Copies the payload under a mutex; updates dashboard |

A **mutex** allows only one thread at a time into a protected section. Here it prevents partially copied arrays, JSON strings, and dashboard snapshots. An **atomic** is used for individual shared flags, such as the shutdown request. Neither makes a group of unrelated operations one transaction. Blocking socket calls stay off the main thread so the window can continue processing messages. Shutdown closes sockets before joining (waiting for) the workers, while their referenced objects are still alive.

Keep changes in the layer that owns the responsibility: motion formulas in `MotionCalculator`, JSON contracts in `ControllerProtocol`, socket operations in `TCPServer`, simulator subscriptions in `SimConnectHandler`, and drawing in `HmiWindow`. The pure calculation/protocol tests let you check these rules without MSFS or a physical platform. Comments explain contracts, coordinate conventions, and ownership; ordinary C++ statements do not need a comment on every line.

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
- Three split cards below the orbit view compare simulator and platform angles in degrees: `MSFS` on the left shows raw simulator values; `TARGET` on the right shows the processed platform attitude after motion sign changes, halving, and clamping. Cards are labeled `PITCH`, `ROLL`, and `RUDDER / YAW`: the third compares simulator rudder deflection with the derived platform yaw, not aircraft heading. Targets are calculated angles, not measured platform orientation.
- A six-leg Stewart-platform orbit visualization driven by pitch, roll, yaw, and actuator feedback, with individual base mounting points and no filled base plate.
- Current position, commanded target, and commanded speed for all six actuators.
- Recent startup, connection, feedback rejection, error, and shutdown messages that also provide the operational context previously available only in the console. Attitude scaling and clamping do not emit warnings; their results remain visible in the raw/target cards.

The window title is "Flight Simulator Motion HMI". In the platform view, drag with the left mouse button to orbit, use the mouse wheel to zoom, and double-click to reset the camera. These controls affect only the view; the drawing is illustrative, not a physical geometry measurement.

Both sides of the cards update without a PLC/Unity client or position feedback. Pitch, roll, and platform targets update on the existing nominal 20 Hz calculation schedule; raw rudder updates when its separate SimConnect message arrives. Cards show placeholders until their required samples arrive and after SimConnect disconnects, including while waiting for new samples on reconnect. The orbit view continues to use the existing processed platform attitude shown in the target column. No barrel-roll return curve has been implemented.

Closing the HMI performs the same orderly worker, socket, and SimConnect shutdown as closing the application. If MSFS is not running at startup, the bridge keeps the HMI available and retries SimConnect every five seconds.

## Safety and review notes

- The six actuator values and JSON keys are protocol contracts. Coordinate changes with the PLC and Unity consumers.
- Output remains disabled until the connected client supplies its first valid six-value position message. A reconnected client must supply fresh feedback before commands resume.
- Motion constants such as attitude limits, maximum step, minimum speed, and maximum speed are grouped at the top of `MotionCalculator.cpp` for review and tuning.
- The geometry implementation in `calculate_legs.cpp` is independent of Windows, sockets, and SimConnect. The HMI drawing is illustrative and does not measure platform orientation.
- The feedback gate has no age timeout. Cached commands are retained across simulator loss or client reconnect; this cleanup preserves that behavior. Unit tests do not establish safe physical operation.
- Protocol logs are appended to `logs/FlightSim_StewartServer_Log.txt`; the directory is created automatically.

## Building

Use Visual Studio 2022 and build `StuartServer.sln` for x64 on Windows. A complete build requires the Microsoft Flight Simulator SimConnect SDK, Winsock, and nlohmann/json headers. The Debug x64 project configuration currently contains developer-specific include and library paths; replace those locally rather than committing another developer's absolute paths.

The auxiliary CMake definition contains the same active source list as the Visual Studio project, but it is not a portable build: the application still uses Windows and SimConnect APIs. A CMake configure on macOS or Linux is therefore not proof that the application builds.

The geometry, motion, and dashboard-model checks do not require SimConnect and can run on any platform with CMake and a C++14 compiler:

```shell
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target calculate_legs_tests dashboard_model_tests motion_calculator_tests
ctest --test-dir build -C Debug --output-on-failure -E controller_protocol_tests
```

Use Debug because the existing dashboard tests use assertions. With Visual Studio, `--config Debug` and `-C Debug` select the configuration; `CMAKE_BUILD_TYPE` applies to single-configuration generators.

To include protocol tests, install nlohmann/json or configure `NLOHMANN_JSON_INCLUDE_DIR` to the include directory containing `nlohmann/json.hpp`. This is a local CMake cache setting, not a machine-specific source change. When found, also build `controller_protocol_tests` before running CTest; otherwise configuration reports that protocol tests are skipped. The application uses this include setting too, but CMake still does not configure SimConnect discovery/linking.

With the JSON dependency available, run the additional suite and then all tests:

```shell
cmake --build build --config Debug --target controller_protocol_tests
ctest --test-dir build -C Debug --output-on-failure
```

The September 2026 cleanup was checked with Debug/Release x64 builds, all four test suites, and 5,324 exact comparisons against the original motion calculation. Temporary PLC and Unity builds bound to loopback received live MSFS telemetry and passed feedback rejection, fragmented/combined/legacy messages, reconnect gating, and HMI-close shutdown checks. Short command-stream samples measured 20.1 Hz and 20.0 Hz respectively. No physical platform was operated; startup without MSFS and simulator loss/reconnect were not exercised in that run.

## Manual validation checklist

- Build Debug x64 and Release x64 on Windows.
- Verify the English window title, orbit dragging (including releasing outside the view), wheel zoom limits, double-click reset, and resizing without the platform overlapping the attitude section.
- Start and stop MSFS/SimConnect cleanly.
- With MSFS 2020 connected and no PLC/Unity client, verify raw pitch, roll, and rudder cards update. Roll through inverted and confirm values beyond +/-30 degrees remain visible. Check placeholders on simulator disconnect and until fresh samples arrive after reconnect.
- Connect, disconnect, and reconnect the selected PLC or Unity client.
- Measure the nominal 20 Hz newline-delimited command stream; Windows scheduling is not a hard real-time guarantee.
- Send fragmented and combined feedback messages and verify all six positions.
- Exercise pitch, roll, and rudder limits with the physical platform in a safe state.
- Repeat the protocol checks in both `TARGET_PLC` and `TARGET_UNITY` modes.

See the [project wiki](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/wikis/home) for additional information.
