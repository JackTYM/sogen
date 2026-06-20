NTSTATUS __stdcall NtDuplicateToken(
        HANDLE ExistingTokenHandle,
        ACCESS_MASK DesiredAccess,
        POBJECT_ATTRIBUTES ObjectAttributes,
        BOOLEAN EffectiveOnly,
        TOKEN_TYPE TokenType,
        PHANDLE NewTokenHandle)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v28 = 0LL;
  v29 = 0;
  WORD1(TokenInformation) = 0;
  v27 = 0LL;
  HandleInformation = 0LL;
  memset(&SubjectContext, 0, sizeof(SubjectContext));
  BYTE1(TokenInformation) = 0;
  v31.ClientToken = 0LL;
  *(_QWORD *)&v31.ImpersonationLevel = 0LL;
  v31.ProcessAuditId = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    v11 = TokenType;
    if ( (unsigned int)(TokenType - 1) > 1 )
    {
      return -1073741811;
    }
    v19 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)NewTokenHandle < 0x7FFFFFFF0000LL )
    {
      v19 = (__int64)NewTokenHandle;
    }
    *(_QWORD *)v19 = *(_QWORD *)v19;
  }
  else
  {
    v11 = TokenType;
  }
  result = SeCaptureSecurityQos(ObjectAttributes, PreviousMode, (char *)&TokenInformation + 2, &v28);
  if ( result >= 0 )
  {
    result = SeCaptureObjectAttributeSecurityDescriptorPresent(
               ObjectAttributes,
               PreviousMode,
               (char *)&TokenInformation + 3);
    if ( result >= 0 )
    {
      Token = 0LL;
      result = ObReferenceObjectByHandle(
                 ExistingTokenHandle,
                 2u,
                 (POBJECT_TYPE)SeTokenObjectType,
                 PreviousMode,
                 &Token,
                 &HandleInformation);
      if ( result >= 0 )
      {
        if ( DesiredAccess )
        {
          SeCaptureSubjectContext(&SubjectContext);
          v31.PrimaryToken = SubjectContext.PrimaryToken;
          v13 = 0;
          LOBYTE(TokenInformation) = 0;
          if ( !PreviousMode )
          {
            goto LABEL_13;
          }
          ClientToken = SubjectContext.ClientToken;
          if ( !SubjectContext.ClientToken )
          {
            ClientToken = SubjectContext.PrimaryToken;
          }
          if ( SeQueryInformationToken(ClientToken, MaxTokenInfoClass, &TokenInformation) >= 0 )
          {
            v13 = (_BYTE)TokenInformation == 0;
          }
          if ( !v13 && (unsigned __int8)RtlIsSandboxedToken(&v31) )
          {
            CurrentThread = KeGetCurrentThread();
            --CurrentThread->KernelApcDisable;
            PrimaryToken = (PERESOURCE *)SubjectContext.PrimaryToken;
            ExAcquireResourceSharedLite(*((PERESOURCE *)SubjectContext.PrimaryToken + 6), 1u);
            v22 = SepNewTokenAsRestrictedAsProcessToken(Token, PrimaryToken);
            if ( Token == PrimaryToken || v22 >= 0 && BYTE1(TokenInformation) )
            {
              HandleInformation.HandleAttributes = DesiredAccess;
            }
            else
            {
              HandleInformation.HandleAttributes = DesiredAccess & (HandleInformation.GrantedAccess | 0x2001F);
            }
            ExReleaseResourceLite(PrimaryToken[6]);
            KeLeaveCriticalRegionThread(KeGetCurrentThread());
          }
          else
          {
LABEL_13:
            HandleInformation.HandleAttributes = DesiredAccess;
          }
          SeReleaseSubjectContext(&SubjectContext);
        }
        else
        {
          HandleInformation.HandleAttributes = HandleInformation.GrantedAccess;
        }
        v15 = Token;
        if ( BYTE2(TokenInformation) )
        {
          v16 = HIDWORD(v28);
        }
        else
        {
          v16 = *((_DWORD *)Token + 49);
        }
        if ( *((_DWORD *)Token + 48) == 2
          && (v11 == TokenImpersonation && v16 > *((_DWORD *)Token + 49)
           || v11 == TokenPrimary && *((int *)Token + 49) < 2) )
        {
          ObfDereferenceObject(Token);
          return -1073741659;
        }
        else
        {
          v26 = 0LL;
          inserted = SepDuplicateToken(
                       (_DWORD)Token,
                       (_DWORD)ObjectAttributes,
                       EffectiveOnly,
                       v11,
                       v16,
                       PreviousMode,
                       0,
                       (__int64)&v26);
          if ( inserted >= 0 )
          {
            inserted = ObInsertObjectEx(v26, 0, 0LL, (__int64)&v27);
            if ( inserted >= 0 )
            {
              if ( !BYTE3(TokenInformation) )
              {
                v18 = v26;
                if ( (int)SepAppendAceToTokenObjectAcl((ULONG_PTR)v26) >= 0 )
                {
                  SepSetProcessTrustLabelAceForToken(v18);
                }
              }
              ObfDereferenceObject(v26);
            }
          }
          ObfDereferenceObject(v15);
          if ( inserted >= 0 )
          {
            *NewTokenHandle = (HANDLE)v27;
          }
          return inserted;
        }
      }
    }
  }
  return result;
}
