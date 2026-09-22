# Flight Simulator Bridge

This Windows application connects Microsoft Flight Simulator or manual HMI input to either a PLC or a Unity client. It reads aircraft orientation and rudder data through the MSFS SimConnect SDK, translates that motion into target lengths and speeds for six platform actuators, and exchanges newline-delimited JSON over TCP.

## Manual input without MSFS

1. Start the application and select **Manual input**. SimConnect closes and connection retries pause. Selecting manual mode does not request a move.
2. Connect the PLC/Unity client and supply valid six-position feedback. **Execute** stays disabled until that feedback is available.
3. Enter **Pitch**, **Roll**, and **Rudder** in degrees and press **Execute**. As requested by the owner, these are simulated MSFS inputs. Pitch and roll are halved, pitch is negated, and rudder is divided by 1.5 before the existing negation/halving into platform yaw. Each platform target is clamped to +/-30 degrees. For example, inputs 20, -40, 12 request targets -10, -20, -4 degrees.
4. The cards compare applied **INPUT** with calculated **TARGET**. Editing fields does not change the active target until Execute is pressed again. Empty, malformed, and nonfinite inputs are rejected; use a period for decimals.
5. The applied target is recalculated every nominal 50 ms using controller feedback and the existing step/speed limits. Output continues to maintain the target. To request level, enter zero in all three fields and press Execute.

Selecting **MSFS (live)** resumes simulator connection attempts and live input. Changing sources clears the cached command and requires new data from the selected source; reentering manual requires Execute again. Source selection is not a physical stop command and cannot cancel a command already sent. A controller disconnect resets the feedback gate; the applied manual target is retained and output resumes after the replacement client supplies valid feedback. There is no feedback-age watchdog or measured-attitude completion signal.

Manual mode removes the need for a running simulator connection. The application still builds against and loads the SimConnect SDK/runtime; this is not an SDK-free build.

## Actuator positions (PLC only)

Select **Actuator positions**, the third input mode in a PLC build, to enter six
absolute controller positions: **A1** through **A6** follow the existing actuator
order. For example, A1 = 200 and A2 = 300 request those positions independently.
This mode does not require MSFS and bypasses the angle-to-leg geometry.

Enter all six fields, then press **Execute**. Fields initially start empty; selecting
the mode or editing fields never sends a new request. Execute requires the connected
PLC's valid six-position feedback. Inputs must be finite values within the PLC's
0..999 representation and are rounded to whole positions before being applied.
These are controller values; the accepted range does not establish physical travel
limits or the mechanical reachability of arbitrary six-position combinations.

The left panel shows the six **applied final positions**. The right actuator cards
show feedback, the current limited command, and its speed. The existing 20-unit
maximum step per nominal 50 ms calculation and speed limits are shared with angle
control. Each calculation uses controller feedback, so successive feedback samples
allow the commands to progress toward all six requested positions. Invalid entries
reject the entire new request and leave the previous applied request active.

No platform orientation is calculated from independent actuator positions. This
mode replaces the orbit drawing with the applied-position panel and shows angle
placeholders. Unity retains its two existing modes and its unchanged orientation
protocol; actuator-position control is PLC-only for now, as requested by the owner.
Switching modes clears the command and requires a new Execute in either manual
mode. Disconnect/reconnect behavior matches manual angles: the applied target is
retained, but output requires valid feedback from the replacement connection.

## Runtime flow

1. `main.cpp` opens the HMI and starts the TCP server for the target selected in `BuildMode.h`. The HMI remains responsive while it waits for external systems.
2. `SimConnectHandler` connects to MSFS and subscribes to pitch, bank, and rudder updates.
3. `MotionController` validates and smooths live pitch, bank, and rudder samples before the existing angle mapping. The time-based exponential filter uses the elapsed sample interval and initializes directly from the first valid sample. Manual inputs remain unfiltered. At most 20 times per second for the selected source, `MotionCalculator` applies the existing rudder division, sign changes, halving, and +/-30-degree clamps, then calculates one rotation matrix shared by all six actuators.
4. With valid controller feedback available, each actuator target is limited to the configured maximum step and speed. `ControllerProtocol` encodes the command as PLC or Unity JSON, which `MotionController` publishes as the latest payload.
5. The output worker sends the latest payload at 20 Hz while the feedback worker receives `currentPositions` messages.
6. If the client disconnects, the feedback worker keeps the listening socket open and waits for a replacement client.

```text
MSFS -> SimConnectHandler -> MotionController [live filter] -+
HMI manual input ----------> MotionController ---------------+-> MotionCalculator -> latest JSON
                                                                                         |
PLC / Unity <- TCPServer <- main.cpp output worker (20 Hz) <-------------------------------+
PLC / Unity -> TCPServer -> ControllerProtocol -> position feedback
                                                     |
                                              MotionCalculator
```

