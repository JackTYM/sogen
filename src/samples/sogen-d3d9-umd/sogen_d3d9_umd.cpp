// sogen thin Direct3D9 WDDM user-mode driver (the vendor-driver slot the official d3d9.dll loads).
//
// Spike-B scope: prove the official Microsoft d3d9.dll loads this DLL via KMTQAITYPE_UMDRIVERNAME,
// calls OpenAdapter, negotiates caps, and reaches CreateDevice. No host GPU work happens here yet —
// OpenAdapter/GetCaps/CreateDevice are pure negotiation, so we only synthesize caps and log via
// OutputDebugStringA (which the sogen analyzer captures). The real D3D9-DDI marshalling over D3DKMT
// is added once the gate passes.

#include "d3d9_ddi.hpp"

#include <d3d9.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace
{
    void log_line(const char* fmt, ...)
    {
        char buf[512];
        va_list ap;
        va_start(ap, fmt);
        vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        OutputDebugStringA(buf);
    }

    // Generic device-function stub. On x64 the calling convention is caller-cleanup, so one stub can
    // back every slot of D3DDDI_DEVICEFUNCS regardless of the real arity — sufficient for the gate
    // (the runtime does not drive rendering during CreateDevice). The x86 port needs typed thunks.
    HRESULT APIENTRY device_stub()
    {
        return S_OK;
    }

    void fill_d3d9caps(D3DCAPS9* caps)
    {
        std::memset(caps, 0, sizeof(*caps));
        caps->DeviceType = D3DDEVTYPE_HAL;
        caps->AdapterOrdinal = 0;
        // d3d9's aggregate HAL validator rejects the adapter as non-HAL if TextureCaps or FVFCaps is 0
        // (before it ever looks at formats/shaders), so FVFCaps must be nonzero.
        caps->FVFCaps = D3DFVFCAPS_PSIZE | 8; // 0x00100008: PSIZE + 8 texcoord sets
        // Report a fixed-function device (no VS/PS) to sidestep the VS2.0+ HAL-disable caps gauntlet.
        // Restoring D3DVS/PS_VERSION(3,0) here re-triggers that gauntlet elsewhere (confirmed: GetDeviceCaps
        // itself starts failing) and needs its own follow-up investigation before SM3.0 can be reported.
        caps->VertexShaderVersion = 0;
        caps->PixelShaderVersion = 0;
        caps->MaxVertexShaderConst = 256;
        caps->PixelShader1xMaxValue = 8.0f;
        caps->DeclTypes = D3DDTCAPS_UBYTE4 | D3DDTCAPS_UBYTE4N | D3DDTCAPS_SHORT2N | D3DDTCAPS_SHORT4N |
                          D3DDTCAPS_USHORT2N | D3DDTCAPS_USHORT4N | D3DDTCAPS_UDEC3 | D3DDTCAPS_DEC3N |
                          D3DDTCAPS_FLOAT16_2 | D3DDTCAPS_FLOAT16_4;
        caps->Caps2 = D3DCAPS2_DYNAMICTEXTURES | D3DCAPS2_FULLSCREENGAMMA | D3DCAPS2_CANAUTOGENMIPMAP;
        caps->Caps3 = D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD | D3DCAPS3_COPY_TO_VIDMEM | D3DCAPS3_COPY_TO_SYSTEMMEM;
        // d3d9's HAL-enable path (sub_1004B19B branch-3, ddcreate.cpp:860) stamps the driver disabled
        // and skips format-table population when (DevCaps2 & 1) == 0, making GetDeviceCaps(HAL) return
        // D3DERR_NOTAVAILABLE. STREAMOFFSET (bit0) satisfies the gate; d3d9 then re-derives DevCaps2.
        caps->DevCaps2 = D3DDEVCAPS2_STREAMOFFSET;
        caps->PresentationIntervals = D3DPRESENT_INTERVAL_IMMEDIATE | D3DPRESENT_INTERVAL_ONE;
        caps->DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_PUREDEVICE |
                        D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_TEXTUREVIDEOMEMORY;
        caps->PrimitiveMiscCaps = D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW |
                                  D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_BLENDOP | D3DPMISCCAPS_SEPARATEALPHABLEND;
        caps->RasterCaps = D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_SCISSORTEST | D3DPRASTERCAPS_DEPTHBIAS |
                           D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS | D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ANISOTROPY;
        caps->ZCmpCaps = 0xFF;
        caps->SrcBlendCaps = 0x1FFF;
        caps->DestBlendCaps = 0x1FFF;
        caps->AlphaCmpCaps = 0xFF;
        caps->ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_ALPHAGOURAUDBLEND;
        caps->TextureCaps = D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_MIPMAP | D3DPTEXTURECAPS_CUBEMAP |
                            D3DPTEXTURECAPS_VOLUMEMAP | D3DPTEXTURECAPS_MIPCUBEMAP | D3DPTEXTURECAPS_MIPVOLUMEMAP;
        caps->TextureFilterCaps = 0x03070700;
        caps->MaxTextureWidth = 8192;
        caps->MaxTextureHeight = 8192;
        caps->MaxVolumeExtent = 2048;
        caps->MaxTextureRepeat = 8192;
        caps->MaxTextureAspectRatio = 8192;
        caps->MaxAnisotropy = 16;
        caps->MaxVertexIndex = 0x00FFFFFF;
        caps->MaxStreams = 16;
        caps->MaxStreamStride = 255;
        caps->MaxPrimitiveCount = 0x00FFFFFF;
        caps->MaxVertexShaderConst = 256;
        caps->NumSimultaneousRTs = 4;
        caps->MaxSimultaneousTextures = 8;
        caps->MaxTextureBlendStages = 8;
        caps->MaxUserClipPlanes = 6;
        caps->MaxActiveLights = 8;
        caps->VertexProcessingCaps = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7 | D3DVTXPCAPS_DIRECTIONALLIGHTS |
                                     D3DVTXPCAPS_POSITIONALLIGHTS | D3DVTXPCAPS_LOCALVIEWER;
        caps->MaxVertexW = 1e10f;
        caps->MaxPointSize = 256.0f;
        caps->VS20Caps.Caps = D3DVS20CAPS_PREDICATION;
        caps->VS20Caps.DynamicFlowControlDepth = D3DVS20_MAX_DYNAMICFLOWCONTROLDEPTH;
        caps->VS20Caps.NumTemps = 32;
        caps->VS20Caps.StaticFlowControlDepth = D3DVS20_MAX_STATICFLOWCONTROLDEPTH;
        caps->PS20Caps.Caps = D3DPS20CAPS_ARBITRARYSWIZZLE | D3DPS20CAPS_GRADIENTINSTRUCTIONS |
                              D3DPS20CAPS_PREDICATION | D3DPS20CAPS_NODEPENDENTREADLIMIT | D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT;
        caps->PS20Caps.DynamicFlowControlDepth = D3DPS20_MAX_DYNAMICFLOWCONTROLDEPTH;
        caps->PS20Caps.NumTemps = 32;
        caps->PS20Caps.StaticFlowControlDepth = D3DPS20_MAX_STATICFLOWCONTROLDEPTH;
        caps->PS20Caps.NumInstructionSlots = 512;
        caps->VertexTextureFilterCaps = 0x03000300;
        caps->MaxVShaderInstructionsExecuted = 0xFFFFFFFF;
        caps->MaxPShaderInstructionsExecuted = 0xFFFFFFFF;
        // With VS/PS reported as fixed-function (0, below vs_3_0/ps_3_0), d3d9's aggregate HAL validator
        // rejects the adapter unless the SM3.0 instruction-slot caps are 0.
        caps->MaxVertexShader30InstructionSlots = 0;
        caps->MaxPixelShader30InstructionSlots = 0;
    }

    // D3DDDI FORMATOP: what d3d9.dll's GetDeviceCaps scans to decide HAL is available. It requires at least one
    // format carrying D3DFORMAT_OP_3DACCELERATION, and d3d9 disables the driver if any format has 3DACCELERATION
    // without DISPLAYMODE — so 0x800 is only ever set together with 0x400 (on the true display formats).
    struct FORMATOP
    {
        uint32_t Format; // D3DDDIFORMAT (== D3DFORMAT for these)
        uint32_t Operations;
        uint32_t FlipMsTypes;
        uint32_t BltMsTypes;
        uint32_t PrivateFormatBitCount;
    };

    enum : uint32_t
    {
        FMT_OP_TEXTURE = 0x00000001,
        FMT_OP_VOLUMETEXTURE = 0x00000002,
        FMT_OP_CUBETEXTURE = 0x00000004,
        FMT_OP_OFFSCREEN_RENDERTARGET = 0x00000008,
        FMT_OP_SAME_FORMAT_RENDERTARGET = 0x00000010,
        FMT_OP_ZSTENCIL = 0x00000040,
        FMT_OP_DISPLAYMODE = 0x00000400,
        FMT_OP_3DACCELERATION = 0x00000800,
        FMT_OP_CONVERT_TO_ARGB = 0x00002000,
        FMT_OP_OFFSCREENPLAIN = 0x00004000,
    };

    constexpr uint32_t RT_TEX = FMT_OP_OFFSCREEN_RENDERTARGET | FMT_OP_SAME_FORMAT_RENDERTARGET | FMT_OP_TEXTURE;
    constexpr uint32_t DISPLAY_RT = FMT_OP_DISPLAYMODE | FMT_OP_3DACCELERATION | RT_TEX;

    // Minimal set: exactly one 32-bit display+3D-accelerated format (satisfies GetDeviceCaps' 0x800 scan)
    // plus one depth-stencil. d3d9 runs a disable "gauntlet" over the format list, so keep it lean while
    // proving HAL; more formats are added back once the gate passes.
    const FORMATOP g_formats[] = {
        {22 /*X8R8G8B8*/, DISPLAY_RT, 0, 0, 0},
        {75 /*D24S8   */, FMT_OP_ZSTENCIL, 0, 0, 0},
    };

    HRESULT APIENTRY umd_GetCaps(HANDLE hAdapter, CONST D3DDDIARG_GETCAPS* pCaps)
    {
        log_line("[sogen-d3d9-umd] GetCaps Type=%u DataSize=%u pData=%p\n", pCaps->Type, pCaps->DataSize, pCaps->pData);
        switch (pCaps->Type)
        {
        case SOGEN_D3DDDICAPS_GETD3D9CAPS:
            if (pCaps->pData && pCaps->DataSize >= sizeof(D3DCAPS9))
            {
                fill_d3d9caps(static_cast<D3DCAPS9*>(pCaps->pData));
            }
            break;
        case SOGEN_D3DDDICAPS_GETFORMATCOUNT:
            if (pCaps->pData && pCaps->DataSize >= sizeof(UINT))
            {
                *static_cast<UINT*>(pCaps->pData) = static_cast<UINT>(sizeof(g_formats) / sizeof(g_formats[0]));
            }
            break;
        case SOGEN_D3DDDICAPS_GETFORMATDATA:
            if (pCaps->pData && pCaps->DataSize >= sizeof(g_formats))
            {
                std::memcpy(pCaps->pData, g_formats, sizeof(g_formats));
            }
            break;
        default:
            if (pCaps->pData && pCaps->DataSize)
            {
                std::memset(pCaps->pData, 0, pCaps->DataSize);
            }
            break;
        }
        return S_OK;
    }

    HRESULT APIENTRY umd_CreateDevice(HANDLE hAdapter, D3DDDIARG_CREATEDEVICE* pArgs)
    {
        log_line("[sogen-d3d9-umd] CreateDevice reached Interface=0x%x Version=0x%x pDeviceFuncs=%p Flags=0x%x\n",
                 pArgs->Interface, pArgs->Version, pArgs->pDeviceFuncs, pArgs->Flags);
        if (pArgs->pDeviceFuncs)
        {
            void** slots = reinterpret_cast<void**>(pArgs->pDeviceFuncs);
            const size_t n = sizeof(D3DDDI_DEVICEFUNCS) / sizeof(void*);
            for (size_t i = 0; i < n; ++i)
            {
                slots[i] = reinterpret_cast<void*>(&device_stub);
            }
        }
        pArgs->hDevice = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0xD9D90001));
        pArgs->CommandBuffer = 0; // no initial command buffer for this bring-up
        log_line("[sogen-d3d9-umd] CreateDevice returning S_OK (device funcs stubbed)\n");
        return S_OK;
    }

    HRESULT APIENTRY umd_CloseAdapter(HANDLE hAdapter)
    {
        log_line("[sogen-d3d9-umd] CloseAdapter\n");
        return S_OK;
    }
}

