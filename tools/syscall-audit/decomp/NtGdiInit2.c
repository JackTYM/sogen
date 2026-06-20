__int64 NtGdiInit2()                                                             | 
{                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                  | 
                                                                                 | 
  CurrentProcessWin32Process = (_QWORD *)PsGetCurrentProcessWin32Process();      | 
  v1 = CurrentProcessWin32Process;                                               | 
  if ( CurrentProcessWin32Process && !*CurrentProcessWin32Process )              | 
  {                                                                              | 
    v1 = 0LL;                                                                    | 
    v2 = 80LL;                                                                   | 
  }                                                                              | 
  else                                                                           | 
  {                                                                              | 
    v2 = (__int64)(CurrentProcessWin32Process + 10);                             | 
    if ( CurrentProcessWin32Process == (_QWORD *)-80LL )                         | 
    {                                                                            | 
      goto LABEL_5;                                                              | 
    }                                                                            | 
  }                                                                              | 
  GreEnterCriticalRegionAndAcquirePushLockExclusive((struct W32_PUSH_LOCK *)v2); | 
LABEL_5:                                                                         | 
  v3 = *((_DWORD *)v1 + 69);                                                     | 
  if ( (v3 & 0x100) == 0 )                                                       | 
  {                                                                              | 
    *((_DWORD *)v1 + 69) = v3 & 0xFFFFFEDF | 0x100;                              | 
  }                                                                              | 
  v4 = v1[35];                                                                   | 
  if ( v2 )                                                                      | 
  {                                                                              | 
    W32ReleasePushLockExclusive((struct W32_PUSH_LOCK *)v2);                     | 
  }                                                                              | 
  return v4;                                                                     | 
}                                                                                | 
33 row(s)
