// Minimal WDDM Direct3D9 user-mode-driver DDI (d3dumddi) subset for the sogen thin UMD.
//
// mingw-w64 does not ship d3dumddi.h (it is a WDK-only header), so the ABI the official Microsoft
// d3d9.dll expects is hand-transcribed here from the documented Windows Driver Kit layout. Only the
// adapter-negotiation cluster (OpenAdapter -> GetCaps/CreateDevice/CloseAdapter) needs exact field
// types for the Spike-B gate; the per-draw device functions are laid out as correctly-ordered,
// correctly-counted slots (their exact signatures are filled in as the decoder milestones need them).
//
// The DDI is version-gated by D3D_UMD_INTERFACE_VERSION: later Windows releases append entries to the
// end of D3DDDI_DEVICEFUNCS. We pin SOGEN_D3D9_UMD_INTERFACE_VERSION so the table size matches the
// version we report to the runtime from OpenAdapter (DriverVersion, out). Bring-up reports a
// conservative version; the runtime only reads/calls the entries valid for that version.

#pragma once

#include <windows.h>
#include <cstddef>
#include <cstdint>

// --- D3D_UMD_INTERFACE_VERSION values (WDK d3dukmdt.h) ---------------------------------------------
#define SOGEN_D3D_UMD_INTERFACE_VERSION_VISTA 0x000C
#define SOGEN_D3D_UMD_INTERFACE_VERSION_WIN7 0x2003
#define SOGEN_D3D_UMD_INTERFACE_VERSION_WIN8 0x3004
#define SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM1_3 0x4002
#define SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM2_0 0x5002
#define SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM2_1_2 0x6001

// Version this UMD is built for. D3D9 is a Vista/Win7-era DDI; WIN7 is the conservative bring-up
// target. Confirmed/adjusted empirically from the Interface/Version the runtime passes to OpenAdapter.
#ifndef SOGEN_D3D9_UMD_INTERFACE_VERSION
#define SOGEN_D3D9_UMD_INTERFACE_VERSION SOGEN_D3D_UMD_INTERFACE_VERSION_WIN7
#endif

#pragma pack(push, 8)

// --- Caps query (pfnGetCaps) ----------------------------------------------------------------------
typedef enum _SOGEN_D3DDDICAPS_TYPE
{
    SOGEN_D3DDDICAPS_GETFORMATCOUNT = 3,
    SOGEN_D3DDDICAPS_GETFORMATDATA = 4,
    SOGEN_D3DDDICAPS_GETD3D9CAPS = 13,
    SOGEN_D3DDDICAPS_GETD3DQUERYCOUNT = 6,
    SOGEN_D3DDDICAPS_GETD3DQUERYDATA = 7,
    SOGEN_D3DDDICAPS_GETGAMMARAMPCAPS = 34,
} SOGEN_D3DDDICAPS_TYPE;

typedef struct _D3DDDIARG_GETCAPS
{
    UINT  Type;     // SOGEN_D3DDDICAPS_TYPE (a plain enum == UINT wide)
    VOID* pInfo;    // in
    VOID* pData;    // out
    UINT  DataSize; // in
} D3DDDIARG_GETCAPS;

// --- Callback tables (opaque to us; we only store the runtime pointer) -----------------------------
typedef struct _D3DDDI_ADAPTERCALLBACKS
{
    VOID* pfnQueryAdapterInfoCb;
    VOID* pfnGetMultisampleMethodListCb;
} D3DDDI_ADAPTERCALLBACKS;

// The device callback table is large and version-gated; we never invoke it during Spike B, so an
// opaque pointer is sufficient (the runtime owns the storage).

// --- Device function table (D3DDDI_DEVICEFUNCS) ---------------------------------------------------
// Ordered exactly as the WDK layout. Slots are void* for bring-up: on x64 the calling convention is
// caller-cleanup, so a single generic stub can back every slot regardless of arity. The x86 port and
// the real decoder replace individual slots with correctly-typed __stdcall thunks.
typedef struct _D3DDDI_DEVICEFUNCS
{
    // --- base (Vista) : 99 entries ---
    void* pfnSetRenderState;
    void* pfnUpdateWInfo;
    void* pfnValidateDevice;
    void* pfnSetTextureStageState;
    void* pfnSetTexture;
    void* pfnSetPixelShader;
    void* pfnSetPixelShaderConst;
    void* pfnSetStreamSourceUm;
    void* pfnSetIndices;
    void* pfnSetIndicesUm;
    void* pfnDrawPrimitive;
    void* pfnDrawIndexedPrimitive;
    void* pfnDrawRectPatch;
    void* pfnDrawTriPatch;
    void* pfnDrawPrimitive2;
    void* pfnDrawIndexedPrimitive2;
    void* pfnVolBlt;
    void* pfnBufBlt;
    void* pfnTexBlt;
    void* pfnStateSet;
    void* pfnSetPriority;
    void* pfnClear;
    void* pfnUpdatePalette;
    void* pfnSetPalette;
    void* pfnSetVertexShaderConst;
    void* pfnMultiplyTransform;
    void* pfnSetTransform;
    void* pfnSetViewport;
    void* pfnSetZRange;
    void* pfnSetMaterial;
    void* pfnSetLight;
    void* pfnCreateLight;
    void* pfnDestroyLight;
    void* pfnSetClipPlane;
    void* pfnGetInfo;
    void* pfnLock;
    void* pfnUnlock;
    void* pfnCreateResource;
    void* pfnDestroyResource;
    void* pfnSetDisplayMode;
    void* pfnPresent;
    void* pfnFlush;
    void* pfnCreateVertexShaderFunc;
    void* pfnDeleteVertexShaderFunc;
    void* pfnSetVertexShaderFunc;
    void* pfnCreateVertexShaderDecl;
    void* pfnDeleteVertexShaderDecl;
    void* pfnSetVertexShaderDecl;
    void* pfnSetVertexShaderConstI;
    void* pfnSetVertexShaderConstB;
    void* pfnSetScissorRect;
    void* pfnSetStreamSource;
    void* pfnSetStreamSourceFreq;
    void* pfnSetConvolutionKernelMono;
    void* pfnComposeRects;
    void* pfnBlt;
    void* pfnColorFill;
    void* pfnDepthFill;
    void* pfnCreateQuery;
    void* pfnDestroyQuery;
    void* pfnIssueQuery;
    void* pfnGetQueryData;
    void* pfnSetRenderTarget;
    void* pfnSetDepthStencil;
    void* pfnGenerateMipSubLevels;
    void* pfnSetPixelShaderConstI;
    void* pfnSetPixelShaderConstB;
    void* pfnCreatePixelShader;
    void* pfnDeletePixelShader;
    void* pfnCreateDecodeDevice;
    void* pfnDestroyDecodeDevice;
    void* pfnSetDecodeRenderTarget;
    void* pfnDecodeBeginFrame;
    void* pfnDecodeEndFrame;
    void* pfnDecodeExecute;
    void* pfnDecodeExtensionExecute;
    void* pfnCreateVideoProcessDevice;
    void* pfnDestroyVideoProcessDevice;
    void* pfnVideoProcessBeginFrame;
    void* pfnVideoProcessEndFrame;
    void* pfnSetVideoProcessRenderTarget;
    void* pfnVideoProcessBlt;
    void* pfnCreateExtensionDevice;
    void* pfnDestroyExtensionDevice;
    void* pfnExtensionExecute;
    void* pfnCreateOverlay;
    void* pfnUpdateOverlay;
    void* pfnFlipOverlay;
    void* pfnGetOverlayColorControls;
    void* pfnSetOverlayColorControls;
    void* pfnDestroyOverlay;
    void* pfnDestroyDevice;
    void* pfnQueryResourceResidency;
    void* pfnOpenResource;
    void* pfnGetCaptureAllocationHandle;
    void* pfnCaptureToSysMem;
    void* pfnLockAsync;
    void* pfnUnlockAsync;
    void* pfnRename;
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WIN7)
    void* pfnCreateVideoProcessor;
    void* pfnSetVideoProcessBltState;
    void* pfnGetVideoProcessBltStatePrivate;
    void* pfnSetVideoProcessStreamState;
    void* pfnGetVideoProcessStreamStatePrivate;
    void* pfnVideoProcessBltHD;
    void* pfnDestroyVideoProcessor;
    void* pfnCreateAuthenticatedChannel;
    void* pfnAuthenticatedChannelKeyExchange;
    void* pfnQueryAuthenticatedChannel;
    void* pfnConfigureAuthenticatedChannel;
    void* pfnDestroyAuthenticatedChannel;
    void* pfnCreateCryptoSession;
    void* pfnCryptoSessionKeyExchange;
    void* pfnDestroyCryptoSession;
    void* pfnEncryptionBlt;
    void* pfnGetPitch;
    void* pfnStartSessionKeyRefresh;
    void* pfnFinishSessionKeyRefresh;
    void* pfnGetEncryptionBltKey;
    void* pfnDecryptionBlt;
    void* pfnResolveSharedResource;
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WIN8)
    void* pfnVolBlt1;
    void* pfnBufBlt1;
    void* pfnTexBlt1;
    void* pfnDiscard;
    void* pfnOfferResources;
    void* pfnReclaimResources;
    void* pfnCheckDirectFlipSupport;
    void* pfnCreateResource2;
    void* pfnCheckMultiPlaneOverlaySupport;
    void* pfnPresentMultiPlaneOverlay;
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM1_3)
    void* pfnReserved1;
    void* pfnFlush1;
    void* pfnCheckCounterInfo;
    void* pfnCheckCounter;
    void* pfnUpdateSubresourceUP;
    void* pfnPresent1;
    void* pfnCheckPresentDurationSupport;
    void* pfnSetMarker;
    void* pfnSetMarkerMode;
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM2_0)
    void* pfnTrimResidencySet;
