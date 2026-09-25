#pragma once
// Presenter-side D3D12: own device + high-priority queue on the game's adapter, a DirectComposition
// swapchain on the overlay window, conversion of shared captures into typed private textures, and
// the final blit. Latewarp evaluation is recorded between ingest() and present().
#include "presenter/latewarp12.hpp"
#include "shared/protocol.hpp"
#include <d3d12.h>
#include <dcomp.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>
#include <vector>

namespace fw {
using Microsoft::WRL::ComPtr;

struct IngestedSource {
    bool valid = false;
    bool has_hudless = false, has_ui = false, has_depth = false;
    std::uint32_t color_w = 0, color_h = 0;
    Rect2 color_rect, depth_rect;
};

class Renderer {
public:
    ~Renderer();
    bool init(const LUID& adapter, HWND window, std::uint32_t width, std::uint32_t height, DXGI_FORMAT game_format,
              std::uint32_t color_space, std::string& error);
    ID3D12Device* device() const { return device_.Get(); }
    HANDLE waitable() const { return waitable_; }
    const char* queue_priority() const { return priority_name_; }
    bool resize(std::uint32_t width, std::uint32_t height);
    // 1: a frame can only start once the previous one is displayed; 2: one frame may wait in the queue.
    void set_frame_latency(UINT frames) { if (frames != frame_latency_) { swapchain_->SetMaximumFrameLatency(frames); frame_latency_ = frames; } }
    std::uint32_t width() const { return width_; }
    std::uint32_t height() const { return height_; }

    // Shared objects of one producer session.
    bool open_session(DWORD pid, std::uint64_t session, std::string& error);
    bool session_open(std::uint64_t session) const { return fence_game_ && session_ == session; }
    // Last game fence value the GPU has finished: only frames at or below it are taken, so the
    // presenter never queues behind the game's in-flight GPU work.
    std::uint64_t game_fence_completed() const { return fence_game_ ? fence_game_->GetCompletedValue() : 0; }

    // Frame recording.
    ID3D12GraphicsCommandList* begin_frame();
    // Records conversion of slot `slot` into private textures; the queue waits for the game's fence.
    IngestedSource ingest(const Shared& shared, int slot);
    // Latewarp inputs referring to the private textures of the last ingest.
    LatewarpInputs latewarp_inputs(const IngestedSource& src, bool use_ui_tags);
    // Blits the warped output (or the unwarped private backbuffer) to the swapchain and presents.
    // marker: 0 none, 1 green (warped), 2 red (original).
    void finish_frame(bool warped, int marker);
    // Signalled value that completes all work recorded so far.
    std::uint64_t submitted_value() const { return fence_value_; }
    bool completed(std::uint64_t value) const { return fence_->GetCompletedValue() >= value; }
    HRESULT device_removed_reason() const { return device_ ? device_->GetDeviceRemovedReason() : S_OK; }
    // Notes about shared buffers opened during ingest (size/format changes), drained for logging.
    std::vector<std::string> take_notes() { std::vector<std::string> n; n.swap(notes_); return n; }
    void wait_idle();
    float last_gpu_ms() const { return gpu_ms_; }
    // Timing of the most recently completed presenter frame, all in CPU QPC ticks.
    struct FrameTiming { std::int64_t submit = 0, gpu_start = 0, gpu_end = 0; bool valid = false; };
    FrameTiming last_timing() const { return timing_; }
    // DXGI frame statistics after the latest present (0 when unavailable).
    struct PresentStats { UINT last_present_count = 0, present_count = 0, present_refresh = 0, sync_refresh = 0; std::int64_t sync_qpc = 0; HRESULT hr = S_OK; };
    PresentStats present_stats() const { return present_stats_; }
    // Test/diagnostic helper: synchronously reads back the warped output (RGBA16F) or private backbuffer.
    bool read_back(bool output, std::vector<std::uint16_t>& pixels, std::uint32_t& w, std::uint32_t& h);

private:
    struct Private {
        ComPtr<ID3D12Resource> texture;
        std::uint32_t width = 0, height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    };
    enum PrivateId { kPBackbuffer, kPHudless, kPUi, kPDepth, kPMotion, kPZeroUi, kPOutput, kPCount };

    bool create_pipelines(std::string& error);
    bool ensure_private(PrivateId id, std::uint32_t w, std::uint32_t h, DXGI_FORMAT format);
    void transition(Private& p, D3D12_RESOURCE_STATES to);
    ID3D12Resource* shared_texture(int slot, int kind, std::uint32_t generation);
    D3D12_CPU_DESCRIPTOR_HANDLE cpu(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE gpu(UINT index) const;
    void convert(ID3D12Resource* source, DXGI_FORMAT source_format, int slot, int kind, PrivateId target, std::uint32_t w, std::uint32_t h);
    void create_swapchain_views();

    ComPtr<ID3D12Device> device_;
    ComPtr<ID3D12CommandQueue> queue_;
    ComPtr<ID3D12CommandAllocator> allocators_[3];
    ComPtr<ID3D12GraphicsCommandList> list_;
    ComPtr<ID3D12Fence> fence_;
    HANDLE fence_event_ = nullptr;
    std::uint64_t fence_value_ = 0, frame_values_[3] = {};
    ComPtr<IDXGIFactory4> factory_;
    ComPtr<IDXGISwapChain3> swapchain_;
    ComPtr<IDCompositionDevice> dcomp_;
    ComPtr<IDCompositionTarget> target_;
    ComPtr<IDCompositionVisual> visual_;
    HANDLE waitable_ = nullptr;
    DXGI_FORMAT swap_format_ = DXGI_FORMAT_R8G8B8A8_UNORM;
    std::uint32_t width_ = 0, height_ = 0, frame_index_ = 0;
    const char* priority_name_ = "normal";

    ComPtr<ID3D12RootSignature> root_;
    ComPtr<ID3D12PipelineState> cs_color_, cs_depth_, blit_;
    ComPtr<ID3D12DescriptorHeap> heap_, rtv_heap_;
    UINT descriptor_size_ = 0, rtv_size_ = 0;
    ComPtr<ID3D12QueryHeap> timestamps_;
    ComPtr<ID3D12Resource> readback_;
    std::uint64_t timestamp_frequency_ = 1;
    std::uint64_t calib_gpu_ = 0, calib_cpu_ = 0;  // GetClockCalibration pair
    std::int64_t submit_qpc_[3] = {};
    FrameTiming timing_;
    std::uint64_t present_counter_ = 0;
    std::vector<std::string> notes_;
    UINT frame_latency_ = 1;
    PresentStats present_stats_;
    float gpu_ms_ = 0;

    Private private_[kPCount];
    DWORD pid_ = 0;
    std::uint64_t session_ = 0;
    ComPtr<ID3D12Fence> fence_game_;
    std::uint64_t pending_game_wait_ = 0;
    struct SharedTex { ComPtr<ID3D12Resource> resource; std::uint32_t generation = 0; };
    SharedTex shared_[kSlots][kTexCount];
    std::vector<ID3D12Resource*> reading_;  // shared textures transitioned for this frame
    ID3D12Resource* srv_resource_[32] = {};  // resource currently described at each source SRV index
};

}  // namespace fw
