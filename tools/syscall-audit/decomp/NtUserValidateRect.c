__int64 __fastcall NtUserValidateRect(__int64 a1, __int128 *a2)                                   | 
{                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                   | 
                                                                                                  | 
  v8 = 0LL;                                                                                       | 
  EnterCrit(0LL, 0LL);                                                                            | 
  v4 = 0;                                                                                         | 
  if ( a2 )                                                                                       | 
  {                                                                                               | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                             | 
    {                                                                                             | 
      a2 = (__int128 *)MmUserProbeAddress;                                                        | 
    }                                                                                             | 
    v8 = *a2;                                                                                     | 
    a2 = &v8;                                                                                     | 
  }                                                                                               | 
  if ( !a1 )                                                                                      | 
  {                                                                                               | 
    v5 = 0LL;                                                                                     | 
    goto LABEL_4;                                                                                 | 
  }                                                                                               | 
  v5 = ValidateHwnd(a1);                                                                          | 
  if ( v5 )                                                                                       | 
  {                                                                                               | 
LABEL_4:                                                                                          | 
    Win32HMThreadLockBase<tagWND,1,0>::Win32HMThreadLockBase<tagWND,1,0>(BugCheckParameter3, v5); | 
    v4 = xxxValidateRect(v5, a2);                                                                 | 
    Win32HMThreadLock<tagCURSOR>::~Win32HMThreadLock<tagCURSOR>((ULONG_PTR)BugCheckParameter3);   | 
  }                                                                                               | 
  UserSessionSwitchLeaveCrit();                                                                   | 
  return v4;                                                                                      | 
}                                                                                                 | 
32 row(s)
