__int64 __fastcall NtQueryWnfStateData(                                                                       | 
        __int64 *Src,                                                                                         | 
        unsigned __int64 a2,                                                                                  | 
        ULONG_PTR BugCheckParameter1,                                                                         | 
        _DWORD *a4,                                                                                           | 
        unsigned __int64 a5,                                                                                  | 
        unsigned int *a6)                                                                                     | 
{                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                               | 
                                                                                                              | 
  v6 = (unsigned __int64)a4;                                                                                  | 
  v64 = a4;                                                                                                   | 
  v9 = a6;                                                                                                    | 
  v63 = (__int64)a6;                                                                                          | 
  v71 = 0LL;                                                                                                  | 
  Sid = 0LL;                                                                                                  | 
  CurrentThread = KeGetCurrentThread();                                                                       | 
  --CurrentThread->KernelApcDisable;                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                          | 
  P = 0LL;                                                                                                    | 
  *(_QWORD *)v58 = 0LL;                                                                                       | 
  v57 = 0LL;                                                                                                  | 
  v54 = 0;                                                                                                    | 
  *(_OWORD *)Object = 0LL;                                                                                    | 
  v68 = (__int128 *)a2;                                                                                       | 
  v67 = 0LL;                                                                                                  | 
  v12 = 0x7FFFFFFF0000LL;                                                                                     | 
  if ( PreviousMode )                                                                                         | 
  {                                                                                                           | 
    v29 = 0x7FFFFFFF0000LL;                                                                                   | 
    if ( (unsigned __int64)Src < 0x7FFFFFFF0000LL )                                                           | 
    {                                                                                                         | 
      v29 = (__int64)Src;                                                                                     | 
    }                                                                                                         | 
    RtlCopyVolatileMemory(&v67, (const void *)v29, 8uLL);                                                     | 
    v13 = v67;                                                                                                | 
    v9 = (unsigned int *)v63;                                                                                 | 
    v6 = (unsigned __int64)v64;                                                                               | 
  }                                                                                                           | 
  else                                                                                                        | 
  {                                                                                                           | 
    v13 = *Src;                                                                                               | 
    v67 = *Src;                                                                                               | 
  }                                                                                                           | 
  v14 = v13 ^ 0x41C64E6DA3BC0074LL;                                                                           | 
  v56 = v14;                                                                                                  | 
  if ( (v14 & 0xF) != 1                                                                                       | 
    || (v15 = (v14 >> 6) & 0xF, v16 = (v14 >> 4) & 3, v69 = v14 >> 10, (_DWORD)v16 != 3)                      | 
    && (_DWORD)v16                                                                                            | 
    && (unsigned int)(v16 - 1) >= 2                                                                           | 
    || (_DWORD)v15 != 5 && (unsigned int)v15 > 2 && (unsigned int)(v15 - 3) > 1                               | 
    || (v14 & 0x400) != 0 && (((v14 >> 6) & 9) != 0 || (_DWORD)v15 == 6) )                                    | 
  {                                                                                                           | 
    NameInstance = -1073741811;                                                                               | 
    goto LABEL_83;                                                                                            | 
  }                                                                                                           | 
  if ( PreviousMode )                                                                                         | 
  {                                                                                                           | 
    if ( a2 )                                                                                                 | 
    {                                                                                                         | 
      v17 = 0x7FFFFFFF0000LL;                                                                                 | 
      if ( a2 < 0x7FFFFFFF0000LL )                                                                            | 
      {                                                                                                       | 
        v17 = a2;                                                                                             | 
      }                                                                                                       | 
      RtlCopyVolatileMemory(&v71, (const void *)v17, 0x10uLL);                                                | 
      v68 = &v71;                                                                                             | 
      v9 = (unsigned int *)v63;                                                                               | 
      v6 = (unsigned __int64)v64;                                                                             | 
    }                                                                                                         | 
    v18 = 0x7FFFFFFF0000LL;                                                                                   | 
    if ( v6 < 0x7FFFFFFF0000LL )                                                                              | 
    {                                                                                                         | 
      v18 = v6;                                                                                               | 
    }                                                                                                         | 
    *(_DWORD *)v18 = *(_DWORD *)v18;                                                                          | 
    v19 = 0x7FFFFFFF0000LL;                                                                                   | 
    if ( (unsigned __int64)v9 < 0x7FFFFFFF0000LL )                                                            | 
    {                                                                                                         | 
      v19 = (__int64)v9;                                                                                      | 
    }                                                                                                         | 
    v20 = *(_DWORD *)v19;                                                                                     | 
    v55 = *(_DWORD *)v19;                                                                                     | 
    v21 = 0x7FFFFFFF0000LL;                                                                                   | 
    if ( (unsigned __int64)v9 < 0x7FFFFFFF0000LL )                                                            | 
    {                                                                                                         | 
      v21 = (__int64)v9;                                                                                      | 
    }                                                                                                         | 
    *(_DWORD *)v21 = *(_DWORD *)v21;                                                                          | 
    v22 = a5;                                                                                                 | 
    if ( v20 )                                                                                                | 
    {                                                                                                         | 
      v23 = a5;                                                                                               | 
      v24 = v20 + a5 - 1;                                                                                     | 
      if ( a5 > v24 || v24 >= 0x7FFFFFFF0000LL )                                                              | 
      {                                                                                                       | 
        ExRaiseAccessViolation();                                                                             | 
      }                                                                                                       | 
      v25 = (v24 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                             | 
      do                                                                                                      | 
      {                                                                                                       | 
        *(_BYTE *)v23 = *(_BYTE *)v23;                                                                        | 
        v23 = (v23 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                           | 
      }                                                                                                       | 
      while ( v23 != v25 );                                                                                   | 
    }                                                                                                         | 
  }                                                                                                           | 
  else                                                                                                        | 
  {                                                                                                           | 
    v55 = *v9;                                                                                                | 
    v22 = a5;                                                                                                 | 
  }                                                                                                           | 
  v65 = (v14 >> 4) & 3;                                                                                       | 
  v26 = (v14 >> 6) & 0xF;                                                                                     | 
  v54 = v26;                                                                                                  | 
  if ( !BugCheckParameter1 )                                                                                  | 
  {                                                                                                           | 
    Sid = 0LL;                                                                                                | 
    goto LABEL_59;                                                                                            | 
  }                                                                                                           | 
  switch ( v26 )                                                                                              | 
  {                                                                                                           | 
    case 3:                                                                                                   | 
      v27 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x20666E57u, (__int64)Object, 0LL, 0LL);    | 
      if ( v27 < 0 )                                                                                          | 
      {                                                                                                       | 
        break;                                                                                                | 
      }                                                                                                       | 
      v28 = Object;                                                                                           | 
      goto LABEL_28;                                                                                          | 
    case 0:                                                                                                   | 
LABEL_42:                                                                                                     | 
      v27 = -1073741811;                                                                                      | 
      break;                                                                                                  | 
    case 1:                                                                                                   | 
      if ( PreviousMode )                                                                                     | 
      {                                                                                                       | 
        if ( BugCheckParameter1 < 0x7FFFFFFF0000LL )                                                          | 
        {                                                                                                     | 
          v12 = BugCheckParameter1;                                                                           | 
        }                                                                                                     | 
        LODWORD(Object[1]) = *(_DWORD *)v12;                                                                  | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        LODWORD(Object[1]) = *(_DWORD *)BugCheckParameter1;                                                   | 
      }                                                                                                       | 
      Object[0] = (PVOID)PsGetSessionById();                                                                  | 
      if ( Object[0] )                                                                                        | 
      {                                                                                                       | 
        Sid = &Object[1];                                                                                     | 
        v27 = 0;                                                                                              | 
        break;                                                                                                | 
      }                                                                                                       | 
      goto LABEL_42;                                                                                          | 
    case 2:                                                                                                   | 
      v27 = SeCaptureSid((void *)BugCheckParameter1, PreviouslyGrantedAccess, 0, (__int64)Object);            | 
      if ( v27 >= 0 )                                                                                         | 
      {                                                                                                       | 
        v28 = (PVOID *)Object[0];                                                                             | 
LABEL_28:                                                                                                     | 
        Sid = v28;                                                                                            | 
      }                                                                                                       | 
      break;                                                                                                  | 
    default:                                                                                                  | 
      v27 = -1073741811;                                                                                      | 
      break;                                                                                                  | 
  }                                                                                                           | 
  NameInstance = v27;                                                                                         | 
  if ( v27 < 0 )                                                                                              | 
  {                                                                                                           | 
    goto LABEL_83;                                                                                            | 
  }                                                                                                           | 
LABEL_59:                                                                                                     | 
  if ( PreviousMode )                                                                                         | 
  {                                                                                                           | 
    v30 = 0;                                                                                                  | 
    if ( BugCheckParameter1 )                                                                                 | 
    {                                                                                                         | 
      NameInstance = ExpWnfCheckCrossScopeAccess(v14);                                                        | 
      if ( NameInstance < 0 )                                                                                 | 
      {                                                                                                       | 
        goto LABEL_83;                                                                                        | 
      }                                                                                                       | 
    }                                                                                                         | 
  }                                                                                                           | 
  else                                                                                                        | 
  {                                                                                                           | 
    v30 = 1;                                                                                                  | 
  }                                                                                                           | 
  if ( PreviousMode )                                                                                         | 
  {                                                                                                           | 
    v31 = KeGetCurrentThread();                                                                               | 
    Process = v31->ApcState.Process;                                                                          | 
  }                                                                                                           | 
  else                                                                                                        | 
  {                                                                                                           | 
    LODWORD(v31) = 0;                                                                                         | 
    LODWORD(Process) = (_DWORD)PsInitialSystemProcess;                                                        | 
  }                                                                                                           | 
  NameInstance = ExpWnfResolveScopeInstance((int)v58, (int)Process, (int)v31, v26, Sid);                      | 
  if ( NameInstance >= 0 )                                                                                    | 
  {                                                                                                           | 
    v33 = (volatile signed __int64 *)(*(_QWORD *)v58 + 48LL);                                                 | 
    v34 = KeAbPreAcquire(*(_QWORD *)v58 + 48LL, 0LL, 0LL);                                                    | 
    if ( _InterlockedCompareExchange64(v33, 17LL, 0LL) )                                                      | 
    {                                                                                                         | 
      ExfAcquirePushLockSharedEx(v33, 0LL, v34, v33);                                                         | 
    }                                                                                                         | 
    if ( v34 )                                                                                                | 
    {                                                                                                         | 
      *(_BYTE *)(v34 + 10) = 1;                                                                               | 
    }                                                                                                         | 
    v35 = *(_QWORD **)(*(_QWORD *)v58 + 56LL);                                                                | 
    if ( !v35 )                                                                                               | 
    {                                                                                                         | 
      goto LABEL_79;                                                                                          | 
    }                                                                                                         | 
    do                                                                                                        | 
    {                                                                                                         | 
      if ( v56 < v35[3] )                                                                                     | 
      {                                                                                                       | 
        v35 = (_QWORD *)*v35;                                                                                 | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        if ( v56 <= v35[3] )                                                                                  | 
        {                                                                                                     | 
          break;                                                                                              | 
        }                                                                                                     | 
        v35 = (_QWORD *)v35[1];                                                                               | 
      }                                                                                                       | 
    }                                                                                                         | 
    while ( v35 );                                                                                            | 
    if ( v35 )                                                                                                | 
    {                                                                                                         | 
      v36 = (struct _EX_RUNDOWN_REF *)(v35 - 2);                                                              | 
      if ( v35 != (_QWORD *)16 )                                                                              | 
      {                                                                                                       | 
        _m_prefetchw(&v36[1]);                                                                                | 
        v37 = v36[1].Count & 0xFFFFFFFFFFFFFFFEuLL;                                                           | 
        if ( v37 != _InterlockedCompareExchange64((volatile signed __int64 *)&v36[1], v37 + 2, v37)           | 
          && !ExfAcquireRundownProtection(v36 + 1) )                                                          | 
        {                                                                                                     | 
          v36 = 0LL;                                                                                          | 
        }                                                                                                     | 
      }                                                                                                       | 
    }                                                                                                         | 
    else                                                                                                      | 
    {                                                                                                         | 
LABEL_79:                                                                                                     | 
      v36 = 0LL;                                                                                              | 
    }                                                                                                         | 
    v51 = *(_QWORD *)v58;                                                                                     | 
    if ( _InterlockedCompareExchange64((volatile signed __int64 *)(*(_QWORD *)v58 + 48LL), 0LL, 17LL) != 17 ) | 
    {                                                                                                         | 
      ExfReleasePushLockShared(v51 + 48);                                                                     | 
    }                                                                                                         | 
    KeAbPostRelease(v51 + 48);                                                                                | 
    if ( v36 )                                                                                                | 
    {                                                                                                         | 
      v57 = v36;                                                                                              | 
      v49 = 0;                                                                                                | 
    }                                                                                                         | 
    else                                                                                                      | 
    {                                                                                                         | 
      v49 = -1073741772;                                                                                      | 
    }                                                                                                         | 
    NameInstance = v49;                                                                                       | 
    if ( v49 == -1073741772 && v65 != 3 )                                                                     | 
    {                                                                                                         | 
      NameInstance = ExpWnfLookupPermanentName(v56, &P);                                                      | 
      if ( NameInstance < 0 )                                                                                 | 
      {                                                                                                       | 
        goto LABEL_83;                                                                                        | 
      }                                                                                                       | 
      NameInstance = ExpWnfValidatePubSubPreconditions(1u, v30);                                              | 
      if ( NameInstance < 0 )                                                                                 | 
      {                                                                                                       | 
        goto LABEL_83;                                                                                        | 
      }                                                                                                       | 
      if ( v54 == 5 || (v69 & 1) != 0 )                                                                       | 
      {                                                                                                       | 
        NameInstance = ExpWnfCreateNameInstance(v58[0], v56, (_DWORD)P, (_DWORD)Process, (__int64)&v57);      | 
        ExFreePoolWithTag(P, 0x20666E57u);                                                                    | 
        P = 0LL;                                                                                              | 
        if ( NameInstance < 0 )                                                                               | 
        {                                                                                                     | 
          goto LABEL_83;                                                                                      | 
        }                                                                                                     | 
      }                                                                                                       | 
LABEL_98:                                                                                                     | 
      v45 = (_DWORD *)v63;                                                                                    | 
      if ( v57 )                                                                                              | 
      {                                                                                                       | 
        NameInstance = ExpWnfReadStateData(v57, v64, v22, v55, v63);                                          | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        *v64 = 0;                                                                                             | 
        *v45 = 0;                                                                                             | 
        NameInstance = 0;                                                                                     | 
      }                                                                                                       | 
      goto LABEL_83;                                                                                          | 
    }                                                                                                         | 
    if ( v49 < 0 )                                                                                            | 
    {                                                                                                         | 
      goto LABEL_83;                                                                                          | 
    }                                                                                                         | 
    if ( v30 )                                                                                                | 
    {                                                                                                         | 
      goto LABEL_80;                                                                                          | 
    }                                                                                                         | 
    Ptr = v57[9].Ptr;                                                                                         | 
    memset(&SubjectContext, 0, sizeof(SubjectContext));                                                       | 
    GrantedAccess = 0;                                                                                        | 
    AccessStatus = 0;                                                                                         | 
    SeCaptureSubjectContext(&SubjectContext);                                                                 | 
    SeAccessCheck(                                                                                            | 
      Ptr,                                                                                                    | 
      &SubjectContext,                                                                                        | 
      0,                                                                                                      | 
      1u,                                                                                                     | 
      0,                                                                                                      | 
      0LL,                                                                                                    | 
      (PGENERIC_MAPPING)&ExpWnfNotificationMapping,                                                           | 
      1,                                                                                                      | 
      &GrantedAccess,                                                                                         | 
      &AccessStatus);                                                                                         | 
    SeReleaseSubjectContext(&SubjectContext);                                                                 | 
    v39 = AccessStatus;                                                                                       | 
    if ( AccessStatus >= 0 )                                                                                  | 
    {                                                                                                         | 
LABEL_80:                                                                                                     | 
      Count = (_QWORD *)v57[8].Count;                                                                         | 
      if ( !Count )                                                                                           | 
      {                                                                                                       | 
LABEL_81:                                                                                                     | 
        v39 = 0;                                                                                              | 
        goto LABEL_82;                                                                                        | 
      }                                                                                                       | 
      if ( v68 )                                                                                              | 
      {                                                                                                       | 
        v48 = *Count - *(_QWORD *)v68;                                                                        | 
        if ( *Count == *(_QWORD *)v68 )                                                                       | 
        {                                                                                                     | 
          v48 = Count[1] - *((_QWORD *)v68 + 1);                                                              | 
        }                                                                                                     | 
        if ( !v48 )                                                                                           | 
        {                                                                                                     | 
          goto LABEL_81;                                                                                      | 
        }                                                                                                     | 
        v39 = -1073741811;                                                                                    | 
      }                                                                                                       | 
      else                                                                                                    | 
      {                                                                                                       | 
        v39 = -1073741811;                                                                                    | 
      }                                                                                                       | 
    }                                                                                                         | 
LABEL_82:                                                                                                     | 
    NameInstance = v39;                                                                                       | 
    if ( v39 < 0 )                                                                                            | 
    {                                                                                                         | 
      goto LABEL_83;                                                                                          | 
    }                                                                                                         | 
    goto LABEL_98;                                                                                            | 
  }                                                                                                           | 
LABEL_83:                                                                                                     | 
  if ( v57 )                                                                                                  | 
  {                                                                                                           | 
    v40 = v57 + 1;                                                                                            | 
    _m_prefetchw(&v57[1]);                                                                                    | 
    v41 = v40->Count & 0xFFFFFFFFFFFFFFFEuLL;                                                                 | 
    if ( v41 != _InterlockedCompareExchange64((volatile signed __int64 *)v40, v41 - 2, v41) )                 | 
    {                                                                                                         | 
      ExfReleaseRundownProtection(v40);                                                                       | 
    }                                                                                                         | 
  }                                                                                                           | 
  if ( *(_QWORD *)v58 )                                                                                       | 
  {                                                                                                           | 
    v42 = (struct _EX_RUNDOWN_REF *)(*(_QWORD *)v58 + 8LL);                                                   | 
    _m_prefetchw((const void *)(*(_QWORD *)v58 + 8LL));                                                       | 
    v43 = v42->Count & 0xFFFFFFFFFFFFFFFEuLL;                                                                 | 
    if ( v43 != _InterlockedCompareExchange64((volatile signed __int64 *)v42, v43 - 2, v43) )                 | 
    {                                                                                                         | 
      ExfReleaseRundownProtection(v42);                                                                       | 
    }                                                                                                         | 
  }                                                                                                           | 
  if ( P )                                                                                                    | 
  {                                                                                                           | 
    ExFreePoolWithTag(P, 0x20666E57u);                                                                        | 
  }                                                                                                           | 
  KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                          | 
  if ( v54 )                                                                                                  | 
  {                                                                                                           | 
    switch ( v54 )                                                                                            | 
    {                                                                                                         | 
      case 1:                                                                                                 | 
        v46 = Object[0];                                                                                      | 
        if ( !Object[0] )                                                                                     | 
        {                                                                                                     | 
          return (unsigned int)NameInstance;                                                                  | 
        }                                                                                                     | 
        v47 = 2035381072;                                                                                     | 
        break;                                                                                                | 
      case 2:                                                                                                 | 
        if ( Object[0] && PreviousMode == 1 )                                                                 | 
        {                                                                                                     | 
          ExFreePoolWithTag(Object[0], 0);                                                                    | 
        }                                                                                                     | 
        return (unsigned int)NameInstance;                                                                    | 
      case 3:                                                                                                 | 
        v46 = Object[0];                                                                                      | 
        if ( !Object[0] )                                                                                     | 
        {                                                                                                     | 
          return (unsigned int)NameInstance;                                                                  | 
        }                                                                                                     | 
        v47 = 543583831;                                                                                      | 
        break;                                                                                                | 
      default:                                                                                                | 
        return (unsigned int)NameInstance;                                                                    | 
    }                                                                                                         | 
    ObfDereferenceObjectWithTag(v46, v47);                                                                    | 
  }                                                                                                           | 
  return (unsigned int)NameInstance;                                                                          | 
}                                                                                                             | 
426 row(s)
