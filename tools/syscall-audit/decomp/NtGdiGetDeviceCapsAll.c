__int64 __fastcall NtGdiGetDeviceCapsAll(HDC a1, struct _DEVCAPS *a2)                               | 
{                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                     | 
                                                                                                    | 
  if ( (unsigned int)GrepGetCurrentProcessBehaviorRestriction() == 1 && a1 == (HDC)-589410304LL )   | 
  {                                                                                                 | 
    SEMOBJSHARED<1>::SEMOBJSHARED<1>(&v8);                                                          | 
    v9 = *(_QWORD *)(*(_QWORD *)(W32GetUserSessionState() + 57104) + 48LL);                         | 
    if ( v9 )                                                                                       | 
    {                                                                                               | 
      DeviceCapsAllInternal = NtGdiGetDeviceCapsAllInternal((struct PDEVOBJ *)&v9, a2);             | 
      GreReleaseSemaphoreCommon<1,void (*)(HSEMAPHORE__ *)>(GreReleaseSemaphoreSharedInternal, v8); | 
      return DeviceCapsAllInternal;                                                                 | 
    }                                                                                               | 
    GreReleaseSemaphoreCommon<1,void (*)(HSEMAPHORE__ *)>(GreReleaseSemaphoreSharedInternal, v8);   | 
    return 0LL;                                                                                     | 
  }                                                                                                 | 
  APIDCOBJ::APIDCOBJ((APIDCOBJ *)v7, a1);                                                           | 
  if ( !v7[0] )                                                                                     | 
  {                                                                                                 | 
    EngSetLastError(6u);                                                                            | 
    goto LABEL_5;                                                                                   | 
  }                                                                                                 | 
  NEEDDYNAMICMODECHANGESHARELOCK::NEEDDYNAMICMODECHANGESHARELOCK(                                   | 
    (NEEDDYNAMICMODECHANGESHARELOCK *)v6,                                                           | 
    (*(_DWORD *)(*(_QWORD *)(v7[0] + 48LL) + 40LL) & 0x8000) == 0);                                 | 
  v8 = *(_QWORD *)(v7[0] + 48LL);                                                                   | 
  if ( !v8 )                                                                                        | 
  {                                                                                                 | 
    NEEDDYNAMICMODECHANGESHARELOCK::vUnlockIfNeeded((NEEDDYNAMICMODECHANGESHARELOCK *)v6);          | 
LABEL_5:                                                                                            | 
    APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v7);                                                            | 
    return 0LL;                                                                                     | 
  }                                                                                                 | 
  DeviceCapsAllInternal = NtGdiGetDeviceCapsAllInternal((struct PDEVOBJ *)&v8, a2);                 | 
  NEEDDYNAMICMODECHANGESHARELOCK::vUnlockIfNeeded((NEEDDYNAMICMODECHANGESHARELOCK *)v6);            | 
  APIDCOBJ::~APIDCOBJ((APIDCOBJ *)v7);                                                              | 
  return DeviceCapsAllInternal;                                                                     | 
}                                                                                                   | 
39 row(s)