#endif
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WDDM2_1_2)
    void* pfnAcquireResource;
    void* pfnReleaseResource;
#endif
} D3DDDI_DEVICEFUNCS;

// --- CreateDevice arg -----------------------------------------------------------------------------
typedef struct _D3DDDIARG_CREATEDEVICE
{
    HANDLE              hDevice;               // in: runtime handle / out: driver handle
    UINT                Interface;             // in
    UINT                Version;               // in
    CONST VOID*         pCallbacks;            // in: D3DDDI_DEVICECALLBACKS*
    VOID*               pCommandBuffer;        // in
    UINT                CommandBufferSize;     // in
    VOID*               pAllocationList;       // out
    UINT                AllocationListSize;    // in
    VOID*               pPatchLocationList;    // out
    UINT                PatchLocationListSize; // in
    D3DDDI_DEVICEFUNCS* pDeviceFuncs;          // out: driver function table (we fill it)
    UINT                Flags;                 // in: D3DDDI_CREATEDEVICEFLAGS
#if (SOGEN_D3D9_UMD_INTERFACE_VERSION >= SOGEN_D3D_UMD_INTERFACE_VERSION_WIN7)
    UINT64              CommandBuffer;         // out: GPU VA of the command buffer
#endif
} D3DDDIARG_CREATEDEVICE;

// --- Adapter function table (filled by OpenAdapter) -----------------------------------------------
typedef HRESULT(APIENTRY* PFND3DDDI_GETCAPS)(HANDLE hAdapter, CONST D3DDDIARG_GETCAPS*);
typedef HRESULT(APIENTRY* PFND3DDDI_CREATEDEVICE)(HANDLE hAdapter, D3DDDIARG_CREATEDEVICE*);
typedef HRESULT(APIENTRY* PFND3DDDI_CLOSEADAPTER)(HANDLE hAdapter);

typedef struct _D3DDDI_ADAPTERFUNCS
{
    PFND3DDDI_GETCAPS      pfnGetCaps;
    PFND3DDDI_CREATEDEVICE pfnCreateDevice;
    PFND3DDDI_CLOSEADAPTER pfnCloseAdapter;
} D3DDDI_ADAPTERFUNCS;

// --- OpenAdapter arg (the DLL's single exported entry point) --------------------------------------
typedef struct _D3DDDIARG_OPENADAPTER
{
    HANDLE                         hAdapter;          // in/out: runtime handle / out: driver handle
    UINT                           Interface;         // in: interface version the runtime speaks
    UINT                           Version;           // in: runtime version
    CONST D3DDDI_ADAPTERCALLBACKS* pAdapterCallbacks; // in
    D3DDDI_ADAPTERFUNCS*           pAdapterFuncs;     // out: we fill GetCaps/CreateDevice/CloseAdapter
    UINT                           DriverVersion;     // out: D3D_UMD_INTERFACE_VERSION we were built for
} D3DDDIARG_OPENADAPTER;

typedef HRESULT(APIENTRY* PFND3DDDI_OPENADAPTER)(D3DDDIARG_OPENADAPTER*);

// --- Device-function DDI args (WDK d3dumddi.h layout; only the subset sogen's D3D9 UMD marshals) --
// D3DDDIARG_RENDERSTATE's {State,Value} shape is confirmed against the real staged d3d9.dll (see
// CBatchFilterI::LHBatchSetRenderState, which copies exactly 8 bytes -- one QWORD -- out of *pArg).
// The rest follow the same well-established WDK Set*-family convention (HANDLE, CONST ARG*).

typedef struct _D3DDDIARG_RENDERSTATE
{
    UINT State; // D3DDDIRENDERSTATETYPE
    UINT Value;
} D3DDDIARG_RENDERSTATE;

typedef struct _D3DDDIARG_TEXTURESTAGESTATE
{
    UINT Stage;
    UINT State; // D3DDDITEXTURESTAGESTATETYPE
    UINT Value;
} D3DDDIARG_TEXTURESTAGESTATE;

typedef struct _D3DDDIARG_SAMPLERSTATE
{
    UINT Sampler;
    UINT State; // D3DDDISAMPLERSTATETYPE
    UINT Value;
} D3DDDIARG_SAMPLERSTATE;

// RE-verified live 2026-07-02: pfnSetTexture is NOT a (HANDLE, CONST ARG*) call -- it takes Stage and
// hTexture as direct value arguments: HRESULT APIENTRY (HANDLE hDevice, UINT Stage, HANDLE hTexture).
// The struct-pointer assumption below crashed the UMD live (Stage's small integer value, interpreted
// as a pointer, is not a valid address). Kept only as a historical note; umd_SetTexture takes the real
// 3-argument signature directly.
typedef struct _D3DDDIARG_SETTEXTURE
{
    UINT Stage;
    HANDLE hTexture; // 0 unbinds; matches whatever pfnCreateResource returned
} D3DDDIARG_SETTEXTURE;

typedef struct _D3DDDIARG_SETSTREAMSOURCE
{
    UINT StreamNumber;
    HANDLE hVertexBuffer;
    UINT Offset;
    UINT Stride;
} D3DDDIARG_SETSTREAMSOURCE;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_SETSTREAMSOURCE) == 24, "D3DDDIARG_SETSTREAMSOURCE x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (hVertexBuffer shrinks from 8 to 4 bytes,
// pulling Offset/Stride in behind it); compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_SETSTREAMSOURCE) == 16, "D3DDDIARG_SETSTREAMSOURCE x86 layout");
#endif

// UP-draw (DrawPrimitiveUP / DrawIndexedPrimitiveUP) stream-source variant. RE-verified live
// 2026-07-06 (x64 AND x86) by bracketing a real DrawPrimitiveUP call and dumping the
// pfnSetStreamSourceUm (device-func-table slot 7) argument: the struct carries only
// { StreamNumber, Stride } -- there is NO hVertexBuffer/Offset (unlike the buffer variant above),
// because the user vertex data does not live in a buffer. pfnSetStreamSourceUm is a THREE-argument
// DDI call:
//   HRESULT APIENTRY (HANDLE hDevice, CONST D3DDDIARG_SETSTREAMSOURCEUM* pArg, CONST VOID* pUMVertices)
// with the user vertex-array pointer passed as the SEPARATE third argument (x64: r8 / x86: last stack
// slot), never inside the struct. Verified against two distinct strides (0x14 and 0x20) on both
// arches; StreamNumber read back as 0 for the UP path (DrawPrimitiveUP always uses stream 0). Two
// plain UINTs, so the layout is identical for x64 and x86.
typedef struct _D3DDDIARG_SETSTREAMSOURCEUM
{
    UINT StreamNumber;
    UINT Stride;
} D3DDDIARG_SETSTREAMSOURCEUM;
static_assert(sizeof(D3DDDIARG_SETSTREAMSOURCEUM) == 8, "D3DDDIARG_SETSTREAMSOURCEUM layout (identical x64/x86)");

typedef struct _D3DDDIARG_SETSTREAMSOURCEFREQ
{
    UINT StreamNumber;
    UINT Divider;
} D3DDDIARG_SETSTREAMSOURCEFREQ;

typedef struct _D3DDDIARG_SETINDICES
{
    HANDLE hIndexBuffer;
    UINT Stride; // 2 = 16-bit indices, 4 = 32-bit indices
} D3DDDIARG_SETINDICES;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_SETINDICES) == 16, "D3DDDIARG_SETINDICES x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (hIndexBuffer shrinks from 8 to 4 bytes);
// compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_SETINDICES) == 8, "D3DDDIARG_SETINDICES x86 layout");
#endif

