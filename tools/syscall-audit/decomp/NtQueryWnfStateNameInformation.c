__int64 __fastcall NtQueryWnfStateNameInformation(void *a1, int a2, __int64 a3, int *a4, unsigned int Length) | 
{                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                               | 
                                                                                                              | 
  v5 = a4;                                                                                                    | 
  v26 = 0LL;                                                                                                  | 
  Sid = 0LL;                                                                                                  | 
  CurrentThread = KeGetCurrentThread();                                                                       | 
  --CurrentThread->KernelApcDisable;                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                          | 
  P = 0LL;                                                                                                    | 
  *(_QWORD *)v28 = 0LL;                                                                                       | 
  v25 = 0LL;                                                                                                  | 
  v23 = 0;                                                                                                    | 
  v31 = 0LL;                                                                                                  | 
  v10 = ExpCaptureWnfStateName(a1);                                                                           | 
  if ( v10 >= 0 )                                                                                             | 
  {                                                                                                           | 
    v12 = v26;                                                                                                | 
    v27 = (v26 >> 4) & 3;                                                                                     | 
    v23 = (v26 >> 6) & 0xF;                                                                                   | 
    LOBYTE(v11) = PreviousMode;                                                                               | 
    v10 = ExpWnfCaptureScopeInstanceId((v26 >> 6) & 0xF, a3, v11, &Sid, &v31);                                | 
    if ( v10 >= 0 )                                                                                           | 
    {                                                                                                         | 
      if ( a2 && (unsigned int)(a2 - 1) >= 2 )                                                                | 
      {                                                                                                       | 
        v10 = -1073741821;                                                                                    | 
        goto LABEL_47;                                                                                        | 
      }                                                                                                       | 
      if ( Length < 4 )                                                                                       | 
      {                                                                                                       | 
        v10 = -1073741811;                                                                                    | 
        goto LABEL_47;                                                                                        | 
      }                                                                                                       | 
      if ( PreviousMode )                                                                                     | 
      {                                                                                                       | 
        ProbeForWrite(v5, Length, 4u);                                                                        | 
      }                                                                                                       | 
      v13 = 1;                                                                                                | 
      if ( a2 )                                                                                               | 
      {                                                                                                       | 
        v24 = 0;                                                                                              | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        v24 = 1;                                                                                              | 
        if ( a3 )                                                                                             | 
        {                                                                                                     | 
          v10 = -1073741811;                                                                                  | 
LABEL_46:                                                                                                     | 
          v5 = a4;                                                                                            | 
          goto LABEL_47;                                                                                      | 
        }                                                                                                     | 
      }                                                                                                       | 
      if ( PreviousMode && a2 )                                                                               | 
      {                                                                                                       | 
        v14 = 0;                                                                                              | 
        if ( a3 )                                                                                             | 
        {                                                                                                     | 
          v10 = ExpWnfCheckCrossScopeAccess(v26);                                                             | 
          if ( v10 < 0 )                                                                                      | 
          {                                                                                                   | 
            goto LABEL_46;                                                                                    | 
          }                                                                                                   | 
        }                                                                                                     | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        v14 = 1;                                                                                              | 
      }                                                                                                       | 
      v15 = 0;                                                                                                | 
      if ( !v14 && (unsigned int)(a2 - 1) <= 1 )                                                              | 
      {                                                                                                       | 
        v15 = 2;                                                                                              | 
      }                                                                                                       | 
      v16 = 0;                                                                                                | 
      if ( PreviousMode )                                                                                     | 
      {                                                                                                       | 
        v18 = KeGetCurrentThread();                                                                           | 
        Process = v18->ApcState.Process;                                                                      | 
        v12 = v26;                                                                                            | 
        if ( !v24 )                                                                                           | 
        {                                                                                                     | 
          v16 = (int)v18;                                                                                     | 
        }                                                                                                     | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        LODWORD(Process) = (_DWORD)PsInitialSystemProcess;                                                    | 
      }                                                                                                       | 
      v10 = ExpWnfResolveScopeInstance((int)v28, (int)Process, v16, v23, Sid);                                | 
      if ( v10 < 0 )                                                                                          | 
      {                                                                                                       | 
        goto LABEL_46;                                                                                        | 
      }                                                                                                       | 
      v19 = ExpWnfLookupNameInstance(*(_QWORD *)v28, v12, &v25);                                              | 
      v10 = v19;                                                                                              | 
      if ( v19 != -1073741772 || v27 == 3 )                                                                   | 
      {                                                                                                       | 
        if ( v19 < 0 )                                                                                        | 
        {                                                                                                     | 
          goto LABEL_46;                                                                                      | 
        }                                                                                                     | 
        if ( !v14 )                                                                                           | 
        {                                                                                                     | 
          v20 = *(void **)(v25 + 72);                                                                         | 
LABEL_36:                                                                                                     | 
          v10 = ExpWnfCheckCallerAccess(v20, v15);                                                            | 
          if ( v10 < 0 )                                                                                      | 
          {                                                                                                   | 
            goto LABEL_46;                                                                                    | 
          }                                                                                                   | 
        }                                                                                                     | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        v10 = ExpWnfLookupPermanentName(v12, &P);                                                             | 
        if ( v10 < 0 )                                                                                        | 
        {                                                                                                     | 
          goto LABEL_46;                                                                                      | 
        }                                                                                                     | 
        if ( !v14 )                                                                                           | 
        {                                                                                                     | 
          v20 = (void *)*((_QWORD *)P + 2);                                                                   | 
          goto LABEL_36;                                                                                      | 
        }                                                                                                     | 
      }                                                                                                       | 
      if ( a2 )                                                                                               | 
      {                                                                                                       | 
        if ( a2 == 1 )                                                                                        | 
        {                                                                                                     | 
          if ( v25 && *(_DWORD *)(v25 + 160) )                                                                | 
          {                                                                                                   | 
            goto LABEL_45;                                                                                    | 
          }                                                                                                   | 
        }                                                                                                     | 
        else if ( !v25 || !*(_DWORD *)(v25 + 164) )                                                           | 
        {                                                                                                     | 
          goto LABEL_45;                                                                                      | 
        }                                                                                                     | 
        v13 = 0;                                                                                              | 
      }                                                                                                       | 
LABEL_45:                                                                                                     | 
      *a4 = v13;                                                                                              | 
      v10 = 0;                                                                                                | 
      v5 = a4;                                                                                                | 
    }                                                                                                         | 
  }                                                                                                           | 
LABEL_47:                                                                                                     | 
  if ( v10 == -1073741772 && !a2 )                                                                            | 
  {                                                                                                           | 
    *v5 = 0;                                                                                                  | 
    v10 = 0;                                                                                                  | 
  }                                                                                                           | 
  if ( v25 )                                                                                                  | 
  {                                                                                                           | 
    ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)(v25 + 8));                                                 | 
  }                                                                                                           | 
  if ( *(_QWORD *)v28 )                                                                                       | 
  {                                                                                                           | 
    ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)(*(_QWORD *)v28 + 8LL));                                    | 
  }                                                                                                           | 
  if ( P )                                                                                                    | 
  {                                                                                                           | 
    ExFreePoolWithTag(P, 0x20666E57u);                                                                        | 
  }                                                                                                           | 
  KeLeaveCriticalRegion();                                                                                    | 
  LOBYTE(v21) = PreviousMode;                                                                                 | 
  ExpWnfReleaseCapturedScopeInstanceId(v23, &v31, v21);                                                       | 
  return (unsigned int)v10;                                                                                   | 
}                                                                                                             | 
172 row(s)