## File map

- `BuildMode.h`: selects exactly one output target and defines its TCP bind address, port, and PLC number format. The current selection is PLC mode.
- `main.cpp`: application entry point, HMI/TCP startup, SimConnect dispatch/retry or manual angle/actuator calculation ticks, Windows message handling, and worker-thread lifetime.
- `DashboardModel.h/.cpp`: synchronized HMI telemetry, connection states, actuator values, message counts, and recent system events.
- `HmiWindow.h/.cpp`: dependency-free Win32/GDI dashboard for the six-actuator platform, system statuses, aircraft attitude, and activity feed.
- `SimConnectHandler.h/.cpp`: owns the SimConnect session, registers telemetry, dispatches callbacks, and forwards orientation samples on the control schedule.
- `MotionController.h/.cpp`: selects the source, filters live simulator input, applies manual input on Execute, schedules manual calculations, shares live/manual angle mapping and command generation, accepts PLC actuator targets, and owns the synchronized payload cache. No SDK or socket dependencies.
- `SimulatorInputFilter.h/.cpp`: time-based first-order low-pass filter for live simulator pitch, bank, and rudder, with reconnect/reset initialization and nonfinite-input rejection.
- `BridgeTypes.h`: shared six-actuator array, platform attitude, and command types without Windows dependencies.
- `MotionCalculator.h/.cpp`: simulator-to-platform angle mapping, platform mounting coordinates, calibration constants, shared actuator step/speed limits for geometric and direct targets, and the control interval. No SDK or networking dependencies.
- `ControllerProtocol.h/.cpp`: PLC/Unity JSON encoding, six-value feedback validation, and buffered extraction of complete messages from TCP chunks. No socket or SDK dependencies.
- `calculate_legs.h/.cpp`: vector operations, Euler-angle rotation matrix, and Stewart-platform actuator-length calculations.
- `TCPServer.h/.cpp`: Winsock server setup, reconnection handling, complete sends, handoff to the protocol parser, keep-alive configuration, synchronized position state, and connection cleanup.
- `ProtocolLogger.h/.cpp`: thread-safe timestamped logging for sent commands and received feedback.
- `tests/calculate_legs_tests.cpp`: platform-independent neutral and boundary-pose checks for all six actuators.
- `tests/dashboard_model_tests.cpp`: platform-independent telemetry state and disconnect-reset checks.
- `tests/motion_calculator_tests.cpp`: existing angle mapping, calibrated targets, actuator order, feedback-relative step limits, and speed regression checks.
- `tests/simulator_input_filter_tests.cpp`: filter initialization, constant/noisy/ramp/step inputs, reset, invalid values, and update-interval independence.
- `tests/controller_protocol_tests.cpp`: both output formats, feedback rejection, fragmented/combined messages, legacy framing, and stream reset checks; requires nlohmann/json.
- `tests/motion_controller_tests.cpp`: live-filter pipeline integration, manual angle and PLC actuator execution without MSFS, source isolation, validation, feedback gating, cadence, rounding, and successive movement calculations; requires nlohmann/json.
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
2. **Filter and map the attitude:** after all three finite live signals are available, `HandleOrientation` passes them and the sample time to `MotionController::UpdateSimulatorInput`. The HMI retains the raw values, while `SimulatorInputFilter` smooths the command path with `alpha = 1 - exp(-dt / tau)` and `filtered += alpha * (raw - filtered)`. The default time constant is 120 ms. The shared mapping then divides rudder by 1.5 before calling `CalculatePlatformAttitude`. Manual Execute uses the same mapping without filtering. Platform pitch and rudder are negated, all three angles are halved, and each is limited to +/-30 degrees. Rudder produces platform yaw; aircraft heading is not subscribed.
3. **Calculate the legs:** `CalculateMotion` takes the platform attitude and a copy of the latest feedback. The geometry applies translation plus a rotated platform mounting point minus a base mounting point. The vector's length becomes a target in the existing controller reference system. Geometry uses yaw about Z, negative platform roll about Y, and platform pitch about X; preserve this established mapping.
4. **Limit the move:** each target is at most 20 position units away from its feedback position per calculation. Speed is the limited distance divided by 0.05 seconds, bounded to 2..500. A level pose currently rounds to 201 on all six actuators, although the reference constant is 200. These are existing calibration results, not new physical-unit claims.
5. **Publish and send:** `BuildCommandPayload` selects the configured JSON format. `MotionController` replaces its cached payload under a mutex. The output worker copies it, and `TCPServer::sendData` adds a newline and handles partial socket sends.
6. **Receive controller feedback:** `TCPServer::receiveData` reads bytes. `FeedbackStream` assembles complete messages. `TryParseFeedback` validates all six values before the server replaces its synchronized position array. The next calculation reads a copy of that array.

