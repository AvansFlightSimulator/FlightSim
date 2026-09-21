# FlightSim agent guide

## Purpose and scope

This repository implements a Windows C++ bridge between Microsoft Flight
Simulator (MSFS) and a six-actuator Stewart platform. It reads aircraft telemetry
through SimConnect, calculates actuator targets and speeds, exchanges JSON with
a PLC or Unity client over TCP, and displays operational telemetry in a native HMI.

The owner-confirmed primary goal is operating the real six-actuator platform
through a PLC. Unity is a supporting simulation/testing tool.

## Owner-confirmed milestones

Work in this order:

1. **Understand the system through data.** Collect as much useful diagnostic data
   as possible to identify what is happening throughout the system. This is the
   first priority, ahead of changing motion calculations.
2. **Scale motion inputs into ranges.** Revisit the roll, pitch, and rudder
   calculations and scale their values into ranges. The owner confirmed linear
   roll/pitch scaling from simulator +/-60 degrees to platform +/-30 degrees,
   holding at the corresponding limit up to 80 degrees in magnitude. Beyond
   80 degrees, the requested behavior is to return toward neutral to avoid an
   opposite-limit command when roll wraps during a barrel roll. The owner further
   clarified that platform roll should return from the first limit to zero at
   inverted, then build toward the opposite limit as the aircraft exits the roll,
   before scaling back to zero near level. This is an angle-dependent return,
   rather than holding neutral through the entire inverted portion. The owner's
   example treated 90 degrees as inverted; inverted roll is 180 degrees. Confirm
   the resulting transition breakpoints (earlier 80 degrees versus the later
   example of reaching the opposite limit at 90 degrees) before implementation.
   Pitch behavior beyond 80 degrees, transition rate requirements, and whether
   other axes should be neutralized during the roll remain unspecified. Rudder
   scaling remains unspecified.
3. **Reassess with the owner.** After these milestones, review what is needed
   next rather than inventing a further roadmap.

The owner requested raw MSFS 2020 data in the existing pitch/roll/yaw card area
to observe simulator angles before deciding motion behavior. At the owner's
request, each card now compares raw MSFS degrees on the left with calculated
platform target degrees on the right. Raw pitch, bank/roll, and rudder retain
simulator signs without motion scaling or clamping; targets use the existing
processed attitude. These targets are not measured platform orientation. The
third card is labeled RUDDER / YAW because it compares rudder input with derived
platform yaw; the bridge does not subscribe to aircraft heading/yaw. Separate synchronized raw
telemetry fields keep these cards independent of the processed platform drawing
and motion calculations. No PLC feedback is needed to display raw samples; cards
show placeholders before their first sample and after simulator disconnect until
new samples arrive. Full live angle-sweep/HMI and hardware validation is still required. The scaling and
neutral-return requirements above are intended behavior, not a validated motion
implementation. The current ScaleAndLimitAngle helper in MotionCalculator divides inputs by two
before clamping, including rudder/yaw; it has no 80-degree neutral-return behavior.
The repeated "Attitude limited" warning was removed because it treated normal
halving as a limit violation. Scaling and clamping remain unchanged; the raw/target
cards still show the resulting angles.

The owner requested a student-readable cleanup of the entire simulator-to-controller
connection: clear responsibilities, useful comments, fewer redundant helpers, and
separate files where appropriate. Motion/protocol behavior must remain unchanged
unless separately agreed. README contains a student reading guide and thread map.

## Working with the owner

- Read this guide and `README.md` before making changes; inspect the relevant
  source for implementation details.
- Ask before assuming missing goals, requirements, hardware specifications,
  units, or intended behavior. Research facts available in the repository first.
- Keep established behavior unless the requested task calls for changing it.
- Update this guide when the owner confirms a goal or a change alters the
  architecture, protocol, build workflow, or important constraints. Distinguish
  intended behavior from what has actually been implemented and validated.

## Architecture and file map

