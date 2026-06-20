__int64 __fastcall NtUserSetFocus(__int64 a1)                                | 
{                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]              | 
                                                                             | 
  EnterCrit(0LL, 0LL);                                                       | 
  v2 = 0LL;                                                                  | 
  if ( a1 )                                                                  | 
  {                                                                          | 
    v5 = (struct tagWND *)ValidateHwnd(a1);                                  | 
    if ( !v5 )                                                               | 
    {                                                                        | 
      v11 = 0LL;                                                             | 
      goto LABEL_12;                                                         | 
    }                                                                        | 
  }                                                                          | 
  else                                                                       | 
  {                                                                          | 
    v5 = 0LL;                                                                | 
  }                                                                          | 
  v6 = (__int64 *)((__int64 (*)(void))W32GetCurrentThreadNonPaged)();        | 
  if ( v6 )                                                                  | 
  {                                                                          | 
    v7 = *v6;                                                                | 
  }                                                                          | 
  else                                                                       | 
  {                                                                          | 
    v7 = 0LL;                                                                | 
  }                                                                          | 
  BugCheckParameter3[0] = *(_QWORD *)(v7 + 456);                             | 
  *(_QWORD *)(v7 + 456) = BugCheckParameter3;                                | 
  BugCheckParameter3[1] = (ULONG_PTR)v5;                                     | 
  if ( v5 )                                                                  | 
  {                                                                          | 
    HMLockObject(v5);                                                        | 
  }                                                                          | 
  v8 = xxxSetFocus(v5);                                                      | 
  v11 = 0LL;                                                                 | 
  if ( v8 )                                                                  | 
  {                                                                          | 
    v11 = *(_QWORD *)v8;                                                     | 
  }                                                                          | 
  CurrentThreadNonPaged = (ULONG_PTR *)W32GetCurrentThreadNonPaged(v10, v9); | 
  if ( CurrentThreadNonPaged )                                               | 
  {                                                                          | 
    v2 = *CurrentThreadNonPaged;                                             | 
  }                                                                          | 
  Win32HM_UnlockFromThread<1>(v2, (ULONG_PTR)BugCheckParameter3);            | 
LABEL_12:                                                                    | 
  UserSessionSwitchLeaveCrit(v4, v3);                                        | 
  return v11;                                                                | 
}                                                                            | 
51 row(s)
