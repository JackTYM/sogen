__int64 __fastcall NtDeleteWnfStateName(void *a1)                                          | 
{                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                            | 
                                                                                           | 
  v21 = 0LL;                                                                               | 
  CurrentThread = KeGetCurrentThread();                                                    | 
  --CurrentThread->KernelApcDisable;                                                       | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                       | 
  v26 = 0;                                                                                 | 
  P = 0LL;                                                                                 | 
  v2 = 0LL;                                                                                | 
  *(_QWORD *)v19 = 0LL;                                                                    | 
  v16 = 0LL;                                                                               | 
  v3 = ExpCaptureWnfStateName(a1);                                                         | 
  v20 = v3;                                                                                | 
  if ( v3 >= 0 )                                                                           | 
  {                                                                                        | 
    v4 = v21;                                                                              | 
    v5 = v21 >> 4;                                                                         | 
    v6 = (v21 >> 6) & 0xF;                                                                 | 
    v23 = v6;                                                                              | 
    v7 = (v21 >> 4) & 3;                                                                   | 
    if ( ((v21 >> 4) & 3) == 0 )                                                           | 
    {                                                                                      | 
      v3 = -1073741811;                                                                    | 
      goto LABEL_21;                                                                       | 
    }                                                                                      | 
    v8 = PreviousMode;                                                                     | 
    v9 = PreviousMode == 0;                                                                | 
    if ( (_DWORD)v7 != 3 )                                                                 | 
    {                                                                                      | 
      if ( PreviousMode )                                                                  | 
      {                                                                                    | 
        v3 = ExpWnfLookupPermanentName(v21, &P);                                           | 
        if ( v3 < 0 )                                                                      | 
        {                                                                                  | 
          goto LABEL_21;                                                                   | 
        }                                                                                  | 
        v15 = (void *)*((_QWORD *)P + 2);                                                  | 
        memset(&SubjectContext, 0, sizeof(SubjectContext));                                | 
        GrantedAccess = 0;                                                                 | 
        AccessStatus = 0;                                                                  | 
        SeCaptureSubjectContext(&SubjectContext);                                          | 
        SeAccessCheck(                                                                     | 
          v15,                                                                             | 
          &SubjectContext,                                                                 | 
          0,                                                                               | 
          0x10000u,                                                                        | 
          0,                                                                               | 
          0LL,                                                                             | 
          (PGENERIC_MAPPING)&ExpWnfNotificationMapping,                                    | 
          1,                                                                               | 
          &GrantedAccess,                                                                  | 
          &AccessStatus);                                                                  | 
        SeReleaseSubjectContext(&SubjectContext);                                          | 
        v3 = AccessStatus;                                                                 | 
        if ( AccessStatus < 0 )                                                            | 
        {                                                                                  | 
          goto LABEL_21;                                                                   | 
        }                                                                                  | 
        v9 = 1;                                                                            | 
      }                                                                                    | 
      v3 = ExpWnfDeletePermanentName(v4);                                                  | 
      if ( v3 < 0 )                                                                        | 
      {                                                                                    | 
        goto LABEL_21;                                                                     | 
      }                                                                                    | 
      v26 = 1;                                                                             | 
      v6 = v23;                                                                            | 
      v8 = PreviousMode;                                                                   | 
    }                                                                                      | 
    if ( v8 )                                                                              | 
    {                                                                                      | 
      Process = KeGetCurrentThread()->ApcState.Process;                                    | 
      v4 = v21;                                                                            | 
    }                                                                                      | 
    else                                                                                   | 
    {                                                                                      | 
      Process = PsInitialSystemProcess;                                                    | 
    }                                                                                      | 
    v18 = Process;                                                                         | 
    if ( (_DWORD)v7 == 3 )                                                                 | 
    {                                                                                      | 
      v3 = ExpWnfResolveScopeInstance((int)v19, (int)Process, 0, v6, 0LL);                 | 
      v2 = *(struct _EX_RUNDOWN_REF **)v19;                                                | 
      if ( v3 < 0 )                                                                        | 
      {                                                                                    | 
        goto LABEL_21;                                                                     | 
      }                                                                                    | 
    }                                                                                      | 
    else                                                                                   | 
    {                                                                                      | 
      v3 = 0;                                                                              | 
      v2 = (struct _EX_RUNDOWN_REF *)ExpWnfEnumerateScopeInstances(v6, 0LL);               | 
    }                                                                                      | 
    v11 = v5 & 3;                                                                          | 
    while ( v2 )                                                                           | 
    {                                                                                      | 
      v3 = ExpWnfLookupNameInstance(v2, v4, &v16);                                         | 
      if ( v3 >= 0 )                                                                       | 
      {                                                                                    | 
        if ( !v9 )                                                                         | 
        {                                                                                  | 
          Ptr = v16[9].Ptr;                                                                | 
          memset(&SubjectContext, 0, sizeof(SubjectContext));                              | 
          GrantedAccess = 0;                                                               | 
          AccessStatus = 0;                                                                | 
          SeCaptureSubjectContext(&SubjectContext);                                        | 
          SeAccessCheck(                                                                   | 
            Ptr,                                                                           | 
            &SubjectContext,                                                               | 
            0,                                                                             | 
            0x10000u,                                                                      | 
            0,                                                                             | 
            0LL,                                                                           | 
            (PGENERIC_MAPPING)&ExpWnfNotificationMapping,                                  | 
            1,                                                                             | 
            &GrantedAccess,                                                                | 
            &AccessStatus);                                                                | 
          SeReleaseSubjectContext(&SubjectContext);                                        | 
          v3 = AccessStatus;                                                               | 
          if ( AccessStatus < 0 )                                                          | 
          {                                                                                | 
            goto LABEL_21;                                                                 | 
          }                                                                                | 
          v9 = 1;                                                                          | 
        }                                                                                  | 
        if ( v11 == 3LL && (_KPROCESS *)v16[19].Count != v18 )                             | 
        {                                                                                  | 
          v3 = -1073741790;                                                                | 
          goto LABEL_21;                                                                   | 
        }                                                                                  | 
        ExpWnfNotifyNameSubscribers(v16, 16LL, 1LL, PreviousMode != 0);                    | 
        LOBYTE(v13) = 1;                                                                   | 
        if ( (unsigned int)ExpWnfDeleteNameInstance(v2, v16, v13) )                        | 
        {                                                                                  | 
          v16 = 0LL;                                                                       | 
        }                                                                                  | 
        else                                                                               | 
        {                                                                                  | 
          v3 = -1073741772;                                                                | 
        }                                                                                  | 
      }                                                                                    | 
      if ( v11 == 3LL )                                                                    | 
      {                                                                                    | 
        break;                                                                             | 
      }                                                                                    | 
      if ( v16 )                                                                           | 
      {                                                                                    | 
        ExReleaseRundownProtection_0(v16 + 1);                                             | 
        v16 = 0LL;                                                                         | 
      }                                                                                    | 
      v2 = (struct _EX_RUNDOWN_REF *)ExpWnfEnumerateScopeInstances((unsigned int)v23, v2); | 
    }                                                                                      | 
    if ( (v4 & 0x400) != 0 )                                                               | 
    {                                                                                      | 
      ExpWnfDeletePermanentStateData(0LL, v4);                                             | 
    }                                                                                      | 
  }                                                                                        | 
LABEL_21:                                                                                  | 
  if ( v26 )                                                                               | 
  {                                                                                        | 
    v3 = 0;                                                                                | 
  }                                                                                        | 
  if ( v16 )                                                                               | 
  {                                                                                        | 
    ExReleaseRundownProtection_0(v16 + 1);                                                 | 
  }                                                                                        | 
  if ( v2 )                                                                                | 
  {                                                                                        | 
    ExReleaseRundownProtection_0(v2 + 1);                                                  | 
  }                                                                                        | 
  if ( P )                                                                                 | 
  {                                                                                        | 
    ExFreePoolWithTag(P, 0x20666E57u);                                                     | 
  }                                                                                        | 
  KeLeaveCriticalRegion();                                                                 | 
  return (unsigned int)v3;                                                                 | 
}                                                                                          | 
179 row(s)
