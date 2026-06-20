__int64 __fastcall NtUserInitThreadCoreMessagingIocp2(__int64 a1, void *a2)                           | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  v4 = EnterCrit(0LL, 0LL);                                                                           | 
  v5 = ValidateReceivingHwnd(a1, 0LL);                                                                | 
  v6 = v5;                                                                                            | 
  if ( v5 )                                                                                           | 
  {                                                                                                   | 
    Win32HMThreadLockAlways<tagHOOK>::Win32HMThreadLockAlways<tagHOOK>(BugCheckParameter3, v4, v5);   | 
    if ( a2 )                                                                                         | 
    {                                                                                                 | 
      Src = 0;                                                                                        | 
      inited = InitThreadCoreMessagingIocp2(v6, &Src);                                                | 
      if ( inited )                                                                                   | 
      {                                                                                               | 
        if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                             | 
        {                                                                                             | 
          a2 = (void *)MmUserProbeAddress;                                                            | 
        }                                                                                             | 
        RtlCopyVolatileMemory(a2, &Src, 4uLL);                                                        | 
      }                                                                                               | 
    }                                                                                                 | 
    else                                                                                              | 
    {                                                                                                 | 
      UserSetLastError(87LL);                                                                         | 
      inited = 0LL;                                                                                   | 
    }                                                                                                 | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
  }                                                                                                   | 
  else                                                                                                | 
  {                                                                                                   | 
    inited = 0LL;                                                                                     | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return inited;                                                                                      | 
}                                                                                                     | 
37 row(s)