// RE-verified live 2026-07-06 (x64 AND x86): the UP-draw index variant pfnSetIndicesUm (device-func-
// table slot 9, reached from within DrawIndexedPrimitiveUP) takes NO struct pointer. Like pfnSetTexture
// above, it is a direct scalar DDI call:
//   HRESULT APIENTRY (HANDLE hDevice, UINT Stride, CONST VOID* pUMIndices)
// where Stride is the index element size in BYTES passed as an immediate value (read back as 2 for
// D3DFMT_INDEX16 and 4 for D3DFMT_INDEX32, both verified, on both arches -- attempting to dereference it
// as a pointer faults) and pUMIndices is the user index-array pointer (x64: r8 / x86: last stack slot).
// There is intentionally no D3DDDIARG_SETINDICESUM struct.

typedef struct _D3DDDIARG_SETRENDERTARGET
{
    UINT RenderTargetIndex;
    HANDLE hRenderTarget; // 0 unbinds
} D3DDDIARG_SETRENDERTARGET;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_SETRENDERTARGET) == 16, "D3DDDIARG_SETRENDERTARGET x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (hRenderTarget shrinks from 8 to 4 bytes);
// compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_SETRENDERTARGET) == 8, "D3DDDIARG_SETRENDERTARGET x86 layout");
#endif

typedef struct _D3DDDIARG_SETDEPTHSTENCIL
{
    HANDLE hZBuffer; // 0 = no depth-stencil
} D3DDDIARG_SETDEPTHSTENCIL;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_SETDEPTHSTENCIL) == 8, "D3DDDIARG_SETDEPTHSTENCIL x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (hZBuffer shrinks from 8 to 4 bytes);
// compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_SETDEPTHSTENCIL) == 4, "D3DDDIARG_SETDEPTHSTENCIL x86 layout");
#endif

typedef struct _D3DDDIARG_VIEWPORTINFO
{
    UINT X;
    UINT Y;
    UINT Width;
    UINT Height;
} D3DDDIARG_VIEWPORTINFO;

typedef struct _D3DDDIARG_ZRANGE
{
    FLOAT MinZ;
    FLOAT MaxZ;
} D3DDDIARG_ZRANGE;

// Matches RECT's field order/widths exactly.
typedef struct _D3DDDIRECT
{
    LONG left;
    LONG top;
    LONG right;
    LONG bottom;
} D3DDDIRECT;

// pfnCreateVertexShaderFunc/pfnCreatePixelShader ARE struct-pointer DDI calls after all -- an earlier
// RE pass (see d3d9-shader-test's original DrawPrimitive/E_OUTOFMEMORY investigation) mis-identified
// the convention. Live tracing of the real call sites (CD3DDDIDX10TL::CreateVertexShaderFunc's call
// through its device-func table at +336 bytes = slot 42*8, and CD3DDDIDX10::CreatePixelShader's at
// +536 bytes = slot 67*8) shows three real arguments: `(HANDLE hDevice, D3DDDIARG_CREATESHADERFUNC*
// pArgs, CONST UINT* pFunction)`. `pArgs` is a small in/out struct: `CodeSize` (in, the token stream's
// byte length -- the runtime already knows this, no self-parsing needed) at offset 0, `ShaderHandle`
// (out) at offset 8. `pFunction` is a separate pointer to the raw token stream. See
// umd_CreateVertexShaderFunc/umd_CreatePixelShader/create_shader_common in sogen_d3d9_umd.cpp.

typedef struct _D3DDDIARG_CREATESHADERFUNC
{
    UINT CodeSize;       // in: token stream size in bytes
    HANDLE ShaderHandle; // out: driver-assigned shader handle
} D3DDDIARG_CREATESHADERFUNC;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_CREATESHADERFUNC) == 16, "D3DDDIARG_CREATESHADERFUNC x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (ShaderHandle shrinks from 8 to 4 bytes);
// compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_CREATESHADERFUNC) == 8, "D3DDDIARG_CREATESHADERFUNC x86 layout");
#endif

// pfnSetPixelShader/pfnSetVertexShaderFunc are direct-value calls, `(HANDLE hDevice, HANDLE hShader)`
// -- same crash-driven RE finding as pfnSetTexture (see umd_SetPixelShader/umd_SetVertexShaderFunc in
// sogen_d3d9_umd.cpp); no D3DDDIARG_SET*SHADERFUNC struct exists on the wire for these two slots.

typedef struct _D3DDDIARG_DELETEPIXELSHADERFUNC
{
    HANDLE ShaderHandle;
} D3DDDIARG_DELETEPIXELSHADERFUNC;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_DELETEPIXELSHADERFUNC) == 8, "D3DDDIARG_DELETEPIXELSHADERFUNC x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (ShaderHandle shrinks from 8 to 4 bytes);
// compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_DELETEPIXELSHADERFUNC) == 4, "D3DDDIARG_DELETEPIXELSHADERFUNC x86 layout");
#endif

typedef struct _D3DDDIARG_DELETEVERTEXSHADERFUNC
{
    HANDLE ShaderHandle;
} D3DDDIARG_DELETEVERTEXSHADERFUNC;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_DELETEVERTEXSHADERFUNC) == 8, "D3DDDIARG_DELETEVERTEXSHADERFUNC x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (ShaderHandle shrinks from 8 to 4 bytes);
// compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_DELETEVERTEXSHADERFUNC) == 4, "D3DDDIARG_DELETEVERTEXSHADERFUNC x86 layout");
#endif

// SUPERSEDED (RE-corrected live, this Task-9 test): pfnSetVertexShaderDecl does NOT take a pointer to
// this struct -- it's a DIRECT-VALUE HANDLE call, same convention as pfnSetVertexShaderFunc/
// pfnSetPixelShader. umd_SetVertexShaderDecl (sogen_d3d9_umd.cpp) now takes a plain HANDLE parameter;
// this type is kept only for the static_asserts' historical record and is otherwise unused.
typedef struct _D3DDDIARG_SETVERTEXSHADERDECL
{
    HANDLE ShaderHandle;
} D3DDDIARG_SETVERTEXSHADERDECL;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_SETVERTEXSHADERDECL) == 8, "D3DDDIARG_SETVERTEXSHADERDECL x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (ShaderHandle shrinks from 8 to 4 bytes);
// compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_SETVERTEXSHADERDECL) == 4, "D3DDDIARG_SETVERTEXSHADERDECL x86 layout");
#endif

// RE-verified live (this Task-9 test, via a diagnostic byte dump of pArgs -- see
// umd_CreateVertexShaderDecl's own comment): NumVertexElements comes FIRST (offset 0), not
// ShaderHandle -- a real, distinct 2-element D3DVERTEXELEMENT9 array read back as
// pArgs->NumVertexElements == 2 (matching bytes 0-3) once this field order was corrected; the
// original guess (ShaderHandle first) instead decoded a garbage/padding 64-bit value at offset 0
// as ShaderHandle and read NumVertexElements == 0 from what is actually just the 4 bytes of
// x64 alignment padding before the 8-byte-aligned HANDLE at offset 8.
typedef struct _D3DDDIARG_CREATEVERTEXSHADERDECL
{
    UINT NumVertexElements; // in
    HANDLE ShaderHandle;    // out (offset 8 on x64: 4 bytes of alignment padding follow NumVertexElements)
    // D3DDDIVERTEXELEMENT elements[NumVertexElements] follow the struct in memory
} D3DDDIARG_CREATEVERTEXSHADERDECL;
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_CREATEVERTEXSHADERDECL) == 16, "D3DDDIARG_CREATEVERTEXSHADERDECL x64 layout");
#else
// x86 layout: HANDLE shrinks from 8 to 4 bytes, and needs no alignment padding after the leading
// UINT; compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_CREATEVERTEXSHADERDECL) == 8, "D3DDDIARG_CREATEVERTEXSHADERDECL x86 layout");
#endif

// Matches D3DVERTEXELEMENT9's real 8-byte layout.
typedef struct _D3DDDIVERTEXELEMENT
{
    WORD Stream;
    WORD Offset;
    BYTE Type;
    BYTE Method;
    BYTE Usage;
    BYTE UsageIndex;
} D3DDDIVERTEXELEMENT;

// pfnSetVertexShaderConst/pfnSetPixelShaderConst take the float array as a separate third DDI
// argument (CONST FLOAT*), not trailing bytes after this header -- matching the same
// header-plus-separate-array-pointer(s) shape already RE-confirmed for pfnClear (D3DDDIARG_CLEAR's
// own comment). A first attempt assuming trailing-bytes read zeroed/garbage constant data (the
// runtime's own automatic zero-init shadow calls made this invisible until a real, distinctive
// non-zero SetVertexShaderConstantF/SetPixelShaderConstantF value was round-tripped end to end).
typedef struct _D3DDDIARG_SETVERTEXSHADERCONST
{
    UINT Register;
    UINT Count; // number of 4-float vectors
} D3DDDIARG_SETVERTEXSHADERCONST;

