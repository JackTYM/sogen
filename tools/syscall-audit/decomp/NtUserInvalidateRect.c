__int64 __fastcall NtUserInvalidateRect(__int64 a1, __int128 *a2, unsigned int a3)              | 
{                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                 | 
                                                                                                | 
  if ( !a1 )                                                                                    | 
  {                                                                                             | 
    return 1LL;                                                                                 | 
  }                                                                                             | 
  v11 = 0LL;                                                                                    | 
  EnterCrit(0LL, 0LL);                                                                          | 
  v7 = ValidateHwnd(a1);                                                                        | 
  if ( a2 )                                                                                     | 
  {                                                                                             | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                           | 
    {                                                                                           | 
      a2 = (__int128 *)MmUserProbeAddress;                                                      | 
    }                                                                                           | 
    v11 = *a2;                                                                                  | 
    a2 = &v11;                                                                                  | 
    CurrentThreadDpiAwarenessContext = W32GetCurrentThreadDpiAwarenessContext();                | 
    if ( (unsigned __int8)ShouldVirtualizeWindowRect(v7, CurrentThreadDpiAwarenessContext) )    | 
    {                                                                                           | 
      TransformRectBetweenCoordinateSpaces(&v11, &v11, 0LL, v7);                                | 
    }                                                                                           | 
  }                                                                                             | 
  Win32HMThreadLockBase<tagWND,1,0>::Win32HMThreadLockBase<tagWND,1,0>(BugCheckParameter3, v7); | 
  v9 = xxxInvalidateRect(v7, a2, a3);                                                           | 
  Win32HMThreadLock<tagCURSOR>::~Win32HMThreadLock<tagCURSOR>((ULONG_PTR)BugCheckParameter3);   | 
  UserSessionSwitchLeaveCrit();                                                                 | 
  return v9;                                                                                    | 
}                                                                                               | 
31 row(s)
