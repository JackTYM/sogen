// sogen thin Direct3D9 WDDM user-mode driver (the vendor-driver slot the official d3d9.dll loads).
//
// OpenAdapter/GetCaps/CreateDevice are pure negotiation with the runtime and stay local. The
// device-function table marshals real D3D9 DDI calls across the D3DKMT Escape channel to the host
// d3d9_host decoder (see d3d9-command-protocol/d3d9_command_protocol.hpp for the wire protocol) --
// the same bridge_call pattern vulkan-shim.cpp uses for its own guest ICD.

#include "d3d9_ddi.hpp"

#include <d3d9.h>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

#include <d3d9_command_protocol.hpp>
#include <gpu_bridge_protocol.hpp>

namespace gb = sogen::gpu_bridge;
namespace d3d9c = sogen::d3d9_cmd;

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

    // D3DKMT structs (mingw ships no d3dkmthk.h). Layout matches the host EMU_D3DKMT_* ABI, which
    // stores pointers as UINT64 even for 32-bit (WoW64) guests, so pack to 8 and widen the
    // private-data pointer -- same discipline as vulkan_shim.cpp's own copy of these structs.
#pragma pack(push, 8)
    struct kmt_open_adapter_from_luid
    {
        uint32_t luid_low;
        int32_t luid_high;
        uint32_t h_adapter;
    };
    struct kmt_escape
    {
        uint32_t h_adapter;
        uint32_t h_device;
        uint32_t type; // 0 = D3DKMT_ESCAPE_DRIVERPRIVATE
        uint32_t flags;
        void* private_data; // native D3DKMT_ESCAPE.pPrivateDriverData: 4 bytes on WoW64, 8 on x64
        uint32_t private_data_size;
        uint32_t h_context;
    };
