__int64 __fastcall NtSystemDebugControl(                                                            | 
        int a1,                                                                                     | 
        _OWORD *a2,                                                                                 | 
        unsigned int a3,                                                                            | 
        bool *a4,                                                                                   | 
        unsigned int Length,                                                                        | 
        unsigned int *a6)                                                                           | 
{                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                     | 
                                                                                                    | 
  LODWORD(v51) = Length;                                                                            | 
  Size = 0;                                                                                         | 
  v54 = 0LL;                                                                                        | 
  v55 = 0LL;                                                                                        | 
  v56 = 0LL;                                                                                        | 
  v57 = 0LL;                                                                                        | 
  memset_0(v58, 0, 0x48uLL);                                                                        | 
  v52 = 0LL;                                                                                        | 
  P = 0LL;                                                                                          | 
  if ( KdPitchDebugger && !KdLocalDebugEnabled && ((a1 - 29) & 0xFFFFFFF7) != 0 )                   | 
  {                                                                                                 | 
    return 3221226324LL;                                                                            | 
  }                                                                                                 | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                | 
  BYTE4(v48) = PreviousMode;                                                                        | 
  BYTE5(v48) = PreviousMode;                                                                        | 
  if ( a1 != 38 && !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode)                          | 
    || (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                           | 
  {                                                                                                 | 
    return 3221225506LL;                                                                            | 
  }                                                                                                 | 
  v12 = 0;                                                                                          | 
  LODWORD(v48) = 0;                                                                                 | 
  if ( PreviousMode )                                                                               | 
  {                                                                                                 | 
    if ( a3 )                                                                                       | 
    {                                                                                               | 
      if ( ((unsigned __int8)a2 & 3) != 0 )                                                         | 
      {                                                                                             | 
        ExRaiseDatatypeMisalignment();                                                              | 
      }                                                                                             | 
      v13 = 0x7FFFFFFF0000LL;                                                                       | 
    }                                                                                               | 
    else                                                                                            | 
    {                                                                                               | 
      v13 = 0x7FFFFFFF0000LL;                                                                       | 
    }                                                                                               | 
    if ( Length )                                                                                   | 
    {                                                                                               | 
      ProbeForWrite(a4, Length, 4u);                                                                | 
    }                                                                                               | 
    if ( a6 )                                                                                       | 
    {                                                                                               | 
      if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )                                                | 
      {                                                                                             | 
        v13 = (__int64)a6;                                                                          | 
      }                                                                                             | 
      *(_DWORD *)v13 = *(_DWORD *)v13;                                                              | 
    }                                                                                               | 
    PreviousMode = BYTE4(v48);                                                                      | 
  }                                                                                                 | 
  if ( a1 > 19 )                                                                                    | 
  {                                                                                                 | 
    if ( a1 <= 29 )                                                                                 | 
    {                                                                                               | 
      if ( a1 == 29 )                                                                               | 
      {                                                                                             | 
        if ( a3 == 56 && Length >= 0x40000 )                                                        | 
        {                                                                                           | 
          v54 = *a2;                                                                                | 
          v55 = a2[1];                                                                              | 
          v56 = a2[2];                                                                              | 
          v57 = *((_QWORD *)a2 + 6);                                                                | 
          if ( DWORD2(v56)                                                                          | 
            || !HIDWORD(v56)                                                                        | 
            || (v54 & 0xFFFFFFFE) != 0                                                              | 
            || 8 * (unsigned __int64)HIDWORD(v56) > 0xFFFFFFFF )                                    | 
          {                                                                                         | 
            return 3221225485LL;                                                                    | 
          }                                                                                         | 
          v35 = 0x100000;                                                                           | 
          if ( Length <= 0x100000 )                                                                 | 
          {                                                                                         | 
            v35 = (unsigned int)v51;                                                                | 
          }                                                                                         | 
          v36 = v35;                                                                                | 
          Pool2 = (void *)ExAllocatePool2(0x40uLL, v35, 0x704E534Bu);                               | 
          v51 = Pool2;                                                                              | 
          if ( Pool2 )                                                                              | 
          {                                                                                         | 
            LOBYTE(v38) = PreviousMode;                                                             | 
            v12 = ExLockUserBuffer(v57, (unsigned int)(8 * HIDWORD(v56)), v38, 0LL, &v52, &P, v48); | 
            if ( v12 >= 0 )                                                                         | 
            {                                                                                       | 
              v57 = v52;                                                                            | 
              v39 = DbgkCaptureLiveDump(&v54, Pool2, v36, &Size);                                   | 
              v12 = v39;                                                                            | 
              if ( v39 >= 0 )                                                                       | 
              {                                                                                     | 
                if ( Size <= Length )                                                               | 
                {                                                                                   | 
                  EtwTiLogSyscallUsage((unsigned int)v39, 25LL);                                    | 
                  memmove(a4, Pool2, Size);                                                         | 
                }                                                                                   | 
                else                                                                                | 
                {                                                                                   | 
                  v12 = -1073741823;                                                                | 
                }                                                                                   | 
              }                                                                                     | 
              ExFreePoolWithTag(Pool2, 0);                                                          | 
              ExUnlockUserBuffer(P);                                                                | 
            }                                                                                       | 
            else                                                                                    | 
            {                                                                                       | 
              ExFreePoolWithTag(Pool2, 0);                                                          | 
            }                                                                                       | 
          }                                                                                         | 
          else                                                                                      | 
          {                                                                                         | 
            v12 = -1073741801;                                                                      | 
          }                                                                                         | 
          goto LABEL_119;                                                                           | 
        }                                                                                           | 
        return 3221225476LL;                                                                        | 
      }                                                                                             | 
      v26 = a1 - 20;                                                                                | 
      if ( !v26 )                                                                                   | 
      {                                                                                             | 
        return 3221225474LL;                                                                        | 
      }                                                                                             | 
      v27 = v26 - 1;                                                                                | 
      if ( v27 )                                                                                    | 
      {                                                                                             | 
        v28 = v27 - 1;                                                                              | 
        if ( v28 )                                                                                  | 
        {                                                                                           | 
          v29 = v28 - 1;                                                                            | 
          if ( !v29 )                                                                               | 
          {                                                                                         | 
            if ( Length == 1 )                                                                      | 
            {                                                                                       | 
              *a4 = KdAutoEnableOnEvent;                                                            | 
              goto LABEL_119;                                                                       | 
            }                                                                                       | 
            return 3221225476LL;                                                                    | 
          }                                                                                         | 
          v30 = v29 - 1;                                                                            | 
          if ( !v30 )                                                                               | 
          {                                                                                         | 
            if ( a3 == 1 )                                                                          | 
            {                                                                                       | 
              KdAutoEnableOnEvent = *(_BYTE *)a2 != 0;                                              | 
              goto LABEL_119;                                                                       | 
            }                                                                                       | 
            return 3221225476LL;                                                                    | 
          }                                                                                         | 
          v31 = v30 - 1;                                                                            | 
          if ( !v31 )                                                                               | 
          {                                                                                         | 
            if ( Length == 4 )                                                                      | 
            {                                                                                       | 
              *(_DWORD *)a4 = KdPrintBufferSize;                                                    | 
              goto LABEL_119;                                                                       | 
            }                                                                                       | 
            return 3221225476LL;                                                                    | 
          }                                                                                         | 
          v32 = v31 - 1;                                                                            | 
          if ( v32 )                                                                                | 
          {                                                                                         | 
            v33 = v32 - 1;                                                                          | 
            if ( v33 )                                                                              | 
            {                                                                                       | 
              if ( v33 != 1 )                                                                       | 
              {                                                                                     | 
                goto LABEL_102;                                                                     | 
              }                                                                                     | 
              if ( a3 == 1 )                                                                        | 
              {                                                                                     | 
                KdIgnoreUmExceptions = *(_BYTE *)a2 == 0;                                           | 
                goto LABEL_119;                                                                     | 
              }                                                                                     | 
            }                                                                                       | 
            else if ( Length == 1 )                                                                 | 
            {                                                                                       | 
              *a4 = KdIgnoreUmExceptions == 0;                                                      | 
              goto LABEL_119;                                                                       | 
            }                                                                                       | 
            return 3221225476LL;                                                                    | 
          }                                                                                         | 
          if ( a3 != 4 )                                                                            | 
          {                                                                                         | 
            return 3221225476LL;                                                                    | 
          }                                                                                         | 
          Size_4 = 0;                                                                               | 
          Size_4 = *(_DWORD *)a2;                                                                   | 
          v34 = KdSetDbgPrintBufferSize();                                                          | 
        }                                                                                           | 
        else                                                                                        | 
        {                                                                                           | 
          v34 = KdDisableDebugger();                                                                | 
        }                                                                                           | 
      }                                                                                             | 
      else                                                                                          | 
      {                                                                                             | 
        v34 = KdEnableDebugger();                                                                   | 
      }                                                                                             | 
      goto LABEL_71;                                                                                | 
    }                                                                                               | 
    v40 = a1 - 30;                                                                                  | 
    if ( !v40 )                                                                                     | 
    {                                                                                               | 
      if ( Length == 1 )                                                                            | 
      {                                                                                             | 
        *a4 = KdBlockEnable;                                                                        | 
        goto LABEL_119;                                                                             | 
      }                                                                                             | 
      return 3221225476LL;                                                                          | 
    }                                                                                               | 
    v41 = v40 - 1;                                                                                  | 
    if ( !v41 )                                                                                     | 
    {                                                                                               | 
      if ( a3 == 1 )                                                                                | 
      {                                                                                             | 
        KdBlockEnable = *(_BYTE *)a2;                                                               | 
        goto LABEL_119;                                                                             | 
      }                                                                                             | 
      return 3221225476LL;                                                                          | 
    }                                                                                               | 
    v42 = v41 - 1;                                                                                  | 
    if ( !v42 )                                                                                     | 
    {                                                                                               | 
      KdUmBreakMarker = -618808389;                                                                 | 
      goto LABEL_104;                                                                               | 
    }                                                                                               | 
    v43 = v42 - 1;                                                                                  | 
    if ( v43 )                                                                                      | 
    {                                                                                               | 
      v44 = v43 - 1;                                                                                | 
      if ( v44 )                                                                                    | 
      {                                                                                             | 
        v45 = v44 - 1;                                                                              | 
        if ( v45 )                                                                                  | 
        {                                                                                           | 
          v46 = v45 - 1;                                                                            | 
          if ( v46 )                                                                                | 
          {                                                                                         | 
            v47 = v46 - 1;                                                                          | 
            if ( !v47 )                                                                             | 
            {                                                                                       | 
              if ( !Length && ((a3 - 64) & 0xFFFFFFF7) == 0 )                                       | 
              {                                                                                     | 
                memmove(v58, a2, a3);                                                               | 
                v12 = DbgkCaptureLiveKernelDump(v58);                                               | 
                EtwTiLogSyscallUsage((unsigned int)v12, 26LL);                                      | 
                goto LABEL_119;                                                                     | 
              }                                                                                     | 
              return 3221225476LL;                                                                  | 
            }                                                                                       | 
            if ( v47 != 1 )                                                                         | 
            {                                                                                       | 
              goto LABEL_102;                                                                       | 
            }                                                                                       | 
            if ( a3 != 16 || Length )                                                               | 
            {                                                                                       | 
              return 3221225476LL;                                                                  | 
            }                                                                                       | 
            v34 = ExpKdPullRemoteFileForUser(a2);                                                   | 
LABEL_71:                                                                                           | 
            v12 = v34;                                                                              | 
            goto LABEL_119;                                                                         | 
          }                                                                                         | 
          goto LABEL_110;                                                                           | 
        }                                                                                           | 
        if ( Length != 4 )                                                                          | 
        {                                                                                           | 
          return 3221225476LL;                                                                      | 
        }                                                                                           | 
        *(_DWORD *)a4 = KdUmAttachPid;                                                              | 
        if ( KdResetUmAttachPid )                                                                   | 
        {                                                                                           | 
LABEL_110:                                                                                          | 
          KdUmAttachPid = 0;                                                                        | 
        }                                                                                           | 
LABEL_104:                                                                                          | 
        v12 = 0;                                                                                    | 
        goto LABEL_119;                                                                             | 
      }                                                                                             | 
    }                                                                                               | 
    else                                                                                            | 
    {                                                                                               | 
      if ( Length != 4 )                                                                            | 
      {                                                                                             | 
        return 3221225476LL;                                                                        | 
      }                                                                                             | 
      *(_DWORD *)a4 = KdUmBreakPid;                                                                 | 
      if ( !KdResetUmBreakPid )                                                                     | 
      {                                                                                             | 
        goto LABEL_104;                                                                             | 
      }                                                                                             | 
    }                                                                                               | 
    KdUmBreakPid = 0;                                                                               | 
    goto LABEL_104;                                                                                 | 
  }                                                                                                 | 
  if ( a1 == 19 )                                                                                   | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  if ( a1 > 9 )                                                                                     | 
  {                                                                                                 | 
    v20 = a1 - 10;                                                                                  | 
    if ( !v20 )                                                                                     | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
    v21 = v20 - 1;                                                                                  | 
    if ( !v21 )                                                                                     | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
    v22 = v21 - 1;                                                                                  | 
    if ( !v22 )                                                                                     | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
    v23 = v22 - 1;                                                                                  | 
    if ( !v23 )                                                                                     | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
    v24 = v23 - 1;                                                                                  | 
    if ( !v24 )                                                                                     | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
    v25 = v24 - 1;                                                                                  | 
    if ( !v25 )                                                                                     | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
    v19 = v25 - 1;                                                                                  | 
    if ( !v19 )                                                                                     | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
    goto LABEL_44;                                                                                  | 
  }                                                                                                 | 
  if ( a1 == 9 )                                                                                    | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  if ( !a1 )                                                                                        | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  v14 = a1 - 1;                                                                                     | 
  if ( !v14 )                                                                                       | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  v15 = v14 - 1;                                                                                    | 
  if ( !v15 )                                                                                       | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  v16 = v15 - 1;                                                                                    | 
  if ( !v16 )                                                                                       | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  v17 = v16 - 1;                                                                                    | 
  if ( !v17 )                                                                                       | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  v18 = v17 - 1;                                                                                    | 
  if ( !v18 )                                                                                       | 
  {                                                                                                 | 
    return 3221225474LL;                                                                            | 
  }                                                                                                 | 
  v19 = v18 - 1;                                                                                    | 
  if ( v19 )                                                                                        | 
  {                                                                                                 | 
LABEL_44:                                                                                           | 
    if ( (unsigned int)(v19 - 1) <= 1 )                                                             | 
    {                                                                                               | 
      return 3221225474LL;                                                                          | 
    }                                                                                               | 
LABEL_102:                                                                                          | 
    v12 = -1073741821;                                                                              | 
    goto LABEL_119;                                                                                 | 
  }                                                                                                 | 
  if ( (_BYTE)KdDebuggerEnabled == 1 )                                                              | 
  {                                                                                                 | 
    DbgBreakPointWithStatus(6u);                                                                    | 
  }                                                                                                 | 
  else                                                                                              | 
  {                                                                                                 | 
    v12 = -1073741823;                                                                              | 
  }                                                                                                 | 
LABEL_119:                                                                                          | 
  if ( a6 )                                                                                         | 
  {                                                                                                 | 
    *a6 = Size;                                                                                     | 
  }                                                                                                 | 
  return (unsigned int)v12;                                                                         | 
}                                                                                                   | 
406 row(s)
