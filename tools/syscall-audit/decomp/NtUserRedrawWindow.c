__int64 __fastcall NtUserRedrawWindow(__int64 a1, __int128 *a2, __int64 a3, int a4)                 | 
{                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                     | 
                                                                                                    | 
  v12 = 0LL;                                                                                        | 
  v7 = EnterCrit(0LL, 0LL);                                                                         | 
  v8 = 0;                                                                                           | 
  if ( a1 )                                                                                         | 
  {                                                                                                 | 
    v9 = (struct tagWND *)ValidateHwnd(a1);                                                         | 
    if ( !v9 )                                                                                      | 
    {                                                                                               | 
      goto LABEL_7;                                                                                 | 
    }                                                                                               | 
  }                                                                                                 | 
  else                                                                                              | 
  {                                                                                                 | 
    v9 = 0LL;                                                                                       | 
  }                                                                                                 | 
  Win32HMOptionalThreadLock<tagWND>::Win32HMOptionalThreadLock<tagWND>(BugCheckParameter3, v7, v9); | 
  if ( a2 )                                                                                         | 
  {                                                                                                 | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                               | 
    {                                                                                               | 
      a2 = (__int128 *)MmUserProbeAddress;                                                          | 
    }                                                                                               | 
    v12 = *a2;                                                                                      | 
  }                                                                                                 | 
  if ( (a4 & 0xFFFFF000) != 0 )                                                                     | 
  {                                                                                                 | 
    UserSetLastError(1004LL);                                                                       | 
  }                                                                                                 | 
  else                                                                                              | 
  {                                                                                                 | 
    v8 = xxxRedrawWindow(v9);                                                                       | 
  }                                                                                                 | 
  Win32HMThreadLock<tagCURSOR>::~Win32HMThreadLock<tagCURSOR>((ULONG_PTR)BugCheckParameter3);       | 
LABEL_7:                                                                                            | 
  UserSessionSwitchLeaveCrit();                                                                     | 
  return v8;                                                                                        | 
}                                                                                                   | 
41 row(s)
