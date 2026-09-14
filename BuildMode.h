// BuildMode.h
#pragma once

// Select exactly one output target.
// #define TARGET_UNITY
#define TARGET_PLC

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

#ifdef TARGET_PLC
static constexpr const char* ACTIVE_TARGET_NAME = "PLC";
static constexpr const char* ACTIVE_BIND_IP = BIND_IP_PLC;
static constexpr int ACTIVE_PORT = PORT_PLC;
#else
static constexpr const char* ACTIVE_TARGET_NAME = "Unity";
static constexpr const char* ACTIVE_BIND_IP = BIND_IP_UNITY;
static constexpr int ACTIVE_PORT = PORT_UNITY;
#endif
