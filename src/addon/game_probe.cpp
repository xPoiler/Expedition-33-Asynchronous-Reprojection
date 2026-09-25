#include "addon/game_probe.hpp"
#include "common/inline_hook.hpp"
#include <array>
#include <cstring>
#include <utility>

namespace fw {
namespace {

struct Target {
    const char* name;
    std::uint32_t rva;
    std::uint8_t expect[8];
    bool force_true;  // detour returns true instead of calling the original
};

// Found by disassembling SandFall-Win64-Shipping.exe (current Steam build); verified byte-for-byte before patching.
const Target kTargets[] = {
    {"ForceTagStreamlineBuffers", 0x495D1F0, {0x48, 0x83, 0xEC, 0x28, 0x8B, 0x0D, 0xDE, 0x0B}, true},
    {"pass 495E6F0", 0x495E6F0, {0x40, 0x55, 0x57, 0x41, 0x56, 0x48, 0x81, 0xEC}, false},
    {"pass 495EA70", 0x495EA70, {0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x57}, false},
    {"pass 4960070", 0x4960070, {0x40, 0x55, 0x53, 0x56, 0x41, 0x57, 0x48, 0x8D}, false},
    {"pass 4960F60", 0x4960F60, {0x40, 0x53, 0x55, 0x56, 0x48, 0x81, 0xEC, 0x00}, false},
    {"pass 4961E90", 0x4961E90, {0x40, 0x55, 0x57, 0x41, 0x56, 0x41, 0x57, 0x48}, false},
    {"gate 4961260", 0x4961260, {0x48, 0x83, 0xEC, 0x28, 0x80, 0x3D, 0x65, 0x06}, false},
};
constexpr int kCount = static_cast<int>(sizeof(kTargets) / sizeof(kTargets[0]));

HookStats* g_stats = nullptr;
InlineHook g_hooks[kCount];

using Fn = std::uint64_t (*)(std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t, std::uint64_t);
template <int I>
std::uint64_t detour(std::uint64_t a, std::uint64_t b, std::uint64_t c, std::uint64_t d, std::uint64_t e, std::uint64_t f) {
    if (g_stats) ++g_stats->probe_calls[I];
    if (kTargets[I].force_true) return 1;
    return reinterpret_cast<Fn>(g_hooks[I].original())(a, b, c, d, e, f);
}
template <int... I>
std::array<void*, sizeof...(I)> detours(std::integer_sequence<int, I...>) { return {reinterpret_cast<void*>(&detour<I>)...}; }

}  // namespace

const ProbeInfo kProbes[] = {
    {kTargets[0].name, kTargets[0].rva}, {kTargets[1].name, kTargets[1].rva}, {kTargets[2].name, kTargets[2].rva},
    {kTargets[3].name, kTargets[3].rva}, {kTargets[4].name, kTargets[4].rva}, {kTargets[5].name, kTargets[5].rva},
    {kTargets[6].name, kTargets[6].rva},
};
const int kProbeCount = kCount;

void install_game_probes(HookStats* stats) {
    g_stats = stats;
    auto* base = reinterpret_cast<std::uint8_t*>(GetModuleHandleW(nullptr));
    wchar_t exe[MAX_PATH]{};
    GetModuleFileNameW(nullptr, exe, MAX_PATH);
    if (!wcsstr(exe, L"SandFall-Win64-Shipping.exe")) return;
    static const auto table = detours(std::make_integer_sequence<int, kCount>{});
    for (int i = 0; i < kCount; ++i) {
        if (g_hooks[i].installed()) continue;
        std::uint8_t* target = base + kTargets[i].rva;
        if (std::memcmp(target, kTargets[i].expect, sizeof(kTargets[i].expect)) != 0) continue;  // different build
        if (g_hooks[i].install(target, table[i]) && stats) stats->probe_installed |= 1u << i;
    }
}

}  // namespace fw