The three threads have distinct jobs:

| Thread | Owns/does | Shared data access |
| --- | --- | --- |
| Main | HMI controls/message loop, SimConnect session or manual calculation ticks, attitude mapping and motion calculation | Copies feedback; publishes the latest payload; updates dashboard |
| Feedback worker | Blocking accept/receive, partial-message buffer, client reconnects | Replaces feedback under a mutex; updates dashboard |
| Output worker | Nominal 50 ms send schedule and protocol send logging | Copies the payload under a mutex; updates dashboard |

A **mutex** allows only one thread at a time into a protected section. Here it prevents partially copied arrays, JSON strings, and dashboard snapshots. An **atomic** is used for individual shared flags, such as the shutdown request. Neither makes a group of unrelated operations one transaction. Blocking socket calls stay off the main thread so the window can continue processing messages. Shutdown closes sockets before joining (waiting for) the workers, while their referenced objects are still alive.

Keep changes in the layer that owns the responsibility: motion formulas in `MotionCalculator`, JSON contracts in `ControllerProtocol`, socket operations in `TCPServer`, simulator subscriptions in `SimConnectHandler`, source selection and command scheduling in `MotionController`, and UI controls/drawing in `HmiWindow`. The pure calculation/protocol tests let you check these rules without MSFS or a physical platform. Comments explain contracts, coordinate conventions, and ownership; ordinary C++ statements do not need a comment on every line.

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
- Three split cards below the orbit view compare simulator and platform angles in degrees: `MSFS` on the left shows raw simulator values; `TARGET` on the right shows the filtered platform attitude after motion sign changes, halving, and clamping. Cards are labeled `PITCH`, `ROLL`, and `RUDDER / YAW`: the third compares simulator rudder deflection with the derived platform yaw, not aircraft heading. Targets are calculated angles, not measured platform orientation.
- A six-leg Stewart-platform orbit visualization driven by pitch, roll, yaw, and actuator feedback, with individual base mounting points and no filled base plate.
- Current position, commanded target, and commanded speed for all six actuators.
- Recent startup, connection, feedback rejection, error, and shutdown messages that also provide the operational context previously available only in the console. Attitude scaling and clamping do not emit warnings; their results remain visible in the raw/target cards.

The window title is "Flight Simulator Motion HMI". The input-source selector and manual fields sit below the status cards. These native controls match the dark dashboard with padded, rounded fields, visible focus/hover states, a dark dropdown, and a cyan Execute button when enabled. Disabled controls use muted colors; keyboard selection, tab navigation, and text editing retain native Windows behavior. In the platform view, drag with the left mouse button to orbit, use the mouse wheel to zoom, and double-click to reset the camera. These controls affect only the view; the drawing is illustrative, not a physical geometry measurement.

In live MSFS mode, both sides of the cards update without a PLC/Unity client or position feedback. Pitch, roll, and filtered platform targets update on the existing nominal 20 Hz calculation schedule; raw rudder updates when its separate SimConnect message arrives. Cards show placeholders until their required samples arrive and after SimConnect disconnects, including while waiting for new samples on reconnect. The orbit view continues to use the filtered, processed platform attitude shown in the target column. No barrel-roll return curve has been implemented.

Closing the HMI performs the same orderly worker, socket, and SimConnect shutdown as closing the application. If MSFS is not running at startup, the bridge keeps the HMI available and retries SimConnect every five seconds while MSFS input is selected. Manual input suspends those retries.

## Safety and review notes

- The six actuator values and JSON keys are protocol contracts. Coordinate changes with the PLC and Unity consumers.
- Output remains disabled until the connected client supplies its first valid six-value position message. A reconnected client must supply fresh feedback before commands resume.
- Motion constants such as attitude limits, maximum step, minimum speed, and maximum speed are grouped at the top of `MotionCalculator.cpp` for review and tuning.
- Live-input filter strength is controlled by `SimulatorFilterSettings::TimeConstant` in `SimulatorInputFilter.h`. A larger value suppresses more noise and adds more lag; a smaller value follows MSFS more quickly.
- The geometry implementation in `calculate_legs.cpp` is independent of Windows, sockets, and SimConnect. The HMI drawing is illustrative and does not measure platform orientation.
- The feedback gate has no age timeout. Cached commands are retained across simulator loss or client reconnect, but cleared when changing input sources. Unit tests do not establish safe physical operation.
- Protocol logs are appended to `logs/FlightSim_StewartServer_Log.txt`; the directory is created automatically.

## Building

Use Visual Studio 2022 and build `StuartServer.sln` for x64 on Windows. A complete build requires the Microsoft Flight Simulator SimConnect SDK, Winsock, and nlohmann/json headers. The Debug x64 project configuration currently contains developer-specific include and library paths; replace those locally rather than committing another developer's absolute paths.

