NTSTATUS __fastcall NtCreateLowBoxToken(                                                                               | 
        HANDLE *a1,                                                                                                    | 
        void *a2,                                                                                                      | 
        int a3,                                                                                                        | 
        int a4,                                                                                                        | 
        void *Src,                                                                                                     | 
        unsigned int a6,                                                                                               | 
        __int64 a7,                                                                                                    | 
        unsigned int a8,                                                                                               | 
        __int64 a9)                                                                                                    | 
{                                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                        | 
                                                                                                                       | 
  v12 = a1;                                                                                                            | 
  if ( !(unsigned int)Feature_DecoupleAppContainerAndIntegrityLevel__private_IsEnabledDeviceUsageNoInline() )          | 
  {                                                                                                                    | 
    v56 = 0LL;                                                                                                         | 
    v61 = 0;                                                                                                           | 
    v66 = 0LL;                                                                                                         | 
    Sid = 0LL;                                                                                                         | 
    v68 = 0LL;                                                                                                         | 
    Handle = 0LL;                                                                                                      | 
    v53 = 0LL;                                                                                                         | 
    v62[0] = 1;                                                                                                        | 
    v52 = 0;                                                                                                           | 
    v18 = 0;                                                                                                           | 
    v51 = 0;                                                                                                           | 
    v58 = 0;                                                                                                           | 
    LOBYTE(v54) = 0;                                                                                                   | 
    v74 = 0LL;                                                                                                         | 
    v75 = 0LL;                                                                                                         | 
    PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
    if ( PreviousMode )                                                                                                | 
    {                                                                                                                  | 
      v20 = 0x7FFFFFFF0000LL;                                                                                          | 
      if ( (unsigned __int64)v12 < 0x7FFFFFFF0000LL )                                                                  | 
      {                                                                                                                | 
        v20 = (__int64)v12;                                                                                            | 
      }                                                                                                                | 
      *(_QWORD *)v20 = *(_QWORD *)v20;                                                                                 | 
      if ( 8LL * a8 && (a9 & 7) != 0 )                                                                                 | 
      {                                                                                                                | 
        ExRaiseDatatypeMisalignment();                                                                                 | 
      }                                                                                                                | 
    }                                                                                                                  | 
    if ( !Src )                                                                                                        | 
    {                                                                                                                  | 
      return -1073741811;                                                                                              | 
    }                                                                                                                  | 
    if ( a8 )                                                                                                          | 
    {                                                                                                                  | 
      if ( !a9 )                                                                                                       | 
      {                                                                                                                | 
        return -1073741776;                                                                                            | 
      }                                                                                                                | 
    }                                                                                                                  | 
    else if ( a9 )                                                                                                     | 
    {                                                                                                                  | 
      return -1073741776;                                                                                              | 
    }                                                                                                                  | 
    Token = 0LL;                                                                                                       | 
    result = ObReferenceObjectByHandle(a2, 2u, (POBJECT_TYPE)SeTokenObjectType, PreviousMode, &Token, &v68);           | 
    if ( result < 0 )                                                                                                  | 
    {                                                                                                                  | 
      return result;                                                                                                   | 
    }                                                                                                                  | 
    AppContainerSidType = SeCaptureSid(Src, (int)Object, 1, (__int64)&Sid);                                            | 
    if ( AppContainerSidType < 0 || (AppContainerSidType = SepCheckCreateAppContainer(Sid), AppContainerSidType < 0) ) | 
    {                                                                                                                  | 
LABEL_107:                                                                                                             | 
      if ( v52 )                                                                                                       | 
      {                                                                                                                | 
        if ( AppContainerSidType >= 0 )                                                                                | 
        {                                                                                                              | 
          *((_QWORD *)v53 + 7) = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);             | 
        }                                                                                                              | 
        _InterlockedOr(v46, 0);                                                                                        | 
        ExReleaseResourceLite(*((PERESOURCE *)v53 + 6));                                                               | 
        KeLeaveCriticalRegion();                                                                                       | 
      }                                                                                                                | 
      if ( AppContainerSidType < 0 )                                                                                   | 
      {                                                                                                                | 
        if ( v18 )                                                                                                     | 
        {                                                                                                              | 
          ObfDereferenceObject(v53);                                                                                   | 
        }                                                                                                              | 
        if ( Handle )                                                                                                  | 
        {                                                                                                              | 
          ObCloseHandle(Handle, PreviousMode);                                                                         | 
        }                                                                                                              | 
      }                                                                                                                | 
      if ( v56 )                                                                                                       | 
      {                                                                                                                | 
        LOBYTE(v22) = PreviousMode;                                                                                    | 
        SeReleaseLuidAndAttributesArray(v56, v22);                                                                     | 
      }                                                                                                                | 
      if ( Sid )                                                                                                       | 
      {                                                                                                                | 
        LOBYTE(v23) = 1;                                                                                               | 
        LOBYTE(v22) = PreviousMode;                                                                                    | 
        SeReleaseSid(Sid, v22, v23);                                                                                   | 
      }                                                                                                                | 
      if ( Token )                                                                                                     | 
      {                                                                                                                | 
        ObfDereferenceObject(Token);                                                                                   | 
      }                                                                                                                | 
      if ( v66 )                                                                                                       | 
      {                                                                                                                | 
        ExFreePoolWithTag(v66, 0);                                                                                     | 
      }                                                                                                                | 
      if ( AppContainerSidType >= 0 )                                                                                  | 
      {                                                                                                                | 
        *v12 = Handle;                                                                                                 | 
      }                                                                                                                | 
      return AppContainerSidType;                                                                                      | 
    }                                                                                                                  | 
    if ( *((_DWORD *)Token + 48) != 1 && *((int *)Token + 49) < 2 )                                                    | 
    {                                                                                                                  | 
      AppContainerSidType = -1073741659;                                                                               | 
      v18 = 0;                                                                                                         | 
      goto LABEL_107;                                                                                                  | 
    }                                                                                                                  | 
    GrantedAccess = v68.GrantedAccess;                                                                                 | 
    if ( a3 )                                                                                                          | 
    {                                                                                                                  | 
      GrantedAccess = a3;                                                                                              | 
    }                                                                                                                  | 
    v62[1] = GrantedAccess;                                                                                            | 
    if ( a7 )                                                                                                          | 
    {                                                                                                                  | 
      v50 = &v61;                                                                                                      | 
      v49 = (__int64 *)&v56;                                                                                           | 
      LOBYTE(v23) = PreviousMode;                                                                                      | 
      AppContainerSidType = SeCaptureSidAndAttributesArray(a7, a6, v23, 0LL, 0);                                       | 
    }                                                                                                                  | 
    if ( AppContainerSidType < 0 )                                                                                     | 
    {                                                                                                                  | 
      goto LABEL_50;                                                                                                   | 
    }                                                                                                                  | 
    AppContainerSidType = SeCaptureHandles(a8, a9, &v66);                                                              | 
    if ( AppContainerSidType < 0 )                                                                                     | 
    {                                                                                                                  | 
      goto LABEL_50;                                                                                                   | 
    }                                                                                                                  | 
    if ( !(unsigned __int8)RtlIsPackageSid(Sid) )                                                                      | 
    {                                                                                                                  | 
      AppContainerSidType = -1073741811;                                                                               | 
LABEL_50:                                                                                                              | 
      v18 = 0;                                                                                                         | 
LABEL_106:                                                                                                             | 
      v12 = a1;                                                                                                        | 
      goto LABEL_107;                                                                                                  | 
    }                                                                                                                  | 
    if ( ((*((_BYTE *)Sid + 1) - 8) & 0xFB) != 0 )                                                                     | 
    {                                                                                                                  | 
      AppContainerSidType = -1073700350;                                                                               | 
      goto LABEL_50;                                                                                                   | 
    }                                                                                                                  | 
    v25 = 0;                                                                                                           | 
    if ( a6 )                                                                                                          | 
    {                                                                                                                  | 
      v26 = v56;                                                                                                       | 
      while ( (unsigned __int8)RtlIsCapabilitySid(*v26) )                                                              | 
      {                                                                                                                | 
        v27 = 0;                                                                                                       | 
        if ( v25 )                                                                                                     | 
        {                                                                                                              | 
          v28 = (void *)*v26;                                                                                          | 
          while ( !RtlEqualSid(v28, (PSID)v56[2 * v27]) )                                                              | 
          {                                                                                                            | 
            if ( ++v27 >= v25 )                                                                                        | 
            {                                                                                                          | 
              goto LABEL_60;                                                                                           | 
            }                                                                                                          | 
          }                                                                                                            | 
          AppContainerSidType = -1073741811;                                                                           | 
          goto LABEL_105;                                                                                              | 
        }                                                                                                              | 
LABEL_60:                                                                                                              | 
        ++v25;                                                                                                         | 
        v26 += 2;                                                                                                      | 
        if ( v25 >= a6 )                                                                                               | 
        {                                                                                                              | 
          goto LABEL_61;                                                                                               | 
        }                                                                                                              | 
      }                                                                                                                | 
    }                                                                                                                  | 
    else                                                                                                               | 
    {                                                                                                                  | 
LABEL_61:                                                                                                              | 
      v29 = v56;                                                                                                       | 
      if ( (unsigned __int8)SepCapabilitiesHasAppSiloBaseSID(a6, v56) || (v30 = 0, !a6) )                              | 
      {                                                                                                                | 
LABEL_65:                                                                                                              | 
        AppContainerSidType = RtlGetAppContainerSidType(Sid);                                                          | 
        if ( AppContainerSidType >= 0 )                                                                                | 
        {                                                                                                              | 
          if ( v58 != 1 || (AppContainerSidType = SepCheckCapabilities(Token, (__int64)&v54), (_BYTE)v54) )            | 
          {                                                                                                            | 
            if ( AppContainerSidType >= 0 )                                                                            | 
            {                                                                                                          | 
              if ( !(unsigned int)Feature_871661881__private_IsEnabledDeviceUsageNoInline() )                          | 
              {                                                                                                        | 
                goto LABEL_82;                                                                                         | 
              }                                                                                                        | 
              v59 = 0;                                                                                                 | 
              RtlQueryElevationFlags(&v59);                                                                            | 
              if ( (v59 & 0x18) != 0x10 )                                                                              | 
              {                                                                                                        | 
                goto LABEL_82;                                                                                         | 
              }                                                                                                        | 
              CurrentThread = KeGetCurrentThread();                                                                    | 
              --CurrentThread->KernelApcDisable;                                                                       | 
              v32 = (PERESOURCE *)Token;                                                                               | 
              ExAcquireResourceSharedLite(*((PERESOURCE *)Token + 6), 1u);                                             | 
              TokenIntegrity = (PSID *)SepLocateTokenIntegrity(v32);                                                   | 
              if ( TokenIntegrity )                                                                                    | 
              {                                                                                                        | 
                v34 = *TokenIntegrity;                                                                                 | 
                v35 = *RtlSubAuthorityCountSid(*TokenIntegrity);                                                       | 
                if ( (_BYTE)v35 )                                                                                      | 
                {                                                                                                      | 
                  if ( *RtlSubAuthoritySid(v34, v35 - 1) >= 0x3000 )                                                   | 
                  {                                                                                                    | 
                    memset(&SubjectContext, 0, sizeof(SubjectContext));                                                | 
                    SeCaptureSubjectContext(&SubjectContext);                                                          | 
                    v36 = KeGetCurrentThread();                                                                        | 
                    --v36->KernelApcDisable;                                                                           | 
                    PrimaryToken = (PERESOURCE *)SubjectContext.PrimaryToken;                                          | 
                    ExAcquireResourceSharedLite(*((PERESOURCE *)SubjectContext.PrimaryToken + 6), 1u);                 | 
                    v38 = *(void **)SepLocateTokenIntegrity(PrimaryToken);                                             | 
                    v39 = *RtlSubAuthorityCountSid(v38);                                                               | 
                    if ( !(_BYTE)v39 || *RtlSubAuthoritySid(v38, v39 - 1) < 0x3000 )                                   | 
                    {                                                                                                  | 
                      AppContainerSidType = -1073741790;                                                               | 
                    }                                                                                                  | 
                    ExReleaseResourceLite(PrimaryToken[6]);                                                            | 
                    KeLeaveCriticalRegion();                                                                           | 
                    SeReleaseSubjectContext(&SubjectContext);                                                          | 
                  }                                                                                                    | 
                }                                                                                                      | 
              }                                                                                                        | 
              ExReleaseResourceLite(v32[6]);                                                                           | 
              KeLeaveCriticalRegion();                                                                                 | 
              if ( AppContainerSidType >= 0 )                                                                          | 
              {                                                                                                        | 
LABEL_82:                                                                                                              | 
                AppContainerSidType = SepDuplicateToken((_DWORD)Token, a4, 0, 1, 0, PreviousMode, 0, (__int64)&v53);   | 
                if ( AppContainerSidType >= 0 )                                                                        | 
                {                                                                                                      | 
                  v18 = 1;                                                                                             | 
                  v51 = 1;                                                                                             | 
                  AppContainerSidType = SeSetMandatoryPolicyToken(v53, v62);                                           | 
                  if ( AppContainerSidType < 0 )                                                                       | 
                  {                                                                                                    | 
                    goto LABEL_106;                                                                                    | 
                  }                                                                                                    | 
                  v40 = KeGetCurrentThread();                                                                          | 
                  --v40->KernelApcDisable;                                                                             | 
                  ExAcquireResourceExclusiveLite(*((PERESOURCE *)v53 + 6), 1u);                                        | 
                  _InterlockedOr(v46, 0);                                                                              | 
                  v52 = 1;                                                                                             | 
                  v41 = SepLocateTokenIntegrity(v53);                                                                  | 
                  if ( v41 )                                                                                           | 
                  {                                                                                                    | 
                    v42 = *(void **)v41;                                                                               | 
                    v43 = *(unsigned __int8 *)(*(_QWORD *)v41 + 1LL);                                                  | 
                    if ( (_BYTE)v43 )                                                                                  | 
                    {                                                                                                  | 
                      *RtlSubAuthoritySid(v42, v43 - 1) = 4096;                                                        | 
                    }                                                                                                  | 
                    *((_QWORD *)v53 + 9) &= 0x200800000uLL;                                                            | 
                    *((_QWORD *)v53 + 10) &= 0x200800000uLL;                                                           | 
                    *((_QWORD *)v53 + 8) &= 0x200800000uLL;                                                            | 
                    *((_DWORD *)v53 + 50) &= (unsigned int)Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline() | 
                                           ? 0xFFFFCFFF                                                                | 
                                           : 0xFFFFDFFF;                                                               | 
                    *((_DWORD *)v53 + 50) |= 0x4000u;                                                                  | 
                  }                                                                                                    | 
                  else                                                                                                 | 
                  {                                                                                                    | 
                    AppContainerSidType = -1073740730;                                                                 | 
                  }                                                                                                    | 
                  if ( AppContainerSidType < 0 )                                                                       | 
                  {                                                                                                    | 
                    goto LABEL_106;                                                                                    | 
                  }                                                                                                    | 
                  v44 = Sid;                                                                                           | 
                  AppContainerSidType = SepSetTokenCapabilities(v53, Sid, v56, a6);                                    | 
                  if ( AppContainerSidType >= 0 )                                                                      | 
                  {                                                                                                    | 
                    AppContainerSidType = SepSetTokenLowboxNumber(v53, v44);                                           | 
                    if ( AppContainerSidType >= 0 )                                                                    | 
                    {                                                                                                  | 
                      *((_QWORD *)&v74 + 1) = v44;                                                                     | 
                      AppContainerSidType = SepSetTokenCachedHandles(v53, &v74, a8, v66);                              | 
                      if ( AppContainerSidType >= 0 )                                                                  | 
                      {                                                                                                | 
                        AppContainerSidType = SepSetTokenPackage(v53, v44);                                            | 
                        if ( AppContainerSidType >= 0 )                                                                | 
                        {                                                                                              | 
                          AppContainerSidType = SepAppendAceToTokenDefaultDacl(v53, v44);                              | 
                          if ( AppContainerSidType >= 0 )                                                              | 
                          {                                                                                            | 
                            v45 = v53;                                                                                 | 
                            LOBYTE(HandleInformation) = 0;                                                             | 
                            LOBYTE(Object) = 1;                                                                        | 
                            if ( (unsigned __int8)SepSidInTokenSidHash(                                                | 
                                                    (char *)v53 + 808,                                                 | 
                                                    0LL,                                                               | 
                                                    SePermissiveLearningModeCapabilitySid,                             | 
                                                    0LL,                                                               | 
                                                    (_DWORD)Object,                                                    | 
                                                    (_DWORD)HandleInformation) )                                       | 
                            {                                                                                          | 
                              _InterlockedAdd(&SepLearningModeTokenCount, 1u);                                         | 
                              *((_DWORD *)v53 + 50) |= 0x3000000u;                                                     | 
                            }                                                                                          | 
                            else                                                                                       | 
                            {                                                                                          | 
                              v45[50] &= 0xFCFFFFFF;                                                                   | 
                              LOBYTE(HandleInformation) = 0;                                                           | 
                              LOBYTE(Object) = 1;                                                                      | 
                              if ( (unsigned __int8)SepSidInTokenSidHash(                                              | 
                                                      (char *)v53 + 808,                                               | 
                                                      0LL,                                                             | 
                                                      SeLearningModeLoggingCapabilitySid,                              | 
                                                      0LL,                                                             | 
                                                      (_DWORD)Object,                                                  | 
                                                      (_DWORD)HandleInformation) )                                     | 
                              {                                                                                        | 
                                _InterlockedAdd(&SepLearningModeTokenCount, 1u);                                       | 
                                *((_DWORD *)v53 + 50) |= 0x1000000u;                                                   | 
                              }                                                                                        | 
                            }                                                                                          | 
                            *((_QWORD *)v53 + 7) = ExpLuidIncrement                                                    | 
                                                 + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);              | 
                            _InterlockedOr(v46, 0);                                                                    | 
                            ExReleaseResourceLite(*((PERESOURCE *)v53 + 6));                                           | 
                            KeLeaveCriticalRegion();                                                                   | 
                            v52 = 0;                                                                                   | 
                            AppContainerSidType = ObInsertObjectEx(v53, 0, 0LL, (__int64)&Handle);                     | 
                            if ( AppContainerSidType < 0 )                                                             | 
                            {                                                                                          | 
                              goto LABEL_50;                                                                           | 
                            }                                                                                          | 
                            AppContainerSidType = SepAppendAceToTokenObjectAcl((ULONG_PTR)v53);                        | 
                            if ( AppContainerSidType >= 0 )                                                            | 
                            {                                                                                          | 
                              SepFinalizeTokenAcls(v53);                                                               | 
                              ObfDereferenceObject(v53);                                                               | 
                              goto LABEL_50;                                                                           | 
                            }                                                                                          | 
                          }                                                                                            | 
                        }                                                                                              | 
                      }                                                                                                | 
                    }                                                                                                  | 
                  }                                                                                                    | 
                }                                                                                                      | 
              }                                                                                                        | 
            }                                                                                                          | 
          }                                                                                                            | 
          else                                                                                                         | 
          {                                                                                                            | 
            AppContainerSidType = -1073741790;                                                                         | 
          }                                                                                                            | 
        }                                                                                                              | 
LABEL_105:                                                                                                             | 
        v18 = v51;                                                                                                     | 
        goto LABEL_106;                                                                                                | 
      }                                                                                                                | 
      while ( !(unsigned __int8)SepIsAppSiloCapability(*v29) )                                                         | 
      {                                                                                                                | 
        ++v30;                                                                                                         | 
        v29 += 2;                                                                                                      | 
        if ( v30 >= a6 )                                                                                               | 
        {                                                                                                              | 
          goto LABEL_65;                                                                                               | 
        }                                                                                                              | 
      }                                                                                                                | 
    }                                                                                                                  | 
    AppContainerSidType = -1073741811;                                                                                 | 
    goto LABEL_105;                                                                                                    | 
  }                                                                                                                    | 
  v63 = 0LL;                                                                                                           | 
  v60 = 0;                                                                                                             | 
  P = 0LL;                                                                                                             | 
  v64 = 0LL;                                                                                                           | 
  v69 = 0LL;                                                                                                           | 
  v70 = 0LL;                                                                                                           | 
  v71 = 0LL;                                                                                                           | 
  *(_OWORD *)v72 = 0LL;                                                                                                | 
  v73 = 0LL;                                                                                                           | 
  v13 = KeGetCurrentThread()->PreviousMode;                                                                            | 
  if ( v13 )                                                                                                           | 
  {                                                                                                                    | 
    v14 = 0x7FFFFFFF0000LL;                                                                                            | 
    if ( (unsigned __int64)v12 < 0x7FFFFFFF0000LL )                                                                    | 
    {                                                                                                                  | 
      v14 = (__int64)v12;                                                                                              | 
    }                                                                                                                  | 
    *(_QWORD *)v14 = *(_QWORD *)v14;                                                                                   | 
    if ( 8LL * a8 && (a9 & 7) != 0 )                                                                                   | 
    {                                                                                                                  | 
      ExRaiseDatatypeMisalignment();                                                                                   | 
    }                                                                                                                  | 
  }                                                                                                                    | 
  if ( !Src )                                                                                                          | 
  {                                                                                                                    | 
    return -1073741811;                                                                                                | 
  }                                                                                                                    | 
  if ( !a8 )                                                                                                           | 
  {                                                                                                                    | 
    if ( !a9 )                                                                                                         | 
    {                                                                                                                  | 
      goto LABEL_12;                                                                                                   | 
    }                                                                                                                  | 
    return -1073741776;                                                                                                | 
  }                                                                                                                    | 
  if ( !a9 )                                                                                                           | 
  {                                                                                                                    | 
    return -1073741776;                                                                                                | 
  }                                                                                                                    | 