```text
MSFS -> SimConnect -> attitude conversion -> Stewart geometry -> latest payload
                                                                  |
PLC / Unity <- newline-delimited JSON <- TCP output worker (20 Hz) -+
PLC / Unity -> position feedback -> TCP receive worker -> motion calculation
All components -> synchronized DashboardModel -> Win32/GDI HMI
```

- `main.cpp`: HMI startup, Windows message loop, SimConnect dispatch/retry,
  output and feedback threads, and shutdown.
- `BuildMode.h`: compile-time target selection, bind addresses, ports, and PLC
  number formatting. Exactly one of `TARGET_PLC` and `TARGET_UNITY` must be set.
- `SimConnectHandler.h/.cpp`: owns the SimConnect handle, subscriptions, dispatch,
  calculation scheduling, dashboard updates, and synchronized latest-payload cache.
- `BridgeTypes.h`: common actuator count/array, platform attitude, and command types.
- `MotionCalculator.h/.cpp`: pure attitude mapping and motion calculations, physical
  mounting coordinates, calibration/limits, and shared 50 ms control interval.
- `ControllerProtocol.h/.cpp`: PLC/Unity serialization, feedback validation, and
  incremental message framing; independent of Winsock and SimConnect.
- `calculate_legs.h/.cpp`: vector operations and Euler rotation geometry. Keep
  this independent of Windows, networking, and SimConnect.
- `TCPServer.h/.cpp`: Winsock listener, client replacement after disconnect,
  complete sends, delegation to the protocol parser, and synchronized position feedback.
- `DashboardModel.h/.cpp`: thread-safe telemetry snapshots and recent events;
  independent of Windows and SimConnect.
- `HmiWindow.h/.cpp`: native Win32/GDI dashboard, refreshed every 50 ms, with an
  English Unicode window title and a mouse-controlled orbit view (drag to rotate,
  wheel to zoom, double-click to reset). The illustrative platform has individual
  base mounting points rather than a filled base plate; camera controls affect
  only the visualization.
- `ProtocolLogger.h/.cpp`: synchronized protocol logging to
  `logs/FlightSim_StewartServer_Log.txt`, relative to the working directory.
- `tests/`: standalone geometry, dashboard-model, motion-calculator, and protocol
  test executables. Protocol tests require nlohmann/json; the others do not.

The HMI opens before external systems connect. SimConnect connection attempts
repeat every five seconds when unavailable. The feedback worker accepts a new
client after a disconnect. Closing the HMI initiates worker/socket/SimConnect
cleanup; a simulator quit event also ends the application.

## Current behavior and contracts

- The selected target is PLC, listening on `0.0.0.0:32760`. Unity mode uses
  `0.0.0.0:4844`. These are build-time settings, not simultaneous endpoints.
- There are exactly six actuators. Preserve actuator order, JSON keys, framing,
  and target-specific types unless a coordinated protocol change is requested.
- PLC commands contain `positions` and `speeds`, each with six values. With
  `PLC_VALUES_ARE_STRINGS = true`, values are rounded, clamped to 0..999, and
  formatted as three-digit strings. Numeric mode emits rounded numbers.
- Unity commands contain numeric `positions`, `legs` (the same position values),
  `speeds`, and an `orientation` object with `yaw`, `roll`, and `pitch` in degrees.
- Outgoing objects end with a newline. Feedback uses six numeric values, e.g.
  `{"currentPositions":[200,200,200,200,200,200]}` followed by a newline.
  A complete single JSON object without a newline is also accepted for legacy
  clients. Invalid JSON, wrong counts, and nonnumeric values leave positions
  unchanged. TCP reads may contain partial or multiple messages.
- Calculations run at most every 50 ms. A separate output worker sends the latest
  payload on a nominal 50 ms schedule. This is not a hard real-time guarantee.
- Output requires a connected client, valid position feedback for that connection,
  and an available payload. Disconnect resets the feedback gate.
- Pitch is negated and converted from radians; bank is converted from radians;
  yaw comes from negated rudder deflection, not aircraft heading. Each attitude
  axis is clamped to +/-30 degrees. Geometry receives yaw, negated roll, pitch.
