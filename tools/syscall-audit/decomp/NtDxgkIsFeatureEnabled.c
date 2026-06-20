__int64 __fastcall NtDxgkIsFeatureEnabled(ULONG64 a1, __int64 a2, __int64 a3)                                               | 
{                                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                             | 
                                                                                                                            | 
  v26 = -1;                                                                                                                 | 
  v27 = 0LL;                                                                                                                | 
  if ( (qword_14015E4C0 & 2) != 0 )                                                                                         | 
  {                                                                                                                         | 
    v28 = 1;                                                                                                                | 
    v26 = 2224;                                                                                                             | 
    if ( (Microsoft_Windows_DxgKrnlEnableBits & 0x10000) != 0 )                                                             | 
    {                                                                                                                       | 
      McTemplateK0q_EtwWriteTransfer(a1, &EventProfilerEnter, a3, 2224LL);                                                  | 
    }                                                                                                                       | 
  }                                                                                                                         | 
  else                                                                                                                      | 
  {                                                                                                                         | 
    v28 = 0;                                                                                                                | 
  }                                                                                                                         | 
  DXGETWPROFILER_BASE_PushProfilerEntry(&v26, 2224LL);                                                                      | 
  if ( !(unsigned int)Feature_Servicing_GraphicsKernel_FeatureInterfaceExtensions__private_IsEnabledDeviceUsageNoInline() ) | 
  {                                                                                                                         | 
    Feature_IsFeatureEnabled__private_IsEnabledPreCheck();                                                                  | 
  }                                                                                                                         | 
  *(_QWORD *)v33 = 0LL;                                                                                                     | 
  Src = 0;                                                                                                                  | 
  v4 = (const void *)a1;                                                                                                    | 
  if ( a1 >= MmUserProbeAddress )                                                                                           | 
  {                                                                                                                         | 
    v4 = (const void *)MmUserProbeAddress;                                                                                  | 
  }                                                                                                                         | 
  RtlCopyVolatileMemory(v33, v4, 0xCuLL);                                                                                   | 
  v5 = g_ClearDatabaseCacheOnQuery;                                                                                         | 
  if ( !v33[0] )                                                                                                            | 
  {                                                                                                                         | 
    IsFeatureEnabled = DpiIsFeatureEnabled(0LL, v33[1], g_ClearDatabaseCacheOnQuery, &Src);                                 | 
LABEL_30:                                                                                                                   | 
    if ( IsFeatureEnabled >= 0 )                                                                                            | 
    {                                                                                                                       | 
      v23 = (void *)(a1 + 8);                                                                                               | 
      if ( a1 + 8 >= MmUserProbeAddress )                                                                                   | 
      {                                                                                                                     | 
        v23 = (void *)MmUserProbeAddress;                                                                                   | 
      }                                                                                                                     | 
      RtlCopyVolatileMemory(v23, &Src, 4uLL);                                                                               | 
    }                                                                                                                       | 
    DXGETWPROFILER_BASE::PopProfilerEntry((DXGETWPROFILER_BASE *)&v26);                                                     | 
    if ( v28 )                                                                                                              | 
    {                                                                                                                       | 
      if ( (Microsoft_Windows_DxgKrnlEnableBits & 0x10000) != 0 )                                                           | 
      {                                                                                                                     | 
        McTemplateK0q_EtwWriteTransfer(v24, &EventProfilerExit, v25, v26);                                                  | 
      }                                                                                                                     | 
    }                                                                                                                       | 
    return (unsigned int)IsFeatureEnabled;                                                                                  | 
  }                                                                                                                         | 
  Current = DXGPROCESS::GetCurrent();                                                                                       | 
  v8 = Current;                                                                                                             | 
  if ( !Current )                                                                                                           | 
  {                                                                                                                         | 
    CurrentProcess = PsGetCurrentProcess();                                                                                 | 
    WdLogSingleEntry1(2LL, CurrentProcess);                                                                                 | 
    WdLogGlobalForLineNumber = 62;                                                                                          | 
    v10 = PsGetCurrentProcess();                                                                                            | 
    DxgkLogInternalTriageEvent(                                                                                             | 
      0,                                                                                                                    | 
      0x40000,                                                                                                              | 
      -1,                                                                                                                   | 
      (unsigned int)L"NtDxgkIsFeatureEnabled: Failed to get DXGPROCESS, EPROCESS=0x%.16x",                                  | 
      v10,                                                                                                                  | 
      0LL,                                                                                                                  | 
      0LL,                                                                                                                  | 
      0LL,                                                                                                                  | 
      0LL);                                                                                                                 | 
    goto LABEL_15;                                                                                                          | 
  }                                                                                                                         | 
  v29 = 0LL;                                                                                                                | 
  DXGADAPTERBYHANDLE::DXGADAPTERBYHANDLE((DXGADAPTERBYHANDLE *)v31, v33[0], Current, &v29, 1);                              | 
  v11 = v29;                                                                                                                | 
  if ( !v29 )                                                                                                               | 
  {                                                                                                                         | 
    WdLogSingleEntry2(3LL, v33[0], v8);                                                                                     | 
    WdLogGlobalForLineNumber = 72;                                                                                          | 
    DXGADAPTERBYHANDLE::~DXGADAPTERBYHANDLE((DXGADAPTERBYHANDLE *)v31);                                                     | 
LABEL_15:                                                                                                                   | 
    DXGETWPROFILER_BASE::PopProfilerEntry((DXGETWPROFILER_BASE *)&v26);                                                     | 
    if ( v28 && (Microsoft_Windows_DxgKrnlEnableBits & 0x10000) != 0 )                                                      | 
    {                                                                                                                       | 
      McTemplateK0q_EtwWriteTransfer(v12, &EventProfilerExit, v13, v26);                                                    | 
    }                                                                                                                       | 
    return 3221225485LL;                                                                                                    | 
  }                                                                                                                         | 
  v30 = 0LL;                                                                                                                | 
  v32 = 0LL;                                                                                                                | 
  PairingAdapters = DxgkpGetPairingAdapters(v29, 0, &v30, &v32, 0LL, 0LL, 0);                                               | 
  if ( PairingAdapters >= 0 )                                                                                               | 
  {                                                                                                                         | 
    v18 = v30;                                                                                                              | 
    COREADAPTERACCESS::COREADAPTERACCESS((COREADAPTERACCESS *)v35, v30, 0LL);                                               | 
    v19 = COREADAPTERACCESS::AcquireShared((COREADAPTERACCESS *)v35, 0LL);                                                  | 
    DXGADAPTER::ReleaseReference(v18, v20);                                                                                 | 
    if ( v19 >= 0 )                                                                                                         | 
    {                                                                                                                       | 
      IsFeatureEnabled = DpiIsFeatureEnabled(*((_QWORD *)v18 + 27), v33[1], v5, &Src);                                      | 
      COREADAPTERACCESS::~COREADAPTERACCESS((COREADAPTERACCESS *)v35);                                                      | 
      DXGADAPTERBYHANDLE::~DXGADAPTERBYHANDLE((DXGADAPTERBYHANDLE *)v31);                                                   | 
      goto LABEL_30;                                                                                                        | 
    }                                                                                                                       | 
    WdLogSingleEntry1(3LL, v18);                                                                                            | 
    WdLogGlobalForLineNumber = 97;                                                                                          | 
    COREADAPTERACCESS::~COREADAPTERACCESS((COREADAPTERACCESS *)v35);                                                        | 
    DXGADAPTERBYHANDLE::~DXGADAPTERBYHANDLE((DXGADAPTERBYHANDLE *)v31);                                                     | 
    DXGETWPROFILER_BASE::PopProfilerEntry((DXGETWPROFILER_BASE *)&v26);                                                     | 
    if ( v28 && (Microsoft_Windows_DxgKrnlEnableBits & 0x10000) != 0 )                                                      | 
    {                                                                                                                       | 
      McTemplateK0q_EtwWriteTransfer(v21, &EventProfilerExit, v22, v26);                                                    | 
    }                                                                                                                       | 
    return (unsigned int)v19;                                                                                               | 
  }                                                                                                                         | 
  else                                                                                                                      | 
  {                                                                                                                         | 
    WdLogSingleEntry1(3LL, v11);                                                                                            | 
    WdLogGlobalForLineNumber = 83;                                                                                          | 
    DXGADAPTERBYHANDLE::~DXGADAPTERBYHANDLE((DXGADAPTERBYHANDLE *)v31);                                                     | 
    DXGETWPROFILER_BASE::PopProfilerEntry((DXGETWPROFILER_BASE *)&v26);                                                     | 
    if ( v28 && (Microsoft_Windows_DxgKrnlEnableBits & 0x10000) != 0 )                                                      | 
    {                                                                                                                       | 
      McTemplateK0q_EtwWriteTransfer(v16, &EventProfilerExit, v17, v26);                                                    | 
    }                                                                                                                       | 
    return (unsigned int)PairingAdapters;                                                                                   | 
  }                                                                                                                         | 
}                                                                                                                           | 
132 row(s)
