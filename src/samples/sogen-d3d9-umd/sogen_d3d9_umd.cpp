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

    HRESULT APIENTRY umd_CreateResource(HANDLE /*hDevice*/, void* pArgs)
    {
        auto* bytes = reinterpret_cast<unsigned char*>(pArgs);
        uint32_t format = 0;
        std::memcpy(&format, bytes, sizeof(format));

        // KNOWN LIMITATION (see HANDOFF_MACBOOK.md): D3DDDIARG_CREATERESOURCE's real width/height/pool
        // offsets are still not RE'd -- every call gets this milestone's fixed 640x480 shape. Fine for
        // every resource this session's tests create (all sized to match the 640x480 window/backbuffer
        // exactly), wrong in general.
        const d3d9c::create_resource_request req{
            .kind = static_cast<uint32_t>(d3d9c::resource_kind::texture_2d),
            .format = format,
            .width = 640,
            .height = 480,
            .depth = 1,
            .mip_levels = 1,
            .usage = classify_resource_usage(format),
            .pool = 0,
        };
        d3d9c::create_resource_response resp{};
        bridge_call(gb::ioctl_d3d9_create_resource, &req, sizeof(req), &resp, sizeof(resp));
        if (resp.hr == 0)
        {
            std::memcpy(bytes + 48, &resp.resource, sizeof(resp.resource));
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
    // byte_size is always 0 in practice (see umd_Lock's own comment on why SizeToLock isn't reliably
    // readable) -- kept as a parameter rather than removed in case a future, routing-path-aware caller
    // can supply a real size.
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
        bridge_call(gb::ioctl_d3d9_set_render_state, &req, sizeof(req), nullptr, 0);
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
            bridge_call(gb::ioctl_d3d9_set_sampler_state, &req, sizeof(req), nullptr, 0);
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
        //
        // KNOWN LIMITATION found during this session's RE work: for a D3DPOOL_MANAGED texture,
        // pfnCreateResource fires TWICE for what the app sees as one CreateTexture() call, with two
        // different output handles -- one that LockRect's hResource later uses (and correctly
        // receives the app's real pixel writes) and a DIFFERENT one that this hTexture uses (which
        // stays an empty, never-written resource, so a sampled texture created with D3DPOOL_MANAGED
        // reads as entirely black/transparent). Not resolved -- d3d9_texture_test.cpp uses
        // D3DUSAGE_DYNAMIC + D3DPOOL_DEFAULT instead (a single pfnCreateResource call, confirmed live)
        // to avoid it, since root-causing and fixing the double-resource case was out of scope on top
        // of this session's two required carried-forward findings.
        d3d9c::set_texture_record req{.stage = Stage, .reserved = 0, .texture = reinterpret_cast<uint64_t>(hTexture)};
        bridge_call(gb::ioctl_d3d9_set_texture, &req, sizeof(req), nullptr, 0);
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
        bridge_call(gb::ioctl_d3d9_set_pixel_shader, &req, sizeof(req), nullptr, 0);
        return S_OK;
    }

    HRESULT APIENTRY umd_SetVertexShaderFunc(HANDLE /*hDevice*/, HANDLE hShader)
    {
        // Same direct-value convention as umd_SetPixelShader above (not D3DDDIARG_SETVERTEXSHADERFUNC*).
        // 0 means "no vertex shader / use fixed-function", e.g. when a D3DFVF_XYZRHW draw follows a
        // shader-bound one.
        d3d9c::set_vertex_shader_record req{.shader = reinterpret_cast<uint64_t>(hShader)};
        bridge_call(gb::ioctl_d3d9_set_vertex_shader, &req, sizeof(req), nullptr, 0);
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

    HRESULT APIENTRY umd_SetVertexShaderDecl(HANDLE /*hDevice*/, CONST D3DDDIARG_SETVERTEXSHADERDECL* pArgs)
    {
        // NULL pArgs crashed this function live (confirmed via a real access violation at the
        // dereference below) -- the runtime passes it for fixed-function (D3DFVF-only, no explicit
        // vertex declaration) draws. 0 = no decl, matching umd_SetVertexShaderFunc's own convention.
        d3d9c::set_vertex_decl_record req{.decl = pArgs != nullptr ? reinterpret_cast<uint64_t>(pArgs->ShaderHandle) : 0};
        bridge_call(gb::ioctl_d3d9_set_vertex_decl, &req, sizeof(req), nullptr, 0);
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
        bridge_call(gb::ioctl_d3d9_set_vs_const_f, buf.data(), static_cast<DWORD>(buf.size()), nullptr, 0);
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
        d3d9c::set_depth_stencil_record req{.surface = resolve_depth_stencil_resource_id(pArgs->hZBuffer)};
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
    // heap buffer here instead of a call-local one. Keyed by the wire resource id (== hResource).
    std::unordered_map<uint64_t, std::vector<uint8_t>> g_locked_buffers;

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
        //
        // Neither OffsetToLock nor SizeToLock has a reliable, routing-path-independent offset in this
        // struct (see d3d9_ddi.hpp's D3DDDIARG_LOCK comment for the full RE finding: two different real
        // d3d9.dll code paths build this struct differently, and umd_Lock -- one function shared by
        // every resource kind -- cannot tell which one produced a given call). Always treat every lock
        // as an implicit whole-buffer lock: offset 0, size unknown (0, meaning "use the resource's own
        // known/fallback byte size" -- both resolve_buffer_resource_id and the host's lock() already
        // implement exactly this convention). This is what actually fixes the index-buffer round-trip
        // bug this RE pass investigated: an earlier model read SizeToLock from an offset that happened
        // to read back 0 for every index-buffer lock, and separately, index buffers were routing
        // through a d3d9.dll internal path that never reached this driver's pfnLock/pfnUnlock at all
        // until fill_d3d9caps gained the matching DevCaps routing bit (see its own comment).
        const auto resource = resolve_buffer_resource_id(pArgs->hResource, 0);
        d3d9c::lock_request req{.resource = resource, .subresource = 0, .offset = 0, .size = 0, .flags = 0, .reserved = 0};

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
            slots[40] = reinterpret_cast<void*>(&umd_Present);                // pfnPresent
            slots[41] = reinterpret_cast<void*>(&umd_Flush);                  // pfnFlush
            slots[42] = reinterpret_cast<void*>(&umd_CreateVertexShaderFunc); // pfnCreateVertexShaderFunc
            slots[43] = reinterpret_cast<void*>(&umd_DeleteVertexShaderFunc); // pfnDeleteVertexShaderFunc
            slots[44] = reinterpret_cast<void*>(&umd_SetVertexShaderFunc);    // pfnSetVertexShaderFunc
            slots[47] = reinterpret_cast<void*>(&umd_SetVertexShaderDecl);    // pfnSetVertexShaderDecl
            slots[50] = reinterpret_cast<void*>(&umd_SetScissorRect);         // pfnSetScissorRect
            slots[51] = reinterpret_cast<void*>(&umd_SetStreamSource);        // pfnSetStreamSource
            slots[52] = reinterpret_cast<void*>(&umd_SetStreamSourceFreq);    // pfnSetStreamSourceFreq
            slots[62] = reinterpret_cast<void*>(&umd_SetRenderTarget);        // pfnSetRenderTarget
            slots[63] = reinterpret_cast<void*>(&umd_SetDepthStencil);        // pfnSetDepthStencil
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
