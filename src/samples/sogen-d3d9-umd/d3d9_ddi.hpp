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

typedef struct _D3DDDIARG_SETRENDERTARGET
{
    UINT RenderTargetIndex;
    HANDLE hRenderTarget; // 0 unbinds
} D3DDDIARG_SETRENDERTARGET;

typedef struct _D3DDDIARG_SETDEPTHSTENCIL
{
    HANDLE hZBuffer; // 0 = no depth-stencil
} D3DDDIARG_SETDEPTHSTENCIL;

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

typedef struct _D3DDDIARG_CREATEVERTEXSHADERFUNC
{
    HANDLE ShaderHandle; // out
    UINT Values[1];      // in: [0] = code size in bytes; the token DWORDs follow the struct in memory
} D3DDDIARG_CREATEVERTEXSHADERFUNC;

typedef struct _D3DDDIARG_CREATEPIXELSHADERFUNC
{
    HANDLE ShaderHandle; // out
    UINT CodeSize;       // in: bytes; the token DWORDs follow the struct in memory
} D3DDDIARG_CREATEPIXELSHADERFUNC;

typedef struct _D3DDDIARG_SETPIXELSHADERFUNC
{
    HANDLE ShaderHandle;
} D3DDDIARG_SETPIXELSHADERFUNC;

typedef struct _D3DDDIARG_DELETEPIXELSHADERFUNC
{
    HANDLE ShaderHandle;
} D3DDDIARG_DELETEPIXELSHADERFUNC;

typedef struct _D3DDDIARG_SETVERTEXSHADERFUNC
{
    HANDLE ShaderHandle; // 0 selects the fixed-function pipeline
} D3DDDIARG_SETVERTEXSHADERFUNC;

typedef struct _D3DDDIARG_SETVERTEXSHADERDECL
{
    HANDLE ShaderHandle;
} D3DDDIARG_SETVERTEXSHADERDECL;

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

typedef struct _D3DDDIARG_SETVERTEXSHADERCONST
{
    UINT Register;
    UINT Count; // number of 4-float vectors; the float data follows the struct in memory
} D3DDDIARG_SETVERTEXSHADERCONST;

typedef struct _D3DDDIARG_SETPIXELSHADERCONST
{
    UINT Register;
    UINT Count;
} D3DDDIARG_SETPIXELSHADERCONST;

typedef struct _D3DDDIARG_SETVERTEXSHADERCONSTI
{
    UINT Register;
    UINT Count; // number of int4 vectors; the int32 data follows the struct in memory
} D3DDDIARG_SETVERTEXSHADERCONSTI;

typedef struct _D3DDDIARG_SETPIXELSHADERCONSTI
{
    UINT Register;
    UINT Count;
} D3DDDIARG_SETPIXELSHADERCONSTI;

typedef struct _D3DDDIARG_SETVERTEXSHADERCONSTB
{
    UINT Register;
    UINT Count; // number of BOOLs (4 bytes each); the data follows the struct in memory
} D3DDDIARG_SETVERTEXSHADERCONSTB;

typedef struct _D3DDDIARG_SETPIXELSHADERCONSTB
{
    UINT Register;
    UINT Count;
} D3DDDIARG_SETPIXELSHADERCONSTB;

typedef struct _D3DDDIARG_CLEAR
{
    UINT Flags; // D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL
    UINT Color;
    FLOAT Z;
    UINT Stencil;
    UINT NumRect; // D3DDDIRECT rects[NumRect] follow the struct in memory; 0 = whole target
} D3DDDIARG_CLEAR;

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

typedef struct _D3DDDIARG_PRESENT
{
    HANDLE hSrcResource;
    HANDLE hDstResource;
} D3DDDIARG_PRESENT;

#pragma pack(pop)

// Layout pins: catch any accidental drift from the WDK ABI at compile time (x64 sizes).
#ifdef _WIN64
static_assert(sizeof(D3DDDIARG_OPENADAPTER) == 40, "D3DDDIARG_OPENADAPTER x64 layout");
static_assert(sizeof(D3DDDI_ADAPTERFUNCS) == 24, "D3DDDI_ADAPTERFUNCS x64 layout");
static_assert(sizeof(D3DDDIARG_GETCAPS) == 32, "D3DDDIARG_GETCAPS x64 layout");
#endif
