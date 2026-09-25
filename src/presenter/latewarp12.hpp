#pragma once
// NVIDIA Latewarp (Reflex 2 Frame Warp, NGX feature Reserved15) on D3D12.
// Matrices are row-major, row-vector, world-to-view / view-to-clip (UE/DirectXMath convention).
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

struct NVSDK_NGX_Parameter;
struct NVSDK_NGX_Handle;

namespace fw {

struct Rect2 { std::uint32_t x = 0, y = 0, w = 0, h = 0; };

struct LatewarpInputs {
    // All inputs in D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, output in UNORDERED_ACCESS.
    ID3D12Resource* backbuffer = nullptr;  // final frame incl. HUD
    ID3D12Resource* hudless = nullptr;     // scene without HUD (may equal backbuffer)
    ID3D12Resource* ui = nullptr;          // UI color + alpha (zero when unavailable)
    ID3D12Resource* depth = nullptr;       // R32_FLOAT
    ID3D12Resource* motion = nullptr;      // R16G16_FLOAT (zeros)
    ID3D12Resource* output = nullptr;
    Rect2 color_rect, depth_rect;
    bool depth_inverted = true;
};

class Latewarp12 {
public:
    using Mat4 = std::array<float, 16>;
    ~Latewarp12();
    bool initialize(ID3D12Device* device, const std::filesystem::path& feature_dir, const std::filesystem::path& log_dir);
    // Records into `list`. `rendered` marks the first evaluation of a new source frame.
    bool evaluate(ID3D12GraphicsCommandList* list, const LatewarpInputs& in, bool rendered, const Mat4& target_view,
                  const Mat4& source_view, const Mat4& projection);
    // Releases the feature; the caller must ensure the GPU is idle.
    void release_feature();
    void shutdown();
    const std::string& status() const { return status_; }
    bool ready() const { return supported_; }

private:
    bool check(int result, const char* what);
    ID3D12Device* device_ = nullptr;
    NVSDK_NGX_Parameter* params_ = nullptr;
    NVSDK_NGX_Handle* feature_ = nullptr;
    std::uint32_t width_ = 0, height_ = 0, frame_id_ = 0;
    bool initialized_ = false, supported_ = false;
    Mat4 target_{}, source_{}, projection_{};
    std::string status_ = "not initialized";
};

}  // namespace fw
