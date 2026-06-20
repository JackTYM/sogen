__int64 __fastcall NtUserSetMenu(__int64 a1, __int64 a2)                                                    | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v4 = EnterCrit(0LL, 0LL);                                                                                 | 
  v5 = ValidateHwnd(a1);                                                                                    | 
  v6 = 0;                                                                                                   | 
  v7 = (struct tagWND *)v5;                                                                                 | 
  if ( v5 && (((*(_WORD *)(*(_QWORD *)(v5 + 40) + 42LL) & 0x2FFF) - 669) & 0xFFFFFFFD) != 0 )               | 
  {                                                                                                         | 
    Win32HM_LockIntoThread<0>(v4, v5, BugCheckParameter3);                                                  | 
    SmartObjStackRef<tagMENU>::SmartObjStackRef<tagMENU>(v15);                                              | 
    if ( a2 )                                                                                               | 
    {                                                                                                       | 
      v8 = ValidateHmenu(a2, 64LL);                                                                         | 
      v16 = 0LL;                                                                                            | 
      SmartObjStackRefBase<tagMENU>::operator=(v15, v8);                                                    | 
      if ( (unsigned __int8)SmartObjStackRef<tagMENU>::operator==(v15) )                                    | 
      {                                                                                                     | 
LABEL_5:                                                                                                    | 
        SmartObjStackRef<tagMENU>::~SmartObjStackRef<tagMENU>(v15);                                         | 
        v9 = PtiCurrent();                                                                                  | 
        Win32HM_UnlockFromThread<0>((ULONG_PTR)v9, (ULONG_PTR)BugCheckParameter3);                          | 
        goto LABEL_6;                                                                                       | 
      }                                                                                                     | 
    }                                                                                                       | 
    else                                                                                                    | 
    {                                                                                                       | 
      v16 = 0LL;                                                                                            | 
      if ( !*(_QWORD *)v15[0] )                                                                             | 
      {                                                                                                     | 
LABEL_10:                                                                                                   | 
        if ( !*(_QWORD *)v15[0] )                                                                           | 
        {                                                                                                   | 
LABEL_11:                                                                                                   | 
          v11 = *(_QWORD *)v15[0];                                                                          | 
LABEL_12:                                                                                                   | 
          Win32HM_LockIntoThread<1>(v4, v11, v14);                                                          | 
          v6 = xxxSetMenu(v7);                                                                              | 
          v12 = PtiCurrent();                                                                               | 
          Win32HM_UnlockFromThread<1>((ULONG_PTR)v12, (ULONG_PTR)v14);                                      | 
          goto LABEL_5;                                                                                     | 
        }                                                                                                   | 
LABEL_13:                                                                                                   | 
        if ( (*(_DWORD *)(*(_QWORD *)(*(_QWORD *)v15[0] + 40LL) + 40LL) & 1) != 0 )                         | 
        {                                                                                                   | 
          UserSetLastError(87LL);                                                                           | 
          SmartObjStackRef<tagMENU>::~SmartObjStackRef<tagMENU>(v15);                                       | 
          Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
          goto LABEL_6;                                                                                     | 
        }                                                                                                   | 
        v11 = v16;                                                                                          | 
        if ( v16 )                                                                                          | 
        {                                                                                                   | 
          goto LABEL_12;                                                                                    | 
        }                                                                                                   | 
        goto LABEL_11;                                                                                      | 
      }                                                                                                     | 
      SmartObjStackRefBase<tagMENU>::DecrementCountAndTryFree(v15);                                         | 
      v15[0] = gSmartObjNullRef;                                                                            | 
    }                                                                                                       | 
    if ( v16 )                                                                                              | 
    {                                                                                                       | 
      goto LABEL_13;                                                                                        | 
    }                                                                                                       | 
    goto LABEL_10;                                                                                          | 
  }                                                                                                         | 
LABEL_6:                                                                                                    | 
  UserSessionSwitchLeaveCrit();                                                                             | 
  return v6;                                                                                                | 
}                                                                                                           | 
71 row(s)
