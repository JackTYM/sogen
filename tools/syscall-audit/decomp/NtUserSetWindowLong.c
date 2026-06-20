__int64 __fastcall NtUserSetWindowLong(__int64 a1, int a2)                                                | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  v4 = EnterCrit(0LL, 0LL);                                                                               | 
  v5 = ValidateHwndEx(a1, 1LL, 1LL);                                                                      | 
  v6 = 0LL;                                                                                               | 
  v7 = (struct tagWND *)v5;                                                                               | 
  if ( v5 )                                                                                               | 
  {                                                                                                       | 
    BugCheckParameter3[0] = *(_QWORD *)(v4 + 456);                                                        | 
    *(_QWORD *)(v4 + 456) = BugCheckParameter3;                                                           | 
    BugCheckParameter3[1] = v5;                                                                           | 
    HMLockObject(v5);                                                                                     | 
    if ( (unsigned int)PsGetWin32KFilterSet() == 5 )                                                      | 
    {                                                                                                     | 
      if ( a2 != -21 )                                                                                    | 
      {                                                                                                   | 
        v8 = 0;                                                                                           | 
        UserSetLastError(87LL);                                                                           | 
        Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
        goto LABEL_6;                                                                                     | 
      }                                                                                                   | 
      DesktopVerifyHeapRange(*((_QWORD *)v7 + 3), *((_QWORD *)v7 + 5), 344LL);                            | 
    }                                                                                                     | 
    v8 = xxxSetWindowLong(v7, 1);                                                                         | 
    CurrentThreadNonPaged = (ULONG_PTR *)W32GetCurrentThreadNonPaged();                                   | 
    if ( CurrentThreadNonPaged )                                                                          | 
    {                                                                                                     | 
      v6 = *CurrentThreadNonPaged;                                                                        | 
    }                                                                                                     | 
    Win32HM_UnlockFromThread<0>(v6, (ULONG_PTR)BugCheckParameter3);                                       | 
  }                                                                                                       | 
  else                                                                                                    | 
  {                                                                                                       | 
    v8 = 0;                                                                                               | 
  }                                                                                                       | 
LABEL_6:                                                                                                  | 
  UserSessionSwitchLeaveCrit();                                                                           | 
  return v8;                                                                                              | 
}                                                                                                         | 
41 row(s)
