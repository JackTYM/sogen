unsigned __int64 __fastcall NtUserSetWindowLongPtr(__int64 a1, int a2, unsigned __int64 a3, int a4)       | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  v8 = EnterCrit(0LL, 0LL);                                                                               | 
  v9 = ValidateHwndEx(a1, 1LL, 1LL);                                                                      | 
  v10 = 0LL;                                                                                              | 
  v11 = (struct tagWND *)v9;                                                                              | 
  if ( v9 )                                                                                               | 
  {                                                                                                       | 
    BugCheckParameter3[0] = *(_QWORD *)(v8 + 456);                                                        | 
    *(_QWORD *)(v8 + 456) = BugCheckParameter3;                                                           | 
    BugCheckParameter3[1] = v9;                                                                           | 
    HMLockObject(v9);                                                                                     | 
    if ( (unsigned int)PsGetWin32KFilterSet() == 5 )                                                      | 
    {                                                                                                     | 
      if ( a2 != -21 )                                                                                    | 
      {                                                                                                   | 
        v12 = 0LL;                                                                                        | 
        UserSetLastError(87LL);                                                                           | 
        Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
        goto LABEL_6;                                                                                     | 
      }                                                                                                   | 
      DesktopVerifyHeapRange(*((_QWORD *)v11 + 3), *((_QWORD *)v11 + 5), 344LL);                          | 
    }                                                                                                     | 
    v12 = xxxSetWindowLongPtr(v11, a2, a3, a4, 1);                                                        | 
    CurrentThreadNonPaged = (ULONG_PTR *)W32GetCurrentThreadNonPaged();                                   | 
    if ( CurrentThreadNonPaged )                                                                          | 
    {                                                                                                     | 
      v10 = *CurrentThreadNonPaged;                                                                       | 
    }                                                                                                     | 
    Win32HM_UnlockFromThread<0>(v10, (ULONG_PTR)BugCheckParameter3);                                      | 
  }                                                                                                       | 
  else                                                                                                    | 
  {                                                                                                       | 
    v12 = 0LL;                                                                                            | 
  }                                                                                                       | 
LABEL_6:                                                                                                  | 
  UserSessionSwitchLeaveCrit();                                                                           | 
  return v12;                                                                                             | 
}                                                                                                         | 
41 row(s)