typedef struct _D3DDDIARG_SETPIXELSHADERCONST
{
    UINT Register;
    UINT Count;
} D3DDDIARG_SETPIXELSHADERCONST;

// RE-confirmed against the real d3d9.dll (CBatchFilterI::LHBatchSetVertexShaderConstI/B and
// ::LHBatchSetPixelShaderConstI/B): each takes this header as one argument and the INT/BOOL data
// as a SEPARATE second argument (CONST INT*), not trailing bytes -- their own fallback path calls
// straight through the device func table as (hDevice, pArgs, pData), the exact same
// header-plus-separate-array-pointer shape already confirmed for SETVERTEXSHADERCONST/
// SETPIXELSHADERCONST above. (The real runtime's internal batch layer reuses the float header
// struct type for its I/B calls, but the layout is identical to the one below.)
typedef struct _D3DDDIARG_SETVERTEXSHADERCONSTI
{
    UINT Register;
    UINT Count; // number of int4 vectors; the int32 data is a separate CONST INT* DDI argument
} D3DDDIARG_SETVERTEXSHADERCONSTI;

typedef struct _D3DDDIARG_SETPIXELSHADERCONSTI
{
    UINT Register;
    UINT Count;
} D3DDDIARG_SETPIXELSHADERCONSTI;

typedef struct _D3DDDIARG_SETVERTEXSHADERCONSTB
{
    UINT Register;
    UINT Count; // number of BOOLs (4 bytes each); the data is a separate CONST BOOL* DDI argument
} D3DDDIARG_SETVERTEXSHADERCONSTB;

typedef struct _D3DDDIARG_SETPIXELSHADERCONSTB
{
    UINT Register;
    UINT Count;
} D3DDDIARG_SETPIXELSHADERCONSTB;

// RE-verified against the real d3d9.dll (CBatchFilterI::LHBatchClear, which copies exactly one
// OWORD i.e. 16 bytes from the caller's D3DDDIARG_CLEAR*): NumRect/the rect array are NOT struct
// fields -- pfnClear's real signature is (HANDLE, CONST D3DDDIARG_CLEAR*, UINT NumRect, CONST
// RECT* pRect), matching LHBatchClear's own (this, pClear, NumRect, pRect) parameters exactly.
typedef struct _D3DDDIARG_CLEAR
{
    UINT Flags; // D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL
    UINT Color;
    FLOAT Z;
    UINT Stencil;
} D3DDDIARG_CLEAR;
static_assert(sizeof(D3DDDIARG_CLEAR) == 16);

typedef struct _D3DDDIARG_DRAWPRIMITIVE
{
    UINT PrimitiveType; // D3DDDIPRIMITIVETYPE
    UINT VStart;
    UINT PrimitiveCount;
} D3DDDIARG_DRAWPRIMITIVE;

typedef struct _D3DDDIARG_DRAWINDEXEDPRIMITIVE
{
    UINT PrimitiveType;
    INT BaseVertexIndex;
    UINT MinIndex;
    UINT NumVertices;
    UINT StartIndex;
    UINT PrimitiveCount;
} D3DDDIARG_DRAWINDEXEDPRIMITIVE;

// Task 4c (2026-07-04) fully RE'd the real 48-byte (x64) struct: a live trace captured pfnTexBlt's
// return address into d3d9.dll (0x180031255) and idasql-decompiled the real caller, CD3DDDIDX10::TexBlt.
// That function builds this exact struct, field by field, from its own four parameters --
// `(void* hDstResource, void* hSrcResource, tagPOINT* pDstPoint, RECTL* pSrcRect)` -- and passes a
// pointer to it straight to the real device-func-table slot 18 (offset 144 = slot 18 * 8, i.e.
// pfnTexBlt itself). Every byte of the 48 bytes is accounted for; there is no pixel-data pointer
// anywhere in it (and no other DDI call this driver receives during a full, live-traced
// D3DPOOL_MANAGED texture lifecycle carries one either -- see umd_TexBlt's own comment in
// sogen_d3d9_umd.cpp for the full trace and conclusion).
#ifdef _WIN64
typedef struct _D3DDDIARG_TEXBLT
{
    HANDLE hDstResource;      // 0
    HANDLE hSrcResource;      // 8
    UINT DstSubResourceIndex; // 16 -- a resource-wrapper-derived index (CD3DDDIDX10::TexBlt computes
                              // it as a division of two fields read from its own hDstResource wrapper
                              // object, not from this struct); always observed 0 live, consistent with
                              // this milestone's single-mip, single-subresource textures.
    LONG DstPointX;           // 20 -- from *pDstPoint
    LONG DstPointY;           // 24
    D3DDDIRECT SrcRect;       // 28, 16 bytes (left/top/right/bottom) -- from *pSrcRect
    UINT Reserved;            // 44 -- always 0, confirmed via decompile (CD3DDDIDX10::TexBlt's own v18)
} D3DDDIARG_TEXBLT;
static_assert(sizeof(D3DDDIARG_TEXBLT) == 48, "size confirmed via real d3d9.dll RE (CD3DDDIDX10::TexBlt)");
#else
// x86 shape live-RE'd the same way (idasql decompile of the real 32-bit d3d9.dll's own
// CD3DDDIDX10::TexBlt, d3d9_x86.dll.i64 address 0x100656d0) -- NOT a guess extrapolated from the x64
// layout above, an independent decompile of the genuine 32-bit function. It is exactly the x64 shape
// with every HANDLE shrunk to 4 bytes (this driver's own `HANDLE` is `void*`, already 4 bytes in an
// x86 build) and every later offset shifted down by 8 to match -- 40 bytes total, not 48:
//   v13[0] = *a2 (hDstResource, offset 0), v14 = a2[1]/a2[2] (subresource index, offset 8),
//   v13[1] = *a3 (hSrcResource, offset 4), x = a4->x (offset 12), v16 = a4->y (offset 16),
//   v17..v20 = *a5 (SrcRect, offset 20, 16 bytes), v21 = 0 (Reserved, offset 36).
typedef struct _D3DDDIARG_TEXBLT
{
    HANDLE hDstResource;      // 0
    HANDLE hSrcResource;      // 4 -- NOT 8: HANDLE is 4 bytes on x86, unlike the x64 shape above.
    UINT DstSubResourceIndex; // 8
    LONG DstPointX;           // 12 -- from *pDstPoint
    LONG DstPointY;           // 16
    D3DDDIRECT SrcRect;       // 20, 16 bytes (left/top/right/bottom) -- from *pSrcRect
    UINT Reserved;            // 36 -- always 0, confirmed via decompile
} D3DDDIARG_TEXBLT;
static_assert(sizeof(D3DDDIARG_TEXBLT) == 40, "size confirmed via real d3d9.dll RE (CD3DDDIDX10::TexBlt, x86)");
#endif

// D3DDDIARG_COLORFILL -- pfnColorFill, device-func-table slot 56 (behind IDirect3DDevice9::ColorFill).
// RE'd this session (2026-07-06) to the same standard as D3DDDIARG_TEXBLT above: BOTH static
// decompilation AND live tracing of the real staged d3d9.dll.
//   * Static: idasql-decompiled the real builder CD3DDDIDX10::Colorfill (d3d9_x64.dll.i64 @ 0x180042428;
//     d3d9_x86.dll.i64 @ 0x1013E4C6). It constructs this exact struct field-by-field from its own
//     parameters (void* hResource-wrapper, D3DDDIRECT* rect, D3DCOLOR color) and passes &struct straight
//     to the device-func slot -- x64 `(*(v7+448))(...)` (448 = 56*8), x86 `(*(...+224))(...)` (224 = 56*4)
//     -- confirming slot 56 == pfnColorFill exactly as this file's device-func-table documents it. The
//     independent batch consumer CBatchFilterI::LHBatchColorFill copies precisely 40 bytes (x64: OWORD@0 +
//     OWORD@16 + QWORD@36) / 32 bytes (x86: qmemcpy 0x20) and ReferenceResource()s offset 0, cross-
//     confirming size and the resource-handle position.
//   * Live (x64): a guest probe called ColorFill(rt, {L=0x11,T=0x22,R=0x33,B=0x44}, 0xDEADBEEF) on a real
//     default-pool render target; a temporary slot-56 dump handler captured the exact bytes:
//       05 00 01 00 00 00 00 00 | 00 00 00 00 | 11 00 00 00 22 00 00 00 33 00 00 00 44 00 00 00 |
//       ef be ad de | 00 00 00 00 00 00 00 00
//     i.e. hResource=0x10005 @0, SubResourceIndex=0 @8, DstRect{0x11,0x22,0x33,0x44} @12, Color=0xDEADBEEF
//     @28, trailing 8 zero bytes @32 -- matching the decompile byte-for-byte.
#ifdef _WIN64
typedef struct _D3DDDIARG_COLORFILL
{
    HANDLE hResource;    // 0
    UINT SubResourceIndex; // 8 -- read from the hResource wrapper (CD3DDDIDX10::Colorfill's a2[2]);
                           // 0 observed live (single-subresource surface).
    D3DDDIRECT DstRect;  // 12, 16 bytes (left/top/right/bottom)
    UINT Color;          // 28 -- D3DCOLOR (ARGB)
    UINT Flags;          // 32 -- always 0 (the builder zeroes an 8-byte slot here; the second UINT is
                         // struct tail padding on x64). Field name per the WDK; value never seen non-zero.
} D3DDDIARG_COLORFILL;
static_assert(sizeof(D3DDDIARG_COLORFILL) == 40,
              "size confirmed via real d3d9.dll RE (CD3DDDIDX10::Colorfill + LHBatchColorFill, x64) + live trace");
