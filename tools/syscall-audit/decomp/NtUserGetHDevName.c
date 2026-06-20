_BOOL8 __fastcall NtUserGetHDevName(int a1, ULONG64 a2)                                                            | 
{                                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                    | 
                                                                                                                   | 
  memset(v31, 0, sizeof(v31));                                                                                     | 
  v32 = 0LL;                                                                                                       | 
  v3 = 0;                                                                                                          | 
  EnterLeaveCritShared::EnterLeaveCritShared(v30, 1LL);                                                            | 
  v4 = 0LL;                                                                                                        | 
  if ( (unsigned __int64)(unsigned __int16)a1 < *(_QWORD *)(*(_QWORD *)(W32GetUserSessionState() + 20016) + 8LL) ) | 
  {                                                                                                                | 
    UserSessionState = W32GetUserSessionState();                                                                   | 
    v6 = *(_DWORD *)(W32GetUserSessionState() + 20040) * (unsigned int)(unsigned __int16)a1                        | 
       + *(_QWORD *)(UserSessionState + 20032);                                                                    | 
    v7 = W32GetUserSessionState();                                                                                 | 
    v8 = (__int64 *)(*(_QWORD *)(v7 + 19976) + 40LL * (unsigned int)((v6 - *(_QWORD *)(v7 + 20032)) >> 5));        | 
    LOWORD(a1) = HIWORD(a1) & 0x7FFF;                                                                              | 
    if ( ((HIWORD(a1) & 0x7FFF) == *(_WORD *)(v6 + 26)                                                             | 
       || (_WORD)a1 == 0x7FFF                                                                                      | 
       || !(_WORD)a1 && PsGetCurrentProcessWow64Process())                                                         | 
      && (*(_BYTE *)(v6 + 25) & 1) == 0                                                                            | 
      && *(_BYTE *)(v6 + 24) == 12 )                                                                               | 
    {                                                                                                              | 
      v4 = *v8;                                                                                                    | 
    }                                                                                                              | 
  }                                                                                                                | 
  if ( v4 )                                                                                                        | 
  {                                                                                                                | 
    v9 = *(_QWORD *)(v4 + 88);                                                                                     | 
    if ( v9 && (unsigned int)DrvGetHdevName(v9, v31) )                                                             | 
    {                                                                                                              | 
      HIWORD(v32) = 0;                                                                                             | 
      v10 = (_BYTE *)a2;                                                                                           | 
      if ( (a2 & 3) != 0 )                                                                                         | 
      {                                                                                                            | 
        ExRaiseDatatypeMisalignment();                                                                             | 
      }                                                                                                            | 
      if ( a2 >= MmUserProbeAddress )                                                                              | 
      {                                                                                                            | 
        v10 = (_BYTE *)MmUserProbeAddress;                                                                         | 
      }                                                                                                            | 
      *v10 = *v10;                                                                                                 | 
      v10[63] = v10[63];                                                                                           | 
      v11 = (_WORD *)a2;                                                                                           | 
      v12 = 2147483646LL;                                                                                          | 
      v13 = (__int16 *)v31;                                                                                        | 
      v14 = 32LL;                                                                                                  | 
      v15 = 0;                                                                                                     | 
      v16 = 0LL;                                                                                                   | 
      while ( v14 )                                                                                                | 
      {                                                                                                            | 
        if ( !v12 )                                                                                                | 
        {                                                                                                          | 
          goto LABEL_24;                                                                                           | 
        }                                                                                                          | 
        v17 = *v13;                                                                                                | 
        if ( !*v13 )                                                                                               | 
        {                                                                                                          | 
          goto LABEL_24;                                                                                           | 
        }                                                                                                          | 
        ++v13;                                                                                                     | 
        *v11++ = v17;                                                                                              | 
        --v14;                                                                                                     | 
        --v12;                                                                                                     | 
        ++v16;                                                                                                     | 
      }                                                                                                            | 
      --v11;                                                                                                       | 
      v15 = -2147483643;                                                                                           | 
LABEL_24:                                                                                                          | 
      *v11 = 0;                                                                                                    | 
      v3 = v15 >= 0;                                                                                               | 
    }                                                                                                              | 
  }                                                                                                                | 
  else                                                                                                             | 
  {                                                                                                                | 
    UserSetLastError(1461LL);                                                                                      | 
  }                                                                                                                | 
  v18 = v3;                                                                                                        | 
  v19 = W32GetUserSessionState();                                                                                  | 
  v20 = v19;                                                                                                       | 
  v21 = *(_DWORD *)(v19 + 69232);                                                                                  | 
  if ( v21 )                                                                                                       | 
  {                                                                                                                | 
    KeReleaseSemaphore(*(PRKSEMAPHORE *)(v19 + 69224), 0, v21, 0);                                                 | 
    *(_DWORD *)(v20 + 69232) = 0;                                                                                  | 
  }                                                                                                                | 
  v22 = W32GetUserSessionState();                                                                                  | 
  CurrentThreadWin32Thread = (_DWORD *)PsGetCurrentThreadWin32Thread();                                            | 
  if ( (CurrentThreadWin32Thread[6] & 0xC) == 8 )                                                                  | 
  {                                                                                                                | 
    v24 = *(void (**)(void))(*(_QWORD *)(W32GetWin32kBaseApiSetTable() + 48) + 3944LL);                            | 
    if ( v24 )                                                                                                     | 
    {                                                                                                              | 
      v24();                                                                                                       | 
    }                                                                                                              | 
    *(_DWORD *)(v22 + 19932) = 0;                                                                                  | 
    *(_QWORD *)(v22 + 19912) = 0LL;                                                                                | 
    DestroyExclusiveUserCritDeferredUnlockList();                                                                  | 
    *(_QWORD *)(v22 + 16) = 0LL;                                                                                   | 
  }                                                                                                                | 
  if ( *(_QWORD *)CurrentThreadWin32Thread && (CurrentThreadWin32Thread[6] & 2) == 0 )                             | 
  {                                                                                                                | 
    *(_BYTE *)(*(_QWORD *)CurrentThreadWin32Thread + 1708LL) = 0;                                                  | 
  }                                                                                                                | 
  v25 = PsGetCurrentThreadWin32Thread();                                                                           | 
  if ( v25 )                                                                                                       | 
  {                                                                                                                | 
    v26 = *(_DWORD *)(v25 + 28);                                                                                   | 
    if ( v26 )                                                                                                     | 
    {                                                                                                              | 
      KeBugCheckEx(0x160u, v26, 0LL, 0LL, 0LL);                                                                    | 
    }                                                                                                              | 
  }                                                                                                                | 
  EtwTraceReleaseUserCrit();                                                                                       | 
  v27 = CurrentThreadWin32Thread[6] & 0xFFFFFFF1;                                                                  | 
  CurrentThreadWin32Thread[6] = v27;                                                                               | 
  if ( (v27 & 0x10) != 0 )                                                                                         | 
  {                                                                                                                | 
    PsSetThreadWin32Thread(KeGetCurrentThread(), 0LL, CurrentThreadWin32Thread);                                   | 
  }                                                                                                                | 
  ExReleaseFastResource(*(_QWORD *)v22, CurrentThreadWin32Thread + 8);                                             | 
  KeLeaveCriticalRegion();                                                                                         | 
  return v18;                                                                                                      | 
}                                                                                                                  | 
124 row(s)
