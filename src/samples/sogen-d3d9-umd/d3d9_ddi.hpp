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

typedef struct _D3DDDIARG_CREATEVERTEXSHADERDECL
{
    HANDLE ShaderHandle; // out
    UINT NumVertexElements;
    // D3DDDIVERTEXELEMENT elements[NumVertexElements] follow the struct in memory
} D3DDDIARG_CREATEVERTEXSHADERDECL;

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

// NOT wired up in sogen_d3d9_umd.cpp yet, and NOT RE-verified -- the "data follows the struct"
// convention below is the same one just proven wrong for SETVERTEXSHADERCONST/SETPIXELSHADERCONST
// above. Do not trust it for these int/bool variants without live verification first.
typedef struct _D3DDDIARG_SETVERTEXSHADERCONSTI
{
    UINT Register;
    UINT Count; // number of int4 vectors; unverified assumption: the int32 data follows in memory
} D3DDDIARG_SETVERTEXSHADERCONSTI;

typedef struct _D3DDDIARG_SETPIXELSHADERCONSTI
{
    UINT Register;
    UINT Count;
} D3DDDIARG_SETPIXELSHADERCONSTI;

typedef struct _D3DDDIARG_SETVERTEXSHADERCONSTB
{
    UINT Register;
    UINT Count; // number of BOOLs (4 bytes each); unverified assumption: the data follows in memory
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

// hResource@0 and pData@40 are RE-verified and reliable across every resource kind and routing path
// (confirmed live 2026-07-03/2026-07-04 -- see below). OffsetToLock/SizeToLock are deliberately NOT
// modeled as named fields: real d3d9.dll builds this 104-byte struct from at least TWO DIFFERENT code
// paths depending on internal buffer routing (see HANDOFF_MACBOOK.md for the full capture), and the
// two paths disagree on which offset (if any) carries which value:
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
// Since umd_Lock is one resource-kind- and routing-path-agnostic function (by design -- see its own
// comment), it cannot statically know which shape a given call used. The safe, correct-either-way
// choice: always treat every lock as an implicit whole-buffer lock (offset 0, size unknown) --
// resolve_buffer_resource_id's existing size-unknown fallback already exists for exactly this case and
// safely over-allocates rather than misreading a garbage value as a byte count.
//
// x86 note: this x64 layout does NOT carry over -- see the separate x86 definition below. Task 6's
// live idasql RE (2026-07-04) found the x86 struct is genuinely different, not just pointer-shrunk.
#ifdef _WIN64
typedef struct _D3DDDIARG_LOCK
{
    HANDLE hResource;    // 0 -- confirmed
    UINT64 Reserved0;    // 8 -- present, purpose unconfirmed
    BYTE Reserved1[24];  // 16..39 -- unconfirmed
    VOID* pData;         // 40 -- RE-verified live 2026-07-03; the real, correct output offset.
    BYTE Reserved2[56];  // 48..103 -- unconfirmed (includes OffsetToLock@80 in the "driver-routed"
                          //            shape only -- not read, see comment above)
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
// IMPORTANT METHODOLOGY NOTE, added after an independent review caught a real gap here: the
// DdLockLH claim above was *initially* backed only by static idasql decompilation (matching the
// helper's name and shape to the two Lock call sites) -- a spec-compliance reviewer correctly pointed
// out that `SELECT ... FROM xrefs WHERE to_ea = <DdLockLH> AND is_code=1` returns six callers, none of
// which is CDriverVertexBuffer::Lock/CDriverMipSurface::InternalLockRect. That is real and reproducible,
// but it does NOT mean DdLockLH is the wrong function: the call from Lock/InternalLockRect is an
// *indirect* call through a per-device, runtime-populated function-pointer field (raw disassembly:
// `mov esi, [esi+308h]` / `call esi`, the ECX load right before it is just the x86 Control Flow Guard
// ABI setting up `___guard_check_icall_fptr`'s argument register, not a real "this"/hDevice parameter --
// it is unrelated to the actual 2-arg DDI call). A static xref pass cannot resolve an indirect call
// through a non-constant, heap/device-object-resident pointer to its runtime target, so it correctly
// finds zero code-xrefs here; the six functions it DOES find are DdLockLH's legacy-DirectDraw callers
// (an address-taken reference from _QueryLHDDICaps builds a *separate* legacy DirectDraw HAL callback
// table at a different offset -- not statically confirmed to be the same per-device slot Lock/
// InternalLockRect index through) -- unrelated to whether Lock/InternalLockRect calls it too.
// This was re-verified LIVE (not just re-argued statically) via sogen's own Python debugger API
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
// This is now confirmed by live execution trace, not just plausible-looking decompilation.
typedef struct _D3DDDIARG_LOCK
{
    HANDLE hResource;    // 0 -- RE-verified live 2026-07-04 (see comment above)
    UINT32 Reserved0;    // 4 -- present, purpose unconfirmed (x86-native 4 bytes; NOT a UINT64)
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
    HANDLE hResource; // 0 -- confirmed
    UINT64 Reserved0; // 8 -- confirmed present (same field as D3DDDIARG_LOCK's Reserved0)
} D3DDDIARG_UNLOCK;
#else
typedef struct _D3DDDIARG_UNLOCK
{
    HANDLE hResource; // 0 -- confirmed
    UINT32 Reserved0; // 4 -- confirmed present (same field as D3DDDIARG_LOCK's Reserved0)
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
    BYTE Reserved[32];   // 8..39, size-confirmed region; individual fields not yet pinned
} D3DDDIARG_PRESENT;

#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_LOCK) == 104, "size confirmed via real d3d9.dll RE");
static_assert(sizeof(D3DDDIARG_UNLOCK) == 16, "size confirmed via real d3d9.dll RE");
static_assert(sizeof(D3DDDIARG_PRESENT) == 40, "size confirmed via real d3d9.dll RE (LHBatchPresent copy pattern)");
#else
// D3DDDIARG_LOCK/D3DDDIARG_UNLOCK's x86 sizes are NOT "unchanged from x64" -- see Task 6's RE (2026-07-04,
// comments on the x86 struct definitions above) for why the earlier "alignment padding absorbs the
// shrinkage" theory was wrong and what the real, live-RE'd x86 sizes (48 / 8 bytes) are.
static_assert(sizeof(D3DDDIARG_LOCK) == 48, "D3DDDIARG_LOCK x86 layout (RE-verified live 2026-07-04)");
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
