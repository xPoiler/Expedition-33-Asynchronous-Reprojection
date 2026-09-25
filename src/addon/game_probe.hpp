#pragma once
#include "shared/protocol.hpp"

namespace fw {
// Expedition 33 (current Steam build) probes into UE's Streamline plugin. Every target is checked
// against its expected bytes first, so a different game build is simply left alone.
struct ProbeInfo { const char* name; std::uint32_t rva; };
extern const ProbeInfo kProbes[];
extern const int kProbeCount;
void install_game_probes(HookStats* stats);
}