- Current constants include neutral position 200, base leg length
  1156.372420286821, start height 1079, maximum step rate 400 per second
  (20 per calculation), and speed bounds 2..500. Confirm physical units and
  hardware limits with the owner before changing or interpreting these values.

Do not describe the feedback gate as a freshness watchdog: no feedback-age timeout
currently disables output. The cached payload is not cleared on simulator loss or
client reconnect. Existing unit tests do not establish safe physical operation.
These are implementation limitations, not an agreed feature backlog.

## Build and validation

Use C++14 and Visual Studio 2022 (v143) on Windows, normally x64.
`StuartServer.sln` contains `StuartClient.vcxproj`; despite the filename, the
project is named StuartServer and builds this bridge.

Full application builds require the MSFS SimConnect SDK, nlohmann/json headers,
Windows SDK, Winsock, and Win32/GDI libraries. Debug x64 currently has
developer-specific include/library paths; other configurations do not have the
same dependency setup. Do not commit new machine-specific absolute paths.

From a configured Visual Studio developer shell:

```powershell
msbuild StuartServer.sln /p:Configuration=Debug /p:Platform=x64
msbuild StuartServer.sln /p:Configuration=Release /p:Platform=x64
```

`CMakeLists.txt` is auxiliary. It lists application sources and Windows libraries,
and discovers nlohmann/json via find_path or NLOHMANN_JSON_INCLUDE_DIR,
but does not configure SimConnect discovery or linking. Successful CMake
configuration alone does not establish that the application builds.
`.gitmodules` declares googletest and nlohmann/json; neither is checked out in
the inspected tree. Existing tests use standalone executables, not googletest.

Portable tests can be built without SimConnect. Use Debug because the dashboard
test relies on assertions, which Release builds may disable:

```powershell
cmake -S . -B build -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build --config Debug --target calculate_legs_tests dashboard_model_tests motion_calculator_tests
ctest --test-dir build -C Debug --output-on-failure -E controller_protocol_tests
```

`CMAKE_BUILD_TYPE` applies to single-configuration generators; `--config` and
`-C` select Debug with Visual Studio's multi-configuration generator.

- Geometry tests check neutral symmetry and finite positive lengths at selected
  boundary poses. Dashboard tests check telemetry updates and disconnect state.
  Motion tests preserve current mapping, step/speed limits, and actuator order.
  The current level-pose calibration rounds to 201, despite reference position 200.
- When nlohmann/json is found, build controller_protocol_tests before CTest. It
  checks both PLC formats, Unity, malformed feedback, fragmentation, combined
  messages, legacy framing, and stream reset; it does not require Windows or MSFS.
  Then run CTest without the -E exclusion to execute all four suites.
- For relevant runtime changes, validate startup without MSFS/client, reconnects,
  orderly shutdown, feedback gating, fragmented/combined messages, and nominal
  20 Hz output. Exercise both target modes when changing shared protocol code.
- Report which checks ran and any missing toolchain, SDK, client, or hardware.
  Do not claim runtime or hardware validation from unit tests alone.

Cleanup validation in September 2026: Debug/Release x64 builds, all four test
suites, and 5,324 original/refactored motion comparisons passed. Temporary PLC
and Unity builds bound to loopback received live MSFS telemetry and passed
feedback rejection/framing, reconnect gating, and HMI-close shutdown checks.
Short output samples measured 20.1 Hz (PLC) and 20.0 Hz (Unity). Physical hardware,
startup without MSFS, and simulator loss/reconnect were not validated in that run.

## Change conventions

- Follow surrounding C++ style and keep changes focused on the requested task.
- Preserve synchronization around sockets, positions, payloads, and dashboard
  snapshots. Keep blocking network work out of the HMI/message loop.
- Use macro-safe `(std::min)(...)` and `(std::max)(...)` in Windows-facing code;
  use the same protection for `min()`/`max()` members where needed.
- When adding/removing application sources, update `CMakeLists.txt`,
  `StuartClient.vcxproj`, and `StuartClient.vcxproj.filters` together.
- Keep build outputs, logs, and local SDK configuration out of source changes.
- Keep protocol and runtime documentation in `README.md` consistent with changes.