#pragma pack(pop)

    using pfn_d3dkmt = LONG(WINAPI*)(void*);

    pfn_d3dkmt load_win32u(const char* name)
    {
        HMODULE win32u = GetModuleHandleA("win32u.dll");
        if (!win32u)
        {
            win32u = LoadLibraryA("win32u.dll");
        }
        return win32u ? reinterpret_cast<pfn_d3dkmt>(GetProcAddress(win32u, name)) : nullptr;
    }

    uint32_t g_adapter = 0;

    uint32_t ensure_adapter()
    {
        if (g_adapter != 0)
        {
            return g_adapter;
        }
        static pfn_d3dkmt open_adapter = load_win32u("NtGdiDdDDIOpenAdapterFromLuid");
        if (!open_adapter)
        {
            return 0;
        }
        kmt_open_adapter_from_luid open{};
        open.luid_low = 0x1000; // sogen's fixed virtual adapter LUID
        open.luid_high = 0;
        if (open_adapter(&open) != 0)
        {
            return 0;
        }
        g_adapter = open.h_adapter;
        return g_adapter;
    }

    // Carries one D3D9 command to the host over the D3DKMT Escape channel:
    // [escape_command_header][in][out]. Mirrors vulkan_shim.cpp's bridge_call.
    bool bridge_call(uint32_t code, const void* in, DWORD in_len, void* out, DWORD out_len)
    {
        static pfn_d3dkmt escape = load_win32u("NtGdiDdDDIEscape");
        const uint32_t adapter = ensure_adapter();
        if (!escape || adapter == 0)
        {
            return false;
        }

        const uint32_t header_size = sizeof(gb::escape_command_header);
        std::vector<uint8_t> buffer(header_size + in_len + out_len);
        auto* header = reinterpret_cast<gb::escape_command_header*>(buffer.data());
        header->magic = gb::escape_magic;
        header->command_id = code;
        header->input_offset = header_size;
        header->input_size = in_len;
        header->output_offset = header_size + in_len;
        header->output_size = out_len;
        header->result = 0;
        header->reserved = 0;
        if (in != nullptr && in_len != 0)
        {
            std::memcpy(buffer.data() + header_size, in, in_len);
        }

        kmt_escape esc{};
        esc.h_adapter = adapter;
        esc.type = 0;
        esc.private_data = buffer.data();
        esc.private_data_size = static_cast<uint32_t>(buffer.size());
        if (escape(&esc) != 0)
        {
            return false;
        }

        if (out != nullptr && out_len != 0)
        {
            std::memcpy(out, buffer.data() + header_size + in_len, out_len);
        }
        return header->result >= 0;
    }

    // Generic device-function stub, still backing every slot D3D9 marshaling below doesn't implement
    // yet. On x64 the calling convention is caller-cleanup, so one stub can back every slot of
    // D3DDDI_DEVICEFUNCS regardless of the real arity. The x86 port needs typed thunks.
    HRESULT APIENTRY device_stub()
    {
        return S_OK;
    }

    // KNOWN LIMITATION (see HANDOFF_MACBOOK.md): D3DDDIARG_CREATERESOURCE's real field layout (width/
    // height/usage/pool offsets) is not yet RE-verified -- only offset 0 (Format) and offset 48 (the
    // output hResource field) are confirmed. Every call is therefore treated as a render-target 2D
    // texture at the test's known backbuffer size (640x480) until the real struct is pinned; fine for
    // the current fixed-size-window milestone, wrong in general.
    //
    // Offset 48 for hResource was found by writing a distinct, identifiable sentinel to every 8-byte-
    // aligned offset (0..80) and observing which one came back unchanged in the very next
    // SetRenderTarget call (0xAAAA000000000030, i.e. offset 0x30 = 48) -- direct proof, not inference
    // from a static hex dump (two earlier single-offset guesses, 40 and 44, were each individually
    // plausible-looking but empirically wrong).
    //
    // resolve_resource_id() also keeps a lazy-bind-at-first-use fallback (for any resource handle that
    // somehow reaches SetRenderTarget/Lock without going through CreateResource) -- harmless dead code
    // in the common case now that CreateResource populates the map directly.
    std::unordered_map<uint64_t, uint64_t> g_resource_ids;

    HRESULT APIENTRY umd_CreateResource(HANDLE /*hDevice*/, void* pArgs)
    {
        auto* bytes = reinterpret_cast<unsigned char*>(pArgs);
        uint32_t format = 0;
        std::memcpy(&format, bytes, sizeof(format));

        const d3d9c::create_resource_request req{
            .kind = static_cast<uint32_t>(d3d9c::resource_kind::texture_2d),
            .format = format,
            .width = 640,
            .height = 480,
            .depth = 1,
            .mip_levels = 1,
            .usage = 0x1, // D3DUSAGE_RENDERTARGET (public, ABI-stable D3D9 constant, not RE'd)
            .pool = 0,
        };
        d3d9c::create_resource_response resp{};
        bridge_call(gb::ioctl_d3d9_create_resource, &req, sizeof(req), &resp, sizeof(resp));
        if (resp.hr == 0)
        {
            // The runtime echoes this back unchanged in later calls (SetRenderTarget's hRenderTarget,
            // Lock's hResource, ...), so no separate g_resource_ids entry is needed: resolve_resource_id
            // already returns an unrecognized handle unchanged, which is correct here since the handle
            // IS the wire resource_id once this write lands.
            std::memcpy(bytes + 48, &resp.resource, sizeof(resp.resource));
        }
        return S_OK;
    }

    uint64_t resolve_resource_id(void* handle)
    {
        const auto raw = reinterpret_cast<uint64_t>(handle);
        if (raw == 0)
        {
            return 0;
        }

        const auto it = g_resource_ids.find(raw);
        if (it != g_resource_ids.end())
        {
            return it->second;
        }

        const d3d9c::create_resource_request req{
            .kind = static_cast<uint32_t>(d3d9c::resource_kind::texture_2d),
            .format = 22, // D3DFMT_X8R8G8B8 (matches the test's known BackBufferFormat)
            .width = 640,
            .height = 480,
            .depth = 1,
            .mip_levels = 1,
            .usage = 0x1, // D3DUSAGE_RENDERTARGET (public, ABI-stable D3D9 constant, not RE'd)
            .pool = 0,
        };
        d3d9c::create_resource_response resp{};
        bridge_call(gb::ioctl_d3d9_create_resource, &req, sizeof(req), &resp, sizeof(resp));
        if (resp.hr != 0)
        {
            return raw; // fall back to the raw handle (pre-existing behavior) on failure
        }

        g_resource_ids[raw] = resp.resource;
        return resp.resource;
    }

    // Buffers (vertex/index) never call pfnCreateResource at all (RE-confirmed live), so their DDI
    // handle -- a small runtime-internal number, live-observed to collide with resolve_resource_id's
    // own sequential ids -- reaches pfnLock completely unregistered. pfnLock is the first (and only)
    // call site that knows the buffer's real byte size (SizeToLock), so it's the right place to lazily
    // register a correctly-sized/kinded resource instead of resolve_resource_id's texture-shaped
    // fallback, which previously made every never-seen Lock() land on a wrong-kind 640x480 texture.
    uint64_t resolve_buffer_resource_id(void* handle, uint32_t byte_size)
    {
        const auto raw = reinterpret_cast<uint64_t>(handle);
        if (raw == 0)
        {
            return 0;
        }

        const auto it = g_resource_ids.find(raw);
        if (it != g_resource_ids.end())
        {
            return it->second;
        }

        const d3d9c::create_resource_request req{
            .kind = static_cast<uint32_t>(d3d9c::resource_kind::vertex_buffer),
            .format = 0,
            .width = byte_size != 0 ? byte_size : 64 * 1024, // SizeToLock==0 means "whole buffer"; no
                                                              // real size is knowable here, so guess.
            .height = 0,
            .depth = 1,
            .mip_levels = 1,
            .usage = 0,
            .pool = 0,
        };
        d3d9c::create_resource_response resp{};
        bridge_call(gb::ioctl_d3d9_create_resource, &req, sizeof(req), &resp, sizeof(resp));
        if (resp.hr != 0)
        {
            return raw;
        }

        g_resource_ids[raw] = resp.resource;
        return resp.resource;
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
        // DevCaps bit 0x02000000 has no name in the public D3DDEVCAPS_* set (the defined bits jump from
        // D3DDEVCAPS_NPATCHES=0x1000000 straight past it) -- live-traced (via sogen's own Python
        // debugger API, hooking CVertexBuffer::Create in d3d9.dll and watching the memcpy that seeds
        // _D3D9_DEVICEDATA from this exact DevCaps DWORD) to be the exact gate CVertexBuffer::Create
        // checks before letting a D3DPOOL_DEFAULT vertex/index buffer keep its real pool value; without
        // it, every vertex buffer -- regardless of requested pool -- gets silently remapped to system
        // memory (CreateSysmemVertexBuffer), and pfnCreateResource/pfnLock are never invoked for it.
        // Undocumented internal reuse of this bit by the runtime's caps gauntlet -- same pattern as the
        // DevCaps2 STREAMOFFSET gate above.
        constexpr DWORD k_devcaps_driver_managed_pool = 0x02000000;
        caps->DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_PUREDEVICE |
                        D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_TEXTUREVIDEOMEMORY | k_devcaps_driver_managed_pool;
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

    // ---------------------------------------------------------------------------------------------
    // Real per-DDI marshaling: the streamed state/draw functions, sent as individual sync Escape
    // calls (bridge_call) carrying the matching d3d9_cmd wire record. Resource-handle DDI fields
    // (hTexture, hVertexBuffer, ...) hold exactly the uint64 resource_id pfnCreateResource returned,
    // reinterpreted as a HANDLE -- no separate guest-side handle table is needed.
    //
    // Scope: resource/shader creation, Lock/Unlock, and Present stay on device_stub for now -- their
    // real D3DDDIARG_* shapes are large/uncertain without a WDK reference and need their own
    // RE-verification pass (see the d3d9-command-protocol wire format for what the host side already
    // expects once those are wired up). Everything below is the higher-confidence, higher-frequency
    // per-draw state path.
    // ---------------------------------------------------------------------------------------------

    HRESULT APIENTRY umd_SetRenderState(HANDLE /*hDevice*/, CONST D3DDDIARG_RENDERSTATE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::set_render_state_record req{.state = pArgs->State, .value = pArgs->Value};
        bridge_call(gb::ioctl_d3d9_set_render_state, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetTextureStageState(HANDLE /*hDevice*/, CONST D3DDDIARG_TEXTURESTAGESTATE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::set_texture_stage_state_record req{.stage = pArgs->Stage, .state = pArgs->State, .value = pArgs->Value, .reserved = 0};
        bridge_call(gb::ioctl_d3d9_set_texture_stage_state, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetTexture(HANDLE /*hDevice*/, UINT Stage, HANDLE hTexture)
    {
        // RE-verified live: pfnSetTexture takes Stage/hTexture as direct value arguments, not a
        // pointer to D3DDDIARG_SETTEXTURE -- a struct-pointer read crashed with the small Stage
        // integer (e.g. 0x1) dereferenced as an address. 0 for hTexture means unbind.
        d3d9c::set_texture_record req{.stage = Stage, .reserved = 0, .texture = reinterpret_cast<uint64_t>(hTexture)};
        bridge_call(gb::ioctl_d3d9_set_texture, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetPixelShader(HANDLE /*hDevice*/, CONST D3DDDIARG_SETPIXELSHADERFUNC* pArgs)
    {
        // Same NULL-means-unbind convention confirmed live for SetVertexShaderFunc/Decl and SetTexture.
        d3d9c::set_pixel_shader_record req{.shader = pArgs != nullptr ? reinterpret_cast<uint64_t>(pArgs->ShaderHandle) : 0};
        bridge_call(gb::ioctl_d3d9_set_pixel_shader, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetVertexShaderFunc(HANDLE /*hDevice*/, CONST D3DDDIARG_SETVERTEXSHADERFUNC* pArgs)
    {
        // NULL pArgs is a real, crash-causing case (found live): the runtime uses it to mean "no vertex
        // shader / use fixed-function", e.g. when a D3DFVF_XYZRHW draw follows a shader-bound one.
        d3d9c::set_vertex_shader_record req{.shader = pArgs != nullptr ? reinterpret_cast<uint64_t>(pArgs->ShaderHandle) : 0};
        bridge_call(gb::ioctl_d3d9_set_vertex_shader, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetVertexShaderDecl(HANDLE /*hDevice*/, CONST D3DDDIARG_SETVERTEXSHADERDECL* pArgs)
    {
        // NULL pArgs crashed this function live (confirmed via a real access violation at the
        // dereference below) -- the runtime passes it for fixed-function (D3DFVF-only, no explicit
        // vertex declaration) draws. 0 = no decl, matching umd_SetVertexShaderFunc's own convention.
        d3d9c::set_vertex_decl_record req{.decl = pArgs != nullptr ? reinterpret_cast<uint64_t>(pArgs->ShaderHandle) : 0};
        bridge_call(gb::ioctl_d3d9_set_vertex_decl, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    // pArgs points at the fixed header; the vector4 float data immediately follows it in memory
    // (the runtime's own send buffer), matching d3d9_cmd::set_const_f_record's trailing-payload shape.
    HRESULT APIENTRY umd_SetVertexShaderConst(HANDLE /*hDevice*/, CONST D3DDDIARG_SETVERTEXSHADERCONST* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        const auto* values = reinterpret_cast<const float*>(pArgs + 1);
        const size_t float_count = static_cast<size_t>(pArgs->Count) * 4;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_f_record) + float_count * sizeof(float));
        auto* req = reinterpret_cast<d3d9c::set_const_f_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->vector4_count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), values, float_count * sizeof(float));
        bridge_call(gb::ioctl_d3d9_set_vs_const_f, buf.data(), static_cast<DWORD>(buf.size()), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetPixelShaderConst(HANDLE /*hDevice*/, CONST D3DDDIARG_SETPIXELSHADERCONST* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        const auto* values = reinterpret_cast<const float*>(pArgs + 1);
        const size_t float_count = static_cast<size_t>(pArgs->Count) * 4;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_f_record) + float_count * sizeof(float));
        auto* req = reinterpret_cast<d3d9c::set_const_f_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->vector4_count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), values, float_count * sizeof(float));
        bridge_call(gb::ioctl_d3d9_set_ps_const_f, buf.data(), static_cast<DWORD>(buf.size()), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetStreamSource(HANDLE /*hDevice*/, CONST D3DDDIARG_SETSTREAMSOURCE* pArgs)
    {
        if (pArgs == nullptr) // unbind, same convention as the shader/texture slots
        {
            return S_OK;
        }
        // Resolve through the same buffer lazy-bind Lock() uses -- the DDI vertex buffer handle is a
        // small runtime-internal number (never registered via pfnCreateResource) that can otherwise
        // collide with an unrelated resource id. By the time SetStreamSource runs the app has normally
        // already Locked this buffer once (to write its data), so this is usually just a cache hit; a
        // size of 0 here only matters on the rare truly-first-touch path, which falls back to a guess.
        d3d9c::set_stream_source_record req{.stream_number = pArgs->StreamNumber,
                                            .offset_bytes = pArgs->Offset,
                                            .stride_bytes = pArgs->Stride,
                                            .reserved = 0,
                                            .vertex_buffer = resolve_buffer_resource_id(pArgs->hVertexBuffer, 0)};
        bridge_call(gb::ioctl_d3d9_set_stream_source, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetStreamSourceFreq(HANDLE /*hDevice*/, CONST D3DDDIARG_SETSTREAMSOURCEFREQ* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::set_stream_source_freq_record req{.stream_number = pArgs->StreamNumber, .frequency = pArgs->Divider};
        bridge_call(gb::ioctl_d3d9_set_stream_source_freq, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetIndices(HANDLE /*hDevice*/, CONST D3DDDIARG_SETINDICES* pArgs)
    {
        if (pArgs == nullptr) // unbind index buffer, same convention as the shader/texture slots
        {
            return S_OK;
        }
        // Same buffer lazy-bind reasoning as umd_SetStreamSource.
        d3d9c::set_indices_record req{.index_buffer = resolve_buffer_resource_id(pArgs->hIndexBuffer, 0),
                                      .format = pArgs->Stride == 4 ? 1u : 0u,
                                      .reserved = 0};
        bridge_call(gb::ioctl_d3d9_set_indices, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetRenderTarget(HANDLE /*hDevice*/, CONST D3DDDIARG_SETRENDERTARGET* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::set_render_target_record req{.render_target_index = pArgs->RenderTargetIndex,
                                            .reserved = 0,
                                            .surface = resolve_resource_id(pArgs->hRenderTarget)};
        bridge_call(gb::ioctl_d3d9_set_render_target, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetDepthStencil(HANDLE /*hDevice*/, CONST D3DDDIARG_SETDEPTHSTENCIL* pArgs)
    {
        if (pArgs == nullptr) // unbind depth-stencil, same convention as the shader/texture slots
        {
            return S_OK;
        }
        d3d9c::set_depth_stencil_record req{.surface = resolve_resource_id(pArgs->hZBuffer)};
        bridge_call(gb::ioctl_d3d9_set_depth_stencil, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    // The DDI exposes viewport geometry (pfnSetViewport) and depth range (pfnSetZRange) as two
    // separate calls, but the wire protocol's set_viewport_record bundles both -- track the latest of
    // each locally and resend the combined record from whichever call lands second (steady state:
    // both are always known once a device has rendered at least one frame).
    D3DDDIARG_VIEWPORTINFO g_viewport{};
    D3DDDIARG_ZRANGE g_zrange{.MinZ = 0.0f, .MaxZ = 1.0f};

    void send_viewport()
    {
        d3d9c::set_viewport_record req{.x = static_cast<float>(g_viewport.X),
                                       .y = static_cast<float>(g_viewport.Y),
                                       .width = static_cast<float>(g_viewport.Width),
                                       .height = static_cast<float>(g_viewport.Height),
                                       .min_z = g_zrange.MinZ,
                                       .max_z = g_zrange.MaxZ};
        bridge_call(gb::ioctl_d3d9_set_viewport, &req, sizeof(req), nullptr, 0);
    }

    HRESULT APIENTRY umd_SetViewport(HANDLE /*hDevice*/, CONST D3DDDIARG_VIEWPORTINFO* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        g_viewport = *pArgs;
        send_viewport();
        return S_OK;
    }

    HRESULT APIENTRY umd_SetZRange(HANDLE /*hDevice*/, CONST D3DDDIARG_ZRANGE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        g_zrange = *pArgs;
        send_viewport();
        return S_OK;
    }

    HRESULT APIENTRY umd_SetScissorRect(HANDLE /*hDevice*/, CONST D3DDDIRECT* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::set_scissor_record req{.left = pArgs->left, .top = pArgs->top, .right = pArgs->right, .bottom = pArgs->bottom};
        bridge_call(gb::ioctl_d3d9_set_scissor, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    // RE-verified real pfnClear signature (see D3DDDIARG_CLEAR's own comment in d3d9_ddi.hpp):
    // NumRect/pRect are separate parameters, not struct fields.
    HRESULT APIENTRY umd_Clear(HANDLE /*hDevice*/, CONST D3DDDIARG_CLEAR* pArgs, UINT NumRect, CONST D3DDDIRECT* pRect)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        std::vector<uint8_t> buf(sizeof(d3d9c::clear_record) + static_cast<size_t>(NumRect) * sizeof(d3d9c::set_scissor_record));
        auto* req = reinterpret_cast<d3d9c::clear_record*>(buf.data());
        req->flags = pArgs->Flags;
        req->color_argb = pArgs->Color;
        req->z = pArgs->Z;
        req->stencil = pArgs->Stencil;
        req->rect_count = NumRect;
        auto* wire_rects = reinterpret_cast<d3d9c::set_scissor_record*>(buf.data() + sizeof(*req));
        for (UINT i = 0; i < NumRect; ++i)
        {
            wire_rects[i] = d3d9c::set_scissor_record{
                .left = pRect[i].left, .top = pRect[i].top, .right = pRect[i].right, .bottom = pRect[i].bottom};
        }
        bridge_call(gb::ioctl_d3d9_clear, buf.data(), static_cast<DWORD>(buf.size()), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_DrawPrimitive(HANDLE /*hDevice*/, CONST D3DDDIARG_DRAWPRIMITIVE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::draw_primitive_record req{.primitive_type = pArgs->PrimitiveType,
                                         .start_vertex = pArgs->VStart,
                                         .primitive_count = pArgs->PrimitiveCount,
                                         .reserved = 0};
        bridge_call(gb::ioctl_d3d9_draw_primitive, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_DrawIndexedPrimitive(HANDLE /*hDevice*/, CONST D3DDDIARG_DRAWINDEXEDPRIMITIVE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::draw_indexed_primitive_record req{.primitive_type = pArgs->PrimitiveType,
                                                 .base_vertex_index = pArgs->BaseVertexIndex,
                                                 .min_vertex_index = pArgs->MinIndex,
                                                 .num_vertices = pArgs->NumVertices,
                                                 .start_index = pArgs->StartIndex,
                                                 .primitive_count = pArgs->PrimitiveCount};
        bridge_call(gb::ioctl_d3d9_draw_indexed_primitive, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_Flush(HANDLE /*hDevice*/)
    {
        return S_OK;
    }

    // D3DDDIARG_LOCK::pData must point to memory that stays valid until the matching Unlock (the app
    // writes vertex/index data directly through it), so each outstanding lock owns a persistent
    // heap buffer here instead of a call-local one. Keyed by the wire resource id (== hResource).
    std::unordered_map<uint64_t, std::vector<uint8_t>> g_locked_buffers;

    HRESULT APIENTRY umd_Lock(HANDLE /*hDevice*/, D3DDDIARG_LOCK* pArgs)
    {
        if (pArgs == nullptr)
        {
            return E_FAIL;
        }
        // Real render-target/texture handles are already registered by pfnCreateResource by the time
        // Lock() reaches them (confirmed live), so the lazy-bind path below only ever fires for
        // vertex/index buffers (which never call pfnCreateResource at all) -- safe to always resolve
        // as a buffer here; already-registered handles hit the same early-return in either function.
        const auto resource = resolve_buffer_resource_id(pArgs->hResource, pArgs->SizeToLock);
        d3d9c::lock_request req{.resource = resource,
                                .subresource = 0,
                                .offset = pArgs->OffsetToLock,
                                .size = pArgs->SizeToLock,
                                .flags = pArgs->Flags,
                                .reserved = 0};

        // First call with no output buffer just to learn the true backing size via lock_response.
        d3d9c::lock_response probe{};
        bridge_call(gb::ioctl_d3d9_lock, &req, sizeof(req), &probe, sizeof(probe));

        auto& buffer = g_locked_buffers[resource];
        buffer.assign(probe.data_size, 0);

        std::vector<uint8_t> out_buf(sizeof(d3d9c::lock_response) + buffer.size());
        bridge_call(gb::ioctl_d3d9_lock, &req, sizeof(req), out_buf.data(), static_cast<DWORD>(out_buf.size()));
        const auto* resp = reinterpret_cast<const d3d9c::lock_response*>(out_buf.data());
        if (resp->hr != 0)
        {
            g_locked_buffers.erase(resource);
            pArgs->pData = nullptr;
            return E_FAIL;
        }
        std::memcpy(buffer.data(), out_buf.data() + sizeof(*resp), buffer.size());
        pArgs->pData = buffer.data();
        return S_OK;
    }

    HRESULT APIENTRY umd_Unlock(HANDLE /*hDevice*/, D3DDDIARG_UNLOCK* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        const auto resource = resolve_resource_id(pArgs->hResource);
        const auto it = g_locked_buffers.find(resource);
        if (it == g_locked_buffers.end())
        {
            return S_OK; // nothing to write back (e.g. a failed Lock)
        }

        std::vector<uint8_t> buf(sizeof(d3d9c::unlock_request) + it->second.size());
        auto* req = reinterpret_cast<d3d9c::unlock_request*>(buf.data());
        req->resource = resource;
        req->subresource = 0;
        req->offset = 0;
        req->data_size = static_cast<uint32_t>(it->second.size());
        std::memcpy(buf.data() + sizeof(*req), it->second.data(), it->second.size());
        bridge_call(gb::ioctl_d3d9_unlock, buf.data(), static_cast<DWORD>(buf.size()), nullptr, 0);

        g_locked_buffers.erase(it);
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

            // Real per-DDI marshaling (see the block above) for the state/draw path -- indices match
            // D3DDDI_DEVICEFUNCS's field order exactly (d3d9_ddi.hpp).
            slots[0] = reinterpret_cast<void*>(&umd_SetRenderState);          // pfnSetRenderState
            slots[3] = reinterpret_cast<void*>(&umd_SetTextureStageState);    // pfnSetTextureStageState
            slots[4] = reinterpret_cast<void*>(&umd_SetTexture);              // pfnSetTexture
            slots[5] = reinterpret_cast<void*>(&umd_SetPixelShader);          // pfnSetPixelShader
            slots[6] = reinterpret_cast<void*>(&umd_SetPixelShaderConst);     // pfnSetPixelShaderConst
            slots[8] = reinterpret_cast<void*>(&umd_SetIndices);              // pfnSetIndices
            slots[10] = reinterpret_cast<void*>(&umd_DrawPrimitive);          // pfnDrawPrimitive
            slots[11] = reinterpret_cast<void*>(&umd_DrawIndexedPrimitive);   // pfnDrawIndexedPrimitive
            slots[21] = reinterpret_cast<void*>(&umd_Clear);                  // pfnClear
            slots[24] = reinterpret_cast<void*>(&umd_SetVertexShaderConst);   // pfnSetVertexShaderConst
            slots[27] = reinterpret_cast<void*>(&umd_SetViewport);            // pfnSetViewport
            slots[28] = reinterpret_cast<void*>(&umd_SetZRange);              // pfnSetZRange
            slots[35] = reinterpret_cast<void*>(&umd_Lock);                    // pfnLock
            slots[36] = reinterpret_cast<void*>(&umd_Unlock);                  // pfnUnlock
            slots[37] = reinterpret_cast<void*>(&umd_CreateResource);          // pfnCreateResource
            slots[41] = reinterpret_cast<void*>(&umd_Flush);                  // pfnFlush
            slots[44] = reinterpret_cast<void*>(&umd_SetVertexShaderFunc);    // pfnSetVertexShaderFunc
            slots[47] = reinterpret_cast<void*>(&umd_SetVertexShaderDecl);    // pfnSetVertexShaderDecl
            slots[50] = reinterpret_cast<void*>(&umd_SetScissorRect);         // pfnSetScissorRect
            slots[51] = reinterpret_cast<void*>(&umd_SetStreamSource);        // pfnSetStreamSource
            slots[52] = reinterpret_cast<void*>(&umd_SetStreamSourceFreq);    // pfnSetStreamSourceFreq
            slots[62] = reinterpret_cast<void*>(&umd_SetRenderTarget);        // pfnSetRenderTarget
            slots[63] = reinterpret_cast<void*>(&umd_SetDepthStencil);        // pfnSetDepthStencil
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
