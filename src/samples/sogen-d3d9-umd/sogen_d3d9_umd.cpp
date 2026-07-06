// sogen thin Direct3D9 WDDM user-mode driver (the vendor-driver slot the official d3d9.dll loads).
//
// OpenAdapter/GetCaps/CreateDevice are pure negotiation with the runtime and stay local. The
// device-function table marshals real D3D9 DDI calls across the D3DKMT Escape channel to the host
// d3d9_host decoder (see d3d9-command-protocol/d3d9_command_protocol.hpp for the wire protocol) --
// the same bridge_call pattern vulkan-shim.cpp uses for its own guest ICD.

#include "d3d9_ddi.hpp"

#include <d3d9.h>
#include <algorithm>
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

    void flush_d3d9_batch();

    // Carries one D3D9 command to the host over the D3DKMT Escape channel:
    // [escape_command_header][in][out]. Mirrors vulkan_shim.cpp's bridge_call.
    bool bridge_call(uint32_t code, const void* in, DWORD in_len, void* out, DWORD out_len)
    {
        // Every other bridge call may make the host observe D3D9 state (draw, clear, lock, present,
        // create a resource, ...), so drain any pending batched commands first to keep host-observed
        // ordering identical to the un-batched path. The `!=` guard also prevents flush_d3d9_batch's
        // own record_commands call from recursing back into itself.
        if (code != gb::ioctl_record_commands)
        {
            flush_d3d9_batch();
        }

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

    // Batched D3D9 streamed opcodes: same wire mechanism vulkan_shim.cpp's g_command_streams/
    // record_command use, but a single global stream since D3D9 has no command-buffer concept.
    // record_d3d9 only appends; bridge_call's flush-on-every-other-call guard above is what actually
    // drains the batch, right before the host needs to observe its effects.
    std::vector<uint8_t> g_d3d9_command_batch;

    // Backstop only: every frame ends in Present, which flushes via bridge_call's guard, so this
    // should never trigger in practice. Guards against unbounded growth if an unusually long run of
    // Group-A calls happens with no Group-B call in between.
    constexpr size_t k_d3d9_batch_flush_threshold = 64 * 1024;

    void flush_d3d9_batch()
    {
        if (g_d3d9_command_batch.empty())
        {
            return;
        }

        std::vector<uint8_t> batch;
        batch.swap(g_d3d9_command_batch);

        gb::result_response resp{};
        bridge_call(gb::ioctl_record_commands, batch.data(), static_cast<DWORD>(batch.size()), &resp, sizeof(resp));
    }

    void record_d3d9(gb::command opcode, const void* in, uint32_t in_len)
    {
        gb::command_record_header header{.command = static_cast<uint32_t>(opcode), .size = in_len};
        const auto* header_bytes = reinterpret_cast<const uint8_t*>(&header);
        g_d3d9_command_batch.insert(g_d3d9_command_batch.end(), header_bytes, header_bytes + sizeof(header));
        const auto* payload_bytes = reinterpret_cast<const uint8_t*>(in);
        g_d3d9_command_batch.insert(g_d3d9_command_batch.end(), payload_bytes, payload_bytes + in_len);

        if (g_d3d9_command_batch.size() > k_d3d9_batch_flush_threshold)
        {
            flush_d3d9_batch();
        }
    }

    // Generic device-function stub, still backing every slot D3D9 marshaling below doesn't implement
    // yet. On x64 the calling convention is caller-cleanup, so one stub can back every slot of
    // D3DDDI_DEVICEFUNCS regardless of the real arity. The x86 port needs typed thunks.
    HRESULT APIENTRY device_stub()
    {
        return S_OK;
    }

#ifndef _WIN64
    // x86 __stdcall is callee-cleanup: the callee's `ret N` epilogue is baked in from its own
    // declared parameter list at compile time. An unimplemented slot must therefore declare the
    // real argument byte count, or the stack desyncs the moment the real runtime calls it expecting
    // more than zero bytes popped. One stub per distinct byte count covers every unimplemented slot
    // (see k_device_func_arity below for which slot needs which one).
    HRESULT APIENTRY stub_args_4(void*)
    {
        return S_OK;
    }

    HRESULT APIENTRY stub_args_8(void*, void*)
    {
        return S_OK;
    }

    HRESULT APIENTRY stub_args_12(void*, void*, void*)
    {
        return S_OK;
    }

    HRESULT APIENTRY stub_args_16(void*, void*, void*, void*)
    {
        return S_OK;
    }

    HRESULT APIENTRY stub_args_20(void*, void*, void*, void*, void*)
    {
        return S_OK;
    }

    HRESULT APIENTRY stub_args_24(void*, void*, void*, void*, void*, void*)
    {
        return S_OK;
    }

    // Per-slot argument byte count for every D3DDDI_DEVICEFUNCS entry, in exact field order and
    // mirroring that struct's own #if version gating (d3d9_ddi.hpp) so this table's length always
    // matches sizeof(D3DDDI_DEVICEFUNCS)/sizeof(void*) for whatever SOGEN_D3D9_UMD_INTERFACE_VERSION
    // is configured. Values for the 28 slots umd_CreateDevice wires to a real implementation are
    // unused -- those slots always keep their real function pointer regardless of this table -- but
    // are filled in from the same WDK-documented signatures for readability. Defaults to 8, the
    // dominant (HANDLE, CONST D3DDDIARG_X*) pattern; deviations noted inline.
    constexpr uint8_t k_device_func_arity[] = {
        // --- base (Vista) : 99 entries ---
        8,  // pfnSetRenderState (real)
        8,  // pfnUpdateWInfo
        8,  // pfnValidateDevice
        8,  // pfnSetTextureStageState (real)
        12, // pfnSetTexture (real) -- (HANDLE, UINT Stage, HANDLE)
        8,  // pfnSetPixelShader (real)
        12, // pfnSetPixelShaderConst (real) -- trailing CONST FLOAT*
        12, // pfnSetStreamSourceUm -- trailing data ptr
        8,  // pfnSetIndices (real)
        12, // pfnSetIndicesUm -- trailing data ptr
        12, // pfnDrawPrimitive (real) -- (HANDLE, CONST D3DDDIARG_DRAWPRIMITIVE*, CONST UINT* pFlags), the
            // WDK-documented 3-arg shape. RE-verified live in d3d9_x86.dll (both normal and UP draw paths
            // push three args) -- see umd_DrawPrimitive's own comment. An earlier 2-arg guess desynced the
            // x86 stack; the mismatch was invisible on x64 (caller-cleanup) until the UP-draw path hit it.
        8,  // pfnDrawIndexedPrimitive (real)
        16, // pfnDrawRectPatch
        16, // pfnDrawTriPatch
        8,  // pfnDrawPrimitive2
        20, // pfnDrawIndexedPrimitive2 -- (H, arg*, UINT, VOID*, UINT*)
        8,  // pfnVolBlt
        8,  // pfnBufBlt
        8,  // pfnTexBlt
        8,  // pfnStateSet
        8,  // pfnSetPriority
        16, // pfnClear (real) -- (H, arg*, UINT, RECT*)
        12, // pfnUpdatePalette
        8,  // pfnSetPalette
        12, // pfnSetVertexShaderConst (real) -- trailing CONST FLOAT*
        8,  // pfnMultiplyTransform
        8,  // pfnSetTransform
        8,  // pfnSetViewport (real)
        8,  // pfnSetZRange (real)
        8,  // pfnSetMaterial
        12, // pfnSetLight
        8,  // pfnCreateLight
        8,  // pfnDestroyLight
        8,  // pfnSetClipPlane
        16, // pfnGetInfo -- (H, UINT, VOID*, UINT)
        8,  // pfnLock (real)
        8,  // pfnUnlock (real)
        8,  // pfnCreateResource (real)
        8,  // pfnDestroyResource
        8,  // pfnSetDisplayMode
        8,  // pfnPresent (real)
        4,  // pfnFlush (real) -- (HANDLE) only
        12, // pfnCreateVertexShaderFunc (real)
        8,  // pfnDeleteVertexShaderFunc (real)
        8,  // pfnSetVertexShaderFunc (real)
        12, // pfnCreateVertexShaderDecl (real)
        8,  // pfnDeleteVertexShaderDecl
        8,  // pfnSetVertexShaderDecl (real)
        12, // pfnSetVertexShaderConstI (real) -- trailing CONST INT*
        12, // pfnSetVertexShaderConstB (real) -- trailing CONST BOOL*
        8,  // pfnSetScissorRect (real)
        8,  // pfnSetStreamSource (real)
        8,  // pfnSetStreamSourceFreq (real)
        8,  // pfnSetConvolutionKernelMono
        8,  // pfnComposeRects
        8,  // pfnBlt
        8,  // pfnColorFill
        8,  // pfnDepthFill
        8,  // pfnCreateQuery
        8,  // pfnDestroyQuery
        8,  // pfnIssueQuery
        8,  // pfnGetQueryData
        8,  // pfnSetRenderTarget (real)
        8,  // pfnSetDepthStencil (real)
        8,  // pfnGenerateMipSubLevels
        12, // pfnSetPixelShaderConstI (real) -- trailing CONST INT*
        12, // pfnSetPixelShaderConstB (real) -- trailing CONST BOOL*
        12, // pfnCreatePixelShader (real)
        8,  // pfnDeletePixelShader (real)
        8,  // pfnCreateDecodeDevice
        8,  // pfnDestroyDecodeDevice
        8,  // pfnSetDecodeRenderTarget
        8,  // pfnDecodeBeginFrame
        8,  // pfnDecodeEndFrame
        8,  // pfnDecodeExecute
        8,  // pfnDecodeExtensionExecute
        8,  // pfnCreateVideoProcessDevice
        8,  // pfnDestroyVideoProcessDevice
        8,  // pfnVideoProcessBeginFrame
        8,  // pfnVideoProcessEndFrame
        8,  // pfnSetVideoProcessRenderTarget
        8,  // pfnVideoProcessBlt
        8,  // pfnCreateExtensionDevice
        8,  // pfnDestroyExtensionDevice
        8,  // pfnExtensionExecute
        8,  // pfnCreateOverlay
        8,  // pfnUpdateOverlay
        8,  // pfnFlipOverlay
        8,  // pfnGetOverlayColorControls
        8,  // pfnSetOverlayColorControls
        8,  // pfnDestroyOverlay
        4,  // pfnDestroyDevice -- (HANDLE) only
        8,  // pfnQueryResourceResidency
        8,  // pfnOpenResource
        8,  // pfnGetCaptureAllocationHandle
        8,  // pfnCaptureToSysMem
        8,  // pfnLockAsync
        8,  // pfnUnlockAsync
        8,  // pfnRename
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WIN7)
        // --- WIN7 : 22 entries ---
        8,  // pfnCreateVideoProcessor
        8,  // pfnSetVideoProcessBltState
        8,  // pfnGetVideoProcessBltStatePrivate
        8,  // pfnSetVideoProcessStreamState
        8,  // pfnGetVideoProcessStreamStatePrivate
        8,  // pfnVideoProcessBltHD
        8,  // pfnDestroyVideoProcessor
        8,  // pfnCreateAuthenticatedChannel
        8,  // pfnAuthenticatedChannelKeyExchange
        8,  // pfnQueryAuthenticatedChannel
        8,  // pfnConfigureAuthenticatedChannel
        8,  // pfnDestroyAuthenticatedChannel
        8,  // pfnCreateCryptoSession
        8,  // pfnCryptoSessionKeyExchange
        8,  // pfnDestroyCryptoSession
        8,  // pfnEncryptionBlt
        8,  // pfnGetPitch
        8,  // pfnStartSessionKeyRefresh
        8,  // pfnFinishSessionKeyRefresh
        8,  // pfnGetEncryptionBltKey
        8,  // pfnDecryptionBlt
        8,  // pfnResolveSharedResource
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WIN8)
        // --- WIN8 : 10 entries ---
        8,  // pfnVolBlt1
        8,  // pfnBufBlt1
        8,  // pfnTexBlt1
        8,  // pfnDiscard
        8,  // pfnOfferResources
        8,  // pfnReclaimResources
        8,  // pfnCheckDirectFlipSupport
        8,  // pfnCreateResource2
        8,  // pfnCheckMultiPlaneOverlaySupport
        8,  // pfnPresentMultiPlaneOverlay
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM1_3)
        // --- WDDM1.3 : 9 entries ---
        8,  // pfnReserved1 -- never invoked by the runtime; arity irrelevant
        8,  // pfnFlush1 -- LOW CONFIDENCE, verify against d3d9_x86.dll.i64 if ever actually hit
        8,  // pfnCheckCounterInfo -- LOW CONFIDENCE, verify against d3d9_x86.dll.i64 if ever hit
        24, // pfnCheckCounter -- LOW CONFIDENCE (many out-params, the biggest outlier), verify
            // against d3d9_x86.dll.i64 if ever actually hit
        8,  // pfnUpdateSubresourceUP
        8,  // pfnPresent1
        8,  // pfnCheckPresentDurationSupport
        8,  // pfnSetMarker -- LOW CONFIDENCE, verify against d3d9_x86.dll.i64 if ever actually hit
        8,  // pfnSetMarkerMode -- LOW CONFIDENCE, verify against d3d9_x86.dll.i64 if ever actually hit
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM2_0)
        8, // pfnTrimResidencySet
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM2_1_2)
        8, // pfnAcquireResource
        8, // pfnReleaseResource
