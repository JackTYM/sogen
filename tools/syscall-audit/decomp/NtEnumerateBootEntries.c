__int64 __fastcall NtEnumerateBootEntries(unsigned __int64 Address, _DWORD *a2)                                | 
{                                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                | 
                                                                                                               | 
  v2 = a2;                                                                                                     | 
  v59 = 0LL;                                                                                                   | 
  P = 0LL;                                                                                                     | 
  if ( dword_140EFED30 != 2 )                                                                                  | 
  {                                                                                                            | 
    return 3221225474LL;                                                                                       | 
  }                                                                                                            | 
  if ( (Address & 0xFFFFFFFFFFFFFFFCuLL) != Address )                                                          | 
  {                                                                                                            | 
    return 3221225485LL;                                                                                       | 
  }                                                                                                            | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                                      | 
  {                                                                                                            | 
    return 3221225474LL;                                                                                       | 
  }                                                                                                            | 
  CurrentThread = KeGetCurrentThread();                                                                        | 
  v5 = CurrentThread->PreviousMode;                                                                            | 
  PreviousMode = v5;                                                                                           | 
  if ( v5 )                                                                                                    | 
  {                                                                                                            | 
    v7 = 0x7FFFFFFF0000LL;                                                                                     | 
    if ( (unsigned __int64)v2 < 0x7FFFFFFF0000LL )                                                             | 
    {                                                                                                          | 
      v7 = (__int64)v2;                                                                                        | 
    }                                                                                                          | 
    *(_DWORD *)v7 = *(_DWORD *)v7;                                                                             | 
    v6 = Address != 0 ? *v2 : 0;                                                                               | 
    v52 = v6;                                                                                                  | 
    if ( v6 )                                                                                                  | 
    {                                                                                                          | 
      ProbeForWrite((volatile void *)Address, v6, 4u);                                                         | 
    }                                                                                                          | 
    v5 = PreviousMode;                                                                                         | 
    if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                                 | 
    {                                                                                                          | 
      return 3221225569LL;                                                                                     | 
    }                                                                                                          | 
  }                                                                                                            | 
  else                                                                                                         | 
  {                                                                                                            | 
    v6 = Address != 0 ? *v2 : 0;                                                                               | 
    v52 = v6;                                                                                                  | 
  }                                                                                                            | 
  if ( !v6 || (LOBYTE(v4) = v5, result = ExLockUserBuffer(Address, v6, v4, 1LL, &v59, &P), (int)result >= 0) ) | 
  {                                                                                                            | 
    v9 = (int *)v59;                                                                                           | 
    v51 = v6 != 0;                                                                                             | 
    v10 = 0;                                                                                                   | 
    v49 = 0;                                                                                                   | 
    v58 = 0LL;                                                                                                 | 
    v11 = KeGetCurrentThread();                                                                                | 
    --v11->KernelApcDisable;                                                                                   | 
    ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                             | 
    v47 = 0x2000;                                                                                              | 
    Pool2 = ExAllocatePool2(0x40uLL, 0x2000uLL, 0x72766E45u);                                                  | 
    v13 = (unsigned int *)Pool2;                                                                               | 
    if ( !Pool2 )                                                                                              | 
    {                                                                                                          | 
      v47 = 0;                                                                                                 | 
    }                                                                                                          | 
    while ( 1 )                                                                                                | 
    {                                                                                                          | 
      v14 = v47;                                                                                               | 
      v15 = IoEnumerateEnvironmentVariablesEx(2LL, ExpIsBootEntry, Pool2, &v47);                               | 
      if ( v15 != -1073741789 )                                                                                | 
      {                                                                                                        | 
        break;                                                                                                 | 
      }                                                                                                        | 
      v46 = v47;                                                                                               | 
      if ( v14 >= v47 )                                                                                        | 
      {                                                                                                        | 
        break;                                                                                                 | 
      }                                                                                                        | 
      if ( v13 )                                                                                               | 
      {                                                                                                        | 
        ExFreePoolWithTag(v13, 0);                                                                             | 
        v46 = v47;                                                                                             | 
      }                                                                                                        | 
      Pool2 = ExAllocatePool2(0x40uLL, v46, 0x72766E45u);                                                      | 
      v13 = (unsigned int *)Pool2;                                                                             | 
      if ( !Pool2 )                                                                                            | 
      {                                                                                                        | 
        v15 = -1073741670;                                                                                     | 
        break;                                                                                                 | 
      }                                                                                                        | 
    }                                                                                                          | 
    ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                             | 
    KeLeaveCriticalRegion();                                                                                   | 
    if ( v15 || !v47 )                                                                                         | 
    {                                                                                                          | 
      if ( v15 == -1073741789 )                                                                                | 
      {                                                                                                        | 
        v15 = -1073741306;                                                                                     | 
      }                                                                                                        | 
    }                                                                                                          | 
    else                                                                                                       | 
    {                                                                                                          | 
      v16 = v13;                                                                                               | 
      v17 = (char *)v13 + 2 * v47;                                                                             | 
      v67 = v17;                                                                                               | 
      v18 = EfiBootVariablesGuid;                                                                              | 
      while ( 1 )                                                                                              | 
      {                                                                                                        | 
        v54 = 0;                                                                                               | 
        v19 = *((_QWORD *)v16 + 2) - v18;                                                                      | 
        if ( !v19 )                                                                                            | 
        {                                                                                                      | 
          v19 = *((_QWORD *)v16 + 3) + 0x73D4FC671FFFF256LL;                                                   | 
        }                                                                                                      | 
        if ( !v19 )                                                                                            | 
        {                                                                                                      | 
          if ( (unsigned int)ExpTranslateBootEntryNameToId(v16 + 8, &v54) )                                    | 
          {                                                                                                    | 
            v20 = v16[2];                                                                                      | 
            if ( v20 >= 8 )                                                                                    | 
            {                                                                                                  | 
              v61 = (char *)v16 + v16[1];                                                                      | 
              v21 = *((unsigned __int16 *)v61 + 2);                                                            | 
              v64 = v61 + 6;                                                                                   | 
              v22 = ExpSafeWcslen(v61 + 6, v17, v61, v21);                                                     | 
              v25 = v22;                                                                                       | 
              v50 = v22;                                                                                       | 
              if ( v22 != -1 )                                                                                 | 
              {                                                                                                | 
                v25 = 2 * v22 + 2;                                                                             | 
                v50 = v25;                                                                                     | 
              }                                                                                                | 
              v26 = v24 + v25 + 6;                                                                             | 
              if ( v25 != -1 && (unsigned int)v24 < v20 && v20 >= v26 )                                        | 
              {                                                                                                | 
                v70 = 1;                                                                                       | 
                Size = v25;                                                                                    | 
                v27 = (char *)(v23 + v25 + 6LL);                                                               | 
                v66 = v27;                                                                                     | 
                v65 = v24;                                                                                     | 
                Src = &v27[v24];                                                                               | 
                v28 = v20 - v26;                                                                               | 
                v29 = ((unsigned __int64)v9 + 3) & 0xFFFFFFFFFFFFFFFCuLL;                                      | 
                if ( (int *)v29 != v9 )                                                                        | 
                {                                                                                              | 
                  v45 = v29 - (_DWORD)v9;                                                                      | 
                  v9 = (int *)(((unsigned __int64)v9 + 3) & 0xFFFFFFFFFFFFFFFCuLL);                            | 
                  if ( v6 >= v45 )                                                                             | 
                  {                                                                                            | 
                    v6 -= v45;                                                                                 | 
                  }                                                                                            | 
                  else                                                                                         | 
                  {                                                                                            | 
                    v51 = 0;                                                                                   | 
                    v6 = 0;                                                                                    | 
                    v49 = -1073741789;                                                                         | 
                  }                                                                                            | 
                }                                                                                              | 
                v30 = v24 + 12;                                                                                | 
                v53 = v24 + 12;                                                                                | 
                v31 = v9 + 1;                                                                                  | 
                if ( v6 < (int)v24 + 12 )                                                                      | 
                {                                                                                              | 
                  v6 = 0;                                                                                      | 
                  v33 = 0;                                                                                     | 
                  v49 = -1073741789;                                                                           | 
                }                                                                                              | 
                else                                                                                           | 
                {                                                                                              | 
                  *v9 = 1;                                                                                     | 
                  *v31 = v30;                                                                                  | 
                  v9[2] = 4;                                                                                   | 
                  memmove(v9 + 3, v27, v24);                                                                   | 
                  v32 = ExpVerifyFilePath(v9, (char *)v9 + (unsigned int)*v31);                                | 
                  v33 = v51;                                                                                   | 
                  if ( v32 < 0 )                                                                               | 
                  {                                                                                            | 
                    v70 = 0;                                                                                   | 
                    v30 = 16;                                                                                  | 
                    v53 = 16;                                                                                  | 
                  }                                                                                            | 
                  else                                                                                         | 
                  {                                                                                            | 
                    v30 = v53;                                                                                 | 
                  }                                                                                            | 
                }                                                                                              | 
                v55 = (v28 + 31) & 0xFFFFFFFC;                                                                 | 
                v50 = (v50 + 3 + v55) & 0xFFFFFFFC;                                                            | 
                v57 = v30 + v50;                                                                               | 
                v34 = v30 + v50 + 4;                                                                           | 
                v56 = v34;                                                                                     | 
                v35 = v49;                                                                                     | 
                if ( v6 < v34 )                                                                                | 
                {                                                                                              | 
                  v35 = -1073741789;                                                                           | 
                }                                                                                              | 
                v49 = v35;                                                                                     | 
                v36 = 0;                                                                                       | 
                if ( v6 >= v34 )                                                                               | 
                {                                                                                              | 
                  v36 = v33;                                                                                   | 
                }                                                                                              | 
                v37 = v36;                                                                                     | 
                v51 = v36;                                                                                     | 
                v38 = v6;                                                                                      | 
                v39 = v6 - v34;                                                                                | 
                v6 = 0;                                                                                        | 
                if ( v38 >= v34 )                                                                              | 
                {                                                                                              | 
                  v6 = v39;                                                                                    | 
                }                                                                                              | 
                if ( v37 )                                                                                     | 
                {                                                                                              | 
                  memset_0(v9, 0, v34);                                                                        | 
                  *v31 = 1;                                                                                    | 
                  v9[2] = v57;                                                                                 | 
                  v9[3] = v54;                                                                                 | 
                  v9[4] = 0;                                                                                   | 
                  v40 = 0;                                                                                     | 
                  v41 = v61;                                                                                   | 
                  if ( (*(_DWORD *)v61 & 1) != 0 )                                                             | 
                  {                                                                                            | 
                    v9[4] = 1;                                                                                 | 
                    v40 = 1;                                                                                   | 
                  }                                                                                            | 
                  if ( (*v41 & 8) != 0 )                                                                       | 
                  {                                                                                            | 
                    v9[4] = v40 | 0x10;                                                                        | 
                  }                                                                                            | 
                  v9[5] = v55;                                                                                 | 
                  v9[6] = v50;                                                                                 | 
                  v9[7] = v28;                                                                                 | 
                  memmove(v9 + 8, Src, v28);                                                                   | 
                  if ( v28 > 0x14                                                                              | 
                    && !strcmp((const char *)v9 + 32, "WINDOWS")                                               | 
                    && (int)ExpVerifyWindowsOsOptions(v9 + 8, v28) >= 0 )                                      | 
                  {                                                                                            | 
                    v9[4] |= 4u;                                                                               | 
                  }                                                                                            | 
                  memmove((char *)v31 + v55, v64, Size);                                                       | 
                  v42 = (char *)v31 + v50;                                                                     | 
                  *(_DWORD *)v42 = 1;                                                                          | 
                  *((_DWORD *)v42 + 1) = v53;                                                                  | 
                  *((_DWORD *)v42 + 2) = 4;                                                                    | 
                  v43 = v42 + 12;                                                                              | 
                  if ( v70 )                                                                                   | 
                  {                                                                                            | 
                    memmove(v43, v66, v65);                                                                    | 
                    if ( (unsigned int)ExpIsDevicePathForRemovableMedia(v66) )                                 | 
                    {                                                                                          | 
                      v9[4] |= 8u;                                                                             | 
                    }                                                                                          | 
                  }                                                                                            | 
                  else                                                                                         | 
                  {                                                                                            | 
                    v9[4] |= 0x20u;                                                                            | 
                    *v43 = 127;                                                                                | 
                    *(_WORD *)(v42 + 13) = 1279;                                                               | 
                    v42[15] = 0;                                                                               | 
                  }                                                                                            | 
                  if ( v58 )                                                                                   | 
                  {                                                                                            | 
                    *v58 = (_DWORD)v9 - (_DWORD)v58;                                                           | 
                  }                                                                                            | 
                  v58 = v9;                                                                                    | 
                  v34 = v56;                                                                                   | 
                }                                                                                              | 
                v9 = (int *)((char *)v9 + v34);                                                                | 
              }                                                                                                | 
            }                                                                                                  | 
          }                                                                                                    | 
          v18 = EfiBootVariablesGuid;                                                                          | 
        }                                                                                                      | 
        v44 = *v16;                                                                                            | 
        if ( !(_DWORD)v44 )                                                                                    | 
        {                                                                                                      | 
          break;                                                                                               | 
        }                                                                                                      | 
        v16 = (unsigned int *)((char *)v16 + v44);                                                             | 
        v17 = v67;                                                                                             | 
      }                                                                                                        | 
      v2 = a2;                                                                                                 | 
      v10 = v49;                                                                                               | 
      if ( v58 )                                                                                               | 
      {                                                                                                        | 
        *v58 = 0;                                                                                              | 
      }                                                                                                        | 
    }                                                                                                          | 
    if ( v13 )                                                                                                 | 
    {                                                                                                          | 
      ExFreePoolWithTag(v13, 0);                                                                               | 
    }                                                                                                          | 
    if ( P )                                                                                                   | 
    {                                                                                                          | 
      ExUnlockUserBuffer(P);                                                                                   | 
    }                                                                                                          | 
    if ( v15 >= 0 )                                                                                            | 
    {                                                                                                          | 
      v15 = v10;                                                                                               | 
    }                                                                                                          | 
    *v2 = (_DWORD)v9 - (_DWORD)v59;                                                                            | 
    return (unsigned int)v15;                                                                                  | 
  }                                                                                                            | 
  return result;                                                                                               | 
}                                                                                                              | 
304 row(s)
