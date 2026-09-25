#include "presenter/latewarp12.hpp"
#include "nvsdk_ngx.h"
#include <dxgi1_4.h>
#include <sstream>
#include <wrl/client.h>

namespace fw {
namespace {
constexpr unsigned long long kAppId = 231313132;  // same local id as the verified D3D11 probe
}

Latewarp12::~Latewarp12() { shutdown(); }

bool Latewarp12::check(int result, const char* what) {
    if (NVSDK_NGX_SUCCEED(static_cast<NVSDK_NGX_Result>(result))) return true;
    std::ostringstream s; s << what << " failed: 0x" << std::hex << static_cast<unsigned>(result);
    status_ = s.str();
    return false;
}

bool Latewarp12::initialize(ID3D12Device* device, const std::filesystem::path& feature_dir, const std::filesystem::path& log_dir) {
    if (initialized_) return supported_;
    device_ = device;
    if (!std::filesystem::is_regular_file(feature_dir / L"nvngx_latewarp.dll")) { status_ = "nvngx_latewarp.dll missing next to the presenter"; return false; }
    const auto folder = std::filesystem::absolute(feature_dir);
    const auto logs = std::filesystem::absolute(log_dir);
    std::filesystem::create_directories(logs);
    const wchar_t* paths[] = {folder.c_str()};
    NVSDK_NGX_FeatureCommonInfo common{};
    common.PathListInfo.Path = paths; common.PathListInfo.Length = 1;
    if (!check(NVSDK_NGX_D3D12_Init(kAppId, logs.c_str(), device, &common, NVSDK_NGX_Version_API), "NGX init")) return false;
    initialized_ = true;
    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) || FAILED(factory->EnumAdapterByLuid(device->GetAdapterLuid(), IID_PPV_ARGS(&adapter)))) {
        status_ = "adapter lookup failed"; return false;
    }
    NVSDK_NGX_FeatureDiscoveryInfo info{};
    info.SDKVersion = NVSDK_NGX_Version_API;
    info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Application_Id;
    info.Identifier.v.ApplicationId = kAppId;
    info.FeatureID = NVSDK_NGX_Feature_Reserved15;
    info.ApplicationDataPath = logs.c_str();
    info.FeatureInfo = &common;
    NVSDK_NGX_FeatureRequirement requirement{};
    if (!check(NVSDK_NGX_D3D12_GetFeatureRequirements(adapter.Get(), &info, &requirement), "feature requirements")) return false;
    if (requirement.FeatureSupported != 0) { status_ = "Latewarp unsupported on this GPU/driver"; return false; }
    if (!check(NVSDK_NGX_D3D12_AllocateParameters(&params_), "allocate parameters")) return false;
    supported_ = true;
    status_ = "ready";
    return true;
}

void Latewarp12::release_feature() {
    if (feature_) { NVSDK_NGX_D3D12_ReleaseFeature(feature_); feature_ = nullptr; }
    width_ = height_ = 0;
}

void Latewarp12::shutdown() {
    release_feature();
    if (params_) { NVSDK_NGX_D3D12_DestroyParameters(params_); params_ = nullptr; }
    if (initialized_) { NVSDK_NGX_D3D12_Shutdown1(device_); initialized_ = false; }
    supported_ = false;
}

bool Latewarp12::evaluate(ID3D12GraphicsCommandList* list, const LatewarpInputs& in, bool rendered, const Mat4& target_view,
                          const Mat4& source_view, const Mat4& projection) {
    if (!supported_ || !in.backbuffer || !in.hudless || !in.ui || !in.depth || !in.motion || !in.output) {
        status_ = "missing inputs"; return false;
    }
    const auto out = in.output->GetDesc();
    auto* p = params_;
    if (!feature_ || width_ != out.Width || height_ != out.Height) {
        release_feature();
        p->Set("Latewarp.Output.Width", static_cast<unsigned int>(out.Width));
        p->Set("Latewarp.Output.Height", static_cast<unsigned int>(out.Height));
        if (!check(NVSDK_NGX_D3D12_CreateFeature(list, NVSDK_NGX_Feature_Reserved15, p, &feature_), "create feature")) return false;
        width_ = static_cast<std::uint32_t>(out.Width); height_ = out.Height;
        rendered = true;
    }
    p->Set("Latewarp.Backbuffer", in.backbuffer);
    p->Set("Latewarp.HudlessColor", in.hudless);
    p->Set("Latewarp.UIColorAlpha", in.ui);
    p->Set("Depth", in.depth);
    p->Set("MotionVectors", in.motion);
    p->Set("Output", in.output);
    p->Set("Latewarp.NoWarpMask", static_cast<ID3D12Resource*>(nullptr));
    auto subrect = [&](const char* key, const Rect2& r) {
        const std::string k = key;
        p->Set((k + ".Subrect.Base.X").c_str(), r.x); p->Set((k + ".Subrect.Base.Y").c_str(), r.y);
        p->Set((k + ".Subrect.Width").c_str(), r.w); p->Set((k + ".Subrect.Height").c_str(), r.h);
    };
    subrect("Latewarp.Backbuffer", in.color_rect);
    subrect("Latewarp.HudlessColor", in.color_rect);
    subrect("Latewarp.UIColorAlpha", in.color_rect);
    subrect("Latewarp.Output", in.color_rect);
    subrect("Latewarp.Depth", in.depth_rect);
    subrect("Latewarp.MV", in.depth_rect);
    target_ = target_view; source_ = source_view; projection_ = projection;
    p->Set("Latewarp.WorldToViewMatrix", static_cast<void*>(target_.data()));
    p->Set("Latewarp.ViewToClipMatrix", static_cast<void*>(projection_.data()));
    p->Set("Latewarp.PrevRenderedWorldToViewMatrix", static_cast<void*>(source_.data()));
    p->Set("Latewarp.PrevRenderedViewToClipMatrix", static_cast<void*>(projection_.data()));
    p->Set("Latewarp.DepthInverted", in.depth_inverted ? 1u : 0u);
    p->Set("Latewarp.EvalFlags", 0u);
    p->Set("Latewarp.UsePremultiplyUIAlpha", 0u);
    // Latewarp ignores the target camera on an IsRenderedFrame=1 evaluation and outputs the frame
    // unwarped (verified in tests/pipeline_tests.cpp). Register a new frame with a throwaway
    // evaluation, then warp with a normal one, so every presented image uses the requested camera.
    if (rendered) {
        p->Set("Latewarp.IsRenderedFrame", 1u);
        p->Set("Latewarp.FrameID", ++frame_id_);
        if (!check(NVSDK_NGX_D3D12_EvaluateFeature(list, feature_, p, nullptr), "evaluate (register frame)")) return false;
        D3D12_RESOURCE_BARRIER uav{};
        uav.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        uav.UAV.pResource = in.output;
        list->ResourceBarrier(1, &uav);
    }
    p->Set("Latewarp.IsRenderedFrame", 0u);
    p->Set("Latewarp.FrameID", ++frame_id_);
    if (!check(NVSDK_NGX_D3D12_EvaluateFeature(list, feature_, p, nullptr), "evaluate")) return false;
    status_ = "ok";
    return true;
}

}  // namespace fw