#else
// x86 shape decompile-RE'd independently (d3d9_x86.dll.i64 CD3DDDIDX10::Colorfill @ 0x1013E4C6 +
// LHBatchColorFill's qmemcpy 0x20) -- the x64 shape with HANDLE shrunk to 4 bytes and the trailing
// zero field 4 bytes (int) instead of 8. 32 bytes total. Not live-traced (no x86 probe run this session,
// matching the D3DDDIARG_TEXBLT x86 precedent); layout is an independent decompile, not an extrapolation.
typedef struct _D3DDDIARG_COLORFILL
{
    HANDLE hResource;    // 0
    UINT SubResourceIndex; // 4 -- read from the hResource wrapper (a2[1] on x86)
    D3DDDIRECT DstRect;  // 8, 16 bytes (left/top/right/bottom)
    UINT Color;          // 24 -- D3DCOLOR (ARGB)
    UINT Flags;          // 28 -- always 0 (builder's `v10 = 0`)
} D3DDDIARG_COLORFILL;
static_assert(sizeof(D3DDDIARG_COLORFILL) == 32,
              "size confirmed via real d3d9.dll RE (CD3DDDIDX10::Colorfill + LHBatchColorFill, x86)");
#endif

// D3DDDIARG_BLT -- pfnBlt, device-func-table slot 55 (behind IDirect3DDevice9::StretchRect). A DIFFERENT
// call from pfnTexBlt: a general source->destination surface blit, not a texture sysmem/vidmem sync.
// RE'd this session (2026-07-06) to the same BOTH-static-AND-live standard.
//   * Static: idasql-decompiled the real builder CD3DDDIDX10::Blt (d3d9_x64.dll.i64 @ 0x180031016;
//     d3d9_x86.dll.i64 @ 0x10062CB0). It builds the struct from two {handle, subresource, rect} pairs
//     plus a flags word and passes &struct to the device-func slot -- x64 `(*(v14+440))(...)` (440 = 55*8),
//     x86 `(*(v11+220))(...)` (220 = 55*4) -- confirming slot 55 == pfnBlt. The independent consumer
//     CBatchFilterI::LHBatchBlt copies precisely 72 bytes (x64) / 56 bytes (x86, qmemcpy 0x38) and
//     ReferenceResource()s offsets 0 and 32 (x64) / 0 and 24 (x86), cross-confirming size and BOTH handle
//     positions.
//   * Live (x64): a guest probe called StretchRect(src, {0x0A,0x0B,0x5A,0x4B}, dst, {0x64,0x65,0xB4,0xA5},
//     D3DTEXF_NONE) between two real default-pool render targets; a temporary slot-55 dump handler captured:
//       06 00 01 00 00 00 00 00 | 00 00 00 00 | 0a.. 0b.. 5a.. 4b.. | 01 00 00 00 |
//       07 00 01 00 00 00 00 00 | 00 00 00 00 | 64.. 65.. b4.. a5.. | 00 00 00 00 | 00 00 00 00
//     The SOURCE surface (id 0x10006, created before the dest 0x10007) and the SOURCE rect land at
//     offsets 0/12; the DEST surface and DEST rect at offsets 32/44. This live result CORRECTED the field
//     ordering: the struct is Src-first-then-Dst (matching the real WDK D3DDDIARG_BLT), which the bare
//     decompile alone did not disambiguate. Offset 8/40 SubResourceIndex both 0; offset 60 Reserved 0;
//     offset 64 Flags 0 (D3DTEXF_NONE). Bytes at offset 28 (the x64 alignment pad before hDstResource) and
//     past offset 68 (tail pad) are uninitialized stack -- not struct fields.
#ifdef _WIN64
typedef struct _D3DDDIARG_BLT
{
    HANDLE hSrcResource;      // 0
    UINT SrcSubResourceIndex; // 8  -- 0 observed live
    D3DDDIRECT SrcRect;       // 12, 16 bytes (left/top/right/bottom)
    // 4 bytes natural alignment padding here (offset 28) so hDstResource is 8-aligned -- NOT a field.
    HANDLE hDstResource;      // 32
    UINT DstSubResourceIndex; // 40 -- 0 observed live
    D3DDDIRECT DstRect;       // 44, 16 bytes (left/top/right/bottom)
    UINT Reserved;            // 60 -- always 0 (builder's `v23 = 0`)
    UINT Flags;               // 64 -- the StretchRect Filter / blt-flags word (CD3DDDIDX10::Blt's last
                              // arg); 0 == D3DTEXF_NONE observed live. Scaling is inferred by the driver
                              // from the Src/Dst rect-size ratio (the DDI carries no explicit scale field).
} D3DDDIARG_BLT;
static_assert(sizeof(D3DDDIARG_BLT) == 72,
              "size confirmed via real d3d9.dll RE (CD3DDDIDX10::Blt + LHBatchBlt, x64) + live trace");
#else
// x86 shape decompile-RE'd independently (d3d9_x86.dll.i64 CD3DDDIDX10::Blt @ 0x10062CB0 + LHBatchBlt's
// qmemcpy 0x38). Same field ORDER as the x64 layout above (Src-first, cross-validated by the x64 live
// trace and the WDK), with HANDLEs 4 bytes and no alignment pad (every field is naturally 4-aligned), so
// hDstResource sits at 24 not 32. 56 bytes total. Not live-traced (matching the TexBlt x86 precedent).
typedef struct _D3DDDIARG_BLT
{
    HANDLE hSrcResource;      // 0
    UINT SrcSubResourceIndex; // 4
    D3DDDIRECT SrcRect;       // 8, 16 bytes
    HANDLE hDstResource;      // 24 -- NOT 32: no alignment pad on x86 (HANDLE is 4 bytes)
    UINT DstSubResourceIndex; // 28
    D3DDDIRECT DstRect;       // 32, 16 bytes
    UINT Reserved;            // 48 -- always 0 (builder's `v20 = 0`)
    UINT Flags;               // 52 -- Filter / blt-flags (builder's a10)
} D3DDDIARG_BLT;
static_assert(sizeof(D3DDDIARG_BLT) == 56,
              "size confirmed via real d3d9.dll RE (CD3DDDIDX10::Blt + LHBatchBlt, x86)");
#endif