LABEL_12:                                                                                                              | 
  AppContainerSidType = SeCaptureSid(Src, (int)Object, 1, (__int64)&v64);                                              | 
  if ( AppContainerSidType >= 0 )                                                                                      | 
  {                                                                                                                    | 
    if ( a7 )                                                                                                          | 
    {                                                                                                                  | 
      v50 = &v60;                                                                                                      | 
      v49 = &v63;                                                                                                      | 
      LOBYTE(v17) = v13;                                                                                               | 
      AppContainerSidType = SeCaptureSidAndAttributesArray(a7, a6, v17, 0LL, 0);                                       | 
    }                                                                                                                  | 
    if ( AppContainerSidType >= 0 )                                                                                    | 
    {                                                                                                                  | 
      AppContainerSidType = SeCaptureHandles(a8, a9, &P);                                                              | 
      if ( AppContainerSidType >= 0 )                                                                                  | 
      {                                                                                                                | 
        v72[1] = (__int64)&v69;                                                                                        | 
        LODWORD(v69) = 48;                                                                                             | 
        DWORD1(v69) |= 1u;                                                                                             | 
        *(_QWORD *)&v70 = v64;                                                                                         | 
        *((_QWORD *)&v70 + 1) = v63;                                                                                   | 
        LODWORD(v71) = a6;                                                                                             | 
        *((_QWORD *)&v69 + 1) = a2;                                                                                    | 
        *(_QWORD *)&v73 = P;                                                                                           | 
        DWORD2(v73) = a8;                                                                                              | 
        AppContainerSidType = SepCreateAppContainerToken((int)a1, 0, a3, a4, v13, (__int64)v72);                       | 
      }                                                                                                                | 
    }                                                                                                                  | 
  }                                                                                                                    | 
  if ( v63 )                                                                                                           | 
  {                                                                                                                    | 
    LOBYTE(v15) = v13;                                                                                                 | 
    SeReleaseLuidAndAttributesArray(v63, v15);                                                                         | 
  }                                                                                                                    | 
  if ( v64 )                                                                                                           | 
  {                                                                                                                    | 
    LOBYTE(v17) = 1;                                                                                                   | 
    LOBYTE(v15) = v13;                                                                                                 | 
    SeReleaseSid(v64, v15, v17);                                                                                       | 
  }                                                                                                                    | 
  if ( P )                                                                                                             | 
  {                                                                                                                    | 
    ExFreePoolWithTag(P, 0);                                                                                           | 
  }                                                                                                                    | 
  return AppContainerSidType;                                                                                          | 
}                                                                                                                      | 
469 row(s)