#endif
    };

    static_assert(sizeof(k_device_func_arity) / sizeof(k_device_func_arity[0]) ==
                      sizeof(D3DDDI_DEVICEFUNCS) / sizeof(void*),
                  "k_device_func_arity must have exactly one entry per D3DDDI_DEVICEFUNCS slot");

    void* stub_for_arity(uint8_t bytes)
    {
        switch (bytes)
        {
        case 4:
            return reinterpret_cast<void*>(&stub_args_4);
        case 12:
            return reinterpret_cast<void*>(&stub_args_12);
        case 16:
            return reinterpret_cast<void*>(&stub_args_16);
        case 20:
            return reinterpret_cast<void*>(&stub_args_20);
        case 24:
            return reinterpret_cast<void*>(&stub_args_24);
        case 8:
        default:
            return reinterpret_cast<void*>(&stub_args_8);
        }
    }
#endif // !_WIN64

    // D3DDDIARG_CREATERESOURCE's real field layout (Format/Pool/pSurfList/SurfCount/MipLevels/hResource/
    // Flags, plus D3DDDI_SURFACEINFO::Width/Height/Depth) is now RE-verified and modeled in d3d9_ddi.hpp,
    // and umd_CreateResource reads width/height/depth/mip/pool/usage from it instead of the old hardcoded
    // 640x480 shape. What REMAINS a limitation: `kind` is still forced to texture_2d (cube/volume from
    // SurfCount>1 is a separate future task), and D3DDDI_SURFACEINFO's pSysMem/pitch fields are inferred,
    // not live-confirmed (see d3d9_ddi.hpp) -- neither is on this milestone's D3DPOOL_DEFAULT path.
    //
    // Offset 48 for hResource (x64) was found by writing a distinct, identifiable sentinel to every
    // 8-byte-aligned offset (0..80) and observing which one came back unchanged in the very next
    // SetRenderTarget call (0xAAAA000000000030, i.e. offset 0x30 = 48) -- direct proof, not inference
    // from a static hex dump (two earlier single-offset guesses, 40 and 44, were each individually
    // plausible-looking but empirically wrong).
    //
    // x86 note: the x64 offset does NOT carry over unchanged -- see umd_CreateResource's own comment
    // for the live sentinel-scan RE that found the real x86 offset is 44, not 48.
    //
    // resolve_resource_id() also keeps a lazy-bind-at-first-use fallback (for any resource handle that
    // somehow reaches SetRenderTarget/Lock without going through CreateResource) -- harmless dead code
    // in the common case now that CreateResource populates the map directly.
    std::unordered_map<uint64_t, uint64_t> g_resource_ids;

    // Handles actually minted by a real pfnCreateResource call (see umd_CreateResource) -- kept
    // SEPARATE from g_resource_ids/resolve_buffer_resource_id's lazy-bind cache rather than merged into
    // it, because the two live in genuinely different, independently-numbered handle spaces that CAN
    // collide: pfnCreateResource's own resource ids (this host's sequential allocate_id() counter,
    // echoed back as the app's handle) vs. the runtime's own small-integer internal handles for
    // vertex/index buffers (which never call pfnCreateResource at all). A real collision was hit live
    // 2026-07-04 merging these into one map: an internal-use pfnCreateResource call (format=100,
    // D3DFMT_VERTEXDATA, fired automatically at device/resource creation for reasons unrelated to any
    // guest-visible resource) happened to mint the exact numeric id a guest vertex buffer's own,
    // unrelated runtime handle later collided with, so resolve_buffer_resource_id's cache hit on that
    // id and silently handed the vertex buffer's Lock() an unrelated, empty resource instead of
    // creating its own correctly-sized one.
    std::unordered_map<uint64_t, uint64_t> g_created_resource_ids;

    // pfnCreateResource's real width/height/usage/pool offsets are still not RE'd (see the KNOWN
    // LIMITATION below), but its Format field (offset 0) IS confirmed, for every resource kind that
    // reaches this call at all (render targets, depth-stencil surfaces, and plain textures alike).
    // Classifying the *usage* purely from the requested Format is a real, evidence-backed heuristic
    // for this bring-up milestone's fixed set of formats, not a guess: D3DFMT_D24S8/D24X8 (75/77) only
    // ever mean a depth-stencil surface; D3DFMT_X8R8G8B8 (22) is this UMD's own only advertised
    // display/render-target format (g_formats); everything else this milestone creates (currently just
    // D3DFMT_A8R8G8B8, 21, for real sampled textures -- see d3d9_texture_test.cpp) is a plain texture
    // with no RT/DS usage bit. This is what makes a genuinely NEW CreateTexture()-backed sampled
    // texture (as opposed to Task 2's host-side-only, never-exercised-via-a-real-guest-call plumbing)
    // land in d3d9_host::create_resource's is_texture branch instead of its is_render_target one.
    uint32_t classify_resource_usage(uint32_t format)
    {
        if (format == 75 || format == 77) // D3DFMT_D24S8 / D3DFMT_D24X8
        {
            return 0x2; // D3DUSAGE_DEPTHSTENCIL
        }
        if (format == 22) // D3DFMT_X8R8G8B8
        {
            return 0x1; // D3DUSAGE_RENDERTARGET
        }
        return 0; // plain texture, no RT/DS usage bit
    }

    // Translate D3DDDIARG_CREATERESOURCE::Flags (a D3DDDI_RESOURCEFLAGS bitfield) into the D3DUSAGE_* bits
    // the host's create_resource actually tests. The host only inspects RENDERTARGET (0x1) and
    // DEPTHSTENCIL (0x2); D3DDDI_RESOURCEFLAGS carries the matching intent in its RenderTarget (bit 0) and
    // ZBuffer (bit 1) flags, which happen to sit at the same bit positions -- but map them explicitly
    // rather than pass the raw flags word and rely on that coincidence (the two bitfields diverge above
    // bit 1). This replaces the Format-based classify_resource_usage heuristic for real resources; that
    // heuristic is kept only as the fallback for the internal-use synthetic buffer formats below, whose
    // Flags carry VertexBuffer/IndexBuffer intent, not RT/DS.
    uint32_t resource_flags_to_usage(uint32_t flags)
    {
        constexpr uint32_t k_resflag_render_target = 0x1; // D3DDDI_RESOURCEFLAGS.RenderTarget
        constexpr uint32_t k_resflag_zbuffer = 0x2;       // D3DDDI_RESOURCEFLAGS.ZBuffer
        uint32_t usage = 0;
        if ((flags & k_resflag_render_target) != 0)
        {
            usage |= 0x1; // D3DUSAGE_RENDERTARGET
        }
        if ((flags & k_resflag_zbuffer) != 0)
        {
            usage |= 0x2; // D3DUSAGE_DEPTHSTENCIL
        }
        return usage;
    }

    HRESULT APIENTRY umd_CreateResource(HANDLE /*hDevice*/, void* pArgs)
    {
        auto* bytes = reinterpret_cast<unsigned char*>(pArgs);
        const auto* args = reinterpret_cast<const D3DDDIARG_CREATERESOURCE*>(pArgs);
        const uint32_t format = args->Format;

        // width/height/mip/pool are now READ FROM THE REAL, RE'd struct (see D3DDDIARG_CREATERESOURCE in
        // d3d9_ddi.hpp), not hardcoded to the old 640x480 shape. Dimensions live in the first
        // D3DDDI_SURFACEINFO element pSurfList points at (pSurfList is a guest pointer this in-guest UMD
        // dereferences directly, the same way it reads every other pArgs field). SurfCount is read too:
        // it is the count of that array (>1 for cube faces / volume slices) -- captured here for a future
        // cube/volume task; this milestone still creates every resource as a single-subresource
        // texture_2d (kind below is unconditional), so SurfCount only guards the pSurfList[0] read for now.
        const uint32_t surf_count = args->SurfCount;
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t depth = 1;
        // pSurfList/SurfCount can genuinely be null/0 here: this same function also handles the
        // internal-use synthetic buffer formats (100/101/102, see is_internal_buffer_format below) --
        // vertex/index buffers have no D3DDDI_SURFACEINFO array at all, since they carry no width/height.
        // Falling through with width=height=0 is safe for that case: their create is orphaned regardless
        // (never registered in g_created_resource_ids, see the format!=100/101/102 guard below), so a
        // width/height of 0 is never acted on host-side.
        if (args->pSurfList != nullptr && surf_count > 0)
        {
            const D3DDDI_SURFACEINFO& surf0 = args->pSurfList[0];
            width = surf0.Width;
            height = surf0.Height;
            depth = surf0.Depth != 0 ? surf0.Depth : 1;
        }

        // Usage comes from the real Flags field for genuine resources (see resource_flags_to_usage); the
        // internal-use synthetic buffer formats (100/101/102, D3DFMT_VERTEXDATA/INDEX16/INDEX32) do not
        // carry meaningful RT/DS resource flags, so keep the Format heuristic (returns 0) for those. Their
        // create is orphaned regardless (see the format!=100/101/102 guard on registration below).
        const bool is_internal_buffer_format = format == 100 || format == 101 || format == 102;
        const uint32_t usage = is_internal_buffer_format ? classify_resource_usage(format) : resource_flags_to_usage(args->Flags);

        // KNOWN LIMITATION: `kind` is still forced to texture_2d unconditionally -- cube/volume
        // dimensionality is now CAPTURED (SurfCount above) but not yet acted on (creating a real
        // texture_cube/texture_volume host resource from SurfCount>1 is deliberately a separate future
        // task). width/height/mip_levels/pool are no longer hardcoded; they are the app's real values.
        const d3d9c::create_resource_request req{
            .kind = static_cast<uint32_t>(d3d9c::resource_kind::texture_2d),
            .format = format,
            .width = width,
            .height = height,
            .depth = depth,
            .mip_levels = args->MipLevels,
            .usage = usage,
            .pool = args->Pool,
        };
        d3d9c::create_resource_response resp{};
        bridge_call(gb::ioctl_d3d9_create_resource, &req, sizeof(req), &resp, sizeof(resp));
        // Output-handle offset: 48 on x64 (see the KNOWN LIMITATION comment above this function's own
        // enclosing comment block). RE-verified live to be 44 on x86 (Task 3b, 2026-07-04, real 32-bit
        // d3d9.dll via WoW64): umd_CreateResource temporarily wrote a distinct, offset-encoding
        // sentinel (0xAAAA0000 | offset) to every 4-byte-aligned offset 0..48 for a real
        // D3DFMT_A8R8G8B8 texture create (d3d9_texture_test.cpp's CreateTexture), instead of the real
        // resource id; the very next Lock() call's D3DDDIARG_LOCK::hResource (independently RE-
        // verified at offset 0 on x86 -- see d3d9_ddi.hpp) came back exactly 0xAAAA002C, i.e. offset
        // 0x2C = 44 -- unambiguous, since every scanned offset carries a distinct value. Reproduced
        // identically (same resource id, same 0xAAAA002C readback) across two independent runs.
        // Consistent with the same single-pointer-field-shrinks-by-4 shift already confirmed for every
        // other x86 DDI struct fixed so far (SETSTREAMSOURCE/SETINDICES/SETRENDERTARGET/
        // SETDEPTHSTENCIL/CREATESHADERFUNC all shift by exactly 4 for exactly one preceding pointer
        // field: 48-4=44) -- not the wholesale field-reordering D3DDDIARG_LOCK's x86 layout turned out
        // to need (a single clean shift fully explains this finding; no second, conflicting offset was
        // ever observed).
#ifdef _WIN64
        constexpr size_t k_create_resource_hresource_offset = 48;
#else
        constexpr size_t k_create_resource_hresource_offset = 44;
#endif
        if (resp.hr == 0)
        {
            std::memcpy(bytes + k_create_resource_hresource_offset, &resp.resource, sizeof(resp.resource));
            // The runtime echoes this back unchanged as the handle in later calls (SetRenderTarget's
            // hRenderTarget, Lock's hResource, SetTexture's hTexture, ...), so the numeric handle value
            // IS the wire resource_id from here on. Recorded in g_created_resource_ids (a SEPARATE map
            // from g_resource_ids -- see its own comment on why they must not be merged) so umd_Lock
            // can recognize "this handle already names a real, correctly-shaped resource" instead of
            // treating it as an unregistered buffer handle needing lazy-bind. Without this, Lock()/
            // Unlock() on any real pfnCreateResource-backed resource (confirmed for plain textures,
            // live 2026-07-04) silently mints a second, wrong-kind/wrong-shape resource that
            // SetTexture/SetRenderTarget's own (unresolved, direct) handle never references -- the
            // app's real pixel writes land in a resource nothing else ever reads, and the texture stays
            // permanently all-zero. Coincidentally harmless for render targets so far (the lazy-bind
            // fallback's hardcoded 640x480 X8R8G8B8 RENDERTARGET shape happens to match every existing
            // test's own render target), but a real, general bug.
            //
            // EXCEPT for D3DFMT_VERTEXDATA (100) and D3DFMT_INDEX16/INDEX32 (101/102): live-confirmed
            // 2026-07-04 that pfnCreateResource DOES fire for vertex/index buffer objects too, with
            // these internal-only format values -- this was previously missed entirely (the "buffers
            // never call pfnCreateResource" finding this whole file's comments repeat was true for
            // every OTHER format, just not these three). This function's own hardcoded kind/width/
            // height/format shape (a 640x480 texture_2d) is wrong for these -- registering them in
            // g_created_resource_ids would make Lock() use that wrong, zero-backing (none of these
            // formats are in d3d9_format_to_vulkan's table) resource instead of
            // resolve_buffer_resource_id's own correctly-shaped, correctly-sized vertex/index buffer
            // lazy-bind. Skip the registration for these formats so those handles keep falling through
            // to that existing, already-correct path -- the resource created above for them is orphaned
            // (harmless) rather than referenced again.
            if (format != 100 && format != 101 && format != 102)
            {
                g_created_resource_ids[resp.resource] = resp.resource;
            }
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

        // Check the real pfnCreateResource registry first (see g_created_resource_ids' own comment on
        // why this must stay a separate map from g_resource_ids) -- a handle that already names a real,
        // correctly-shaped resource must never fall into this function's own texture-shaped lazy-bind.
        const auto created_it = g_created_resource_ids.find(raw);
        if (created_it != g_created_resource_ids.end())
        {
            return created_it->second;
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

    // KNOWN LIMITATION, resolved 2026-07-04 (see HANDOFF_MACBOOK.md for the full RE trail): a real
    // depth-stencil surface's DDI handle does NOT reach pfnSetDepthStencil as the same value
    // pfnCreateResource's Format/output-handle write registered (live-confirmed: pfnCreateResource
    // fires with Format=75/D3DFMT_D24S8 for CreateDepthStencilSurface, but pfnSetDepthStencil's
    // hZBuffer -- itself only invoked once a real Clear()/draw actually references the bound Z-buffer,
    // via the same worker-thread DP2-batch deferral documented elsewhere in this file -- carries a
    // small, unrelated runtime-internal handle instead, exactly like vertex/index buffer handles never
    // reaching pfnCreateResource at all). resolve_resource_id's generic lazy-bind fallback (640x480
    // X8R8G8B8 RENDERTARGET) is therefore wrong for this handle in exactly the way Task 5's KNOWN
    // LIMITATION comment predicted: a depth-stencil surface would silently get a color-shaped resource.
    // Fixed the same way umd_Lock's buffer handles are: a dedicated lazy-bind that mints the CORRECT
    // shape (D3DFMT_D24S8 format + D3DUSAGE_DEPTHSTENCIL usage) for this one DDI call site, which is
    // architecturally guaranteed to only ever be used for depth-stencil surfaces -- no full
    // D3DDDIARG_CREATERESOURCE width/height/usage/pool RE needed for this specific, narrow case.
    uint64_t resolve_depth_stencil_resource_id(void* handle)
    {
        const auto raw = reinterpret_cast<uint64_t>(handle);
        if (raw == 0)
        {
            return 0;
        }

        const auto created_it = g_created_resource_ids.find(raw);
        if (created_it != g_created_resource_ids.end())
        {
            return created_it->second;
        }

        const auto it = g_resource_ids.find(raw);
        if (it != g_resource_ids.end())
        {
            return it->second;
        }

        const d3d9c::create_resource_request req{
            .kind = static_cast<uint32_t>(d3d9c::resource_kind::texture_2d),
            .format = 75, // D3DFMT_D24S8 -- matches this UMD's own advertised depth-stencil format.
            .width = 640,
            .height = 480,
            .depth = 1,
            .mip_levels = 1,
            .usage = 0x2, // D3DUSAGE_DEPTHSTENCIL (public, ABI-stable D3D9 constant, not RE'd)
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

    // Buffers (vertex/index) never call pfnCreateResource at all (RE-confirmed live), so their DDI
    // handle -- a small runtime-internal number, live-observed to collide with resolve_resource_id's
    // own sequential ids -- reaches pfnLock completely unregistered. pfnLock is the right place to
    // lazily register a correctly-kinded resource instead of resolve_resource_id's texture-shaped
    // fallback, which previously made every never-seen Lock() land on a wrong-kind 640x480 texture.
    // byte_size is umd_Lock's real OffsetToLock (Task 6, 2026-07-04) when known, used only as a lower
    // bound -- there is still no reliable SizeToLock in any routing path (see umd_Lock's own comment),
    // so the fallback floor below stays the effective size for the common (offset-0 first lock) case.
    uint64_t resolve_buffer_resource_id(void* handle, uint32_t byte_size)
    {
        const auto raw = reinterpret_cast<uint64_t>(handle);
        if (raw == 0)
        {
            return 0;
        }

        const auto created_it = g_created_resource_ids.find(raw);
        if (created_it != g_created_resource_ids.end())
        {
            return created_it->second;
        }

        const auto it = g_resource_ids.find(raw);
        if (it != g_resource_ids.end())
        {
            return it->second;
        }

        const d3d9c::create_resource_request req{
            .kind = static_cast<uint32_t>(d3d9c::resource_kind::vertex_buffer),
            .format = 0,
            .width = std::max<uint32_t>(byte_size, 64 * 1024), // no real total size is knowable here,
                                                                // so guess a floor and grow it if a
                                                                // known offset needs more than that.
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
        caps->VertexShaderVersion = D3DVS_VERSION(2, 0);
        caps->PixelShaderVersion = D3DPS_VERSION(2, 0);
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
        // DevCaps bit 0x04000000 (also undocumented, same reuse pattern as 0x02000000 above) is the
        // analogous gate in CIndexBuffer::Create's OWN routing logic (a genuinely separate check on the
        // same DevCaps DWORD, not shared code with CVertexBuffer::Create) -- confirmed live 2026-07-04
        // by hooking CreateDriverIndexBuffer/CreateDriverManagedIndexBuffer/CreateSysmemIndexBuffer
        // directly: without this bit, EVERY index buffer routes through CreateSysmemIndexBuffer
        // regardless of requested pool, and its Lock() (CIndexBuffer::Lock's own direct dispatch) never
        // updates the app-visible Data() pointer from anything this driver returns -- pfnLock/pfnUnlock
        // are still invoked (confirmed: hr=S_OK every time) but purely as vestigial bookkeeping, so an
        // index buffer's Lock()/Unlock() round-trips only through the runtime's own pre-allocated
        // system-memory shadow, never reaching this driver at all. This is the real, confirmed root
        // cause of the "index buffer Lock data never reaches the host" finding -- not a struct-offset
        // bug (D3DDDIARG_LOCK's fields are read the exact same way for every resource kind; see its own
        // comment for the separate, secondary struct-shape issue this pass also found and fixed). With
        // this bit set, CreateDriverIndexBuffer is used instead and pfnLock/pfnUnlock's real return
        // values genuinely reach the app.
        constexpr DWORD k_devcaps_driver_managed_index_pool = 0x04000000;
        caps->DevCaps = D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_HWRASTERIZATION | D3DDEVCAPS_PUREDEVICE |
                        D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_TEXTUREVIDEOMEMORY | k_devcaps_driver_managed_pool |
                        k_devcaps_driver_managed_index_pool;
        // PrimitiveMiscCaps bit 0x2000 has no name in the public D3DPMISCCAPS_* set (the defined bits jump
        // from D3DPMISCCAPS_NULLREFERENCE=0x1000 straight past it to D3DPMISCCAPS_INDEPENDENTWRITEMASKS=
        // 0x4000) -- found via objdump disassembly of d3d9.dll's VS/PS-2.0+ HAL-enable validator (the
        // function that GetCaps type=13's own buffer feeds straight back into, confirmed live via sogen's
        // Python debugger API watching reads of D3DCAPS9::VertexShaderVersion): once VertexShaderVersion >=
        // D3DVS_VERSION(2,0), the validator requires this bit set (`test [caps+0x20],0x2000; je <fail>`)
        // plus D3DPMISCCAPS_MASKZ, alongside the already-public bits below (`and eax,0x2882; cmp
        // eax,0x2882; jne <fail>` -- 0x2882 == MASKZ|COLORWRITEENABLE|BLENDOP|this bit). Same undocumented-
        // internal-reuse pattern as the DevCaps/DevCaps2 gates above.
        constexpr DWORD k_primitivemisc_vs20_gate = 0x00002000;
        caps->PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW |
                                  D3DPMISCCAPS_CULLCCW | D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_BLENDOP |
                                  D3DPMISCCAPS_SEPARATEALPHABLEND | k_primitivemisc_vs20_gate;
        // The same validator also requires D3DPRASTERCAPS_FOGVERTEX (0x80, bit 7 of RasterCaps) once
        // VertexShaderVersion >= 2.0, via objdump on the same function.
        caps->RasterCaps = D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_SCISSORTEST |
                           D3DPRASTERCAPS_DEPTHBIAS | D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS |
                           D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ANISOTROPY;
        caps->ZCmpCaps = 0xFF;
        // The validator also requires D3DPBLENDCAPS_BLENDFACTOR (0x2000, a real documented bit) set in both
        // Src/DestBlendCaps once VertexShaderVersion >= 2.0 (`and eax,0x3fff/0x23ff; cmp; jne <fail>`).
        caps->SrcBlendCaps = 0x1FFF | D3DPBLENDCAPS_BLENDFACTOR;
        caps->DestBlendCaps = 0x1FFF | D3DPBLENDCAPS_BLENDFACTOR;
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
        // The VS2.0+ HAL-enable validator also requires |GuardBand{Left,Top,Right,Bottom}| >= 8192.0 (the
        // exact float constant it compares against, read from d3d9.dll's own .rdata via objdump); these
        // were previously left at the memset-to-0 default, which fails that check once VS/PS report 2.0.
        caps->GuardBandLeft = -8192.0f;
        caps->GuardBandTop = -8192.0f;
        caps->GuardBandRight = 8192.0f;
        caps->GuardBandBottom = 8192.0f;
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
        // With VS/PS reported as SM2.0 (below vs_3_0/ps_3_0), d3d9's aggregate HAL validator rejects the
        // adapter unless the SM3.0 instruction-slot caps are 0.
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
        {21 /*A8R8G8B8*/, FMT_OP_TEXTURE, 0, 0, 0}, // real sampled textures (d3d9_texture_test.cpp)
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
    // Resource/shader creation, Lock/Unlock, and Present are all wired and RE-verified (see the
    // pfnCreateResource/pfnLock/pfnPresent/pfnCreateVertexShaderFunc/pfnCreatePixelShader functions
    // below). Everything below is the higher-confidence, higher-frequency per-draw state path.
    // ---------------------------------------------------------------------------------------------

    HRESULT APIENTRY umd_SetRenderState(HANDLE /*hDevice*/, CONST D3DDDIARG_RENDERSTATE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::set_render_state_record req{.state = pArgs->State, .value = pArgs->Value};
        record_d3d9(gb::command::d3d9_set_render_state, &req, sizeof(req));
        return S_OK;
    }

    // RE-verified live (M2 Task 3): D3D9's real WDDM DDI has no separate pfnSetSamplerState slot at all
    // (confirmed by re-reading D3DDDI_DEVICEFUNCS's declared members) -- sampler state reaches the driver
    // through this same pfnSetTextureStageState call, via extra D3DDDITEXTURESTAGESTATETYPE values that
    // have no counterpart in the public D3DTEXTURESTAGESTATETYPE enum. The two are told apart purely by
    // which State value arrives, not by a numeric threshold: captured live via a real guest
    // SetSamplerState()-driven test against the actual staged d3d9.dll, both from the runtime's own
    // per-sampler default-initialization sequence (State=13,14,25,15,16,17,18,19,20,21,29,31,30 emitted
    // for every one of the 16 real samplers, Stage/Sampler 0-15 with no offset) and from explicit
    // non-default SetSamplerState() calls that changed a cached value and so weren't optimized away:
    // ADDRESSU(13)->CLAMP=3, MAGFILTER(16)->LINEAR=2, MINFILTER(17)->LINEAR=2 (sampler 2, confirming the
    // Stage field carries the sampler index unmodified), ADDRESSV(14)->MIRROR=2 (sampler 3). See
    // HANDOFF_MACBOOK.md for the full capture.
    uint32_t sampler_state_for_ddi_tss_state(UINT ddi_state)
    {
        switch (ddi_state)
        {
        case 13: return D3DSAMP_ADDRESSU;
        case 14: return D3DSAMP_ADDRESSV;
        case 25: return D3DSAMP_ADDRESSW;
        case 15: return D3DSAMP_BORDERCOLOR;
        case 16: return D3DSAMP_MAGFILTER;
        case 17: return D3DSAMP_MINFILTER;
        case 18: return D3DSAMP_MIPFILTER;
        case 19: return D3DSAMP_MIPMAPLODBIAS;
        case 20: return D3DSAMP_MAXMIPLEVEL;
        case 21: return D3DSAMP_MAXANISOTROPY;
        // Confirmed sampler-shaped (present in every sampler's default-init sequence, distinct from any
        // TSS default) but never individually round-tripped through an explicit non-default
        // SetSamplerState() call, so their real D3DSAMPLERSTATETYPE identity (candidates: SRGBTEXTURE/
        // ELEMENTINDEX/DMAPOFFSET, D3DSAMP 11-13) isn't confirmed. Still routed to the sampler bucket
        // (the correct category) under reserved values outside D3DSAMPLERSTATETYPE's real range, instead
        // of guessing a specific identity or silently misfiling them as texture-stage-state.
        case 29: return 1029;
        case 30: return 1030;
        case 31: return 1031;
        default: return 0; // a genuine D3DTEXTURESTAGESTATETYPE value; 0 is never a real D3DSAMPLERSTATETYPE
        }
    }

    HRESULT APIENTRY umd_SetTextureStageState(HANDLE /*hDevice*/, CONST D3DDDIARG_TEXTURESTAGESTATE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        if (const uint32_t sampler_state = sampler_state_for_ddi_tss_state(pArgs->State); sampler_state != 0)
        {
            d3d9c::set_sampler_state_record req{
                .sampler = pArgs->Stage, .state = sampler_state, .value = pArgs->Value, .reserved = 0};
            record_d3d9(gb::command::d3d9_set_sampler_state, &req, sizeof(req));
            return S_OK;
        }
        d3d9c::set_texture_stage_state_record req{.stage = pArgs->Stage, .state = pArgs->State, .value = pArgs->Value, .reserved = 0};
        record_d3d9(gb::command::d3d9_set_texture_stage_state, &req, sizeof(req));
        return S_OK;
    }

    // D3DPOOL_MANAGED fix (see docs/d3d9-roadmap.md and HANDOFF_MACBOOK.md for the full live-RE
    // trail): a D3DPOOL_MANAGED texture's single CreateTexture() call makes the real d3d9.dll issue
    // pfnCreateResource TWICE -- once immediately (a lightweight "sysmem master" copy, the one
    // LockRect/UnlockRect actually read/write) and again lazily, on first bind, for a second "vidmem"
    // copy (the one SetTexture forwards to the driver for sampling). This is genuine, expected D3D9
    // MANAGED-pool architecture, not a bug in the double-create itself -- what WAS missing is the sync
    // step: real d3d9.dll issues a pfnTexBlt call between the second pfnCreateResource and the
    // following pfnSetTexture to copy the sysmem master's pixels into the new vidmem copy, and this
    // driver's pfnTexBlt slot was an unwired no-op stub, so the vidmem copy stayed permanently empty
    // (black/transparent when sampled).
    //
    // Live-RE trail: instrumenting every device-func-table slot with its own labeled stub (temporary,
    // reverted) and running a real D3DPOOL_MANAGED CreateTexture()/LockRect()/SetTexture() sequence
    // showed slot 18 (pfnTexBlt) fires exactly once, between the second pfnCreateResource and
    // pfnSetTexture, with no other unimplemented slot invoked in between. Dumping pfnTexBlt's raw
    // D3DDDIARG_TEXBLT argument bytes at that call site gave {q0=hDstResource, q1=hSrcResource, ...},
    // exactly matching the vidmem copy's handle (the one SetTexture went on to bind) and the sysmem
    // copy's handle (the one Lock had just written into).
    //
    // Fix: forward {hDstResource, hSrcResource} (offsets 0/8, the same direct resource-id convention
    // every other real DDI call in this file already uses) to the host, which copies the source
    // resource's entire pixel backing into the destination resource's -- ensure_texture_uploaded
    // already re-uploads a texture's backing to its GPU image unconditionally on every draw, so no
    // further dirty-tracking is needed for the copied data to reach the sampler.
    //
    // KNOWN LIMITATION -- found live-RE'ing this fix (2026-07-04), root-caused two layers deep
    // (2026-07-04), and FIXED on x64 by a mechanism outside this driver's own DDI surface entirely
    // (2026-07-05): this TexBlt sync alone does NOT make a real D3DPOOL_MANAGED texture sample
    // correctly by itself, because a SECOND, decoupled bug sits upstream of this one -- pfnLock/pfnUnlock
    // never carry the app's real pixel writes for the "sysmem master" copy in the first place, UNLESS
    // the routing below is forced onto the driver-managed path. Live-verified: this driver's own pfnLock
    // returns pArgs->pData correctly, but the app's IDirect3DTexture9::LockRect() hands back a DIFFERENT
    // pointer (confirmed by comparing the two addresses directly) whenever the gate below is closed, so
    // the app writes into d3d9.dll's own CMipMap-owned system-memory allocation (CMipMap::CMipMap calls
    // MallocAligned/LocalAlloc for exactly this) and pfnUnlock forwards zeros. Root cause:
    // CBaseTexture::CanCreateLightWeight requires CBaseDevice::CanDriverManageResource --
    // `(*(this+120) & 0x100) == 0 && (*(this+444) & 0x10000000) != 0` -- to be true before the runtime
    // will let CMipMap share ONE real driver resource and route Lock/Unlock through it. Traced live
    // (memory-write watch on `this+444`) all the way back to `d3d9.dll`'s own `QueryLHDDICaps`, which
    // unconditionally clears `D3DCAPS2_CANMANAGERESOURCE` (`& 0xEFFFFFFF`) after querying the driver,
    // regardless of what `GetCaps` reports -- empirically re-verified by temporarily setting the bit in
    // `fill_d3d9caps` and watching it get stripped again, live, on the very next write. This part remains
    // true and permanent: no `D3DCAPS9` field any real D3DDDI/WDDM driver reports can make this gate pass
    // through the reported-caps mechanism (see HANDOFF_MACBOOK.md's §18 for the full trace).
    //
    // Task 4c (2026-07-04) then asked the necessary follow-up: does pfnTexBlt's REAL argument struct
    // carry a sysmem-source pixel pointer that could bypass the broken Lock/Unlock path entirely? A live
    // trace captured pfnTexBlt's return address into d3d9.dll and idasql-decompiled the real caller,
    // CD3DDDIDX10::TexBlt (d3d9.dll+0x180031255) -- see D3DDDIARG_TEXBLT in d3d9_ddi.hpp for the full
    // 48-byte field-by-field breakdown this decompile produced. The struct carries exactly: two resource
    // handles, a subresource-derived index (always 0, single-mip textures), a destination point, a
    // source rect, and a reserved dword (always 0) -- NO pixel-data pointer anywhere, confirmed against
    // the decompiled source of the real function that builds it, not just an empirical byte dump. A
    // second, independent live trace instrumented every one of this driver's 143 device-func-table slots
    // for the ENTIRE `d3d9_managed_texture_test.cpp` run (not just the narrow CreateResource..SetTexture
    // window already known) and confirmed every other call this driver receives is metadata/state only
    // (pfnSetClipPlane, pfnUpdateWInfo, pfnCreateVertexShaderDecl, pfnDestroyResource, and the
    // already-implemented real slots) -- none of them carry texture pixel bytes either. Conclusion: the
    // real MANAGED-pool sysmem pixel data is structurally never exposed to this (or any) D3DDDI/WDDM
    // driver through ANY DDI call for this resource kind -- `d3d9.dll` keeps it entirely inside its own
    // private `CMipMap` buffer end to end, AS LONG AS the driver-managed gate stays closed. Through the
    // DDI surface alone (reported caps, or a TexBlt argument change), this remains permanently unfixable.
    //
    // FIX (2026-07-05, x64 only): `windows_emulator::install_d3d9_caps_patch_hook` (windows_emulator.cpp)
    // installs a permanent runtime memory-execution hook on x64 d3d9.dll load that re-sets
    // `D3DCAPS2_CANMANAGERESOURCE` immediately after `QueryLHDDICaps`'s own strip -- a patch to d3d9.dll's
    // in-memory *behavior*, not a DDI-surface or reported-caps change, so the "no D3DCAPS9 field survives
    // the strip" conclusion above is unaffected and still correct. With the gate forced open this way,
    // `d3d9.dll` routes the sysmem/vidmem MANAGED-pool lock through the driver-managed path instead of its
    // own private CMipMap copy, and this driver's existing `umd_Lock`/`g_locked_buffers` machinery --
    // originally built for ordinary, non-MANAGED resources, unchanged for this fix -- turned out to
    // already serve a real pixel backing once that path is taken; no new UMD code was needed.
    // `d3d9_managed_texture_test.cpp` now genuinely passes (magenta, not black). Independently verified
    // via A/B (disabling the hook reproduces the old black-pixel failure; re-enabling it restores the
    // pass) plus a full x64/x86 guest-test regression sweep. x86/WoW64 scope: this hook is x64-only -- the
    // RVA pattern was verified only against the staged 64-bit system32/d3d9.dll build; the 32-bit
    // syswow64/d3d9.dll real MW2 (a 32-bit game) would actually use has not had an equivalent RE pass, so
    // this fix does not yet help a 32-bit guest -- separately-scoped follow-up work. Full trail:
    // docs/d3d9-roadmap.md's D3DPOOL_MANAGED entries, HANDOFF_MACBOOK.md.
    HRESULT APIENTRY umd_TexBlt(HANDLE /*hDevice*/, CONST D3DDDIARG_TEXBLT* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::tex_blt_request req{.dst_resource = reinterpret_cast<uint64_t>(pArgs->hDstResource),
                                   .src_resource = reinterpret_cast<uint64_t>(pArgs->hSrcResource)};
        bridge_call(gb::ioctl_d3d9_tex_blt, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetTexture(HANDLE /*hDevice*/, UINT Stage, HANDLE hTexture)
    {
        // RE-verified live: pfnSetTexture takes Stage/hTexture as direct value arguments, not a
        // pointer to D3DDDIARG_SETTEXTURE -- a struct-pointer read crashed with the small Stage
        // integer (e.g. 0x1) dereferenced as an address. 0 for hTexture means unbind.
        d3d9c::set_texture_record req{.stage = Stage, .reserved = 0, .texture = reinterpret_cast<uint64_t>(hTexture)};
        record_d3d9(gb::command::d3d9_set_texture, &req, sizeof(req));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetPixelShader(HANDLE /*hDevice*/, HANDLE hShader)
    {
        // RE-verified live (crash-driven, same pattern as pfnSetTexture in §10.6): pfnSetPixelShader
        // takes the shader handle as a direct value argument, not a pointer to
        // D3DDDIARG_SETPIXELSHADERFUNC -- once a real (non-null) driver shader handle started flowing
        // through (after the create_shader_common ShaderHandle-offset fix), a struct-pointer read
        // crashed dereferencing the small handle value (e.g. 0xB) as an address. 0 means unbind.
        d3d9c::set_pixel_shader_record req{.shader = reinterpret_cast<uint64_t>(hShader)};
        record_d3d9(gb::command::d3d9_set_pixel_shader, &req, sizeof(req));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetVertexShaderFunc(HANDLE /*hDevice*/, HANDLE hShader)
    {
        // Same direct-value convention as umd_SetPixelShader above (not D3DDDIARG_SETVERTEXSHADERFUNC*).
        // 0 means "no vertex shader / use fixed-function", e.g. when a D3DFVF_XYZRHW draw follows a
        // shader-bound one.
        d3d9c::set_vertex_shader_record req{.shader = reinterpret_cast<uint64_t>(hShader)};
        record_d3d9(gb::command::d3d9_set_vertex_shader, &req, sizeof(req));
        return S_OK;
    }

    // RE-verified live (real d3d9.dll, CD3DDDIDX10TL::CreateVertexShaderFunc / CD3DDDIDX10::
    // CreatePixelShader, via sogen's own Python debugger API breakpointed directly on the device-func-
    // table call instruction): pfnCreateVertexShaderFunc/pfnCreatePixelShader ARE struct-pointer DDI
    // calls, `(HANDLE hDevice, D3DDDIARG_CREATESHADERFUNC* pArgs, CONST UINT* pFunction)` -- an earlier
    // RE pass mis-identified this as a 3-direct-value-argument convention (no length, self-measured
    // token stream) matching pfnSetTexture; that was wrong; `pArgs->CodeSize` is the real, authoritative
    // byte length supplied by the runtime, and `pArgs->ShaderHandle` (offset 8, not offset 0) is where
    // the driver must write the resulting handle. The previous convention silently corrupted the
    // runtime's own shader-handle bookkeeping (wrote 8 bytes at pArgs+0, never touching the real
    // ShaderHandle slot at pArgs+8), which is what caused DrawPrimitive's internal shader-cache flush
    // (ff2ps::CConverterToPixelShader::PrepareToDraw / ff2vs::CConverterToVertexShader::PrepareToDraw)
    // to see a null cached shader and fail with E_OUTOFMEMORY on every draw, fixed-function or
    // programmable alike.
    HRESULT create_shader_common(const uint32_t* pFunction, D3DDDIARG_CREATESHADERFUNC* pArgs, uint32_t opcode)
    {
        if (pFunction == nullptr || pArgs == nullptr)
        {
            return E_INVALIDARG;
        }
        const UINT token_size_bytes = pArgs->CodeSize;

        std::vector<uint8_t> buf(sizeof(d3d9c::create_shader_request) + token_size_bytes);
        auto* req = reinterpret_cast<d3d9c::create_shader_request*>(buf.data());
        req->token_size_bytes = token_size_bytes;
        req->reserved = 0;
        std::memcpy(buf.data() + sizeof(*req), pFunction, token_size_bytes);

        d3d9c::create_shader_response resp{};
        bridge_call(opcode, buf.data(), static_cast<DWORD>(buf.size()), &resp, sizeof(resp));
        pArgs->ShaderHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(resp.shader));
        return resp.hr;
    }

    HRESULT APIENTRY umd_CreateVertexShaderFunc(HANDLE /*hDevice*/, D3DDDIARG_CREATESHADERFUNC* pArgs, CONST UINT* pFunction)
    {
        return create_shader_common(pFunction, pArgs, gb::ioctl_d3d9_create_vertex_shader);
    }

    HRESULT APIENTRY umd_DeleteVertexShaderFunc(HANDLE /*hDevice*/, CONST D3DDDIARG_DELETEVERTEXSHADERFUNC* /*pArgs*/)
    {
        // No host-side shader-destroy wire call exists yet; adding one is out of scope for this task
        // (same asymmetry precedent as elsewhere in this file where not every Create has a wired Delete).
        return S_OK;
    }

    HRESULT APIENTRY umd_CreatePixelShader(HANDLE /*hDevice*/, D3DDDIARG_CREATESHADERFUNC* pArgs, CONST UINT* pFunction)
    {
        return create_shader_common(pFunction, pArgs, gb::ioctl_d3d9_create_pixel_shader);
    }

    HRESULT APIENTRY umd_DeletePixelShader(HANDLE /*hDevice*/, CONST D3DDDIARG_DELETEPIXELSHADERFUNC* /*pArgs*/)
    {
        return S_OK;
    }

    // pfnCreateVertexShaderDecl was, until this Task-9 test needed a real end-to-end multi-stream
    // declaration path, still an unwired device_stub (see this file's own arity-table comment on the
    // slot, pre-fix) -- CreateVertexDeclaration() therefore never reached the host at all, silently
    // leaving Tasks 6-8's already-built host-side parse_vertex_decl/multi-stream-binding plumbing
    // permanently unreachable by any real D3D9 app. Wired following the exact same struct-pointer-plus-
    // separate-trailing-array convention this driver's own umd_CreateVertexShaderFunc/umd_CreatePixelShader
    // (create_shader_common) already use and already got live-RE-verified for -- the WDK's own
    // PFND3DDDI_CREATEVERTEXSHADERDECL prototype takes pVertexElements as this DDI's own third argument,
    // matching that shape. ShaderHandle (D3DDDIARG_CREATEVERTEXSHADERDECL's second field, at offset 8 --
    // NumVertexElements is first, see that struct's own comment) is the driver's output slot, same
    // convention as D3DDDIARG_CREATESHADERFUNC::ShaderHandle.
    HRESULT APIENTRY umd_CreateVertexShaderDecl(HANDLE /*hDevice*/, D3DDDIARG_CREATEVERTEXSHADERDECL* pArgs,
                                                CONST D3DDDIVERTEXELEMENT* pVertexElements)
    {
        if (pArgs == nullptr)
        {
            return E_INVALIDARG;
        }
        const UINT element_count = pArgs->NumVertexElements;
        const size_t elements_size_bytes = element_count * sizeof(d3d9c::vertex_element);

        std::vector<uint8_t> buf(sizeof(d3d9c::create_vertex_decl_request) + elements_size_bytes);
        auto* req = reinterpret_cast<d3d9c::create_vertex_decl_request*>(buf.data());
        req->element_count = element_count;
        req->reserved = 0;
        if (element_count != 0 && pVertexElements != nullptr)
        {
            std::memcpy(buf.data() + sizeof(*req), pVertexElements, elements_size_bytes);
        }

        d3d9c::create_vertex_decl_response resp{};
        bridge_call(gb::ioctl_d3d9_create_vertex_decl, buf.data(), static_cast<DWORD>(buf.size()), &resp, sizeof(resp));
        pArgs->ShaderHandle = reinterpret_cast<HANDLE>(static_cast<uintptr_t>(resp.decl));
        return resp.hr;
    }

    // RE-corrected live (this Task-9 test, via a diagnostic dump of the incoming parameter): despite
    // the struct-pointer signature this function used before, pfnSetVertexShaderDecl is actually a
    // DIRECT-VALUE HANDLE call -- same convention as umd_SetVertexShaderFunc/umd_SetPixelShader, NOT a
    // pointer to D3DDDIARG_SETVERTEXSHADERDECL. Before this fix, a real, non-null decl handle (0x10007)
    // was observed arriving in the "pArgs" parameter slot itself (an implausibly small value for a real
    // heap/stack pointer, unlike every genuine pointer elsewhere in this driver's traces), and
    // `pArgs->ShaderHandle` dereferenced through it read back 0 -- silently forwarding decl=0 (i.e.
    // "no declaration") to the host on every real SetVertexDeclaration() call, permanently defeating
    // Tasks 6-8's multi-stream draw path even after pfnCreateVertexShaderDecl itself was fixed. The
    // previous "NULL pArgs crashes" observation (an earlier session, testing the fixed-function/no-decl
    // case) is still consistent with this: a null decl handle IS what the runtime passes there too --
    // it was never really a struct pointer being null, just the handle value 0. 0 continues to mean
    // "no decl / fixed-function", matching every other direct-value Set* slot's own convention.
    HRESULT APIENTRY umd_SetVertexShaderDecl(HANDLE /*hDevice*/, HANDLE hDecl)
    {
        d3d9c::set_vertex_decl_record req{.decl = reinterpret_cast<uint64_t>(hDecl)};
        record_d3d9(gb::command::d3d9_set_vertex_decl, &req, sizeof(req));
        return S_OK;
    }

    // pArgs is the fixed {Register, Count} header; the float data is a separate third DDI argument
    // (see D3DDDIARG_SETVERTEXSHADERCONST's header comment), not trailing bytes after pArgs.
    HRESULT APIENTRY umd_SetVertexShaderConst(HANDLE /*hDevice*/, CONST D3DDDIARG_SETVERTEXSHADERCONST* pArgs,
                                              CONST FLOAT* pRegisters)
    {
        if (pArgs == nullptr || pRegisters == nullptr)
        {
            return S_OK;
        }
        const size_t float_count = static_cast<size_t>(pArgs->Count) * 4;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_f_record) + float_count * sizeof(float));
        auto* req = reinterpret_cast<d3d9c::set_const_f_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->vector4_count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), pRegisters, float_count * sizeof(float));
        record_d3d9(gb::command::d3d9_set_vs_const_f, buf.data(), static_cast<uint32_t>(buf.size()));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetPixelShaderConst(HANDLE /*hDevice*/, CONST D3DDDIARG_SETPIXELSHADERCONST* pArgs,
                                             CONST FLOAT* pRegisters)
    {
        if (pArgs == nullptr || pRegisters == nullptr)
        {
            return S_OK;
        }
        const size_t float_count = static_cast<size_t>(pArgs->Count) * 4;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_f_record) + float_count * sizeof(float));
        auto* req = reinterpret_cast<d3d9c::set_const_f_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->vector4_count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), pRegisters, float_count * sizeof(float));
        record_d3d9(gb::command::d3d9_set_ps_const_f, buf.data(), static_cast<uint32_t>(buf.size()));
        return S_OK;
    }

    // pArgs is the fixed {Register, Count} header; the INT data is a separate third DDI argument
    // (see D3DDDIARG_SETVERTEXSHADERCONSTI's header comment), RE-confirmed the same shape as
    // umd_SetVertexShaderConst's CONST FLOAT* -- not trailing bytes after pArgs.
    HRESULT APIENTRY umd_SetVertexShaderConstI(HANDLE /*hDevice*/, CONST D3DDDIARG_SETVERTEXSHADERCONSTI* pArgs,
                                               CONST INT* pRegisters)
    {
        if (pArgs == nullptr || pRegisters == nullptr)
        {
            return S_OK;
        }
        const size_t int_count = static_cast<size_t>(pArgs->Count) * 4;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_i_record) + int_count * sizeof(int32_t));
        auto* req = reinterpret_cast<d3d9c::set_const_i_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->vector4_count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), pRegisters, int_count * sizeof(int32_t));
        record_d3d9(gb::command::d3d9_set_vs_const_i, buf.data(), static_cast<uint32_t>(buf.size()));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetVertexShaderConstB(HANDLE /*hDevice*/, CONST D3DDDIARG_SETVERTEXSHADERCONSTB* pArgs,
                                               CONST BOOL* pRegisters)
    {
        if (pArgs == nullptr || pRegisters == nullptr)
        {
            return S_OK;
        }
        const size_t bool_count = static_cast<size_t>(pArgs->Count);
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_b_record) + bool_count * sizeof(uint32_t));
        auto* req = reinterpret_cast<d3d9c::set_const_b_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), pRegisters, bool_count * sizeof(uint32_t));
        record_d3d9(gb::command::d3d9_set_vs_const_b, buf.data(), static_cast<uint32_t>(buf.size()));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetPixelShaderConstI(HANDLE /*hDevice*/, CONST D3DDDIARG_SETPIXELSHADERCONSTI* pArgs,
                                              CONST INT* pRegisters)
    {
        if (pArgs == nullptr || pRegisters == nullptr)
        {
            return S_OK;
        }
        const size_t int_count = static_cast<size_t>(pArgs->Count) * 4;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_i_record) + int_count * sizeof(int32_t));
        auto* req = reinterpret_cast<d3d9c::set_const_i_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->vector4_count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), pRegisters, int_count * sizeof(int32_t));
        record_d3d9(gb::command::d3d9_set_ps_const_i, buf.data(), static_cast<uint32_t>(buf.size()));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetPixelShaderConstB(HANDLE /*hDevice*/, CONST D3DDDIARG_SETPIXELSHADERCONSTB* pArgs,
                                              CONST BOOL* pRegisters)
    {
        if (pArgs == nullptr || pRegisters == nullptr)
        {
            return S_OK;
        }
        const size_t bool_count = static_cast<size_t>(pArgs->Count);
        std::vector<uint8_t> buf(sizeof(d3d9c::set_const_b_record) + bool_count * sizeof(uint32_t));
        auto* req = reinterpret_cast<d3d9c::set_const_b_record*>(buf.data());
        req->start_register = pArgs->Register;
        req->count = pArgs->Count;
        std::memcpy(buf.data() + sizeof(*req), pRegisters, bool_count * sizeof(uint32_t));
        record_d3d9(gb::command::d3d9_set_ps_const_b, buf.data(), static_cast<uint32_t>(buf.size()));
        return S_OK;
    }

    // DrawPrimitiveUP/DrawIndexedPrimitiveUP state. Real d3d9.dll implements the UP draws by binding the
    // user vertex/index arrays via pfnSetStreamSourceUm (slot 7) / pfnSetIndicesUm (slot 9) and then
    // reusing the ordinary pfnDrawPrimitive/pfnDrawIndexedPrimitive slot (10/11) -- so those slots see
    // only a stride + a raw user pointer, never a vertex/primitive count. We therefore stash the pointer
    // here and, at the subsequent draw (which does carry the counts), copy exactly the referenced bytes
    // into a set_stream_source_um / set_indices_um wire record before emitting the normal draw record.
    struct pending_um_stream
    {
        bool active = false;
        uint32_t stream_number = 0;
        uint32_t stride = 0;
        const void* data = nullptr;
    };
    struct pending_um_indices
    {
        bool active = false;
        uint32_t element_size = 0;
        const void* data = nullptr;
    };
    pending_um_stream g_um_stream;
    pending_um_indices g_um_indices;

    // Vertex/index element count a primitive draw of `count` primitives of `type` references.
    // D3DDDIPRIMITIVETYPE values match D3DPRIMITIVETYPE (POINTLIST=1 .. TRIANGLEFAN=6).
    uint32_t primitive_element_count(UINT type, UINT primitive_count)
    {
        switch (type)
        {
        case 1: // D3DPT_POINTLIST
            return primitive_count;
        case 2: // D3DPT_LINELIST
            return primitive_count * 2;
        case 3: // D3DPT_LINESTRIP
            return primitive_count + 1;
        case 4: // D3DPT_TRIANGLELIST
            return primitive_count * 3;
        case 5: // D3DPT_TRIANGLESTRIP
        case 6: // D3DPT_TRIANGLEFAN
            return primitive_count + 2;
        default:
            return primitive_count * 3;
        }
    }

    void emit_um_stream_source(uint32_t vertex_count)
    {
        if (!g_um_stream.active || g_um_stream.data == nullptr)
        {
            return;
        }
        const size_t data_size = static_cast<size_t>(vertex_count) * g_um_stream.stride;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_stream_source_um_record) + data_size);
        auto* req = reinterpret_cast<d3d9c::set_stream_source_um_record*>(buf.data());
        req->stream_number = g_um_stream.stream_number;
        req->stride_bytes = g_um_stream.stride;
        req->offset_bytes = 0;
        req->vertex_data_size = static_cast<uint32_t>(data_size);
        std::memcpy(buf.data() + sizeof(*req), g_um_stream.data, data_size);
        record_d3d9(gb::command::d3d9_set_stream_source_um, buf.data(), static_cast<uint32_t>(buf.size()));
    }

    void emit_um_indices(uint32_t index_count)
    {
        if (!g_um_indices.active || g_um_indices.data == nullptr)
        {
            return;
        }
        const size_t data_size = static_cast<size_t>(index_count) * g_um_indices.element_size;
        std::vector<uint8_t> buf(sizeof(d3d9c::set_indices_um_record) + data_size);
        auto* req = reinterpret_cast<d3d9c::set_indices_um_record*>(buf.data());
        req->index_element_size = g_um_indices.element_size;
        req->index_data_size = static_cast<uint32_t>(data_size);
        std::memcpy(buf.data() + sizeof(*req), g_um_indices.data, data_size);
        record_d3d9(gb::command::d3d9_set_indices_um, buf.data(), static_cast<uint32_t>(buf.size()));
    }

    // pfnSetStreamSourceUm (slot 7): (HANDLE, CONST D3DDDIARG_SETSTREAMSOURCEUM*, CONST VOID* pUMVertices).
    // See D3DDDIARG_SETSTREAMSOURCEUM in d3d9_ddi.hpp -- the user vertex pointer is a separate third arg.
    HRESULT APIENTRY umd_SetStreamSourceUm(HANDLE /*hDevice*/, CONST D3DDDIARG_SETSTREAMSOURCEUM* pArgs,
                                           CONST VOID* pUMVertices)
    {
        if (pArgs == nullptr || pUMVertices == nullptr)
        {
            g_um_stream.active = false;
            return S_OK;
        }
        g_um_stream = {.active = true,
                       .stream_number = pArgs->StreamNumber,
                       .stride = pArgs->Stride,
                       .data = pUMVertices};
        return S_OK;
    }

    // pfnSetIndicesUm (slot 9): (HANDLE, UINT Stride, CONST VOID* pUMIndices). Stride is the raw index
    // element size in bytes (2 or 4) passed by value, NOT a struct pointer (see d3d9_ddi.hpp's note).
    HRESULT APIENTRY umd_SetIndicesUm(HANDLE /*hDevice*/, UINT Stride, CONST VOID* pUMIndices)
    {
        if (pUMIndices == nullptr)
        {
            g_um_indices.active = false;
            return S_OK;
        }
        g_um_indices = {.active = true, .element_size = Stride, .data = pUMIndices};
        return S_OK;
    }

    HRESULT APIENTRY umd_SetStreamSource(HANDLE /*hDevice*/, CONST D3DDDIARG_SETSTREAMSOURCE* pArgs)
    {
        if (pArgs == nullptr) // unbind, same convention as the shader/texture slots
        {
            return S_OK;
        }
        // A real buffer bind supersedes any pending UP user-memory vertex source for this stream.
        if (g_um_stream.active && g_um_stream.stream_number == pArgs->StreamNumber)
        {
            g_um_stream.active = false;
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
        record_d3d9(gb::command::d3d9_set_stream_source, &req, sizeof(req));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetStreamSourceFreq(HANDLE /*hDevice*/, CONST D3DDDIARG_SETSTREAMSOURCEFREQ* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::set_stream_source_freq_record req{.stream_number = pArgs->StreamNumber, .frequency = pArgs->Divider};
        record_d3d9(gb::command::d3d9_set_stream_source_freq, &req, sizeof(req));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetIndices(HANDLE /*hDevice*/, CONST D3DDDIARG_SETINDICES* pArgs)
    {
        if (pArgs == nullptr) // unbind index buffer, same convention as the shader/texture slots
        {
            return S_OK;
        }
        // A real index buffer bind supersedes any pending UP user-memory index source.
        g_um_indices.active = false;
        // Same buffer lazy-bind reasoning as umd_SetStreamSource.
        d3d9c::set_indices_record req{.index_buffer = resolve_buffer_resource_id(pArgs->hIndexBuffer, 0),
                                      .format = pArgs->Stride == 4 ? 1u : 0u,
                                      .reserved = 0};
        record_d3d9(gb::command::d3d9_set_indices, &req, sizeof(req));
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
        record_d3d9(gb::command::d3d9_set_render_target, &req, sizeof(req));
        return S_OK;
    }

    HRESULT APIENTRY umd_SetDepthStencil(HANDLE /*hDevice*/, CONST D3DDDIARG_SETDEPTHSTENCIL* pArgs)
    {
        if (pArgs == nullptr) // unbind depth-stencil, same convention as the shader/texture slots
        {
            return S_OK;
        }
        d3d9c::set_depth_stencil_record req{.surface = resolve_depth_stencil_resource_id(pArgs->hZBuffer)};
        record_d3d9(gb::command::d3d9_set_depth_stencil, &req, sizeof(req));
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
        record_d3d9(gb::command::d3d9_set_viewport, &req, sizeof(req));
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
        record_d3d9(gb::command::d3d9_set_scissor, &req, sizeof(req));
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
        record_d3d9(gb::command::d3d9_clear, buf.data(), static_cast<uint32_t>(buf.size()));
        return S_OK;
    }

    // pfnDrawPrimitive is a THREE-argument DDI: (HANDLE, CONST D3DDDIARG_DRAWPRIMITIVE*, CONST UINT*
    // pFlagBuffer) -- the WDK-documented shape. RE-verified live in d3d9_x86.dll (both the normal
    // CD3DDDIDX10_DrawPrimitive at 0x1004DFE0 and the DrawPrimitiveUP path at 0x1013F730 push three
    // args: pFlags(=0), the D3DDDIARG_DRAWPRIMITIVE*, and hDevice). On x86 __stdcall (callee-cleanup)
    // the callee's `ret N` MUST pop all 12 bytes or the caller's stack desyncs -- a 2-arg declaration
    // faults d3d9.dll's /GS check (STATUS_STACK_BUFFER_OVERRUN). The extra pFlagBuffer pointer is unused
    // by this bring-up. (On x64 caller-cleanup, a 2-arg declaration happened to work, which masked this.)
    HRESULT APIENTRY umd_DrawPrimitive(HANDLE /*hDevice*/, CONST D3DDDIARG_DRAWPRIMITIVE* pArgs, CONST UINT* /*pFlags*/)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        // DrawPrimitiveUP path: flush the pending user-memory vertex source (bound via slot 7) with
        // exactly the bytes this draw references before recording the reused draw call.
        emit_um_stream_source(pArgs->VStart + primitive_element_count(pArgs->PrimitiveType, pArgs->PrimitiveCount));
        d3d9c::draw_primitive_record req{.primitive_type = pArgs->PrimitiveType,
                                         .start_vertex = pArgs->VStart,
                                         .primitive_count = pArgs->PrimitiveCount,
                                         .reserved = 0};
        record_d3d9(gb::command::d3d9_draw_primitive, &req, sizeof(req));
        return S_OK;
    }

    HRESULT APIENTRY umd_DrawIndexedPrimitive(HANDLE /*hDevice*/, CONST D3DDDIARG_DRAWINDEXEDPRIMITIVE* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        // DrawIndexedPrimitiveUP path: flush the pending user-memory vertex source (slot 7) and index
        // source (slot 9) with exactly the bytes this draw references before recording the reused draw
        // call. Vertices span [0, MinIndex+NumVertices); indices span [0, StartIndex+index_count).
        emit_um_stream_source(pArgs->MinIndex + pArgs->NumVertices);
        emit_um_indices(pArgs->StartIndex + primitive_element_count(pArgs->PrimitiveType, pArgs->PrimitiveCount));
        d3d9c::draw_indexed_primitive_record req{.primitive_type = pArgs->PrimitiveType,
                                                 .base_vertex_index = pArgs->BaseVertexIndex,
                                                 .min_vertex_index = pArgs->MinIndex,
                                                 .num_vertices = pArgs->NumVertices,
                                                 .start_index = pArgs->StartIndex,
                                                 .primitive_count = pArgs->PrimitiveCount};
        record_d3d9(gb::command::d3d9_draw_indexed_primitive, &req, sizeof(req));
        return S_OK;
    }

    // Does NOT drain g_d3d9_command_batch -- no query/fence DDI is wired yet that would need pending
    // state visible host-side before this call returns. Revisit if one ever is.
    HRESULT APIENTRY umd_Flush(HANDLE /*hDevice*/)
    {
        return S_OK;
    }

    HRESULT APIENTRY umd_Present(HANDLE /*hDevice*/, CONST D3DDDIARG_PRESENT* pArgs)
    {
        if (pArgs == nullptr)
        {
            return S_OK;
        }
        d3d9c::present_request req{.resource = resolve_resource_id(pArgs->hSrcResource)};
        d3d9c::present_response resp{};
        bridge_call(gb::ioctl_d3d9_present, &req, sizeof(req), &resp, sizeof(resp));
        return resp.hr == 0 ? S_OK : E_FAIL;
    }

    // D3DDDIARG_LOCK::pData must point to memory that stays valid until the matching Unlock (the app
    // writes vertex/index data directly through it), so each outstanding lock owns a persistent
    // heap buffer here instead of a call-local one. Keyed by the wire resource id (== hResource). The
    // buffer holds only `[offset, end)` of the resource (see umd_Lock), so g_locked_offsets remembers
    // which offset it started at, for umd_Unlock to write it back to the right place.
    std::unordered_map<uint64_t, std::vector<uint8_t>> g_locked_buffers;
    std::unordered_map<uint64_t, uint32_t> g_locked_offsets;

    HRESULT APIENTRY umd_Lock(HANDLE /*hDevice*/, D3DDDIARG_LOCK* pArgs)
    {
        if (pArgs == nullptr)
        {
            return E_FAIL;
        }
        // Real render-target/texture handles are already registered by pfnCreateResource by the time
        // Lock() reaches them (confirmed live) -- resolve_buffer_resource_id checks g_created_resource_ids
        // first and uses that resource id directly for them; only an unregistered handle (vertex/index
        // buffers, which never call pfnCreateResource) falls through to its own buffer lazy-bind path.
        // Only buffers get OffsetToLock treatment below: LockRect (textures/render targets/depth-
        // stencil) uses this same struct region for Rect/Box input, not a byte offset.
        const auto raw_handle = reinterpret_cast<uint64_t>(pArgs->hResource);
        const bool is_buffer = g_created_resource_ids.find(raw_handle) == g_created_resource_ids.end();

#ifdef _WIN64
        // OffsetToLock (d3d9_ddi.hpp, offset 80) is reliable for "driver-routed" buffer locks -- the
        // routing this UMD's own DevCaps bits (k_devcaps_driver_managed_pool/_index_pool) make the
        // common case for D3DPOOL_DEFAULT buffers. "Sysmem-routed" locks read something unrelated from
        // this offset, but their driver-returned pData is discarded by the app regardless (confirmed
        // live, HANDOFF_MACBOOK.md #16.1), and an out-of-range offset is safely rejected by the host's
        // own lock() below rather than misbehaving locally -- so reading it unconditionally for every
        // buffer lock is safe. SizeToLock still has no reliable offset in either shape (see
        // d3d9_ddi.hpp) -- size = 0 below means "from offset to the end of the resource", which is
        // exactly right for the common D3DLOCK_NOOVERWRITE tail-append pattern this fixes: the app
        // only ever writes forward from OffsetToLock anyway.
        const uint32_t offset = is_buffer ? pArgs->OffsetToLock : 0;
#else
        // x86's driver-routed OffsetToLock offset is not yet RE-verified (see d3d9_ddi.hpp's own
        // D3DDDIARG_LOCK comment) -- keep the pre-existing whole-buffer-lock behavior here.
        const uint32_t offset = 0;
#endif

        const auto resource = resolve_buffer_resource_id(pArgs->hResource, offset);
        d3d9c::lock_request req{.resource = resource, .subresource = 0, .offset = offset, .size = 0, .flags = 0, .reserved = 0};

        // First call with no output buffer just to learn the true backing size via lock_response
        // (lock_response::data_size is "bytes available from `offset` to the end of the resource").
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
        g_locked_offsets[resource] = offset;
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

        const auto offset_it = g_locked_offsets.find(resource);
        const uint32_t offset = offset_it != g_locked_offsets.end() ? offset_it->second : 0;

        std::vector<uint8_t> buf(sizeof(d3d9c::unlock_request) + it->second.size());
        auto* req = reinterpret_cast<d3d9c::unlock_request*>(buf.data());
        req->resource = resource;
        req->subresource = 0;
        req->offset = offset;
        req->data_size = static_cast<uint32_t>(it->second.size());
        std::memcpy(buf.data() + sizeof(*req), it->second.data(), it->second.size());
        bridge_call(gb::ioctl_d3d9_unlock, buf.data(), static_cast<DWORD>(buf.size()), nullptr, 0);

        g_locked_buffers.erase(it);
        g_locked_offsets.erase(resource);
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
#ifdef _WIN64
            // x64 is caller-cleanup regardless of a callee's declared arity, so one generic
            // zero-arg stub can safely back every unimplemented slot.
            for (size_t i = 0; i < n; ++i)
            {
                slots[i] = reinterpret_cast<void*>(&device_stub);
            }
#else
            // x86 __stdcall is callee-cleanup: each slot needs a stub declaring its real argument
            // byte count, or the stack desyncs on return (see k_device_func_arity above).
            for (size_t i = 0; i < n; ++i)
            {
                slots[i] = stub_for_arity(k_device_func_arity[i]);
            }
#endif

            // Real per-DDI marshaling (see the block above) for the state/draw path -- indices match
            // D3DDDI_DEVICEFUNCS's field order exactly (d3d9_ddi.hpp).
            slots[0] = reinterpret_cast<void*>(&umd_SetRenderState);          // pfnSetRenderState
            slots[3] = reinterpret_cast<void*>(&umd_SetTextureStageState);    // pfnSetTextureStageState
            slots[4] = reinterpret_cast<void*>(&umd_SetTexture);              // pfnSetTexture
            slots[5] = reinterpret_cast<void*>(&umd_SetPixelShader);          // pfnSetPixelShader
            slots[6] = reinterpret_cast<void*>(&umd_SetPixelShaderConst);     // pfnSetPixelShaderConst
            slots[7] = reinterpret_cast<void*>(&umd_SetStreamSourceUm);       // pfnSetStreamSourceUm
            slots[8] = reinterpret_cast<void*>(&umd_SetIndices);              // pfnSetIndices
            slots[9] = reinterpret_cast<void*>(&umd_SetIndicesUm);            // pfnSetIndicesUm
            slots[10] = reinterpret_cast<void*>(&umd_DrawPrimitive);          // pfnDrawPrimitive
            slots[11] = reinterpret_cast<void*>(&umd_DrawIndexedPrimitive);   // pfnDrawIndexedPrimitive
            slots[18] = reinterpret_cast<void*>(&umd_TexBlt);                 // pfnTexBlt
            slots[21] = reinterpret_cast<void*>(&umd_Clear);                  // pfnClear
            slots[24] = reinterpret_cast<void*>(&umd_SetVertexShaderConst);   // pfnSetVertexShaderConst
            slots[27] = reinterpret_cast<void*>(&umd_SetViewport);            // pfnSetViewport
            slots[28] = reinterpret_cast<void*>(&umd_SetZRange);              // pfnSetZRange
            slots[35] = reinterpret_cast<void*>(&umd_Lock);                    // pfnLock
            slots[36] = reinterpret_cast<void*>(&umd_Unlock);                  // pfnUnlock
            slots[37] = reinterpret_cast<void*>(&umd_CreateResource);          // pfnCreateResource
            slots[40] = reinterpret_cast<void*>(&umd_Present);                // pfnPresent
            slots[41] = reinterpret_cast<void*>(&umd_Flush);                  // pfnFlush
            slots[42] = reinterpret_cast<void*>(&umd_CreateVertexShaderFunc); // pfnCreateVertexShaderFunc
            slots[43] = reinterpret_cast<void*>(&umd_DeleteVertexShaderFunc); // pfnDeleteVertexShaderFunc
            slots[44] = reinterpret_cast<void*>(&umd_SetVertexShaderFunc);    // pfnSetVertexShaderFunc
            slots[45] = reinterpret_cast<void*>(&umd_CreateVertexShaderDecl); // pfnCreateVertexShaderDecl
            slots[47] = reinterpret_cast<void*>(&umd_SetVertexShaderDecl);    // pfnSetVertexShaderDecl
            slots[48] = reinterpret_cast<void*>(&umd_SetVertexShaderConstI);  // pfnSetVertexShaderConstI
            slots[49] = reinterpret_cast<void*>(&umd_SetVertexShaderConstB);  // pfnSetVertexShaderConstB
            slots[50] = reinterpret_cast<void*>(&umd_SetScissorRect);         // pfnSetScissorRect
            slots[51] = reinterpret_cast<void*>(&umd_SetStreamSource);        // pfnSetStreamSource
            slots[52] = reinterpret_cast<void*>(&umd_SetStreamSourceFreq);    // pfnSetStreamSourceFreq
            slots[62] = reinterpret_cast<void*>(&umd_SetRenderTarget);        // pfnSetRenderTarget
            slots[63] = reinterpret_cast<void*>(&umd_SetDepthStencil);        // pfnSetDepthStencil
            slots[65] = reinterpret_cast<void*>(&umd_SetPixelShaderConstI);   // pfnSetPixelShaderConstI
            slots[66] = reinterpret_cast<void*>(&umd_SetPixelShaderConstB);   // pfnSetPixelShaderConstB
            slots[67] = reinterpret_cast<void*>(&umd_CreatePixelShader);      // pfnCreatePixelShader
            slots[68] = reinterpret_cast<void*>(&umd_DeletePixelShader);      // pfnDeletePixelShader
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
