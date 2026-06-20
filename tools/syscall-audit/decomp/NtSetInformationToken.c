NTSTATUS __stdcall NtSetInformationToken(                                                                                     | 
        HANDLE TokenHandle,                                                                                                   | 
        TOKEN_INFORMATION_CLASS TokenInformationClass,                                                                        | 
        PVOID TokenInformation,                                                                                               | 
        ULONG TokenInformationLength)                                                                                         | 
{                                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                               | 
                                                                                                                              | 
  v8 = 0;                                                                                                                     | 
  v144 = 0;                                                                                                                   | 
  v146 = 0;                                                                                                                   | 
  v162 = 0LL;                                                                                                                 | 
  v163 = 0LL;                                                                                                                 | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                          | 
  v145 = PreviousMode;                                                                                                        | 
  if ( PreviousMode && TokenInformationLength && ((unsigned __int8)TokenInformation & 3) != 0 )                               | 
  {                                                                                                                           | 
    ExRaiseDatatypeMisalignment();                                                                                            | 
  }                                                                                                                           | 
  if ( (unsigned int)TokenInformationClass > (TokenRestrictedDeviceClaimAttributes|TokenAuditPolicy)                          | 
    || (v10 = 0x1024802F8B5070LL, !_bittest64(&v10, TokenInformationClass)) )                                                 | 
  {                                                                                                                           | 
    if ( !(unsigned int)Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline() )                                         | 
    {                                                                                                                         | 
      return -1073741821;                                                                                                     | 
    }                                                                                                                         | 
    v149 = 0;                                                                                                                 | 
    if ( (int)RtlQueryElevationFlags(&v149) < 0 || (v149 & 0x18) != 0x10 || TokenInformationClass != -2 )                     | 
    {                                                                                                                         | 
      return -1073741821;                                                                                                     | 
    }                                                                                                                         | 
  }                                                                                                                           | 
  if ( !(unsigned int)Feature_ShadowAdminAppCompat__private_IsEnabledDeviceUsageNoInline()                                    | 
    && TokenInformationClass == (TokenRestrictedDeviceClaimAttributes|TokenAuditPolicy) )                                     | 
  {                                                                                                                           | 
    return -1073741821;                                                                                                       | 
  }                                                                                                                           | 
  v11 = 128;                                                                                                                  | 
  if ( TokenInformationClass == TokenSessionId )                                                                              | 
  {                                                                                                                           | 
    v11 = 384;                                                                                                                | 
  }                                                                                                                           | 
  else if ( TokenInformationClass == TokenLinkedToken || TokenInformationClass == -2 )                                        | 
  {                                                                                                                           | 
    v11 = 136;                                                                                                                | 
  }                                                                                                                           | 
  Token = 0LL;                                                                                                                | 
  result = ObReferenceObjectByHandle(TokenHandle, v11, (POBJECT_TYPE)SeTokenObjectType, PreviousMode, &Token, 0LL);           | 
  v15 = (PERESOURCE *)Token;                                                                                                  | 
  v154 = Token;                                                                                                               | 
  if ( result >= 0 )                                                                                                          | 
  {                                                                                                                           | 
    if ( TokenInformationClass <= TokenVirtualizationAllowed )                                                                | 
    {                                                                                                                         | 
      if ( TokenInformationClass != TokenVirtualizationAllowed )                                                              | 
      {                                                                                                                       | 
        switch ( TokenInformationClass )                                                                                      | 
        {                                                                                                                     | 
          case -2:                                                                                                            | 
            if ( TokenInformationLength != 8 )                                                                                | 
            {                                                                                                                 | 
              goto LABEL_30;                                                                                                  | 
            }                                                                                                                 | 
            v175 = *(PSID *)TokenInformation;                                                                                 | 
            LOBYTE(v14) = PreviousMode;                                                                                       | 
            v17 = SepOneWayLinkLogonSessions(Token, v175, v14);                                                               | 
            goto LABEL_60;                                                                                                    | 
          case 4:                                                                                                             | 
            if ( TokenInformationLength < 8 )                                                                                 | 
            {                                                                                                                 | 
              goto LABEL_97;                                                                                                  | 
            }                                                                                                                 | 
            Sid1 = *(PSID *)TokenInformation;                                                                                 | 
            v16 = SeCaptureSid(Sid1, (int)Object, 1, (__int64)&Sid1);                                                         | 
            v143 = v16;                                                                                                       | 
            if ( v16 < 0 )                                                                                                    | 
            {                                                                                                                 | 
              goto LABEL_99;                                                                                                  | 
            }                                                                                                                 | 
            v58 = 0;                                                                                                          | 
            CurrentThread = KeGetCurrentThread();                                                                             | 
            --CurrentThread->KernelApcDisable;                                                                                | 
            ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                   | 
            _InterlockedOr(v137, 0);                                                                                          | 
            while ( 1 )                                                                                                       | 
            {                                                                                                                 | 
              if ( v58 >= *((_DWORD *)v15 + 31) )                                                                             | 
              {                                                                                                               | 
                _InterlockedOr(v137, 0);                                                                                      | 
                ExReleaseResourceLite(v15[6]);                                                                                | 
                KeLeaveCriticalRegion();                                                                                      | 
                ObfDereferenceObject(v15);                                                                                    | 
                LOBYTE(v63) = 1;                                                                                              | 
                LOBYTE(v64) = PreviousMode;                                                                                   | 
                SeReleaseSid(Sid1, v64, v63);                                                                                 | 
                return -1073741734;                                                                                           | 
              }                                                                                                               | 
              if ( RtlEqualSid(Sid1, (&v15[19]->SystemResourcesList.Flink)[2 * v58]) )                                        | 
              {                                                                                                               | 
                break;                                                                                                        | 
              }                                                                                                               | 
              ++v58;                                                                                                          | 
            }                                                                                                                 | 
            if ( (unsigned __int8)SepIdAssignableAsOwner(v15, v58) )                                                          | 
            {                                                                                                                 | 
              *((_DWORD *)v15 + 36) = v58;                                                                                    | 
              v60 = 1;                                                                                                        | 
              v144 = 1;                                                                                                       | 
              v143 = 0;                                                                                                       | 
            }                                                                                                                 | 
            else                                                                                                              | 
            {                                                                                                                 | 
              v143 = -1073741734;                                                                                             | 
              v60 = v144;                                                                                                     | 
            }                                                                                                                 | 
            if ( v60 )                                                                                                        | 
            {                                                                                                                 | 
              Sid = (PSID)(ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement));                         | 
              v15[7] = (PERESOURCE)Sid;                                                                                       | 
            }                                                                                                                 | 
            _InterlockedOr(v137, 0);                                                                                          | 
            ExReleaseResourceLite(v15[6]);                                                                                    | 
            KeLeaveCriticalRegion();                                                                                          | 
            ObfDereferenceObject(v15);                                                                                        | 
            LOBYTE(v61) = 1;                                                                                                  | 
            LOBYTE(v62) = PreviousMode;                                                                                       | 
            SeReleaseSid(Sid1, v62, v61);                                                                                     | 
            return v143;                                                                                                      | 
          case 5:                                                                                                             | 
            if ( TokenInformationLength < 8 )                                                                                 | 
            {                                                                                                                 | 
              goto LABEL_97;                                                                                                  | 
            }                                                                                                                 | 
            v151 = *(_QWORD *)TokenInformation;                                                                               | 
            v16 = SeCaptureSid((void *)v151, (int)Object, 1, (__int64)&v151);                                                 | 
            v143 = v16;                                                                                                       | 
            v50 = v15;                                                                                                        | 
            if ( v16 < 0 )                                                                                                    | 
            {                                                                                                                 | 
LABEL_100:                                                                                                                    | 
              ObfDereferenceObject(v50);                                                                                      | 
              return v16;                                                                                                     | 
            }                                                                                                                 | 
            if ( (unsigned __int8)SepIdAssignableAsGroup(v15, v151) )                                                         | 
            {                                                                                                                 | 
              v51 = 4 * *(unsigned __int8 *)(v151 + 1) + 8;                                                                   | 
              v146 = v51;                                                                                                     | 
              v52 = KeGetCurrentThread();                                                                                     | 
              --v52->KernelApcDisable;                                                                                        | 
              v53 = (PERESOURCE *)Token;                                                                                      | 
              ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                 | 
              _InterlockedOr(v137, 0);                                                                                        | 
              v54 = Token;                                                                                                    | 
              v55 = *((_QWORD *)Token + 23);                                                                                  | 
              if ( v55 )                                                                                                      | 
              {                                                                                                               | 
                v51 += *(unsigned __int16 *)(v55 + 2);                                                                        | 
                v146 = v51;                                                                                                   | 
              }                                                                                                               | 
              if ( v51 <= *((_DWORD *)Token + 34) )                                                                           | 
              {                                                                                                               | 
                v143 = SepExpandDynamic(Token, v146);                                                                         | 
                if ( v143 >= 0 )                                                                                              | 
                {                                                                                                             | 
                  SepFreePrimaryGroup(v54);                                                                                   | 
                  SepAppendPrimaryGroup(v54, v151);                                                                           | 
                  v54[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                          | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  ExReleaseResourceLite(v53[6]);                                                                              | 
                  KeLeaveCriticalRegion();                                                                                    | 
                }                                                                                                             | 
                else                                                                                                          | 
                {                                                                                                             | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  ExReleaseResourceLite(v53[6]);                                                                              | 
                  KeLeaveCriticalRegion();                                                                                    | 
                  v8 = v143;                                                                                                  | 
                }                                                                                                             | 
              }                                                                                                               | 
              else                                                                                                            | 
              {                                                                                                               | 
                _InterlockedOr(v137, 0);                                                                                      | 
                ExReleaseResourceLite(v53[6]);                                                                                | 
                KeLeaveCriticalRegion();                                                                                      | 
                v8 = -1073741671;                                                                                             | 
              }                                                                                                               | 
              v15 = (PERESOURCE *)Token;                                                                                      | 
            }                                                                                                                 | 
            else                                                                                                              | 
            {                                                                                                                 | 
              v8 = -1073741733;                                                                                               | 
            }                                                                                                                 | 
            ObfDereferenceObject(v15);                                                                                        | 
            LOBYTE(v56) = 1;                                                                                                  | 
            LOBYTE(v57) = PreviousMode;                                                                                       | 
            SeReleaseSid(v151, v57, v56);                                                                                     | 
            return v8;                                                                                                        | 
          case 6:                                                                                                             | 
            if ( TokenInformationLength < 8 )                                                                                 | 
            {                                                                                                                 | 
              goto LABEL_97;                                                                                                  | 
            }                                                                                                                 | 
            v39 = *(PSID *)TokenInformation;                                                                                  | 
            v148 = (__int64)v39;                                                                                              | 
            if ( v39 )                                                                                                        | 
            {                                                                                                                 | 
              v16 = SeCaptureAcl(v39, 1, (int)HandleInformation, (__int64)&v148, (__int64)&v146);                             | 
              v143 = v16;                                                                                                     | 
            }                                                                                                                 | 
            else                                                                                                              | 
            {                                                                                                                 | 
              v146 = 0;                                                                                                       | 
              v16 = 0;                                                                                                        | 
              v143 = 0;                                                                                                       | 
            }                                                                                                                 | 
            if ( v16 < 0 )                                                                                                    | 
            {                                                                                                                 | 
              goto LABEL_99;                                                                                                  | 
            }                                                                                                                 | 
            v40 = KeGetCurrentThread();                                                                                       | 
            --v40->KernelApcDisable;                                                                                          | 
            v41 = (PERESOURCE *)Token;                                                                                        | 
            ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                   | 
            _InterlockedOr(v137, 0);                                                                                          | 
            v42 = Token;                                                                                                      | 
            v43 = v146 + 8 + 4 * *(unsigned __int8 *)(*((_QWORD *)Token + 21) + 1LL);                                         | 
            v146 = v43;                                                                                                       | 
            if ( (unsigned int)v43 <= *((_DWORD *)Token + 34) )                                                               | 
            {                                                                                                                 | 
              v143 = SepExpandDynamic(Token, v43);                                                                            | 
              if ( v143 >= 0 )                                                                                                | 
              {                                                                                                               | 
                v46 = (void *)v42[23];                                                                                        | 
                if ( !v46                                                                                                     | 
                  || (v143 = SeCaptureAcl(v46, 1, (int)HandleInformation, (__int64)&v162, (__int64)&v164), v143 >= 0) )       | 
                {                                                                                                             | 
                  SepFreeDefaultDacl(v42);                                                                                    | 
                  v48 = v148;                                                                                                 | 
                  if ( v148 )                                                                                                 | 
                  {                                                                                                           | 
                    SepAppendDefaultDacl(v42, v148);                                                                          | 
                  }                                                                                                           | 
                  SeTokenDefaultDaclChangedAuditAlarm(v47, (_DWORD)v42, (_DWORD)TokenHandle, v162, v48);                      | 
                  v42[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                          | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  ExReleaseResourceLite(v41[6]);                                                                              | 
                  KeLeaveCriticalRegion();                                                                                    | 
                  ObfDereferenceObject(Token);                                                                                | 
                  if ( v148 )                                                                                                 | 
                  {                                                                                                           | 
                    LOBYTE(v49) = PreviousMode;                                                                               | 
                    SeReleaseAcl(v148, v49);                                                                                  | 
                  }                                                                                                           | 
                  if ( v162 )                                                                                                 | 
                  {                                                                                                           | 
                    SeReleaseAcl(v162, 0LL);                                                                                  | 
                  }                                                                                                           | 
                  return 0;                                                                                                   | 
                }                                                                                                             | 
              }                                                                                                               | 
              _InterlockedOr(v137, 0);                                                                                        | 
              ExReleaseResourceLite(v41[6]);                                                                                  | 
              KeLeaveCriticalRegion();                                                                                        | 
              ObfDereferenceObject(Token);                                                                                    | 
              if ( v148 )                                                                                                     | 
              {                                                                                                               | 
                LOBYTE(v45) = PreviousMode;                                                                                   | 
                SeReleaseAcl(v148, v45);                                                                                      | 
              }                                                                                                               | 
              return v143;                                                                                                    | 
            }                                                                                                                 | 
            else                                                                                                              | 
            {                                                                                                                 | 
              _InterlockedOr(v137, 0);                                                                                        | 
              ExReleaseResourceLite(v41[6]);                                                                                  | 
              KeLeaveCriticalRegion();                                                                                        | 
              ObfDereferenceObject(Token);                                                                                    | 
              if ( v148 )                                                                                                     | 
              {                                                                                                               | 
                LOBYTE(v44) = PreviousMode;                                                                                   | 
                SeReleaseAcl(v148, v44);                                                                                      | 
              }                                                                                                               | 
              return -1073741671;                                                                                             | 
            }                                                                                                                 | 
          case 12:                                                                                                            | 
            if ( TokenInformationLength == 4 )                                                                                | 
            {                                                                                                                 | 
              v38 = *(_DWORD *)TokenInformation;                                                                              | 
              v169 = *(_DWORD *)TokenInformation;                                                                             | 
              if ( SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
              {                                                                                                               | 
                v17 = SeSetSessionIdToken(v15, v38);                                                                          | 
                goto LABEL_60;                                                                                                | 
              }                                                                                                               | 
              goto LABEL_58;                                                                                                  | 
            }                                                                                                                 | 
LABEL_97:                                                                                                                     | 
            ObfDereferenceObject(Token);                                                                                      | 
            return -1073741820;                                                                                               | 
          default:                                                                                                            | 
            switch ( TokenInformationClass )                                                                                  | 
            {                                                                                                                 | 
              case TokenSessionReference:                                                                                     | 
                if ( TokenInformationLength != 4 )                                                                            | 
                {                                                                                                             | 
                  goto LABEL_30;                                                                                              | 
                }                                                                                                             | 
                v35 = *(_DWORD *)TokenInformation;                                                                            | 
                v168 = *(_DWORD *)TokenInformation;                                                                           | 
                if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                  | 
                {                                                                                                             | 
                  goto LABEL_58;                                                                                              | 
                }                                                                                                             | 
                if ( !v35 )                                                                                                   | 
                {                                                                                                             | 
                  v36 = KeGetCurrentThread();                                                                                 | 
                  --v36->KernelApcDisable;                                                                                    | 
                  v37 = Token;                                                                                                | 
                  ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                             | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  v15 = (PERESOURCE *)Token;                                                                                  | 
                  if ( (*((_DWORD *)Token + 50) & 0x20) == 0 )                                                                | 
                  {                                                                                                           | 
                    if ( SeTokenLeakTracking )                                                                                | 
                    {                                                                                                         | 
                      SepRemoveTokenLogonSession(Token);                                                                      | 
                    }                                                                                                         | 
                    v16 = SepStopReferencingLogonSession(v15);                                                                | 
                    v143 = v16;                                                                                               | 
                    if ( v16 < 0 )                                                                                            | 
                    {                                                                                                         | 
                      goto LABEL_99;                                                                                          | 
                    }                                                                                                         | 
                  }                                                                                                           | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  v28 = (struct _ERESOURCE *)*((_QWORD *)v37 + 6);                                                            | 
                  goto LABEL_54;                                                                                              | 
                }                                                                                                             | 
                break;                                                                                                        | 
              case TokenAuditPolicy:                                                                                          | 
                v155 = 0LL;                                                                                                   | 
                if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                  | 
                {                                                                                                             | 
                  goto LABEL_58;                                                                                              | 
                }                                                                                                             | 
                v22 = KeGetCurrentThread();                                                                                   | 
                --v22->KernelApcDisable;                                                                                      | 
                v15 = (PERESOURCE *)Token;                                                                                    | 
                ExAcquireResourceSharedLite(*((PERESOURCE *)Token + 6), 1u);                                                  | 
                v23 = *((_BYTE *)v15 + 119);                                                                                  | 
                ExReleaseResourceLite(v15[6]);                                                                                | 
                KeLeaveCriticalRegion();                                                                                      | 
                if ( !v23 )                                                                                                   | 
                {                                                                                                             | 
                  if ( !TokenInformation )                                                                                    | 
                  {                                                                                                           | 
                    v27 = KeGetCurrentThread();                                                                               | 
                    --v27->KernelApcDisable;                                                                                  | 
                    ExAcquireResourceExclusiveLite(v15[6], 1u);                                                               | 
                    _InterlockedOr(v137, 0);                                                                                  | 
                    *((_BYTE *)v15 + 119) = 1;                                                                                | 
                    *((_QWORD *)Token + 7) = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);        | 
                    _InterlockedOr(v137, 0);                                                                                  | 
                    v28 = v15[6];                                                                                             | 
LABEL_54:                                                                                                                     | 
                    ExReleaseResourceLite(v28);                                                                               | 
                    KeLeaveCriticalRegion();                                                                                  | 
                    v15 = (PERESOURCE *)Token;                                                                                | 
                    goto LABEL_55;                                                                                            | 
                  }                                                                                                           | 
                  LOBYTE(v24) = PreviousMode;                                                                                 | 
                  v16 = SepCaptureAuditPolicy(                                                                                | 
                          (_DWORD)TokenInformation,                                                                           | 
                          v24,                                                                                                | 
                          v25,                                                                                                | 
                          v26,                                                                                                | 
                          (_DWORD)Object,                                                                                     | 
                          (_DWORD)HandleInformation,                                                                          | 
                          (__int64)&v155);                                                                                    | 
                  v143 = v16;                                                                                                 | 
                  if ( v16 < 0 )                                                                                              | 
                  {                                                                                                           | 
                    goto LABEL_99;                                                                                            | 
                  }                                                                                                           | 
                  v29 = KeGetCurrentThread();                                                                                 | 
                  --v29->KernelApcDisable;                                                                                    | 
                  ExAcquireResourceExclusiveLite(v15[6], 1u);                                                                 | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  *((_BYTE *)v15 + 119) = 2;                                                                                  | 
                  v30 = Token;                                                                                                | 
                  v31 = (char *)Token + 88;                                                                                   | 
                  v32 = v155;                                                                                                 | 
                  *(_OWORD *)((char *)Token + 88) = *v155;                                                                    | 
                  *((_QWORD *)v31 + 2) = *((_QWORD *)v32 + 2);                                                                | 
                  *((_DWORD *)v31 + 6) = *((_DWORD *)v32 + 6);                                                                | 
                  *((_WORD *)v31 + 14) = *((_WORD *)v32 + 14);                                                                | 
                  v31[30] = *((_BYTE *)v32 + 30);                                                                             | 
                  v30[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                          | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  ExReleaseResourceLite(v15[6]);                                                                              | 
                  KeLeaveCriticalRegion();                                                                                    | 
                  LOBYTE(v33) = 1;                                                                                            | 
                  SepModifyTokenPolicyCounter(v31, v33);                                                                      | 
                  ObfDereferenceObject(Token);                                                                                | 
                  LOBYTE(v34) = PreviousMode;                                                                                 | 
                  SepReleaseAuditPolicy(v155, v34);                                                                           | 
                  return 0;                                                                                                   | 
                }                                                                                                             | 
                break;                                                                                                        | 
              case TokenOrigin:                                                                                               | 
                if ( TokenInformationLength != 8 )                                                                            | 
                {                                                                                                             | 
                  goto LABEL_30;                                                                                              | 
                }                                                                                                             | 
                v170 = *(PSID *)TokenInformation;                                                                             | 
                if ( SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                   | 
                {                                                                                                             | 
                  v18 = KeGetCurrentThread();                                                                                 | 
                  --v18->KernelApcDisable;                                                                                    | 
                  v19 = Token;                                                                                                | 
                  ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                             | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  v20 = Token;                                                                                                | 
                  if ( !*((_QWORD *)Token + 28) )                                                                             | 
                  {                                                                                                           | 
                    *((_QWORD *)Token + 28) = v170;                                                                           | 
                  }                                                                                                           | 
                  v20[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                          | 
                  _InterlockedOr(v137, 0);                                                                                    | 
                  v21 = (struct _ERESOURCE *)*((_QWORD *)v19 + 6);                                                            | 
LABEL_37:                                                                                                                     | 
                  ExReleaseResourceLite(v21);                                                                                 | 
                  KeLeaveCriticalRegion();                                                                                    | 
                  v16 = 0;                                                                                                    | 
LABEL_38:                                                                                                                     | 
                  v15 = (PERESOURCE *)Token;                                                                                  | 
                  goto LABEL_99;                                                                                              | 
                }                                                                                                             | 
                goto LABEL_58;                                                                                                | 
              case TokenLinkedToken:                                                                                          | 
                if ( TokenInformationLength != 8 )                                                                            | 
                {                                                                                                             | 
LABEL_30:                                                                                                                     | 
                  v16 = -1073741820;                                                                                          | 
LABEL_99:                                                                                                                     | 
                  v50 = v15;                                                                                                  | 
                  goto LABEL_100;                                                                                             | 
                }                                                                                                             | 
                v174 = *(PSID *)TokenInformation;                                                                             | 
                LOBYTE(v14) = PreviousMode;                                                                                   | 
                v17 = SepLinkLogonSessions(Token, v174, v14);                                                                 | 
LABEL_60:                                                                                                                     | 
                v16 = v17;                                                                                                    | 
                goto LABEL_99;                                                                                                | 
              default:                                                                                                        | 
                return -1073741811;                                                                                           | 
            }                                                                                                                 | 
            v16 = -1073741811;                                                                                                | 
            goto LABEL_99;                                                                                                    | 
        }                                                                                                                     | 
      }                                                                                                                       | 
      if ( TokenInformationLength != 4 )                                                                                      | 
      {                                                                                                                       | 
        goto LABEL_30;                                                                                                        | 
      }                                                                                                                       | 
      v157 = *(_DWORD *)TokenInformation;                                                                                     | 
      if ( !SeSinglePrivilegeCheck(SeCreateTokenPrivilege, PreviousMode) )                                                    | 
      {                                                                                                                       | 
        goto LABEL_58;                                                                                                        | 
      }                                                                                                                       | 
      v65 = KeGetCurrentThread();                                                                                             | 
      --v65->KernelApcDisable;                                                                                                | 
      ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                         | 
      _InterlockedOr(v137, 0);                                                                                                | 
      v66 = Token;                                                                                                            | 
      v67 = *((_DWORD *)Token + 50);                                                                                          | 
      if ( v157 )                                                                                                             | 
      {                                                                                                                       | 
        v68 = v67 | 0x200;                                                                                                    | 
      }                                                                                                                       | 
      else                                                                                                                    | 
      {                                                                                                                       | 
        v68 = v67 & 0xFFFFFDFF;                                                                                               | 
      }                                                                                                                       | 
      *((_DWORD *)Token + 50) = v68;                                                                                          | 
      v66[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                                      | 
      _InterlockedOr(v137, 0);                                                                                                | 
      v15 = (PERESOURCE *)Token;                                                                                              | 
      ExReleaseResourceLite(*((PERESOURCE *)Token + 6));                                                                      | 
      KeLeaveCriticalRegion();                                                                                                | 
LABEL_121:                                                                                                                    | 
      v16 = 0;                                                                                                                | 
      goto LABEL_99;                                                                                                          | 
    }                                                                                                                         | 
    v69 = TokenInformationClass - 24;                                                                                         | 
    if ( !v69 )                                                                                                               | 
    {                                                                                                                         | 
      if ( TokenInformationLength != 4 )                                                                                      | 
      {                                                                                                                       | 
        goto LABEL_30;                                                                                                        | 
      }                                                                                                                       | 
      v167 = *(_DWORD *)TokenInformation;                                                                                     | 
      SeSetVirtualizationToken(Token);                                                                                        | 
      goto LABEL_55;                                                                                                          | 
    }                                                                                                                         | 
    v70 = v69 - 1;                                                                                                            | 
    if ( v70 )                                                                                                                | 
    {                                                                                                                         | 
      v71 = v70 - 1;                                                                                                          | 
      if ( v71 )                                                                                                              | 
      {                                                                                                                       | 
        v72 = v71 - 1;                                                                                                        | 
        if ( !v72 )                                                                                                           | 
        {                                                                                                                     | 
          if ( TokenInformationLength != 4 )                                                                                  | 
          {                                                                                                                   | 
            goto LABEL_30;                                                                                                    | 
          }                                                                                                                   | 
          v160 = *(_DWORD *)TokenInformation;                                                                                 | 
          if ( SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                         | 
          {                                                                                                                   | 
            if ( *((_BYTE *)v15 + 204) )                                                                                      | 
            {                                                                                                                 | 
              v16 = -1073741525;                                                                                              | 
              goto LABEL_99;                                                                                                  | 
            }                                                                                                                 | 
            v17 = SeSetMandatoryPolicyToken(v15, &v160);                                                                      | 
            goto LABEL_60;                                                                                                    | 
          }                                                                                                                   | 
          goto LABEL_58;                                                                                                      | 
        }                                                                                                                     | 
        v73 = v72 - 2;                                                                                                        | 
        if ( !v73 )                                                                                                           | 
        {                                                                                                                     | 
          if ( (unsigned int)Feature_DecoupleAppContainerAndIntegrityLevel__private_IsEnabledDeviceUsageNoInline() )          | 
          {                                                                                                                   | 
            if ( TokenInformationLength != 4 )                                                                                | 
            {                                                                                                                 | 
              goto LABEL_30;                                                                                                  | 
            }                                                                                                                 | 
            if ( PreviousMode )                                                                                               | 
            {                                                                                                                 | 
              ULongFromUser = RtlReadULongFromUser(TokenInformation);                                                         | 
              v159 = ULongFromUser;                                                                                           | 
            }                                                                                                                 | 
            else                                                                                                              | 
            {                                                                                                                 | 
              ULongFromUser = *(_DWORD *)TokenInformation;                                                                    | 
              v159 = *(_DWORD *)TokenInformation;                                                                             | 
              v15 = (PERESOURCE *)Token;                                                                                      | 
            }                                                                                                                 | 
            if ( ULongFromUser )                                                                                              | 
            {                                                                                                                 | 
              v16 = -1073741637;                                                                                              | 
              goto LABEL_99;                                                                                                  | 
            }                                                                                                                 | 
            if ( ((_DWORD)v15[25] & 0x4000) == 0 )                                                                            | 
            {                                                                                                                 | 
              v16 = -1073700352;                                                                                              | 
              goto LABEL_99;                                                                                                  | 
            }                                                                                                                 | 
            if ( SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                       | 
            {                                                                                                                 | 
              v91 = KeGetCurrentThread();                                                                                     | 
              --v91->KernelApcDisable;                                                                                        | 
              v92 = Token;                                                                                                    | 
              ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                 | 
              _InterlockedOr(v137, 0);                                                                                        | 
              v93 = Token;                                                                                                    | 
              SepRemoveAceFromTokenDefaultDacl(Token, *((_QWORD *)Token + 98));                                               | 
              *((_DWORD *)v15 + 50) &= ~0x4000u;                                                                              | 
              v94 = (void *)v93[98];                                                                                          | 
              if ( v94 )                                                                                                      | 
              {                                                                                                               | 
                ExFreePoolWithTag(v94, 0);                                                                                    | 
                v93[98] = 0LL;                                                                                                | 
              }                                                                                                               | 
              v95 = (void *)v93[99];                                                                                          | 
              if ( v95 )                                                                                                      | 
              {                                                                                                               | 
                ExFreePoolWithTag(v95, 0);                                                                                    | 
                v93[99] = 0LL;                                                                                                | 
                *((_DWORD *)v93 + 200) = 0;                                                                                   | 
                memset_0(v93 + 101, 0, 0x110uLL);                                                                             | 
              }                                                                                                               | 
              if ( v93[135] )                                                                                                 | 
              {                                                                                                               | 
                SepDereferenceLowBoxNumberEntry(*((unsigned int *)v93 + 30));                                                 | 
                v93[135] = 0LL;                                                                                               | 
              }                                                                                                               | 
              if ( v93[136] )                                                                                                 | 
              {                                                                                                               | 
                SepDereferenceCachedHandlesEntryInternal(v93[27] + 96LL);                                                     | 
                v93[136] = 0LL;                                                                                               | 
              }                                                                                                               | 
              _InterlockedOr(v137, 0);                                                                                        | 
              v21 = (struct _ERESOURCE *)*((_QWORD *)v92 + 6);                                                                | 
              goto LABEL_37;                                                                                                  | 
            }                                                                                                                 | 
LABEL_58:                                                                                                                     | 
            v16 = -1073741727;                                                                                                | 
            goto LABEL_99;                                                                                                    | 
          }                                                                                                                   | 
          goto LABEL_141;                                                                                                     | 
        }                                                                                                                     | 
        v74 = v73 - 10;                                                                                                       | 
        if ( !v74 )                                                                                                           | 
        {                                                                                                                     | 
          P = 0LL;                                                                                                            | 
          if ( !TokenInformation || TokenInformationLength < 0x10 )                                                           | 
          {                                                                                                                   | 
            goto LABEL_30;                                                                                                    | 
          }                                                                                                                   | 
          LOBYTE(v13) = PreviousMode;                                                                                         | 
          v83 = SepCaptureTokenSecurityAttributesAndOperationsInformation(TokenInformation, v13, &P);                         | 
          if ( v83 >= 0 )                                                                                                     | 
          {                                                                                                                   | 
            if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                      | 
            {                                                                                                                 | 
              v83 = -1073741727;                                                                                              | 
            }                                                                                                                 | 
            v143 = v83;                                                                                                       | 
            if ( v83 >= 0 )                                                                                                   | 
            {                                                                                                                 | 
              v84 = P;                                                                                                        | 
              ShouldSetDelinkFlags = SepShouldSetDelinkFlags(*((_QWORD *)P + 1), *(_QWORD *)P);                               | 
              v86 = KeGetCurrentThread();                                                                                     | 
              --v86->KernelApcDisable;                                                                                        | 
              v87 = (PERESOURCE *)Token;                                                                                      | 
              ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                 | 
              _InterlockedOr(v137, 0);                                                                                        | 
              v88 = v84[1];                                                                                                   | 
              v89 = Token;                                                                                                    | 
              v143 = AuthzBasepSetSecurityAttributesToken(*((_QWORD *)Token + 97), v88, *(_QWORD *)P);                        | 
              if ( v143 >= 0 && ShouldSetDelinkFlags )                                                                        | 
              {                                                                                                               | 
                v89[50] |= 0x20000u;                                                                                          | 
              }                                                                                                               | 
              *((_QWORD *)v89 + 7) = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                | 
              _InterlockedOr(v137, 0);                                                                                        | 
              ExReleaseResourceLite(v87[6]);                                                                                  | 
              KeLeaveCriticalRegion();                                                                                        | 
              SepReleaseTokenSecurityAttributesAndOperationsInformation(P);                                                   | 
              v16 = v143;                                                                                                     | 
              goto LABEL_38;                                                                                                  | 
            }                                                                                                                 | 
            SepReleaseTokenSecurityAttributesAndOperationsInformation(P);                                                     | 
          }                                                                                                                   | 
          v16 = v83;                                                                                                          | 
          goto LABEL_99;                                                                                                      | 
        }                                                                                                                     | 
        v75 = v74 - 3;                                                                                                        | 
        if ( !v75 )                                                                                                           | 
        {                                                                                                                     | 
          if ( TokenInformationLength != 4 )                                                                                  | 
          {                                                                                                                   | 
            goto LABEL_30;                                                                                                    | 
          }                                                                                                                   | 
          v82 = *(_DWORD *)TokenInformation;                                                                                  | 
          v165 = *(_DWORD *)TokenInformation;                                                                                 | 
          if ( SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                         | 
          {                                                                                                                   | 
            v17 = SeSetPrivateNameSpaceToken(v15, v82);                                                                       | 
            goto LABEL_60;                                                                                                    | 
          }                                                                                                                   | 
          goto LABEL_58;                                                                                                      | 
        }                                                                                                                     | 
        v76 = v75 - 3;                                                                                                        | 
        if ( v76 )                                                                                                            | 
        {                                                                                                                     | 
          if ( v76 != 7 )                                                                                                     | 
          {                                                                                                                   | 
            return -1073741811;                                                                                               | 
          }                                                                                                                   | 
          if ( (unsigned int)Feature_ShadowAdminAppCompat__private_IsEnabledDeviceUsageNoInline() )                           | 
          {                                                                                                                   | 
            if ( TokenInformationLength != 4 )                                                                                | 
            {                                                                                                                 | 
              goto LABEL_30;                                                                                                  | 
            }                                                                                                                 | 
            if ( PreviousMode )                                                                                               | 
            {                                                                                                                 | 
              v77 = RtlReadULongFromUser(TokenInformation);                                                                   | 
              v158 = v77;                                                                                                     | 
            }                                                                                                                 | 
            else                                                                                                              | 
            {                                                                                                                 | 
              v77 = *(_DWORD *)TokenInformation;                                                                              | 
              v158 = *(_DWORD *)TokenInformation;                                                                             | 
              v15 = (PERESOURCE *)Token;                                                                                      | 
            }                                                                                                                 | 
            v78 = *((_DWORD *)v15 + 292);                                                                                     | 
            if ( v77 )                                                                                                        | 
            {                                                                                                                 | 
              v79 = v78 | 1;                                                                                                  | 
            }                                                                                                                 | 
            else                                                                                                              | 
            {                                                                                                                 | 
              v79 = v78 & 0xFFFFFFFE;                                                                                         | 
            }                                                                                                                 | 
            *((_DWORD *)v15 + 292) = v79;                                                                                     | 
            goto LABEL_121;                                                                                                   | 
          }                                                                                                                   | 
LABEL_141:                                                                                                                    | 
          v16 = -1073741821;                                                                                                  | 
          goto LABEL_99;                                                                                                      | 
        }                                                                                                                     | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                          | 
        {                                                                                                                     | 
          goto LABEL_58;                                                                                                      | 
        }                                                                                                                     | 
        if ( TokenInformationLength != 4 )                                                                                    | 
        {                                                                                                                     | 
          goto LABEL_30;                                                                                                      | 
        }                                                                                                                     | 
        HIDWORD(v164) = *(_DWORD *)TokenInformation;                                                                          | 
        if ( !HIDWORD(v164) && ((_DWORD)v15[25] & 0x80000) != 0 )                                                             | 
        {                                                                                                                     | 
          v80 = KeGetCurrentThread();                                                                                         | 
          --v80->KernelApcDisable;                                                                                            | 
          v81 = Token;                                                                                                        | 
          ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                     | 
          _InterlockedOr(v137, 0);                                                                                            | 
          *((_DWORD *)v15 + 50) &= ~0x80000u;                                                                                 | 
          *((_QWORD *)Token + 7) = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                  | 
          _InterlockedOr(v137, 0);                                                                                            | 
          v28 = (struct _ERESOURCE *)*((_QWORD *)v81 + 6);                                                                    | 
          goto LABEL_54;                                                                                                      | 
        }                                                                                                                     | 
      }                                                                                                                       | 
      else                                                                                                                    | 
      {                                                                                                                       | 
        if ( TokenInformationLength != 4 )                                                                                    | 
        {                                                                                                                     | 
          goto LABEL_30;                                                                                                      | 
        }                                                                                                                     | 
        v161 = *(_DWORD *)TokenInformation;                                                                                   | 
        if ( v161 && !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                  | 
        {                                                                                                                     | 
          goto LABEL_58;                                                                                                      | 
        }                                                                                                                     | 
        v96 = KeGetCurrentThread();                                                                                           | 
        --v96->KernelApcDisable;                                                                                              | 
        ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                       | 
        _InterlockedOr(v137, 0);                                                                                              | 
        v97 = Token;                                                                                                          | 
        v98 = *((_DWORD *)Token + 50);                                                                                        | 
        if ( v161 )                                                                                                           | 
        {                                                                                                                     | 
          v99 = v98 | 0x1000;                                                                                                 | 
        }                                                                                                                     | 
        else                                                                                                                  | 
        {                                                                                                                     | 
          v99 = v98 & 0xFFFFEFFF;                                                                                             | 
        }                                                                                                                     | 
        *((_DWORD *)Token + 50) = v99;                                                                                        | 
        v97[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                                    | 
        _InterlockedOr(v137, 0);                                                                                              | 
        v15 = (PERESOURCE *)Token;                                                                                            | 
        ExReleaseResourceLite(*((PERESOURCE *)Token + 6));                                                                    | 
        KeLeaveCriticalRegion();                                                                                              | 
      }                                                                                                                       | 
LABEL_55:                                                                                                                     | 
      v16 = 0;                                                                                                                | 
      goto LABEL_99;                                                                                                          | 
    }                                                                                                                         | 
    v100 = 0;                                                                                                                 | 
    if ( TokenInformationLength < 0x10 )                                                                                      | 
    {                                                                                                                         | 
      goto LABEL_30;                                                                                                          | 
    }                                                                                                                         | 
    v141 = &v166;                                                                                                             | 
    v140 = (__int64)&v163;                                                                                                    | 
    LODWORD(Object) = 0;                                                                                                      | 
    LOBYTE(v14) = PreviousMode;                                                                                               | 
    v17 = SeCaptureSidAndAttributesArray(TokenInformation, 1LL, v14);                                                         | 
    v143 = v17;                                                                                                               | 
    if ( v17 < 0 )                                                                                                            | 
    {                                                                                                                         | 
      goto LABEL_60;                                                                                                          | 
    }                                                                                                                         | 
    v101 = v163;                                                                                                              | 
    v102 = (unsigned __int16 *)*v163;                                                                                         | 
    v103 = *(_DWORD *)((char *)*v163 + 2) - *(_DWORD *)((char *)SeUntrustedMandatorySid + 2);                                 | 
    if ( !v103 )                                                                                                              | 
    {                                                                                                                         | 
      v103 = v102[3] - *((unsigned __int16 *)SeUntrustedMandatorySid + 3);                                                    | 
    }                                                                                                                         | 
    if ( v103 )                                                                                                               | 
    {                                                                                                                         | 
LABEL_198:                                                                                                                    | 
      ObfDereferenceObject(v15);                                                                                              | 
      LOBYTE(v104) = PreviousMode;                                                                                            | 
      SeReleaseLuidAndAttributesArray(v101, v104);                                                                            | 
      return -1073740730;                                                                                                     | 
    }                                                                                                                         | 
    v105 = *RtlSubAuthorityCountSid(*v163);                                                                                   | 
    if ( (_BYTE)v105 )                                                                                                        | 
    {                                                                                                                         | 
      v106 = *RtlSubAuthoritySid(v102, v105 - 1);                                                                             | 
      if ( v106 > 0x4000 )                                                                                                    | 
      {                                                                                                                       | 
        goto LABEL_198;                                                                                                       | 
      }                                                                                                                       | 
    }                                                                                                                         | 
    else                                                                                                                      | 
    {                                                                                                                         | 
      v106 = 0;                                                                                                               | 
    }                                                                                                                         | 
    v107 = KeGetCurrentThread();                                                                                              | 
    --v107->KernelApcDisable;                                                                                                 | 
    v171 = (PERESOURCE *)Token;                                                                                               | 
    ExAcquireResourceExclusiveLite(*((PERESOURCE *)Token + 6), 1u);                                                           | 
    _InterlockedOr(v137, 0);                                                                                                  | 
    v108 = Token;                                                                                                             | 
    TokenIntegrity = (PSID *)SepLocateTokenIntegrity(Token);                                                                  | 
    v110 = TokenIntegrity;                                                                                                    | 
    if ( !TokenIntegrity )                                                                                                    | 
    {                                                                                                                         | 
      v16 = -1073740730;                                                                                                      | 
LABEL_264:                                                                                                                    | 
      _InterlockedOr(v137, 0);                                                                                                | 
      ExReleaseResourceLite(v171[6]);                                                                                         | 
      KeLeaveCriticalRegion();                                                                                                | 
      ObfDereferenceObject(Token);                                                                                            | 
      LOBYTE(v136) = PreviousMode;                                                                                            | 
      SeReleaseLuidAndAttributesArray(v163, v136);                                                                            | 
      return v16;                                                                                                             | 
    }                                                                                                                         | 
    Sid = *TokenIntegrity;                                                                                                    | 
    v111 = *RtlSubAuthorityCountSid(Sid);                                                                                     | 
    LODWORD(v154) = v111;                                                                                                     | 
    v112 = v111;                                                                                                              | 
    if ( (_BYTE)v111 )                                                                                                        | 
    {                                                                                                                         | 
      v113 = *RtlSubAuthoritySid(Sid, v111 - 1);                                                                              | 
      LOBYTE(v111) = (_BYTE)v154;                                                                                             | 
    }                                                                                                                         | 
    else                                                                                                                      | 
    {                                                                                                                         | 
      v113 = 0;                                                                                                               | 
    }                                                                                                                         | 
    if ( v106 <= v113 )                                                                                                       | 
    {                                                                                                                         | 
      if ( (_BYTE)v111 )                                                                                                      | 
      {                                                                                                                       | 
        v114 = *RtlSubAuthoritySid(Sid, v112 - 1);                                                                            | 
      }                                                                                                                       | 
      else                                                                                                                    | 
      {                                                                                                                       | 
        v114 = 0;                                                                                                             | 
      }                                                                                                                       | 
      if ( v106 < v114 )                                                                                                      | 
      {                                                                                                                       | 
        if ( (unsigned int)Feature_871661881__private_IsEnabledDeviceUsageNoInline() )                                        | 
        {                                                                                                                     | 
          v150 = 0;                                                                                                           | 
          RtlQueryElevationFlags(&v150);                                                                                      | 
          if ( (v150 & 0x18) == 0x10 )                                                                                        | 
          {                                                                                                                   | 
            v115 = *v110;                                                                                                     | 
            v116 = *RtlSubAuthorityCountSid(*v110);                                                                           | 
            if ( (_BYTE)v116 )                                                                                                | 
            {                                                                                                                 | 
              if ( *RtlSubAuthoritySid(v115, v116 - 1) >= 0x3000 )                                                            | 
              {                                                                                                               | 
                memset(&SubjectContext, 0, sizeof(SubjectContext));                                                           | 
                SeCaptureSubjectContext(&SubjectContext);                                                                     | 
                v117 = KeGetCurrentThread();                                                                                  | 
                --v117->KernelApcDisable;                                                                                     | 
                PrimaryToken = (struct _ERESOURCE **)SubjectContext.PrimaryToken;                                             | 
                ExAcquireResourceSharedLite(*((PERESOURCE *)SubjectContext.PrimaryToken + 6), 1u);                            | 
                v119 = *(void **)SepLocateTokenIntegrity(PrimaryToken);                                                       | 
                v120 = *RtlSubAuthorityCountSid(v119);                                                                        | 
                if ( (_BYTE)v120 )                                                                                            | 
                {                                                                                                             | 
                  v121 = *RtlSubAuthoritySid(v119, v120 - 1);                                                                 | 
                }                                                                                                             | 
                else                                                                                                          | 
                {                                                                                                             | 
                  v121 = 0;                                                                                                   | 
                }                                                                                                             | 
                v122 = PrimaryToken[6];                                                                                       | 
                if ( v121 < 0x3000 )                                                                                          | 
                {                                                                                                             | 
                  ExReleaseResourceLite(v122);                                                                                | 
                  KeLeaveCriticalRegion();                                                                                    | 
                  SeReleaseSubjectContext(&SubjectContext);                                                                   | 
                  v16 = -1073741790;                                                                                          | 
                  goto LABEL_264;                                                                                             | 
                }                                                                                                             | 
                ExReleaseResourceLite(v122);                                                                                  | 
                KeLeaveCriticalRegion();                                                                                      | 
                SeReleaseSubjectContext(&SubjectContext);                                                                     | 
                v108 = Token;                                                                                                 | 
              }                                                                                                               | 
            }                                                                                                                 | 
          }                                                                                                                   | 
        }                                                                                                                     | 
        v108[50] &= ~0x1000u;                                                                                                 | 
      }                                                                                                                       | 
    }                                                                                                                         | 
    else                                                                                                                      | 
    {                                                                                                                         | 
      if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                            | 
      {                                                                                                                       | 
        v16 = -1073741727;                                                                                                    | 
        goto LABEL_264;                                                                                                       | 
      }                                                                                                                       | 
      if ( *((_BYTE *)v108 + 204) )                                                                                           | 
      {                                                                                                                       | 
        v16 = -1073741525;                                                                                                    | 
        goto LABEL_264;                                                                                                       | 
      }                                                                                                                       | 
    }                                                                                                                         | 
    if ( (unsigned int)Feature_DecoupleAppContainerAndIntegrityLevel__private_IsEnabledDeviceUsageNoInline() )                | 
    {                                                                                                                         | 
      v123 = *v110;                                                                                                           | 
      v124 = *RtlSubAuthorityCountSid(*v110);                                                                                 | 
      v125 = (_BYTE)v124 ? *RtlSubAuthoritySid(v123, v124 - 1) : 0;                                                           | 
      if ( v106 >= v125 )                                                                                                     | 
      {                                                                                                                       | 
        memset(&v172, 0, sizeof(v172));                                                                                       | 
        SeCaptureSubjectContext(&v172);                                                                                       | 
        SeLockSubjectContext(&v172);                                                                                          | 
        v126 = *((_DWORD *)v172.PrimaryToken + 50);                                                                           | 
        SeUnlockSubjectContext(&v172);                                                                                        | 
        SeReleaseSubjectContext(&v172);                                                                                       | 
        v100 = (v126 & 0x4000) == 0;                                                                                          | 
      }                                                                                                                       | 
    }                                                                                                                         | 
    v127 = *((unsigned __int8 *)*v110 + 1);                                                                                   | 
    if ( (_BYTE)v127 )                                                                                                        | 
    {                                                                                                                         | 
      *RtlSubAuthoritySid(*v110, v127 - 1) = v106;                                                                            | 
      if ( v106 < 0x3000 )                                                                                                    | 
      {                                                                                                                       | 
        *((_QWORD *)v108 + 9) &= 0xFFFFFFEEDFE9F97BuLL;                                                                       | 
        *((_QWORD *)v108 + 10) &= 0xFFFFFFEEDFE9F97BuLL;                                                                      | 
      }                                                                                                                       | 
      if ( v106 >= 0x2000 )                                                                                                   | 
      {                                                                                                                       | 
        IsEnabledDeviceUsageNoInline = Feature_DecoupleAppContainerAndIntegrityLevel__private_IsEnabledDeviceUsageNoInline(); | 
        v129 = v108[50] & 0x4000;                                                                                             | 
        if ( IsEnabledDeviceUsageNoInline )                                                                                   | 
        {                                                                                                                     | 
          if ( v129 && v100 )                                                                                                 | 
          {                                                                                                                   | 
            SepRemoveAceFromTokenDefaultDacl(v108, *((_QWORD *)v108 + 98));                                                   | 
            v108[50] &= ~0x4000u;                                                                                             | 
            v130 = (void *)*((_QWORD *)v108 + 98);                                                                            | 
            if ( v130 )                                                                                                       | 
            {                                                                                                                 | 
              ExFreePoolWithTag(v130, 0);                                                                                     | 
              *((_QWORD *)v108 + 98) = 0LL;                                                                                   | 
            }                                                                                                                 | 
            v131 = (void *)*((_QWORD *)v108 + 99);                                                                            | 
            if ( v131 )                                                                                                       | 
            {                                                                                                                 | 
              ExFreePoolWithTag(v131, 0);                                                                                     | 
              *((_QWORD *)v108 + 99) = 0LL;                                                                                   | 
              v108[200] = 0;                                                                                                  | 
              memset_0(v108 + 202, 0, 0x110uLL);                                                                              | 
            }                                                                                                                 | 
            if ( *((_QWORD *)v108 + 135) )                                                                                    | 
            {                                                                                                                 | 
              SepDereferenceLowBoxNumberEntry((unsigned int)v108[30]);                                                        | 
              *((_QWORD *)v108 + 135) = 0LL;                                                                                  | 
            }                                                                                                                 | 
            if ( *((_QWORD *)v108 + 136) )                                                                                    | 
            {                                                                                                                 | 
              SepDereferenceCachedHandlesEntryInternal(*((_QWORD *)v108 + 27) + 96LL);                                        | 
              *((_QWORD *)v108 + 136) = 0LL;                                                                                  | 
            }                                                                                                                 | 
          }                                                                                                                   | 
          v132 = *v110;                                                                                                       | 
          v133 = *RtlSubAuthorityCountSid(*v110);                                                                             | 
          if ( (_BYTE)v133 && *RtlSubAuthoritySid(v132, v133 - 1) >= 0x2000 )                                                 | 
          {                                                                                                                   | 
            v108[50] |= 0x2000u;                                                                                              | 
          }                                                                                                                   | 
        }                                                                                                                     | 
        else                                                                                                                  | 
        {                                                                                                                     | 
          if ( v129 )                                                                                                         | 
          {                                                                                                                   | 
            SepRemoveAceFromTokenDefaultDacl(v108, *((_QWORD *)v108 + 98));                                                   | 
            v108[50] &= ~0x4000u;                                                                                             | 
            v134 = (void *)*((_QWORD *)v108 + 98);                                                                            | 
            if ( v134 )                                                                                                       | 
            {                                                                                                                 | 
              ExFreePoolWithTag(v134, 0);                                                                                     | 
              *((_QWORD *)v108 + 98) = 0LL;                                                                                   | 
            }                                                                                                                 | 
            v135 = (void *)*((_QWORD *)v108 + 99);                                                                            | 
            if ( v135 )                                                                                                       | 
            {                                                                                                                 | 
              ExFreePoolWithTag(v135, 0);                                                                                     | 
              *((_QWORD *)v108 + 99) = 0LL;                                                                                   | 
              v108[200] = 0;                                                                                                  | 
              memset_0(v108 + 202, 0, 0x110uLL);                                                                              | 
            }                                                                                                                 | 
            if ( *((_QWORD *)v108 + 135) )                                                                                    | 
            {                                                                                                                 | 
              SepDereferenceLowBoxNumberEntry((unsigned int)v108[30]);                                                        | 
              *((_QWORD *)v108 + 135) = 0LL;                                                                                  | 
            }                                                                                                                 | 
            if ( *((_QWORD *)v108 + 136) )                                                                                    | 
            {                                                                                                                 | 
              SepDereferenceCachedHandlesEntryInternal(*((_QWORD *)v108 + 27) + 96LL);                                        | 
              *((_QWORD *)v108 + 136) = 0LL;                                                                                  | 
            }                                                                                                                 | 
          }                                                                                                                   | 
          v108[50] |= 0x2000u;                                                                                                | 
        }                                                                                                                     | 
      }                                                                                                                       | 
      else                                                                                                                    | 
      {                                                                                                                       | 
        *((_QWORD *)v108 + 9) &= 0x202800000uLL;                                                                              | 
        *((_QWORD *)v108 + 10) &= 0x202800000uLL;                                                                             | 
        v108[50] &= ~0x2000u;                                                                                                 | 
      }                                                                                                                       | 
      *((_QWORD *)v108 + 7) = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                       | 
      v16 = 0;                                                                                                                | 
    }                                                                                                                         | 
    else                                                                                                                      | 
    {                                                                                                                         | 
      v16 = -1073741811;                                                                                                      | 
    }                                                                                                                         | 
    goto LABEL_264;                                                                                                           | 
  }                                                                                                                           | 
  return result;                                                                                                              | 
}                                                                                                                             | 
1033 row(s)