// D3DDDI_SURFACEINFO and D3DDDIARG_CREATERESOURCE -- RE'd this session (2026-07-05) the same way the
// sibling structs above were, and to the same standard: BOTH static decompilation of the real staged
// d3d9.dll's resource-creation path AND live tracing of the actual field values through a real
// pfnCreateResource call, on x64 AND x86, with three distinct non-640x480 texture sizes so a
// coincidental match against this milestone's old hardcoded 640x480 shape could not masquerade as a
// confirmed offset.
//
// INDEPENDENTLY, LIVE-confirmed (a distinct expected value read back at the stated offset for each of
// the three probe sizes, on both arches): D3DDDIARG_CREATERESOURCE::{Format, Pool, pSurfList, SurfCount,
// MipLevels, hResource, Flags} and D3DDDI_SURFACEINFO::{Width, Height, Depth}.
//
// NOT independently live-confirmed -- INFERRED only: D3DDDI_SURFACEINFO::{pSysMem, SysMemPitch,
// SysMemSlicePitch}. These were 0 in every probe (consistent with D3DPOOL_DEFAULT resources created
// with no initial system-memory data), so their offsets are derived from the confirmed element stride
// (the live-confirmed sizeof one D3DDDI_SURFACEINFO) plus the standard WDK field order, not observed
// live. Structurally sound but weaker evidence than the dimension fields above -- do not rely on them
// until a forcing function (a D3DPOOL_SYSTEMMEM create carrying real init data) pins them live.
typedef struct _D3DDDI_SURFACEINFO
{
    UINT Width;            // 0 -- live-confirmed
    UINT Height;           // 4 -- live-confirmed
    UINT Depth;            // 8 -- live-confirmed (1 for 2D surfaces)
    VOID* pSysMem;         // x64:16 / x86:12 -- INFERRED (0 in every probe; see block comment above). On
                          // x64, 4 bytes of alignment padding precede this (Depth ends at 12, an 8-byte
                          // pointer aligns up to 16); on x86 the 4-byte pointer already sits at 12.
    UINT SysMemPitch;      // x64:24 / x86:16 -- INFERRED
    UINT SysMemSlicePitch; // x64:28 / x86:20 -- INFERRED
} D3DDDI_SURFACEINFO;
#ifdef _WIN64
static_assert(offsetof(D3DDDI_SURFACEINFO, Width) == 0, "D3DDDI_SURFACEINFO x64 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, Height) == 4, "D3DDDI_SURFACEINFO x64 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, Depth) == 8, "D3DDDI_SURFACEINFO x64 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, pSysMem) == 16, "D3DDDI_SURFACEINFO x64 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, SysMemPitch) == 24, "D3DDDI_SURFACEINFO x64 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, SysMemSlicePitch) == 28, "D3DDDI_SURFACEINFO x64 layout");
static_assert(sizeof(D3DDDI_SURFACEINFO) == 32, "D3DDDI_SURFACEINFO x64 element stride (live-confirmed)");
#else
static_assert(offsetof(D3DDDI_SURFACEINFO, Width) == 0, "D3DDDI_SURFACEINFO x86 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, Height) == 4, "D3DDDI_SURFACEINFO x86 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, Depth) == 8, "D3DDDI_SURFACEINFO x86 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, pSysMem) == 12, "D3DDDI_SURFACEINFO x86 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, SysMemPitch) == 16, "D3DDDI_SURFACEINFO x86 layout");
static_assert(offsetof(D3DDDI_SURFACEINFO, SysMemSlicePitch) == 20, "D3DDDI_SURFACEINFO x86 layout");
static_assert(sizeof(D3DDDI_SURFACEINFO) == 24, "D3DDDI_SURFACEINFO x86 element stride (live-confirmed)");
#endif

// hResource@48/44 re-confirms the struct version this UMD sees: umd_CreateResource's own live
// sentinel-scan (Task 3b, 2026-07-04) already independently pinned that offset the same way, so a fresh
// build whose resources still round-trip correctly is a standing re-verification that these offsets
// describe the real struct (see umd_CreateResource's comment). Flags is D3DDDI_RESOURCEFLAGS, whose low
// two bits (RenderTarget=0x1, ZBuffer=0x2) numerically coincide with the D3DUSAGE_RENDERTARGET/
// D3DUSAGE_DEPTHSTENCIL bits the host's create_resource actually tests -- umd_CreateResource translates
// them explicitly rather than relying on that coincidence.
#ifdef _WIN64
typedef struct _D3DDDIARG_CREATERESOURCE
{
    UINT Format;                         // 0 -- live-confirmed (the only field read pre-RE)
    UINT Pool;                           // 4 -- live-confirmed (D3DDDI_POOL)
    UINT MultisampleType;                // 8 -- present
    UINT MultisampleQuality;             // 12 -- present
    const D3DDDI_SURFACEINFO* pSurfList; // 16 -- live-confirmed (pointer to per-subresource dim array)
    UINT SurfCount;                      // 24 -- live-confirmed (array length)
    UINT MipLevels;                      // 28 -- live-confirmed
    UINT Fvf;                            // 32
    UINT VidPnSourceId;                  // 36
    UINT RefreshRateNumerator;           // 40 -- D3DDDI_RATIONAL.Numerator
    UINT RefreshRateDenominator;         // 44 -- D3DDDI_RATIONAL.Denominator
    HANDLE hResource;                    // 48 -- live-confirmed in/out driver handle (umd_CreateResource
                                         //       writes it here; see its sentinel-scan comment)
    UINT Flags;                          // 56 -- live-confirmed (D3DDDI_RESOURCEFLAGS usage bitfield)
    BYTE ReservedTail[12];               // 60..71 -- Rotation/Flags2/etc.: region size-confirmed (total
                                         //           struct is 72), individual fields not pinned
} D3DDDIARG_CREATERESOURCE;
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, Format) == 0, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, Pool) == 4, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, MultisampleType) == 8, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, MultisampleQuality) == 12, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, pSurfList) == 16, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, SurfCount) == 24, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, MipLevels) == 28, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, hResource) == 48, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, Flags) == 56, "D3DDDIARG_CREATERESOURCE x64 layout");
static_assert(sizeof(D3DDDIARG_CREATERESOURCE) == 72, "size confirmed via real d3d9.dll RE (x64)");
#else
typedef struct _D3DDDIARG_CREATERESOURCE
{
    UINT Format;                         // 0 -- live-confirmed
    UINT Pool;                           // 4 -- live-confirmed
    UINT MultisampleType;                // 8 -- present
    UINT MultisampleQuality;             // 12 -- present
    const D3DDDI_SURFACEINFO* pSurfList; // 16 -- live-confirmed (4-byte pointer on x86)
    UINT SurfCount;                      // 20 -- live-confirmed
    UINT MipLevels;                      // 24 -- live-confirmed
    UINT Fvf;                            // 28
    UINT VidPnSourceId;                  // 32
    UINT RefreshRateNumerator;           // 36
    UINT RefreshRateDenominator;         // 40
    HANDLE hResource;                    // 44 -- live-confirmed (sentinel-scan read back 0xAAAA002C,
                                         //       offset 0x2C = 44; see umd_CreateResource's comment)
    UINT Flags;                          // 48 -- live-confirmed
    BYTE ReservedTail[8];                // 52..59 -- region size-confirmed (total struct is 60)
} D3DDDIARG_CREATERESOURCE;
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, Format) == 0, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, Pool) == 4, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, MultisampleType) == 8, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, MultisampleQuality) == 12, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, pSurfList) == 16, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, SurfCount) == 20, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, MipLevels) == 24, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, hResource) == 44, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(offsetof(D3DDDIARG_CREATERESOURCE, Flags) == 48, "D3DDDIARG_CREATERESOURCE x86 layout");
static_assert(sizeof(D3DDDIARG_CREATERESOURCE) == 60, "size confirmed via real d3d9.dll RE (x86)");
#endif

