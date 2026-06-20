__int64 NtGdiInit()                                                              | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  CurrentProcessWin32Process = (_QWORD *)PsGetCurrentProcessWin32Process();      | 
  v1 = CurrentProcessWin32Process;                                               | 
  if ( !CurrentProcessWin32Process || *CurrentProcessWin32Process )              | 
  {                                                                              | 
    v2 = (__int64)(CurrentProcessWin32Process + 10);                             | 
    if ( CurrentProcessWin32Process == (_QWORD *)-80LL )                         | 
    {                                                                            | 
      goto LABEL_6;                                                              | 
    }                                                                            | 
  }                                                                              | 
  else                                                                           | 
  {                                                                              | 
    v1 = 0LL;                                                                    | 
    v2 = 80LL;                                                                   | 
  }                                                                              | 
  GreEnterCriticalRegionAndAcquirePushLockExclusive((struct W32_PUSH_LOCK *)v2); | 
LABEL_6:                                                                         | 
  v3 = *((_DWORD *)v1 + 69);                                                     | 
  if ( (v3 & 0x100) == 0 )                                                       | 
  {                                                                              | 
    *((_DWORD *)v1 + 69) = v3 | 0x120;                                           | 
  }                                                                              | 
  if ( v2 )                                                                      | 
  {                                                                              | 
    W32ReleasePushLockExclusive((struct W32_PUSH_LOCK *)v2);                     | 
  }                                                                              | 
  return 1LL;                                                                    | 
}                                                                                | 
32 row(s)
