__int64 __fastcall NtEnumerateDriverEntries(unsigned __int64 Address, _DWORD *a2)                                        | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  v2 = a2;                                                                                                               | 
  v42 = 0LL;                                                                                                             | 
  P = 0LL;                                                                                                               | 
  v4 = 0LL;                                                                                                              | 
  if ( dword_140EFED30 != 2 )                                                                                            | 
  {                                                                                                                      | 
    return 3221225474LL;                                                                                                 | 
  }                                                                                                                      | 
  if ( (Address & 0xFFFFFFFFFFFFFFFCuLL) != Address )                                                                    | 
  {                                                                                                                      | 
    return 3221225485LL;                                                                                                 | 
  }                                                                                                                      | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                                                | 
  {                                                                                                                      | 
    return 3221225474LL;                                                                                                 | 
  }                                                                                                                      | 
  CurrentThread = KeGetCurrentThread();                                                                                  | 
  PreviousMode = CurrentThread->PreviousMode;                                                                            | 
  if ( PreviousMode )                                                                                                    | 
  {                                                                                                                      | 
    v8 = 0x7FFFFFFF0000LL;                                                                                               | 
    if ( (unsigned __int64)v2 < 0x7FFFFFFF0000LL )                                                                       | 
    {                                                                                                                    | 
      v8 = (__int64)v2;                                                                                                  | 
    }                                                                                                                    | 
    *(_DWORD *)v8 = *(_DWORD *)v8;                                                                                       | 
    v9 = Address != 0 ? *v2 : 0;                                                                                         | 
    if ( v9 )                                                                                                            | 
    {                                                                                                                    | 
      ProbeForWrite((volatile void *)Address, v9, 4u);                                                                   | 
    }                                                                                                                    | 
    if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                                           | 
    {                                                                                                                    | 
      return 3221225569LL;                                                                                               | 
    }                                                                                                                    | 
  }                                                                                                                      | 
  else                                                                                                                   | 
  {                                                                                                                      | 
    v9 = Address != 0 ? *v2 : 0;                                                                                         | 
  }                                                                                                                      | 
  if ( !v9 || (LOBYTE(v6) = PreviousMode, result = ExLockUserBuffer(Address, v9, v6, 1LL, &v42, &P), (int)result >= 0) ) | 
  {                                                                                                                      | 
    v10 = v42;                                                                                                           | 
    v11 = 0;                                                                                                             | 
    v35 = v9 != 0;                                                                                                       | 
    v12 = 0LL;                                                                                                           | 
    v13 = KeGetCurrentThread();                                                                                          | 
    --v13->KernelApcDisable;                                                                                             | 
    ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                                       | 
    v50 = 0;                                                                                                             | 
    v14 = IoEnumerateEnvironmentVariablesEx(2LL, ExpIsDriverEntry, 0LL, &v50);                                           | 
    if ( v14 == -1073741789 )                                                                                            | 
    {                                                                                                                    | 
      Pool2 = ExAllocatePool2(0x40uLL, v50, 0x72766E45u);                                                                | 
      v4 = (unsigned int *)Pool2;                                                                                        | 
      if ( Pool2 )                                                                                                       | 
      {                                                                                                                  | 
        v14 = IoEnumerateEnvironmentVariablesEx(2LL, ExpIsDriverEntry, Pool2, &v50);                                     | 
      }                                                                                                                  | 
      else                                                                                                               | 
      {                                                                                                                  | 
        v14 = -1073741670;                                                                                               | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      v50 = 0;                                                                                                           | 
    }                                                                                                                    | 
    ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                                       | 
    KeLeaveCriticalRegion();                                                                                             | 
    if ( v14 || !v50 )                                                                                                   | 
    {                                                                                                                    | 
      goto LABEL_54;                                                                                                     | 
    }                                                                                                                    | 
    v16 = v4;                                                                                                            | 
    v44 = (char *)v4 + 2 * v50;                                                                                          | 
    v17 = EfiDriverVariablesGuid;                                                                                        | 
    while ( 1 )                                                                                                          | 
    {                                                                                                                    | 
      v36 = 0;                                                                                                           | 
      v18 = *((_QWORD *)v16 + 2) - v17;                                                                                  | 
      if ( !v18 )                                                                                                        | 
      {                                                                                                                  | 
        v18 = *((_QWORD *)v16 + 3) + 0x73D4FC671FFFF256LL;                                                               | 
      }                                                                                                                  | 
      if ( !v18 )                                                                                                        | 
      {                                                                                                                  | 
        break;                                                                                                           | 
      }                                                                                                                  | 
LABEL_50:                                                                                                                | 
      v34 = *v16;                                                                                                        | 
      if ( !(_DWORD)v34 )                                                                                                | 
      {                                                                                                                  | 
        v2 = a2;                                                                                                         | 
        if ( v12 )                                                                                                       | 
        {                                                                                                                | 
          *v12 = 0;                                                                                                      | 
        }                                                                                                                | 
LABEL_54:                                                                                                                | 
        if ( v4 )                                                                                                        | 
        {                                                                                                                | 
          ExFreePoolWithTag(v4, 0);                                                                                      | 
        }                                                                                                                | 
        if ( P )                                                                                                         | 
        {                                                                                                                | 
          ExUnlockUserBuffer(P);                                                                                         | 
        }                                                                                                                | 
        if ( v14 >= 0 )                                                                                                  | 
        {                                                                                                                | 
          v14 = v11;                                                                                                     | 
        }                                                                                                                | 
        *v2 = (_DWORD)v10 - (_DWORD)v42;                                                                                 | 
        return (unsigned int)v14;                                                                                        | 
      }                                                                                                                  | 
      v16 = (unsigned int *)((char *)v16 + v34);                                                                         | 
    }                                                                                                                    | 
    if ( !(unsigned int)ExpTranslateDriverEntryNameToId(v16 + 8, &v36) || v16[2] < 8 )                                   | 
    {                                                                                                                    | 
      goto LABEL_49;                                                                                                     | 
    }                                                                                                                    | 
    v19 = (char *)v16 + v16[1];                                                                                          | 
    v39 = *((unsigned __int16 *)v19 + 2);                                                                                | 
    Src = v19 + 6;                                                                                                       | 
    v20 = ExpSafeWcslen(v19 + 6, v44);                                                                                   | 
    if ( v20 != -1 )                                                                                                     | 
    {                                                                                                                    | 
      v20 = 2 * v20 + 2;                                                                                                 | 
    }                                                                                                                    | 
    if ( v20 == -1 || v23 >= v22 || v22 < v23 + v20 + 6 )                                                                | 
    {                                                                                                                    | 
LABEL_49:                                                                                                                | 
      v17 = EfiDriverVariablesGuid;                                                                                      | 
      goto LABEL_50;                                                                                                     | 
    }                                                                                                                    | 
    Size = v20;                                                                                                          | 
    v47 = (void *)(v21 + v20 + 6LL);                                                                                     | 
    v24 = ((unsigned __int64)v10 + 3) & 0xFFFFFFFFFFFFFFFCuLL;                                                           | 
    if ( (_DWORD *)v24 != v10 )                                                                                          | 
    {                                                                                                                    | 
      v25 = v24 - (_DWORD)v10;                                                                                           | 
      v10 = (_DWORD *)(((unsigned __int64)v10 + 3) & 0xFFFFFFFFFFFFFFFCuLL);                                             | 
      if ( v9 < v25 )                                                                                                    | 
      {                                                                                                                  | 
        v26 = 0;                                                                                                         | 
        v9 = 0;                                                                                                          | 
        v11 = -1073741789;                                                                                               | 
LABEL_38:                                                                                                                | 
        v37 = (v20 + 23) & 0xFFFFFFFC;                                                                                   | 
        v38 = v23 + v37 + 12;                                                                                            | 
        v27 = v38 + 4;                                                                                                   | 
        v40 = v38 + 4;                                                                                                   | 
        if ( v9 < v38 + 4 )                                                                                              | 
        {                                                                                                                | 
          v11 = -1073741789;                                                                                             | 
        }                                                                                                                | 
        v41 = v11;                                                                                                       | 
        v28 = 0;                                                                                                         | 
        if ( v9 >= v27 )                                                                                                 | 
        {                                                                                                                | 
          v28 = v26;                                                                                                     | 
        }                                                                                                                | 
        v29 = v28;                                                                                                       | 
        v35 = v28;                                                                                                       | 
        v30 = v9;                                                                                                        | 
        v31 = v9 - v27;                                                                                                  | 
        v9 = 0;                                                                                                          | 
        if ( v30 >= v27 )                                                                                                | 
        {                                                                                                                | 
          v9 = v31;                                                                                                      | 
        }                                                                                                                | 
        if ( v29 )                                                                                                       | 
        {                                                                                                                | 
          memset_0(v10, 0, v27);                                                                                         | 
          v10[1] = 1;                                                                                                    | 
          v10[2] = v38;                                                                                                  | 
          v10[3] = v36;                                                                                                  | 
          v10[4] = 20;                                                                                                   | 
          v10[5] = v37;                                                                                                  | 
          memmove(v10 + 6, Src, Size);                                                                                   | 
          v32 = (_DWORD *)((char *)v10 + v37 + 4);                                                                       | 
          *v32 = 1;                                                                                                      | 
          v33 = v39;                                                                                                     | 
          v32[1] = v39 + 12;                                                                                             | 
          v32[2] = 4;                                                                                                    | 
          memmove(v32 + 3, v47, v33);                                                                                    | 
          if ( v12 )                                                                                                     | 
          {                                                                                                              | 
            *v12 = (_DWORD)v10 - (_DWORD)v12;                                                                            | 
          }                                                                                                              | 
          v12 = v10;                                                                                                     | 
          v27 = v40;                                                                                                     | 
          v11 = v41;                                                                                                     | 
        }                                                                                                                | 
        v10 = (_DWORD *)((char *)v10 + v27);                                                                             | 
        goto LABEL_49;                                                                                                   | 
      }                                                                                                                  | 
      v9 -= v25;                                                                                                         | 
    }                                                                                                                    | 
    v26 = v35;                                                                                                           | 
    goto LABEL_38;                                                                                                       | 
  }                                                                                                                      | 
  return result;                                                                                                         | 
}                                                                                                                        | 
207 row(s)
