__int64 __fastcall NtSetUuidSeed(__int64 a1)                                                                    | 
{                                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                 | 
                                                                                                                | 
  AccessStatus = 0;                                                                                             | 
  memset(&SubjectSecurityContext, 0, sizeof(SubjectSecurityContext));                                           | 
  v34[0] = 80;                                                                                                  | 
  v34[1] = 521322694;                                                                                           | 
  v34[2] = 906040134;                                                                                           | 
  v34[3] = -430256637;                                                                                          | 
  v34[4] = 1525148216;                                                                                          | 
  v34[5] = -843743134;                                                                                          | 
  *(_DWORD *)IdentifierAuthority.Value = 0;                                                                     | 
  *(_WORD *)&IdentifierAuthority.Value[4] = 1280;                                                               | 
  GrantedAccess = 0;                                                                                            | 
  memset(SecurityDescriptor, 0, sizeof(SecurityDescriptor));                                                    | 
  v33 = 0LL;                                                                                                    | 
  v27 = 0LL;                                                                                                    | 
  SeCaptureSubjectContext(&SubjectSecurityContext);                                                             | 
  v28 = RtlLengthRequiredSid(6u);                                                                               | 
  Pool2 = (void *)ExAllocatePool2(0x100uLL, v28, 0x64695555u);                                                  | 
  v3 = Pool2;                                                                                                   | 
  P = Pool2;                                                                                                    | 
  if ( !Pool2 )                                                                                                 | 
  {                                                                                                             | 
    RtlRaiseStatus(3221225626LL);                                                                               | 
  }                                                                                                             | 
  v4 = RtlInitializeSid(Pool2, &IdentifierAuthority, 6u);                                                       | 
  AccessStatus = v4;                                                                                            | 
  if ( v4 < 0 )                                                                                                 | 
  {                                                                                                             | 
    RtlRaiseStatus((unsigned int)v4);                                                                           | 
  }                                                                                                             | 
  v5 = 0;                                                                                                       | 
  v24 = 0;                                                                                                      | 
  while ( v5 < 6 )                                                                                              | 
  {                                                                                                             | 
    v6 = v34[v5];                                                                                               | 
    *RtlSubAuthoritySid(v3, v5++) = v6;                                                                         | 
    v24 = v5;                                                                                                   | 
  }                                                                                                             | 
  v7 = RtlLengthSid(v3) + 20;                                                                                   | 
  v29 = v7;                                                                                                     | 
  v8 = (ACL *)ExAllocatePool2(0x100uLL, v7, 0x64695555u);                                                       | 
  v9 = v8;                                                                                                      | 
  v27 = v8;                                                                                                     | 
  if ( !v8 )                                                                                                    | 
  {                                                                                                             | 
    RtlRaiseStatus(3221225626LL);                                                                               | 
  }                                                                                                             | 
  Acl = RtlCreateAcl(v8, v7, 2u);                                                                               | 
  AccessStatus = Acl;                                                                                           | 
  if ( Acl < 0 )                                                                                                | 
  {                                                                                                             | 
    RtlRaiseStatus((unsigned int)Acl);                                                                          | 
  }                                                                                                             | 
  v11 = RtlpAddKnownAce((int)v9, 2, 0, 1, v3, 0);                                                               | 
  v30 = v11;                                                                                                    | 
  AccessStatus = v11;                                                                                           | 
  if ( v11 < 0 )                                                                                                | 
  {                                                                                                             | 
    RtlRaiseStatus((unsigned int)v11);                                                                          | 
  }                                                                                                             | 
  v12 = RtlCreateSecurityDescriptor(SecurityDescriptor, 1u);                                                    | 
  AccessStatus = v12;                                                                                           | 
  if ( v12 < 0 )                                                                                                | 
  {                                                                                                             | 
    RtlRaiseStatus((unsigned int)v12);                                                                          | 
  }                                                                                                             | 
  v13 = RtlSetDaclSecurityDescriptor(SecurityDescriptor, 1u, v9, 0);                                            | 
  AccessStatus = v13;                                                                                           | 
  if ( v13 < 0 )                                                                                                | 
  {                                                                                                             | 
    RtlRaiseStatus((unsigned int)v13);                                                                          | 
  }                                                                                                             | 
  if ( !SeAccessCheck(                                                                                          | 
          SecurityDescriptor,                                                                                   | 
          &SubjectSecurityContext,                                                                              | 
          0,                                                                                                    | 
          1u,                                                                                                   | 
          0,                                                                                                    | 
          0LL,                                                                                                  | 
          (PGENERIC_MAPPING)&ExpUuidSeedGenericMapping,                                                         | 
          1,                                                                                                    | 
          &GrantedAccess,                                                                                       | 
          &AccessStatus) )                                                                                      | 
  {                                                                                                             | 
    RtlRaiseStatus(3221225506LL);                                                                               | 
  }                                                                                                             | 
  v22 = *(_DWORD *)a1;                                                                                          | 
  v23 = *(_WORD *)(a1 + 4);                                                                                     | 
  AccessStatus = 0;                                                                                             | 
  if ( P )                                                                                                      | 
  {                                                                                                             | 
    ExFreePoolWithTag(P, 0);                                                                                    | 
  }                                                                                                             | 
  if ( v27 )                                                                                                    | 
  {                                                                                                             | 
    ExFreePoolWithTag(v27, 0);                                                                                  | 
  }                                                                                                             | 
  if ( !AccessStatus )                                                                                          | 
  {                                                                                                             | 
    CurrentThread = KeGetCurrentThread();                                                                       | 
    --CurrentThread->KernelApcDisable;                                                                          | 
    v15 = KeAbPreAcquire(&ExpUuidLock, 0LL, 0LL);                                                               | 
    v16 = v15;                                                                                                  | 
    if ( _interlockedbittestandset64((volatile signed __int32 *)&ExpUuidLock, 0LL) )                            | 
    {                                                                                                           | 
      ExfAcquirePushLockExclusiveEx(&ExpUuidLock, v15, &ExpUuidLock);                                           | 
    }                                                                                                           | 
    if ( v16 )                                                                                                  | 
    {                                                                                                           | 
      *(_BYTE *)(v16 + 10) = 1;                                                                                 | 
    }                                                                                                           | 
    *(int *)((char *)&dword_140FD7254 + 2) = v22;                                                               | 
    word_140FD725A = v23;                                                                                       | 
    HIBYTE(NlsMbOemCodePageTag) = (unsigned __int8)v22 >> 7 == 0;                                               | 
    if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)&ExpUuidLock, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 ) | 
    {                                                                                                           | 
      ExfTryToWakePushLock(&ExpUuidLock);                                                                       | 
    }                                                                                                           | 
    KeAbPostRelease((ULONG_PTR)&ExpUuidLock);                                                                   | 
    KiLeaveCriticalRegionUnsafe(CurrentThread);                                                                 | 
  }                                                                                                             | 
  SeReleaseSubjectContext(&SubjectSecurityContext);                                                             | 
  return (unsigned int)AccessStatus;                                                                            | 
}                                                                                                               | 
127 row(s)
