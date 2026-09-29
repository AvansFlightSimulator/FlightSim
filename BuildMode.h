// BuildMode.h
#pragma once

// Select exactly one output target.
// Define TARGET_UNITY in the compiler options for the supporting Unity build.
#if !defined(TARGET_UNITY) && !defined(TARGET_PLC)
#define TARGET_PLC
#endif

#if defined(TARGET_PLC) && defined(TARGET_UNITY)
#error "Define only ONE of TARGET_PLC or TARGET_UNITY"
#endif
#if !defined(TARGET_PLC) && !defined(TARGET_UNITY)
#error "Define TARGET_PLC or TARGET_UNITY"
#endif

// Listening port and bind address for each target.
static constexpr int   PORT_UNITY = 4844;
static constexpr int   PORT_PLC = 32760;

static constexpr const char* BIND_IP_UNITY = "0.0.0.0"; // All interfaces, including loopback.
static constexpr const char* BIND_IP_PLC = "0.0.0.0"; // All interfaces.

// Some PLCs expect three-digit strings ("005") instead of JSON numbers.
// Set this to false when the PLC accepts numeric values.
static constexpr bool PLC_VALUES_ARE_STRINGS = true;

// PLC mode must match CSP_Config.ControlMode in the CODESYS project.
// PP remains the commissioning default. CSP sends final fractional positions;
// the PLC, not this application, creates the velocity/acceleration trajectory.
enum PlatformControlMode { CONTROL_MODE_PP = 1, CONTROL_MODE_CSP = 8 };
#ifndef FLIGHTSIM_PLC_CONTROL_MODE
#define FLIGHTSIM_PLC_CONTROL_MODE 8 // 1 = PP, 8 = CSP
#endif
static_assert(FLIGHTSIM_PLC_CONTROL_MODE == 1 || FLIGHTSIM_PLC_CONTROL_MODE == 8,
    "PLC control mode must be 1 (PP) or 8 (CSP)");
static constexpr PlatformControlMode PLC_CONTROL_MODE =
    static_cast<PlatformControlMode>(FLIGHTSIM_PLC_CONTROL_MODE);
static constexpr int MSFS_FILTER_TIME_CONSTANT_MS = 120;
#ifdef TARGET_PLC
static constexpr bool USE_PLC_CSP = PLC_CONTROL_MODE == CONTROL_MODE_CSP;
#else
static constexpr bool USE_PLC_CSP = false;
#endif

#ifdef TARGET_PLC
static constexpr const char* ACTIVE_TARGET_NAME = "PLC";
static constexpr const char* ACTIVE_BIND_IP = BIND_IP_PLC;
static constexpr int ACTIVE_PORT = PORT_PLC;
#else
static constexpr const char* ACTIVE_TARGET_NAME = "Unity";
static constexpr const char* ACTIVE_BIND_IP = BIND_IP_UNITY;
static constexpr int ACTIVE_PORT = PORT_UNITY;
#endif