The auxiliary CMake definition contains the same active source list as the Visual Studio project, but it is not a portable build: the application still uses Windows and SimConnect APIs. A CMake configure on macOS or Linux is therefore not proof that the application builds.

The geometry, motion, live-input-filter, and dashboard-model checks do not require SimConnect and can run on any platform with CMake and a C++14 compiler:

```shell
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target calculate_legs_tests dashboard_model_tests motion_calculator_tests simulator_input_filter_tests
ctest --test-dir build -C Debug --output-on-failure -E "controller_protocol_tests|motion_controller_tests"
```

Use Debug because the existing dashboard tests use assertions. With Visual Studio, `--config Debug` and `-C Debug` select the configuration; `CMAKE_BUILD_TYPE` applies to single-configuration generators.

To include protocol tests, install nlohmann/json or configure `NLOHMANN_JSON_INCLUDE_DIR` to the include directory containing `nlohmann/json.hpp`. This is a local CMake cache setting, not a machine-specific source change. When found, also build `controller_protocol_tests` and `motion_controller_tests` before running CTest; otherwise configuration reports that both suites are skipped. The application uses this include setting too, but CMake still does not configure SimConnect discovery/linking.

With the JSON dependency available, run the additional suites and then all tests:

```shell
cmake --build build --config Debug --target controller_protocol_tests motion_controller_tests
ctest --test-dir build -C Debug --output-on-failure
```

The September 2026 cleanup was checked with Debug/Release x64 builds, all four test suites, and 5,324 exact comparisons against the original motion calculation. Temporary PLC and Unity builds bound to loopback received live MSFS telemetry and passed feedback rejection, fragmented/combined/legacy messages, reconnect gating, and HMI-close shutdown checks. Short command-stream samples measured 20.1 Hz and 20.0 Hz respectively. No physical platform was operated; startup without MSFS and simulator loss/reconnect were not exercised in that run.

Manual-input validation in September 2026: Debug/Release x64 builds and all five test suites passed. Temporary PLC and Unity builds bound only to loopback, with SimConnect_Open deliberately bypassed to simulate an unavailable simulator, passed native UI startup, feedback gating, invalid-input rejection, Execute, draft isolation, input mapping, step limits, fragmented/combined feedback, reconnect gating, source switching, resize, and HMI-close shutdown checks. Short streams measured about 19.8 Hz in both modes. A separate loopback Unity run passed live MSFS -> manual -> live MSFS switching, and the UI was visually checked at minimum window size. This did not operate physical hardware or test actual MSFS process shutdown/restart.

Actuator-position validation in September 2026: Debug/Release x64 builds and all
five suites passed. The motion-controller suite also passed in an isolated Unity
build, including rejection of unsupported direct-position mode. A loopback PLC
run reached six different requested positions with the retained step/speed limits
and passed blank/malformed/nonfinite/range rejection, draft isolation, all three
mode transitions, reconnect gating, minimum-size UI inspection, and shutdown;
output measured about 20.0 Hz. Existing manual-angle loopback checks passed for
both PLC and Unity, whose HMI retained exactly two modes. SimConnect was deliberately
unavailable in these runtime checks; no physical hardware was operated.

## Manual validation checklist

- Build Debug x64 and Release x64 on Windows.
- Verify the English window title, orbit dragging (including releasing outside the view), wheel zoom limits, double-click reset, and resizing without the platform overlapping the attitude section.
- Start and stop MSFS/SimConnect cleanly. Switch from live MSFS to manual and back.
- With MSFS stopped, select manual, enter pitch/roll/rudder degrees, and Execute using a simulated controller first. Verify draft edits and rejected inputs leave the applied target unchanged; select level and Execute to return toward level.
- With MSFS 2020 connected and no PLC/Unity client, verify raw pitch, roll, and rudder cards update. Roll through inverted and confirm values beyond +/-30 degrees remain visible. Check placeholders on simulator disconnect and until fresh samples arrive after reconnect.
- In PLC actuator-position mode, verify A1-A6 ordering, applied positions versus intermediate commands, invalid-input rejection, and a new Execute requirement after switching modes.
- Connect, disconnect, and reconnect the selected PLC or Unity client.
- Measure the nominal 20 Hz newline-delimited command stream; Windows scheduling is not a hard real-time guarantee.
- Send fragmented and combined feedback messages and verify all six positions.
- Exercise pitch, roll, and rudder limits with the physical platform in a safe state.
- Repeat the protocol checks in both `TARGET_PLC` and `TARGET_UNITY` modes.

See the [project wiki](https://gitlab.com/AvansInformatica/flight-simulator/flight-simulator/-/wikis/home) for additional information.