extern "C" __declspec(dllexport) HRESULT APIENTRY OpenAdapter(D3DDDIARG_OPENADAPTER* pArgs)
{
    log_line("[sogen-d3d9-umd] OpenAdapter reached Interface=0x%x Version=0x%x pAdapterFuncs=%p pCallbacks=%p\n",
             pArgs->Interface, pArgs->Version, pArgs->pAdapterFuncs, pArgs->pAdapterCallbacks);
    if (!pArgs->pAdapterFuncs)
    {
        return E_INVALIDARG;
    }
    pArgs->pAdapterFuncs->pfnGetCaps = umd_GetCaps;
    pArgs->pAdapterFuncs->pfnCreateDevice = umd_CreateDevice;
    pArgs->pAdapterFuncs->pfnCloseAdapter = umd_CloseAdapter;
    // Report our OWN actually-implemented interface version, not the runtime's. Echoing back
    // pArgs->Version (observed as 0xe000, far beyond WDDM2_1_2) made the runtime believe our
    // D3DDDI_DEVICEFUNCS table extends to WDDM2.1+ slots (e.g. pfnAcquireResource/pfnReleaseResource)
    // that SOGEN_D3D9_UMD_INTERFACE_VERSION=WIN7 doesn't declare, so ValidateUMDeviceFuncs read
    // uninitialized memory past our table and failed CreateDevice with D3DERR_NOTAVAILABLE.
    pArgs->DriverVersion = SOGEN_D3D9_UMD_INTERFACE_VERSION;
    pArgs->hAdapter = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(0xD9D9A001));
    log_line("[sogen-d3d9-umd] OpenAdapter returning S_OK (DriverVersion=0x%x)\n", pArgs->DriverVersion);
    return S_OK;
}
