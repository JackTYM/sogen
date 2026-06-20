__int64 __fastcall NtGdiGetDeviceCaps(HDC a1, int a2)                                                | 
{                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                      | 
                                                                                                     | 
  if ( (unsigned __int8)PsIsWin32KFilterEnabled()                                                    | 
    && (unsigned int)PsGetWin32KFilterSet() == 5                                                     | 
    && a1 == (HDC)-589410304LL )                                                                     | 
  {                                                                                                  | 
    SEMOBJSHARED<1>::SEMOBJSHARED<1>(&v11);                                                          | 
    v12 = *(_QWORD *)(*(_QWORD *)(W32GetUserSessionState() + 57104) + 48LL);                         | 
    if ( v12 )                                                                                       | 
    {                                                                                                | 
      DeviceCapsInternal = GreGetDeviceCapsInternal((struct PDEVOBJ *)&v12, a2);                     | 
      GreReleaseSemaphoreCommon<1,void (*)(HSEMAPHORE__ *)>(GreReleaseSemaphoreSharedInternal, v11); | 
      return DeviceCapsInternal;                                                                     | 
    }                                                                                                | 
    GreReleaseSemaphoreCommon<1,void (*)(HSEMAPHORE__ *)>(GreReleaseSemaphoreSharedInternal, v11);   | 
    return 0;                                                                                        | 
  }                                                                                                  | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v10, a1);                                                           | 
  if ( !v10[0] )                                                                                     | 
  {                                                                                                  | 
    EngSetLastError(6u);                                                                             | 
    goto LABEL_10;                                                                                   | 
  }                                                                                                  | 
  v4 = *(_DWORD *)(*(_QWORD *)(v10[0] + 48LL) + 40LL) & 0x8000;                                      | 
  v9 = v4 == 0;                                                                                      | 
  v5 = *(_QWORD *)(W32GetSessionState() + 88);                                                       | 
  v8 = v5;                                                                                           | 
  if ( !v4 )                                                                                         | 
  {                                                                                                  | 
    GreAcquireSemaphoreShared<1,>(v5);                                                               | 
  }                                                                                                  | 
  v11 = *(_QWORD *)(v10[0] + 48LL);                                                                  | 
  if ( !v11 )                                                                                        | 
  {                                                                                                  | 
    NEEDDYNAMICMODECHANGESHARELOCK::vUnlockIfNeeded((NEEDDYNAMICMODECHANGESHARELOCK *)&v8);          | 
LABEL_10:                                                                                            | 
    APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v10);                                                            | 
    return 0;                                                                                        | 
  }                                                                                                  | 
  DeviceCapsInternal = GreGetDeviceCapsInternal((struct PDEVOBJ *)&v11, a2);                         | 
  if ( !v4 )                                                                                         | 
  {                                                                                                  | 
    GreReleaseSemaphoreShared<1,>(v5);                                                               | 
  }                                                                                                  | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v10);                                                              | 
  return DeviceCapsInternal;                                                                         | 
}                                                                                                    | 
49 row(s)