// hResource@0 and pData@40 are RE-verified and reliable across every resource kind and routing path
// (confirmed live 2026-07-03/2026-07-04 -- see below). SizeToLock is deliberately NOT modeled as a
// named field (no reliable offset exists for it in either routing path -- see below); OffsetToLock IS
// named (Task 6, 2026-07-04) since it has one reliable offset (80) in the routing path this UMD's own
// DevCaps bits actually enable (see the field's own comment for why reading it unconditionally, even
// on the other routing path, is safe). Real d3d9.dll builds this 104-byte struct from at least TWO
// DIFFERENT code paths depending on internal buffer routing (see HANDOFF_MACBOOK.md for the full
// capture), and the two paths disagree on which offset (if any) carries which value:
//   - "sysmem-routed" buffers (CVertexBuffer::Lock / CIndexBuffer::Lock's own direct dispatch, taken
//     when CreateXxxBuffer's DevCaps-gated routing picks CreateSysmemXxxBuffer): offset 72 reliably
//     carries the app's requested SizeToLock (confirmed across 4 distinct sizes: 12, 12, 6, 40 bytes);
//     OffsetToLock has no field at all here (confirmed by locking at a distinctive nonzero offset, 96,
//     and finding that value nowhere in the captured bytes) -- the driver is only ever asked to lock
//     from its own resource's base; the runtime adds OffsetToLock to the driver's returned base
//     pointer itself, after the DDI call returns.
//   - "driver-routed" buffers (CDriverVertexBuffer::Lock/CDriverIndexBuffer::Lock -> ::LockI, taken
//     once the real per-resource-kind DevCaps routing bit is set -- see fill_d3d9caps's
//     k_devcaps_driver_managed_pool/k_devcaps_driver_managed_index_pool): offset 80 carries the app's
//     requested OffsetToLock instead (confirmed: locking at offsets 6 and 96 on two different buffers
//     read back exactly 6 and 96 at this offset); SizeToLock has no reliable field in THIS shape --
//     the offset umd_Lock originally also tried (72) holds an unrelated caller-stack address here, not
//     a size (confirmed: it produced a ~25MB "size" once index buffers started using this path).
// umd_Lock is one resource-kind- and routing-path-agnostic function (by design -- see its own
// comment) and cannot statically know which shape a given call used -- but per-call detection turns
// out unnecessary on x64 (Task 6, 2026-07-04): fill_d3d9caps's k_devcaps_driver_managed_pool/
// k_devcaps_driver_managed_index_pool are set UNCONDITIONALLY, so every real D3DPOOL_DEFAULT buffer
// (the common case, including every D3DLOCK_NOOVERWRITE append) is always driver-routed in practice --
// offset 80 is real there. For the sysmem-routed shape, the app discards whatever pData/offset this
// driver returns regardless (confirmed live), so reading offset 80 as "OffsetToLock" in that shape too
// is harmless even though it isn't really that field there. umd_Lock therefore reads offset 80
// unconditionally on x64. resolve_buffer_resource_id's size-unknown fallback (size 0 = "to end of
// resource") still covers SizeToLock, which no shape reliably carries.
//
// x86 note: this x64 layout does NOT carry over -- see the separate x86 definition below. Task 6's
// live idasql RE (2026-07-04) found the x86 struct is genuinely different, not just pointer-shrunk.
//
// SubResourceIndex -- RE-verified (2026-07-06, BOTH live AND static) at offset 8 (x64) / 4 (x86). This
// is the field that identifies WHICH subresource (mip level, cube face, volume slice) of a resource a
// LockRect(level)/LockRect(face,level)/LockBox(level) call targets. The field previously modeled here
// as "Reserved0" IS the SubResourceIndex -- confirmed decisively, refuting an earlier static-only
// reading of CDriverMipSurface::InternalLockRect (whose 84-byte OUTER bookkeeping struct genuinely
// carries no such field; the level reaches the driver only through the SMALLER inner struct DdLockLH
// builds, below).
//   * Static: DdLockLH -- the single function that builds the struct actually crossing into pfnLock,
//     for EVERY resource kind and routing path that reaches the driver -- writes, as the 2nd field of
//     its local wire struct, `*(DWORD*)(resource_context + 8)` (x64, DdLockLH @ 0x180030ba0) /
//     `*(DWORD*)(resource_context + 4)` (x86, DdLockLH @ 0x10065460), i.e. the per-subresource index
//     stored on the surface object the runtime selected. This is the same "index derived from the
//     resource wrapper" mechanism D3DDDIARG_TEXBLT/COLORFILL/BLT already document. d3d9.dll selects the
//     surface object from the app's Level/FaceType itself (CMipMap::LockRect indexes an array of
//     per-level surface objects by Level; CCubeMap::LockRect by `Level + FaceType*MipLevels`), so the
//     flattened subresource value the driver sees is `Level` for a plain mip texture and (inferred from
//     that array-index formula, not yet live-confirmed for cube/volume specifically)
//     `FaceType*MipLevels + Level` for cube/array textures.
//   * Live: a 3-mip 2D texture (CreateTexture(64,64,3,D3DUSAGE_DYNAMIC,...D3DPOOL_DEFAULT)) locked at
//     levels 0/1/2 with a NULL rect. Hooking umd_Lock's entry (sogen's Python debugger API,
//     read-only) and dumping pArgs showed hResource@0 identical across all three calls, and offset
//     8 (x64) / 4 (x86) holding exactly {0, 1, 2} -- the level -- and NOTHING else in the struct
//     varying. On the buffer path (a real vertex-buffer Lock via the triangle test) the same offset
//     reads 0, as expected (buffers have no subresources).
//   * Safe to read UNCONDITIONALLY, by the same argument that already justifies OffsetToLock@80:
//     DdLockLH is the single builder for the driver-routed path that actually reaches pfnLock, and it
//     writes 0 there for buffers (correct) and the real level for textures/surfaces; the only other
//     path (sysmem-routed buffers) has its driver-returned output discarded by the app regardless. So
//     umd_Lock reads this field for every lock without per-call routing detection. As of the real
//     mip-mapping work, umd_Lock/umd_Unlock DO consume it (replacing the former hardcoded subresource
//     0): it rides the wire's `subresource` field and addresses the correct per-mip-level backing store
//     host-side (d3d9_host.cpp's extra_mips / subresource_backing).
#ifdef _WIN64
typedef struct _D3DDDIARG_LOCK
{
    HANDLE hResource;        // 0 -- confirmed
    UINT SubResourceIndex;   // 8 -- RE-verified live + static 2026-07-06 (see block comment above):
                             //      flattened subresource index (Level for mips; FaceType*MipLevels +
                             //      Level for cube/array). 0 for buffers. Consumed by umd_Lock as of the
                             //      real mip-mapping work (see block comment above).
    UINT Reserved0Hi;        // 12 -- always 0 live (high half of the former UINT64 Reserved0 slot)
    BYTE Reserved1[24];      // 16..39 -- Range/Box input region (DdLockLH's v30/v31); not modeled
    VOID* pData;         // 40 -- RE-verified live 2026-07-03; the real, correct output offset.
    BYTE Reserved2[32];  // 48..79 -- unconfirmed
    UINT OffsetToLock;   // 80 -- RE-verified live (see comment above): the app's requested byte offset,
                         // reliably present in the "driver-routed" shape. In the "sysmem-routed" shape
                         // this offset holds unrelated data instead, but that path's driver-returned
                         // pData is discarded by the app regardless (confirmed live,
                         // HANDOFF_MACBOOK.md #16.1), so umd_Lock reading this field unconditionally
                         // for buffer resources is safe either way -- the host's own lock() already
                         // rejects an out-of-range offset.
    BYTE Reserved3[20];  // 84..103 -- unconfirmed
} D3DDDIARG_LOCK;
#else
// x86 D3DDDIARG_LOCK is a GENUINELY DIFFERENT, smaller (48-byte) struct, not a pointer-shrunk copy of
// the x64 one above -- Task 6's RE (2026-07-04) found a prior x86 static_assert (hResource@0/pData@40,
// "unchanged from x64" on the theory that the UINT64 Reserved0 field's 8-byte alignment padding
// absorbs the pointer shrinkage) was wrong: the discrepancy from the real, live-observed layout (a
// naive single-offset fix landed at byte 68) was ~28 bytes, far more than alignment slop could explain.
//
// Root cause, found via idasql decompilation of the real 32-bit d3d9.dll (d3d9_x86.dll.i64):
// CDriverVertexBuffer::Lock (0x100761D0) and CDriverMipSurface::InternalLockRect (0x10045E70) each
// build their OWN 84-byte "outer" bookkeeping struct (21 DWORDs) and pass it to a SEPARATE function,
// DdLockLH (0x10065460) -- exactly the same two-tier shape the x64 struct above was already known to
// have (CDriverVertexBuffer::Lock's own 104-byte v14 vs. DdLockLH's real, 64-byte v28..v34, pData@40).
// A naive read of the OUTER struct alone (offset 68, where the outer struct's own bookkeeping copy of
// the result lands) is what caused the earlier crash: DdLockLH allocates only a 48-byte local struct
// (v29, memset'd to exactly sizeof(v29)=48) for the real DDI call, so writing a driver output at byte
// 68 (outside DdLockLH's 48-byte allocation) corrupts unrelated stack contents (DdLockLH's own other
// locals / saved registers -- consistent with the observed "SEH stack scaffolding" corruption and the
// null-vtable crash at teardown).
//
// The REAL struct crossing the DDI boundary is DdLockLH's own local `v29` (confirmed via three
// independent, cross-checked call sites: CDriverVertexBuffer::Lock -> DdLockLH, and
// CDriverMipSurface::InternalLockRect -> DdLockLH, both landing on the identical v29 shape; x64's
// already-proven-correct DdLockLH was also re-decompiled as a methodology sanity check and shows the
// exact same two-tier "outer struct calls DdLockLH, which builds its own smaller wire struct" pattern):
// `v29[0] = *(DWORD*)v1` (hResource, single dereference through the resource-context pointer -- same
// derivation x64's `v28 = *v1` uses for its own hResource@0), `v29[1] = *(DWORD*)(v1+4)` (Reserved0),
// `v29[2..7]` (OffsetToLock/SizeToLock, or Rect/Box input, depending on lock kind), `v29[8]` (pData,
// OUTPUT -- confirmed: `a1[17] = v29[8]` is exactly what the outer struct's own offset-68 bookkeeping
// copy at issue above is populated FROM), `v29[9]` (Pitch, OUTPUT), `v29[10]` (a conditional third
// OUTPUT field), `v29[11]` (Flags). Total struct size: 48 bytes (12 DWORDs), matching DdLockLH's own
// `memset(v29, 0, sizeof(v29))`.
//
// The call from Lock/InternalLockRect to DdLockLH is *indirect*, through a per-device, runtime-
// populated function-pointer field (raw disassembly: `mov esi, [esi+308h]` / `call esi` -- the ECX
// load right before it is just the x86 Control Flow Guard ABI setting up
// `___guard_check_icall_fptr`'s argument register, not a real "this"/hDevice parameter, and is
// unrelated to the actual 2-arg DDI call). A static xref pass cannot resolve an indirect call through
// a non-constant, heap/device-object-resident pointer to its runtime target, so
// `SELECT ... FROM xrefs WHERE to_ea = <DdLockLH> AND is_code=1` correctly finds zero code-xrefs for
// Lock/InternalLockRect -- the six functions it DOES find are DdLockLH's legacy-DirectDraw callers (an
// address-taken reference from _QueryLHDDICaps builds a *separate* legacy DirectDraw HAL callback
// table at a different offset -- not statically confirmed to be the same per-device slot that
// Lock/InternalLockRect index through when making their call) and are unrelated to whether
// Lock/InternalLockRect calls DdLockLH too.
//
// Confirmed LIVE (not just via decompilation) via sogen's own Python debugger API
// (`sogen.windows.create_application` + `hooks.memory_execution_at`, read-only registers/memory, no
// writes) against the real emulator, hooking the exact "call esi" instructions in the staged 32-bit
// d3d9.dll: the call target read from ESI at CDriverVertexBuffer::Lock's call site is
// `<d3d9.dll base> + 0x65460`, and at CDriverVertexBuffer::Unlock's call site is
// `<d3d9.dll base> + 0x65BF0` -- exactly DdLockLH's and DdUnlockLH's RVAs (0x10065460/0x10065BF0 minus
// the IDA image base 0x10000000), for every d3d9.dll module load observed. The same live session also
// hooked our own umd_Lock's entry and read `pArgs` (the runtime hidden-arg/CFG-artifact confusion at the
// call site does not affect the real callee args: pArgs lands correctly at the second stdcall stack
// slot) and confirmed `pArgs->hResource` reads a real, plausible resource handle and -- the decisive
// check -- the value umd_Lock writes to `pArgs->pData` (offset 32) is byte-for-byte identical to the
// pointer the guest app receives from `IDirect3DVertexBuffer9::Lock()` (both `0x41f46e0` in one capture).
typedef struct _D3DDDIARG_LOCK
{
    HANDLE hResource;    // 0 -- RE-verified live 2026-07-04 (see comment above)
    UINT SubResourceIndex; // 4 -- RE-verified live + static 2026-07-06 (see block comment above the x64
                         //      struct): flattened subresource index (Level for mips; FaceType*MipLevels
                         //      + Level for cube/array). 0 for buffers. Was "Reserved0"; NOT a UINT64.
                         //      Consumed by umd_Lock as of the real mip-mapping work.
    BYTE Reserved1[24];  // 8..31 -- unconfirmed (OffsetToLock/SizeToLock or Rect/Box input region)
    VOID* pData;         // 32 -- RE-verified live 2026-07-04; the real, correct output offset.
    BYTE Reserved2[12];  // 36..47 -- unconfirmed (Pitch/SlicePitch/Flags; not read by umd_Lock)
} D3DDDIARG_LOCK;
#endif

