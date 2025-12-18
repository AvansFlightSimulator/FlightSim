// BuildMode.h
#pragma once

// ==== KIES PRECIES ��N DOEL ====
 //#define TARGET_UNITY
 #define TARGET_PLC

#if defined(TARGET_PLC) && defined(TARGET_UNITY)
#error "Define only ONE of TARGET_PLC or TARGET_UNITY"
#endif
#if !defined(TARGET_PLC) && !defined(TARGET_UNITY)
#error "Define TARGET_PLC or TARGET_UNITY"
#endif

// Poorten en bind-IP per doel
static constexpr int   PORT_UNITY = 4844;
static constexpr int   PORT_PLC = 32760;

static constexpr const char* BIND_IP_UNITY = "0.0.0.0"; // alle interfaces (incl. loopback)
static constexpr const char* BIND_IP_PLC = "0.0.0.0"; // alle interfaces

// Sommige PLC's verwachten 3-cijferige strings ("005") i.p.v. ints.
// Zet op false als jouw PLC echte getallen wil.
static constexpr bool PLC_VALUES_ARE_STRINGS = true;
