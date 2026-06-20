__int64 __fastcall NtUserRemoveMenu(__int64 a1, unsigned int a2, unsigned int a3)   | 
{                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                     | 
                                                                                    | 
  EnterCrit(0LL, 0LL);                                                              | 
  SmartObjStackRef<tagMENU>::SmartObjStackRef<tagMENU>(v13);                        | 
  if ( (a3 & 0xFFFF0000) != 0 )                                                     | 
  {                                                                                 | 
    UserSetLastError(1004LL);                                                       | 
    v7 = 0;                                                                         | 
  }                                                                                 | 
  else                                                                              | 
  {                                                                                 | 
    v6 = ValidateHmenu(a1, 32LL);                                                   | 
    v7 = 0;                                                                         | 
    v14 = 0LL;                                                                      | 
    if ( v6 != *(_QWORD *)v13[0] )                                                  | 
    {                                                                               | 
      SmartObjStackRefBase<tagMENU>::DecrementCountAndTryFree(v13);                 | 
      if ( v6 )                                                                     | 
      {                                                                             | 
        v13[0] = *(_QWORD *)(v6 + 152);                                             | 
        ++*(_DWORD *)(v13[0] + 8LL);                                                | 
      }                                                                             | 
      else                                                                          | 
      {                                                                             | 
        v13[0] = gSmartObjNullRef;                                                  | 
      }                                                                             | 
    }                                                                               | 
    if ( !(unsigned __int8)SmartObjStackRef<tagMENU>::operator==(v13)               | 
      && (*(_DWORD *)(*(_QWORD *)(*(_QWORD *)v13[0] + 40LL) + 40LL) & 0x40) == 0    | 
      && (*(_DWORD *)(*(_QWORD *)(*(_QWORD *)v13[0] + 40LL) + 40LL) & 0x200) == 0 ) | 
    {                                                                               | 
      v8 = v14;                                                                     | 
      if ( !v14 )                                                                   | 
      {                                                                             | 
        v8 = *(_QWORD *)v13[0];                                                     | 
      }                                                                             | 
      v9 = PtiCurrent();                                                            | 
      Win32HM_LockIntoThread<1>(v9, v8, BugCheckParameter3);                        | 
      v7 = xxxRemoveDeleteMenuHelper(v13, a2, a3, 0LL);                             | 
      v10 = PtiCurrent();                                                           | 
      Win32HM_UnlockFromThread<1>((ULONG_PTR)v10, (ULONG_PTR)BugCheckParameter3);   | 
    }                                                                               | 
  }                                                                                 | 
  SmartObjStackRef<tagMENU>::~SmartObjStackRef<tagMENU>(v13);                       | 
  UserSessionSwitchLeaveCrit();                                                     | 
  return v7;                                                                        | 
}                                                                                   | 
49 row(s)