// RE-verified against the real staged d3d9.dll (CDriverVertexBuffer::Unlock, ddi.cpp) -- NOT guessed.
// Exactly 2 QWORDS, matching D3DDDIARG_LOCK's first two fields.
//
// x86 note: same two-tier pitfall as D3DDDIARG_LOCK -- CDriverVertexBuffer::Unlock's own 8-byte local
// struct is passed to DdUnlockLH (0x10065BF0), whose own local `v5` (2 DWORDS, 8 bytes total: `v5[0] =
// *v1` hResource, `v5[1] = v1[1]` Reserved0) is what actually crosses into pfnUnlock. Like
// D3DDDIARG_LOCK above, the call from CDriverVertexBuffer::Unlock to DdUnlockLH is an indirect call
// through a runtime-populated device-object field (`[edi+30Ch]`), invisible to a static xref query --
// live-verified the same way (hooking the exact "call esi" instruction with sogen's Python debugger
// API): the call target read from ESI is `<d3d9.dll base> + 0x65BF0`, exactly DdUnlockLH's RVA, for
// every observed module load. Unlike D3DDDIARG_LOCK, this one genuinely IS just a pointer-shrunk copy
// of the x64 shape (2 native-width slots, no extra fields), so it needs a size fix only, not a
// full field-order rewrite.
#ifdef _WIN64
typedef struct _D3DDDIARG_UNLOCK
{
    HANDLE hResource;        // 0 -- confirmed
    UINT64 SubResourceIndex; // 8 -- same field as D3DDDIARG_LOCK's SubResourceIndex (DdUnlockLH's
                             // v5[1]); the flattened subresource index. Truncated to 32 bits when read
                             // (the real value fits a UINT; the high half is always 0 live).
} D3DDDIARG_UNLOCK;
#else
typedef struct _D3DDDIARG_UNLOCK
{
    HANDLE hResource;        // 0 -- confirmed
    UINT32 SubResourceIndex; // 4 -- same field as D3DDDIARG_LOCK's SubResourceIndex (DdUnlockLH's v5[1]).
} D3DDDIARG_UNLOCK;
#endif

// RE-verified via CBatchFilterI::LHBatchPresent (which copies exactly 40 bytes -- one OWORD at
// offset 0, one OWORD at offset 16, one QWORD at offset 32 -- into the batch token): the raw struct
// is 40 bytes, not 44; the earlier 44-byte figure measured the DP2 token's total footprint (4-byte
// tag + 40-byte payload), not the struct alone. hSrcResource is confirmed at offset 0 (LHBatchPresent
// passes *(void**)a2 straight to CBatchFilterI::ReferenceResource as a HANDLE). A flags-like byte at
// offset 28 is tested by the runtime for bit 0x4 before deciding batch vs. immediate dispatch. Fields
// beyond hSrcResource are not yet individually pinned -- do not add accessors here until a forcing
// function (Present() actually completing, or Phase 1's follow-up) RE-verifies them the way LOCK was.
typedef struct _D3DDDIARG_PRESENT
{
    HANDLE hSrcResource; // 0, confirmed
    BYTE Reserved[32];   // starts right after hSrcResource (8..39 on x64, 4..35 on x86, since HANDLE
                         // is 8/4 bytes respectively) -- size-confirmed region; individual fields not
                         // yet pinned
} D3DDDIARG_PRESENT;

#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_LOCK) == 104, "size confirmed via real d3d9.dll RE");
static_assert(offsetof(D3DDDIARG_LOCK, SubResourceIndex) == 8, "SubResourceIndex RE-verified live+static 2026-07-06");
static_assert(sizeof(D3DDDIARG_UNLOCK) == 16, "size confirmed via real d3d9.dll RE");
static_assert(sizeof(D3DDDIARG_PRESENT) == 40, "size confirmed via real d3d9.dll RE (LHBatchPresent copy pattern)");
#else
// D3DDDIARG_LOCK/D3DDDIARG_UNLOCK's x86 sizes are NOT "unchanged from x64" -- see Task 6's RE (2026-07-04,
// comments on the x86 struct definitions above) for why the earlier "alignment padding absorbs the
// shrinkage" theory was wrong and what the real, live-RE'd x86 sizes (48 / 8 bytes) are.
static_assert(sizeof(D3DDDIARG_LOCK) == 48, "D3DDDIARG_LOCK x86 layout (RE-verified live 2026-07-04)");
static_assert(offsetof(D3DDDIARG_LOCK, SubResourceIndex) == 4, "SubResourceIndex RE-verified live+static 2026-07-06 (x86)");
static_assert(sizeof(D3DDDIARG_UNLOCK) == 8, "D3DDDIARG_UNLOCK x86 layout (RE-verified live 2026-07-04)");
static_assert(sizeof(D3DDDIARG_PRESENT) == 36, "D3DDDIARG_PRESENT x86 layout");
#endif

#pragma pack(pop)

// Layout pins: catch any accidental drift from the WDK ABI at compile time (x64 sizes).
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_OPENADAPTER) == 40, "D3DDDIARG_OPENADAPTER x64 layout");
static_assert(sizeof(D3DDDI_ADAPTERFUNCS) == 24, "D3DDDI_ADAPTERFUNCS x64 layout");
static_assert(sizeof(D3DDDIARG_GETCAPS) == 32, "D3DDDIARG_GETCAPS x64 layout");
static_assert(sizeof(D3DDDIARG_CREATEDEVICE) == 96, "D3DDDIARG_CREATEDEVICE x64 layout");
#else
// x86 layout, hand-recomputed from the x64 layout above (every HANDLE/pointer field shrinks from 8 to
// 4 bytes, shifting every subsequent field); compiler-verified via i686-w64-mingw32-g++.
static_assert(sizeof(D3DDDIARG_OPENADAPTER) == 24, "D3DDDIARG_OPENADAPTER x86 layout");
static_assert(sizeof(D3DDDI_ADAPTERFUNCS) == 12, "D3DDDI_ADAPTERFUNCS x86 layout");
static_assert(sizeof(D3DDDIARG_GETCAPS) == 16, "D3DDDIARG_GETCAPS x86 layout");
static_assert(sizeof(D3DDDIARG_CREATEDEVICE) == 56, "D3DDDIARG_CREATEDEVICE x86 layout");
#endif
