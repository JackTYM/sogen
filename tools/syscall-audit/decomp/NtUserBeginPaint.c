__int64 __fastcall NtUserBeginPaint(__int64 a1, void *a2)                                             | 
{                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                       | 
                                                                                                      | 
  memset_0(Src, 0, 0x48uLL);                                                                          | 
  v4 = EnterCrit(0LL, 0LL);                                                                           | 
  v5 = ValidateReceivingHwnd(a1, 1LL);                                                                | 
  v6 = (struct tagWND *)v5;                                                                           | 
  v7 = 0LL;                                                                                           | 
  if ( v5 )                                                                                           | 
  {                                                                                                   | 
    Win32HMThreadLockAlways<tagHOOK>::Win32HMThreadLockAlways<tagHOOK>(BugCheckParameter3, v4, v5);   | 
    v7 = xxxBeginPaint(v6);                                                                           | 
    v11 = 0;                                                                                          | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                                 | 
    {                                                                                                 | 
      a2 = (void *)MmUserProbeAddress;                                                                | 
    }                                                                                                 | 
    RtlCopyVolatileMemory(a2, Src, 0x48uLL);                                                          | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
  }                                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                                       | 
  return v7;                                                                                          | 
}                                                                                                     | 
24 row(s)
