__int64 __fastcall NtCreateUserProcess(                                                                                  | 
        HANDLE *a1,                                                                                                      | 
        __int64 a2,                                                                                                      | 
        int a3,                                                                                                          | 
        int a4,                                                                                                          | 
        __int64 a5,                                                                                                      | 
        __int64 a6,                                                                                                      | 
        unsigned int a7,                                                                                                 | 
        unsigned int a8,                                                                                                 | 
        __int64 a9,                                                                                                      | 
        __int64 a10,                                                                                                     | 
        __int64 a11)                                                                                                     | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  v86 = a4;                                                                                                              | 
  v87 = a3;                                                                                                              | 
  v96 = a2;                                                                                                              | 
  v97 = a1;                                                                                                              | 
  v94 = a5;                                                                                                              | 
  v99[1] = a6;                                                                                                           | 
  v95 = a9;                                                                                                              | 
  v82 = 0;                                                                                                               | 
  memset(v80, 0, sizeof(v80));                                                                                           | 
  *(_DWORD *)(&DriverContext.Size + 1) = 0;                                                                              | 
  *(&DriverContext.Size + 3) = 0;                                                                                        | 
  IoStatusBlock = 0LL;                                                                                                   | 
  v93 = 0LL;                                                                                                             | 
  memset(&ObjectAttributes, 0, 44);                                                                                      | 
  v85 = 0LL;                                                                                                             | 
  memset_0(v138, 0, 0x190uLL);                                                                                           | 
  v103 = 0LL;                                                                                                            | 
  v79[0] = 0;                                                                                                            | 
  v74 = 0;                                                                                                               | 
  v78 = 0;                                                                                                               | 
  *(_OWORD *)v145 = 0LL;                                                                                                 | 
  v146 = 0LL;                                                                                                            | 
  v147 = 0;                                                                                                              | 
  memset_0(v142, 0, 0x190uLL);                                                                                           | 
  v99[0] = 0LL;                                                                                                          | 
  *(_DWORD *)v83 = 0;                                                                                                    | 
  v92 = 0LL;                                                                                                             | 
  *(_OWORD *)v105 = 0LL;                                                                                                 | 
  v106 = 0LL;                                                                                                            | 
  ImpersonationState = 0LL;                                                                                              | 
  CurrentThread = KeGetCurrentThread();                                                                                  | 
  v90 = CurrentThread;                                                                                                   | 
  Process = (__int64)CurrentThread->ApcState.Process;                                                                    | 
  v88 = Process;                                                                                                         | 
  v99[2] = Process;                                                                                                      | 
  PreviousMode = CurrentThread->PreviousMode;                                                                            | 
  v81 = PreviousMode;                                                                                                    | 
  v91 = 0LL;                                                                                                             | 
  v84 = 0LL;                                                                                                             | 
  LOBYTE(CurrentThread) = 0;                                                                                             | 
  v73 = (int)CurrentThread;                                                                                              | 
  v77 = 0;                                                                                                               | 
  v76 = 0;                                                                                                               | 
  memset(&DriverContext, 0, sizeof(DriverContext));                                                                      | 
  v102 = 0LL;                                                                                                            | 
  memset_0(v107, 0, 0x48uLL);                                                                                            | 
  v14 = a7;                                                                                                              | 
  if ( (a7 & 0xFF317838) != 0 || (a8 & 0xFFFFFFFE) != 0 )                                                                | 
  {                                                                                                                      | 
    return 3221225485LL;                                                                                                 | 
  }                                                                                                                      | 
  if ( (a7 & 0x8400) == 0x8400 )                                                                                         | 
  {                                                                                                                      | 
    return 3221225520LL;                                                                                                 | 
  }                                                                                                                      | 
  v140 = PreviousMode;                                                                                                   | 
  if ( PreviousMode )                                                                                                    | 
  {                                                                                                                      | 
    v16 = 0x7FFFFFFF0000LL;                                                                                              | 
    if ( (unsigned __int64)v97 < 0x7FFFFFFF0000LL )                                                                      | 
    {                                                                                                                    | 
      v16 = (__int64)v97;                                                                                                | 
    }                                                                                                                    | 
    *(_QWORD *)v16 = *(_QWORD *)v16;                                                                                     | 
    v17 = 0x7FFFFFFF0000LL;                                                                                              | 
    if ( (unsigned __int64)v96 < 0x7FFFFFFF0000LL )                                                                      | 
    {                                                                                                                    | 
      v17 = v96;                                                                                                         | 
    }                                                                                                                    | 
    *(_QWORD *)v17 = *(_QWORD *)v17;                                                                                     | 
  }                                                                                                                      | 
  if ( a5 )                                                                                                              | 
  {                                                                                                                      | 
    if ( PreviousMode && (a5 & 3) != 0 )                                                                                 | 
    {                                                                                                                    | 
      ExRaiseDatatypeMisalignment();                                                                                     | 
    }                                                                                                                    | 
    if ( PreviousMode )                                                                                                  | 
    {                                                                                                                    | 
      v18 = *(_DWORD *)(a5 + 24) & 0x1DF2;                                                                               | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      v18 = *(_DWORD *)(a5 + 24) & 0x11FF2;                                                                              | 
    }                                                                                                                    | 
    v139 = v18;                                                                                                          | 
  }                                                                                                                      | 
  memset_0(&v108, 0, 0x210uLL);                                                                                          | 
  if ( a11 )                                                                                                             | 
  {                                                                                                                      | 
    LOBYTE(v19) = PreviousMode;                                                                                          | 
    result = PspBuildCreateProcessContext(a11, v19, 0LL, &v108);                                                         | 
    if ( (int)result < 0 )                                                                                               | 
    {                                                                                                                    | 
      return result;                                                                                                     | 
    }                                                                                                                    | 
    v76 = v131 != 0;                                                                                                     | 
  }                                                                                                                      | 
  if ( (a7 & 0x40) != 0 && (v109 & 0x20000) != 0 && !BYTE1(v108) )                                                       | 
  {                                                                                                                      | 
    v14 = a7 & 0xFFFFFFBF;                                                                                               | 
    a7 &= ~0x40u;                                                                                                        | 
    v109 &= ~0x20000uLL;                                                                                                 | 
  }                                                                                                                      | 
  if ( (v14 & 4) == 0 && (v109 & 0x800) != 0 || (v109 & 0x20000) != 0 && (v14 & 0x40) == 0 )                             | 
  {                                                                                                                      | 
    goto LABEL_179;                                                                                                      | 
  }                                                                                                                      | 
  LOBYTE(v20) = PreviousMode;                                                                                            | 
  ProcessProtection = PspCaptureCreateInfo(v20, a10, &v108);                                                             | 
  if ( ProcessProtection < 0 )                                                                                           | 
  {                                                                                                                      | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  v22 = 1;                                                                                                               | 
  if ( (v109 & 1) != 0 )                                                                                                 | 
  {                                                                                                                      | 
    ProcessProtection = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x72437350u, (__int64)&v85, 0LL, 0LL);     | 
    if ( ProcessProtection >= 0 )                                                                                        | 
    {                                                                                                                    | 
      Process = v85;                                                                                                     | 
      v119 = v85;                                                                                                        | 
      goto LABEL_34;                                                                                                     | 
    }                                                                                                                    | 
LABEL_180:                                                                                                               | 
    v33 = v73;                                                                                                           | 
    goto LABEL_181;                                                                                                      | 
  }                                                                                                                      | 
  v85 = Process;                                                                                                         | 
LABEL_34:                                                                                                                | 
  v137 = PspEstimateNewProcessServerSilo(Process, v132, v133);                                                           | 
  if ( (v111 & 0xC) == 4 && (Process != v88 || (*(_DWORD *)(v88 + 1532) & 0x1000) != 0) )                                | 
  {                                                                                                                      | 
    goto LABEL_179;                                                                                                      | 
  }                                                                                                                      | 
  LOBYTE(v23) = PreviousMode;                                                                                            | 
  ProcessProtection = PspReferenceTokenForNewProcess(Process, v121, v23, &v122);                                         | 
  if ( ProcessProtection < 0 )                                                                                           | 
  {                                                                                                                      | 
    v122 = 0LL;                                                                                                          | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  if ( v121 && (int)SeQueryServerSiloToken(v122, &v92) >= 0 && !(unsigned __int8)PspIsSiloInSilo(v92, v137) )            | 
  {                                                                                                                      | 
    goto LABEL_179;                                                                                                      | 
  }                                                                                                                      | 
  if ( (v109 & 0x20) == 0 )                                                                                              | 
  {                                                                                                                      | 
    if ( Process == v88                                                                                                  | 
      && PreviousMode                                                                                                    | 
      && !v95                                                                                                            | 
      && (v111 & 0xC) == 0                                                                                               | 
      && (v109 & 0x80u) == 0LL                                                                                           | 
      && (v109 & 0x20000) == 0                                                                                           | 
      && (v110 & 0x10) == 0 )                                                                                            | 
    {                                                                                                                    | 
      v110 &= ~4u;                                                                                                       | 
      v43 = *(_BYTE *)(Process + 1530);                                                                                  | 
      HIBYTE(v74) = *(_BYTE *)(v85 + 1529);                                                                              | 
      v30 = *(_BYTE *)(v85 + 1528);                                                                                      | 
      if ( (v14 & 0x40) != 0 && (v43 & 7) == 0 )                                                                         | 
      {                                                                                                                  | 
        ProcessProtection = -1073741790;                                                                                 | 
        goto LABEL_180;                                                                                                  | 
      }                                                                                                                  | 
      LOBYTE(v24) = PreviousMode;                                                                                        | 
      ProcessProtection = PspValidateCreateProcessProtection(v88, (unsigned int)&v108, v24, v14, v43);                   | 
      if ( ProcessProtection < 0 )                                                                                       | 
      {                                                                                                                  | 
        goto LABEL_180;                                                                                                  | 
      }                                                                                                                  | 
      v56 = 0LL;                                                                                                         | 
      goto LABEL_122;                                                                                                    | 
    }                                                                                                                    | 
LABEL_179:                                                                                                               | 
    ProcessProtection = -1073741811;                                                                                     | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  v26 = (v109 & 0x20000) != 0 ? BYTE1(v108) : 0;                                                                         | 
  v75 = v26;                                                                                                             | 
  if ( (NtGlobalFlag & 0x40000) != 0 )                                                                                   | 
  {                                                                                                                      | 
    v103 = v130[0];                                                                                                      | 
    ZwSystemDebugControl(38LL, &v103, 16LL);                                                                             | 
  }                                                                                                                      | 
  if ( (v14 & 0x400000) != 0 && !SeSinglePrivilegeCheck(SeLockMemoryPrivilege, PreviousMode) )                           | 
  {                                                                                                                      | 
    goto LABEL_47;                                                                                                       | 
  }                                                                                                                      | 
  LOBYTE(v25) = v26;                                                                                                     | 
  ProcessProtection = SeQuerySigningPolicy(v122, v130, (v14 >> 6) & 1, v25, &v74, &v78, &v75);                           | 
  if ( ProcessProtection < 0 )                                                                                           | 
  {                                                                                                                      | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  if ( (v14 & 0x20000) != 0 )                                                                                            | 
  {                                                                                                                      | 
    LOBYTE(v27) = 8;                                                                                                     | 
    LOBYTE(v28) = v74;                                                                                                   | 
    v29 = SeCompareSigningLevels(v28, v27);                                                                              | 
    v30 = v74;                                                                                                           | 
    if ( !v29 )                                                                                                          | 
    {                                                                                                                    | 
      v30 = 8;                                                                                                           | 
    }                                                                                                                    | 
    LOBYTE(v74) = v30;                                                                                                   | 
  }                                                                                                                      | 
  else                                                                                                                   | 
  {                                                                                                                      | 
    v30 = v74;                                                                                                           | 
  }                                                                                                                      | 
  ObjectAttributes.Length = 48;                                                                                          | 
  ObjectAttributes.RootDirectory = 0LL;                                                                                  | 
  v31 = 1600;                                                                                                            | 
  if ( PreviousMode != 1 )                                                                                               | 
  {                                                                                                                      | 
    v31 = 576;                                                                                                           | 
  }                                                                                                                      | 
  ObjectAttributes.Attributes = v31;                                                                                     | 
  ObjectAttributes.ObjectName = (PUNICODE_STRING)v130;                                                                   | 
  *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;                                                                 | 
  ProcessProtection = 0;                                                                                                 | 
  if ( qword_140FD73D0 )                                                                                                 | 
  {                                                                                                                      | 
    ProcessProtection = guard_dispatch_icall_no_overrides(v122);                                                         | 
  }                                                                                                                      | 
  if ( ProcessProtection < 0 )                                                                                           | 
  {                                                                                                                      | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  ProcessProtection = PspGetMemoryPartitionContext(v85, v14, v132, v133, (__int64)&v91);                                 | 
  if ( ProcessProtection < 0 )                                                                                           | 
  {                                                                                                                      | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  ProcessProtection = PspCreateUserProcessEcp(&DriverContext, v122);                                                     | 
  if ( ProcessProtection < 0 )                                                                                           | 
  {                                                                                                                      | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  ProcessProtection = IoCreateFileEx(                                                                                    | 
                        &FileHandle,                                                                                     | 
                        v123 | 0x100020,                                                                                 | 
                        &ObjectAttributes,                                                                               | 
                        &IoStatusBlock,                                                                                  | 
                        0LL,                                                                                             | 
                        0x80u,                                                                                           | 
                        5u,                                                                                              | 
                        1u,                                                                                              | 
                        0x60u,                                                                                           | 
                        0LL,                                                                                             | 
                        0,                                                                                               | 
                        CreateFileTypeNone,                                                                              | 
                        0LL,                                                                                             | 
                        0,                                                                                               | 
                        &DriverContext);                                                                                 | 
  if ( ProcessProtection < 0 && v123 )                                                                                   | 
  {                                                                                                                      | 
    ProcessProtection = IoCreateFileEx(                                                                                  | 
                          &FileHandle,                                                                                   | 
                          0x100020u,                                                                                     | 
                          &ObjectAttributes,                                                                             | 
                          &IoStatusBlock,                                                                                | 
                          0LL,                                                                                           | 
                          0x80u,                                                                                         | 
                          5u,                                                                                            | 
                          1u,                                                                                            | 
                          0x60u,                                                                                         | 
                          0LL,                                                                                           | 
                          0,                                                                                             | 
                          CreateFileTypeNone,                                                                            | 
                          0LL,                                                                                           | 
                          0,                                                                                             | 
                          &DriverContext);                                                                               | 
  }                                                                                                                      | 
  if ( ProcessProtection < 0 )                                                                                           | 
  {                                                                                                                      | 
    FileHandle = 0LL;                                                                                                    | 
    v32 = 1LL;                                                                                                           | 
LABEL_66:                                                                                                                | 
    PspUpdateCreateInfo(v32, &v108, 0LL);                                                                                | 
    goto LABEL_180;                                                                                                      | 
  }                                                                                                                      | 
  Object = 0LL;                                                                                                          | 
  ProcessProtection = ObReferenceObjectByHandle(FileHandle, 0x100020u, (POBJECT_TYPE)IoFileObjectType, 0, &Object, 0LL); | 
  v125 = Object;                                                                                                         | 
  if ( ProcessProtection >= 0 )                                                                                          | 
  {                                                                                                                      | 
    ObjectAttributes.ObjectName = 0LL;                                                                                   | 
    v34 = 4 * v76 + 1;                                                                                                   | 
    LODWORD(v92) = v34;                                                                                                  | 
    if ( (unsigned int)Feature_TrustedLaunchHosts__private_IsEnabledDeviceUsageNoInline() )                              | 
    {                                                                                                                    | 
      v36 = SeDuplicateTokenWithPredictedClaims((_DWORD)v122, (_DWORD)v125, v134, v136, v135, (__int64)&v84);            | 
      ProcessProtection = v36;                                                                                           | 
      if ( v36 < 0 )                                                                                                     | 
      {                                                                                                                  | 
        goto LABEL_180;                                                                                                  | 
      }                                                                                                                  | 
      if ( v84 )                                                                                                         | 
      {                                                                                                                  | 
        LOBYTE(v37) = 1;                                                                                                 | 
        v73 = v37;                                                                                                       | 
        v77 = 1;                                                                                                         | 
      }                                                                                                                  | 
      else                                                                                                               | 
      {                                                                                                                  | 
        LOBYTE(v36) = 0;                                                                                                 | 
        v73 = v36;                                                                                                       | 
        v77 = 0;                                                                                                         | 
        v84 = v122;                                                                                                      | 
      }                                                                                                                  | 
      v38 = v92;                                                                                                         | 
      if ( (v134 & 8) != 0 )                                                                                             | 
      {                                                                                                                  | 
        v39 = PsDisableImpersonation(KeGetCurrentThread(), &ImpersonationState);                                         | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
        {                                                                                                                | 
          if ( v39 )                                                                                                     | 
          {                                                                                                              | 
            PsRestoreImpersonation(KeGetCurrentThread(), &ImpersonationState);                                           | 
          }                                                                                                              | 
LABEL_47:                                                                                                                | 
          ProcessProtection = -1073741727;                                                                               | 
          goto LABEL_180;                                                                                                | 
        }                                                                                                                | 
        if ( v39 )                                                                                                       | 
        {                                                                                                                | 
          PsRestoreImpersonation(KeGetCurrentThread(), &ImpersonationState);                                             | 
        }                                                                                                                | 
        v38 |= 8u;                                                                                                       | 
        v30 = v74;                                                                                                       | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      if ( v136 )                                                                                                        | 
      {                                                                                                                  | 
        v40 = SeDuplicateTokenAndAddOriginClaim(v122, v136, v135, &v84);                                                 | 
        v41 = (unsigned __int8)v73;                                                                                      | 
        if ( v40 >= 0 )                                                                                                  | 
        {                                                                                                                | 
          v41 = 1;                                                                                                       | 
        }                                                                                                                | 
        v73 = v41;                                                                                                       | 
        v77 = v41;                                                                                                       | 
      }                                                                                                                  | 
      else                                                                                                               | 
      {                                                                                                                  | 
        LOBYTE(v41) = v73;                                                                                               | 
      }                                                                                                                  | 
      v38 = v34;                                                                                                         | 
      if ( !(_BYTE)v41 )                                                                                                 | 
      {                                                                                                                  | 
        v84 = v122;                                                                                                      | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    LOBYTE(v35) = v30;                                                                                                   | 
    ProcessProtection = MmCreateSpecialImageSection(                                                                     | 
                          (unsigned int)Handle,                                                                          | 
                          (unsigned int)&ObjectAttributes,                                                               | 
                          (_DWORD)v84,                                                                                   | 
                          v35,                                                                                           | 
                          (__int64)FileHandle,                                                                           | 
                          v38);                                                                                          | 
    if ( ProcessProtection < 0 )                                                                                         | 
    {                                                                                                                    | 
LABEL_109:                                                                                                               | 
      Handle[0] = 0LL;                                                                                                   | 
      v32 = 2LL;                                                                                                         | 
      goto LABEL_66;                                                                                                     | 
    }                                                                                                                    | 
    while ( 1 )                                                                                                          | 
    {                                                                                                                    | 
      Object = 0LL;                                                                                                      | 
      ProcessProtection = ObReferenceObjectByHandle(Handle[0], 8u, MmSectionObjectType, 0, &Object, 0LL);                | 
      v127 = Object;                                                                                                     | 
      if ( ProcessProtection < 0 )                                                                                       | 
      {                                                                                                                  | 
        v127 = 0LL;                                                                                                      | 
        goto LABEL_180;                                                                                                  | 
      }                                                                                                                  | 
      v43 = v75;                                                                                                         | 
      LOBYTE(v42) = v75;                                                                                                 | 
      ProcessProtection = PspGetProcessProtectionRequirementsFromImage(Object, v42, v79);                                | 
      if ( ProcessProtection < 0 )                                                                                       | 
      {                                                                                                                  | 
        goto LABEL_180;                                                                                                  | 
      }                                                                                                                  | 
      LOBYTE(v45) = v79[0];                                                                                              | 
      if ( v79[0] == v43 )                                                                                               | 
      {                                                                                                                  | 
        break;                                                                                                           | 
      }                                                                                                                  | 
      ProcessProtection = SeQuerySigningPolicy(v122, v130, 1LL, v45, (char *)&v74 + 1, &v78, &v75);                      | 
      if ( ProcessProtection < 0 )                                                                                       | 
      {                                                                                                                  | 
        goto LABEL_180;                                                                                                  | 
      }                                                                                                                  | 
      v48 = HIBYTE(v74);                                                                                                 | 
      if ( HIBYTE(v74) == v30 )                                                                                          | 
      {                                                                                                                  | 
        v43 = v75;                                                                                                       | 
        break;                                                                                                           | 
      }                                                                                                                  | 
      if ( (v30 & 0x30) != 0 && (HIBYTE(v74) & 0x30) != (v30 & 0x30) )                                                   | 
      {                                                                                                                  | 
        goto LABEL_179;                                                                                                  | 
      }                                                                                                                  | 
      LOBYTE(v46) = v30;                                                                                                 | 
      LOBYTE(v47) = HIBYTE(v74);                                                                                         | 
      if ( !(unsigned int)SeCompareSigningLevels(v47, v46) )                                                             | 
      {                                                                                                                  | 
        goto LABEL_179;                                                                                                  | 
      }                                                                                                                  | 
      ObCloseHandle(Handle[0], 0);                                                                                       | 
      ObfDereferenceObject(v127);                                                                                        | 
      Handle[0] = 0LL;                                                                                                   | 
      v127 = 0LL;                                                                                                        | 
      v30 = v48;                                                                                                         | 
      LOBYTE(v49) = v48;                                                                                                 | 
      ProcessProtection = MmCreateSpecialImageSection(                                                                   | 
                            (unsigned int)Handle,                                                                        | 
                            (unsigned int)&ObjectAttributes,                                                             | 
                            (_DWORD)v84,                                                                                 | 
                            v49,                                                                                         | 
                            (__int64)FileHandle,                                                                         | 
                            v38);                                                                                        | 
      if ( ProcessProtection < 0 )                                                                                       | 
      {                                                                                                                  | 
        goto LABEL_109;                                                                                                  | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    LOBYTE(v44) = PreviousMode;                                                                                          | 
    ProcessProtection = PspValidateCreateProcessProtection(v88, (unsigned int)&v108, v44, a7, v43);                      | 
    if ( ProcessProtection < 0 )                                                                                         | 
    {                                                                                                                    | 
      goto LABEL_180;                                                                                                    | 
    }                                                                                                                    | 
    if ( (a7 & 0x40000) != 0                                                                                             | 
      || (v43 & 7) != 1                                                                                                  | 
      && (LOBYTE(v52) = v43,                                                                                             | 
          LOBYTE(v51) = PreviousMode,                                                                                    | 
          LOBYTE(v50) = *(_BYTE *)(v88 + 1530),                                                                          | 
          (unsigned __int8)PspCheckForInvalidAccessByProtection(v51, v50, v52)) )                                        | 
    {                                                                                                                    | 
      v110 |= 8u;                                                                                                        | 
    }                                                                                                                    | 
    PspGetProcessParameterOverrides(&v108, v91);                                                                         | 
    LOBYTE(v53) = PreviousMode;                                                                                          | 
    ProcessProtection = PspCaptureProcessParameters(v53, v95, &v108);                                                    | 
    if ( ProcessProtection < 0 )                                                                                         | 
    {                                                                                                                    | 
      v110 &= ~4u;                                                                                                       | 
      goto LABEL_180;                                                                                                    | 
    }                                                                                                                    | 
    v56 = v145;                                                                                                          | 
    HIBYTE(v74) = v78;                                                                                                   | 
LABEL_122:                                                                                                               | 
    LOBYTE(v55) = v43;                                                                                                   | 
    LOBYTE(v54) = PreviousMode;                                                                                          | 
    ProcessProtection = PspAllocateProcess(                                                                              | 
                          v85,                                                                                           | 
                          v54,                                                                                           | 
                          v94,                                                                                           | 
                          v55,                                                                                           | 
                          v30,                                                                                           | 
                          SHIBYTE(v74),                                                                                  | 
                          (__int64)v127,                                                                                 | 
                          (__int64)v122,                                                                                 | 
                          a7,                                                                                            | 
                          0,                                                                                             | 
                          (__int64)&v108,                                                                                | 
                          v121 != 0,                                                                                     | 
                          v91,                                                                                           | 
                          (__int64)&v82,                                                                                 | 
                          (__int64)&v80[1]);                                                                             | 
    if ( ProcessProtection < 0 )                                                                                         | 
    {                                                                                                                    | 
      goto LABEL_180;                                                                                                    | 
    }                                                                                                                    | 
    v57 = v127 != 0LL ? 1048587 : 1048603;                                                                               | 
    v58 = *(_DWORD *)(*(_QWORD *)&v80[1] + 1876LL) & 0x4000;                                                             | 
    v94 = v58 != 0 ? 0x800 : 0;                                                                                          | 
    if ( v58 )                                                                                                           | 
    {                                                                                                                    | 
      v57 |= 0x100040u;                                                                                                  | 
    }                                                                                                                    | 
    RtlGetExtendedContextLength2(v57, v83, v58 != 0 ? 0x800 : 0);                                                        | 
    v59 = 0LL;                                                                                                           | 
    if ( v76 )                                                                                                           | 
    {                                                                                                                    | 
      v56 = 0LL;                                                                                                         | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      v60 = *(unsigned int *)v83 + 15LL;                                                                                 | 
      if ( v60 <= *(unsigned int *)v83 )                                                                                 | 
      {                                                                                                                  | 
        v60 = 0xFFFFFFFFFFFFFF0LL;                                                                                       | 
      }                                                                                                                  | 
      v61 = alloca(v60 & 0xFFFFFFFFFFFFFFF0uLL);                                                                         | 
      v59 = &v73;                                                                                                        | 
      memset_0(&v73, 0, *(unsigned int *)v83);                                                                           | 
      RtlInitializeExtendedContext2(&v73, v57, v99, v94);                                                                | 
      if ( v127 )                                                                                                        | 
      {                                                                                                                  | 
        v63 = v129;                                                                                                      | 
        if ( !*(_QWORD *)(*(_QWORD *)&v80[1] + 784LL) )                                                                  | 
        {                                                                                                                | 
          v63 = *(_QWORD *)(*(_QWORD *)&v80[1] + 736LL);                                                                 | 
        }                                                                                                                | 
        PspCreateUserContext((unsigned int)&v73, 1, qword_140FC64C0, v114, v63);                                         | 
      }                                                                                                                  | 
      else                                                                                                               | 
      {                                                                                                                  | 
        LOBYTE(v62) = 1;                                                                                                 | 
        ProcessProtection = PspGetContextThreadInternal((_DWORD)v90, (unsigned int)&v73, 0, v62, 1);                     | 
        if ( ProcessProtection < 0 )                                                                                     | 
        {                                                                                                                | 
          PspUnlockProcessExclusive(*(_QWORD *)&v80[1], v90);                                                            | 
          PspRundownSingleProcess(*(PVOID *)&v80[1]);                                                                    | 
          goto LABEL_180;                                                                                                | 
        }                                                                                                                | 
        v95 = 297LL;                                                                                                     | 
      }                                                                                                                  | 
      v105[0] = (__int64)v107;                                                                                           | 
    }                                                                                                                    | 
    ObfReferenceObjectWithTag(*(PVOID *)&v80[1], 0x72437350u);                                                           | 
    if ( v56 )                                                                                                           | 
    {                                                                                                                    | 
      *(_BYTE *)v56 = 0;                                                                                                 | 
      v64 = v115;                                                                                                        | 
      if ( (unsigned __int64)v115 < 0x40000 )                                                                            | 
      {                                                                                                                  | 
        v64 = 0x40000LL;                                                                                                 | 
      }                                                                                                                  | 
      v56[3] = v64;                                                                                                      | 
      v56[2] = *((_QWORD *)&v115 + 1);                                                                                   | 
      v56[1] = DWORD2(v114);                                                                                             | 
    }                                                                                                                    | 
    *(_DWORD *)v83 = 0;                                                                                                  | 
    PspMapThreadCreationFlags(a8, v80);                                                                                  | 
    if ( (_BYTE)v82 )                                                                                                    | 
    {                                                                                                                    | 
      *(_DWORD *)v83 = 2;                                                                                                | 
      v80[0] |= 0x10u;                                                                                                   | 
    }                                                                                                                    | 
    if ( v76 )                                                                                                           | 
    {                                                                                                                    | 
      v80[0] |= 0x400u;                                                                                                  | 
    }                                                                                                                    | 
    v80[0] |= 0x60u;                                                                                                     | 
    ProcessProtection = PspAllocateThread(                                                                               | 
                          *(ULONG_PTR *)&v80[1],                                                                         | 
                          (__int64)v59,                                                                                  | 
                          (__int64)v105,                                                                                 | 
                          0LL,                                                                                           | 
                          0LL,                                                                                           | 
                          (__int64)v80,                                                                                  | 
                          (__int64)&v93,                                                                                 | 
                          (__int64)v56,                                                                                  | 
                          (__int64)v142);                                                                                | 
    if ( ProcessProtection < 0 )                                                                                         | 
    {                                                                                                                    | 
      PspUnlockProcessExclusive(*(_QWORD *)&v80[1], v90);                                                                | 
      PspRundownSingleProcess(*(PVOID *)&v80[1]);                                                                        | 
      v33 = v73;                                                                                                         | 
LABEL_165:                                                                                                               | 
      ObfDereferenceObjectWithTag(*(PVOID *)&v80[1], 0x72437350u);                                                       | 
LABEL_181:                                                                                                               | 
      if ( v125 && qword_140F04A30 )                                                                                     | 
      {                                                                                                                  | 
        guard_dispatch_icall_no_overrides(v125);                                                                         | 
      }                                                                                                                  | 
      goto LABEL_184;                                                                                                    | 
    }                                                                                                                    | 
    v66 = v88;                                                                                                           | 
    LOBYTE(v65) = PreviousMode;                                                                                          | 
    if ( (unsigned __int8)PsTestProtectedProcessIncompatibility(v65, v88, *(_QWORD *)&v80[1]) )                          | 
    {                                                                                                                    | 
      Feature_ID51912085__private_IsEnabledPreCheck();                                                                   | 
      v67 = v86;                                                                                                         | 
      if ( (v86 & 0x2000000) != 0 )                                                                                      | 
      {                                                                                                                  | 
        v67 = ~*(_DWORD *)&RtlProtectedAccess2[12 * ((unsigned __int64)v43 >> 4) + 8] & 0x1FFFFF | v86 & 0xFDFFFFFF;     | 
        if ( !v119 || v66 == v119 )                                                                                      | 
        {                                                                                                                | 
          v67 |= 1u;                                                                                                     | 
        }                                                                                                                | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      v67 = v86;                                                                                                         | 
    }                                                                                                                    | 
    if ( v128 )                                                                                                          | 
    {                                                                                                                    | 
      v68 = v128 + 112;                                                                                                  | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      v68 = 0LL;                                                                                                         | 
    }                                                                                                                    | 
    inserted = PspInsertProcess(*(PVOID *)&v80[1], v120, v83[0], v68, (__int64)v138);                                    | 
    ProcessProtection = PspInsertThread(                                                                                 | 
                          v93,                                                                                           | 
                          *(ULONG_PTR *)&v80[1],                                                                         | 
                          v67,                                                                                           | 
                          (__int64)v56,                                                                                  | 
                          (__int64)&v108,                                                                                | 
                          0LL,                                                                                           | 
                          (__int64)v142,                                                                                 | 
                          v96,                                                                                           | 
                          v112);                                                                                         | 
    KiLeaveCriticalRegionUnsafe(v90);                                                                                    | 
    if ( inserted < 0 )                                                                                                  | 
    {                                                                                                                    | 
      PspRundownSingleProcess(*(PVOID *)&v80[1]);                                                                        | 
      ProcessProtection = inserted;                                                                                      | 
      v33 = v73;                                                                                                         | 
LABEL_164:                                                                                                               | 
      ObfDereferenceObject(v93);                                                                                         | 
      goto LABEL_165;                                                                                                    | 
    }                                                                                                                    | 
    if ( ProcessProtection >= 0 )                                                                                        | 
    {                                                                                                                    | 
      ProcessServerSilo = PsGetProcessServerSilo(*(_QWORD *)&v80[1]);                                                    | 
      if ( ProcessServerSilo == v137 )                                                                                   | 
      {                                                                                                                  | 
        ProcessProtection = PspCreateObjectHandle(*(_QWORD *)&v80[1], v138, PsProcessType);                              | 
        if ( ProcessProtection < 0 )                                                                                     | 
        {                                                                                                                | 
          v33 = v73;                                                                                                     | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v71 = v113;                                                                                                    | 
          if ( v113 )                                                                                                    | 
          {                                                                                                              | 
            *v113 = v114;                                                                                                | 
            v71[1] = v115;                                                                                               | 
            v71[2] = v116;                                                                                               | 
            v71[3] = v117;                                                                                               | 
          }                                                                                                              | 
          *v97 = v141;                                                                                                   | 
          v33 = v73;                                                                                                     | 
          ProcessProtection = PspUpdateCreateInfo(6LL, &v108, *(_QWORD *)&v80[1]);                                       | 
          if ( ProcessProtection >= 0 )                                                                                  | 
          {                                                                                                              | 
            goto LABEL_162;                                                                                              | 
          }                                                                                                              | 
          if ( (v139 & 0x200) != 0 || (v72 = 1, (*(_DWORD *)(v66 + 1532) & 0x1000) != 0) )                               | 
          {                                                                                                              | 
            v72 = 0;                                                                                                     | 
          }                                                                                                              | 
          ObCloseHandle(v141, v72);                                                                                      | 
        }                                                                                                                | 
        if ( (v143 & 0x200) != 0 || (*(_DWORD *)(v66 + 1532) & 0x1000) != 0 )                                            | 
        {                                                                                                                | 
          v22 = 0;                                                                                                       | 
        }                                                                                                                | 
        ObCloseHandle(v144, v22);                                                                                        | 
LABEL_162:                                                                                                               | 
        SeDeleteAccessState(v138);                                                                                       | 
        if ( ProcessProtection < 0 )                                                                                     | 
        {                                                                                                                | 
          PsTerminateProcess(*(_QWORD *)&v80[1], (unsigned int)ProcessProtection);                                       | 
        }                                                                                                                | 
        goto LABEL_164;                                                                                                  | 
      }                                                                                                                  | 
      ProcessProtection = -1073741267;                                                                                   | 
    }                                                                                                                    | 
    v33 = v73;                                                                                                           | 
    goto LABEL_162;                                                                                                      | 
  }                                                                                                                      | 
  v125 = 0LL;                                                                                                            | 
  v33 = v73;                                                                                                             | 
LABEL_184:                                                                                                               | 
  PspDeleteMemoryPartitionContext(v91);                                                                                  | 
  PspDeleteCreateProcessContext(&v108);                                                                                  | 
  if ( DriverContext.ExtraCreateParameter )                                                                              | 
  {                                                                                                                      | 
    FsRtlFreeExtraCreateParameterList(DriverContext.ExtraCreateParameter);                                               | 
  }                                                                                                                      | 
  if ( v33 )                                                                                                             | 
  {                                                                                                                      | 
    ObfDereferenceObject(v84);                                                                                           | 
  }                                                                                                                      | 
  return (unsigned int)ProcessProtection;                                                                                | 
}                                                                                                                        | 
706 row(s)
