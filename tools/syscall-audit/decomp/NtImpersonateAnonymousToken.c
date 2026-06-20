NTSTATUS __stdcall NtImpersonateAnonymousToken(HANDLE ThreadHandle)                                                       | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  *(_QWORD *)&v19 = 0LL;                                                                                                  | 
  DWORD2(v19) = 0;                                                                                                        | 
  v17 = 0LL;                                                                                                              | 
  v14 = 0LL;                                                                                                              | 
  v18 = 0LL;                                                                                                              | 
  v22 = 0LL;                                                                                                              | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                      | 
  v20 = 0;                                                                                                                | 
  memset(&SubjectContext, 0, sizeof(SubjectContext));                                                                     | 
  Token = 0LL;                                                                                                            | 
  Object = 0LL;                                                                                                           | 
  result = ObReferenceObjectByHandle(ThreadHandle, 0x100u, (POBJECT_TYPE)PsThreadType, PreviousMode, &Object, 0LL);       | 
  if ( result < 0 )                                                                                                       | 
  {                                                                                                                       | 
    return result;                                                                                                        | 
  }                                                                                                                       | 
  SeCaptureSubjectContext(&SubjectContext);                                                                               | 
  PrimaryToken = (const signed __int32 *)SubjectContext.PrimaryToken;                                                     | 
  ClientToken = SubjectContext.PrimaryToken;                                                                              | 
  if ( SubjectContext.ClientToken )                                                                                       | 
  {                                                                                                                       | 
    ClientToken = SubjectContext.ClientToken;                                                                             | 
  }                                                                                                                       | 
  if ( ((int)SepRegQueryDwordValue(v3, L"AnonymousAppContainerImpersonationLevelCheck", &v20) < 0 || !v20)                | 
    && *((_DWORD *)ClientToken + 48) != 1                                                                                 | 
    && _bittest(PrimaryToken + 50, 0xEu)                                                                                  | 
    && *((int *)ClientToken + 49) < 2 )                                                                                   | 
  {                                                                                                                       | 
    AnonymousToken = -1073741659;                                                                                         | 
    goto LABEL_13;                                                                                                        | 
  }                                                                                                                       | 
  if ( _bittest((const signed __int32 *)ClientToken + 50, 0xEu) )                                                         | 
  {                                                                                                                       | 
    AnonymousToken = SepGetAnonymousToken(ClientToken, &Token);                                                           | 
    if ( AnonymousToken >= 0 )                                                                                            | 
    {                                                                                                                     | 
      AnonymousToken = PsImpersonateClient((PETHREAD)Object, Token, 1u, 0, SecurityImpersonation);                        | 
      ObfDereferenceObject(Token);                                                                                        | 
    }                                                                                                                     | 
    goto LABEL_13;                                                                                                        | 
  }                                                                                                                       | 
  if ( (int)SepRegQueryDwordValue(v6, L"EveryoneIncludesAnonymous", &v20) < 0 || (v8 = SeAnonymousLogonToken, v20 != 1) ) | 
  {                                                                                                                       | 
    v8 = SeAnonymousLogonTokenNoEveryone;                                                                                 | 
  }                                                                                                                       | 
  if ( !(unsigned __int8)PsIsCurrentThreadInServerSilo() )                                                                | 
  {                                                                                                                       | 
    if ( (*((_DWORD *)ClientToken + 50) & 0x380000) != 0 )                                                                | 
    {                                                                                                                     | 
      LODWORD(v17) = 48;                                                                                                  | 
      LOBYTE(v9) = 1;                                                                                                     | 
      *((_QWORD *)&v17 + 1) = 0LL;                                                                                        | 
      DWORD2(v18) = 0;                                                                                                    | 
      *(_QWORD *)&v18 = 0LL;                                                                                              | 
      v19 = 0LL;                                                                                                          | 
      AnonymousToken = SepDuplicateToken((_DWORD)v8, (unsigned int)&v17, v9, 2, 2, 0, 1, (__int64)&v14);                  | 
      if ( AnonymousToken < 0 )                                                                                           | 
      {                                                                                                                   | 
        goto LABEL_13;                                                                                                    | 
      }                                                                                                                   | 
      *((_DWORD *)v14 + 50) |= *((_DWORD *)ClientToken + 50) & 0x380000;                                                  | 
      v8 = v14;                                                                                                           | 
    }                                                                                                                     | 
LABEL_22:                                                                                                                 | 
    AnonymousToken = ObReferenceObjectByPointer(                                                                          | 
                       v8,                                                                                                | 
                       4u,                                                                                                | 
                       (POBJECT_TYPE)SeTokenObjectType,                                                                   | 
                       KeGetCurrentThread()->PreviousMode);                                                               | 
    if ( AnonymousToken >= 0 )                                                                                            | 
    {                                                                                                                     | 
      ObfDereferenceObject(v8);                                                                                           | 
      Process = KeGetCurrentThread()->ApcState.Process;                                                                   | 
      v12 = (void *)PsReferencePrimaryTokenWithTag(Process, 1953261124LL);                                                | 
      v13 = v12;                                                                                                          | 
      if ( v12 )                                                                                                          | 
      {                                                                                                                   | 
        if ( !SeTokenIsRestricted(v12) || SeTokenIsWriteRestricted(v13) )                                                 | 
        {                                                                                                                 | 
          ObFastDereferenceObject(&Process[1].ActiveProcessors, v13, 1953261124LL);                                       | 
          AnonymousToken = PsImpersonateClient((PETHREAD)Object, v8, v22 == 0LL, 0, SecurityImpersonation);               | 
        }                                                                                                                 | 
        else                                                                                                              | 
        {                                                                                                                 | 
          ObfDereferenceObject(v13);                                                                                      | 
          AnonymousToken = -1073741790;                                                                                   | 
        }                                                                                                                 | 
      }                                                                                                                   | 
      else                                                                                                                | 
      {                                                                                                                   | 
        AnonymousToken = -1073741823;                                                                                     | 
      }                                                                                                                   | 
    }                                                                                                                     | 
    goto LABEL_13;                                                                                                        | 
  }                                                                                                                       | 
  CurrentServerSilo = PsGetCurrentServerSilo();                                                                           | 
  AnonymousToken = SepCopyAnonymousTokenAndSetSilo(CurrentServerSilo, &v22);                                              | 
  if ( AnonymousToken >= 0 )                                                                                              | 
  {                                                                                                                       | 
    v8 = v22;                                                                                                             | 
    goto LABEL_22;                                                                                                        | 
  }                                                                                                                       | 
LABEL_13:                                                                                                                 | 
  SeReleaseSubjectContext(&SubjectContext);                                                                               | 
  if ( Object )                                                                                                           | 
  {                                                                                                                       | 
    ObfDereferenceObject(Object);                                                                                         | 
  }                                                                                                                       | 
  if ( v22 )                                                                                                              | 
  {                                                                                                                       | 
    ObfDereferenceObject(v22);                                                                                            | 
  }                                                                                                                       | 
  if ( v14 )                                                                                                              | 
  {                                                                                                                       | 
    ObfDereferenceObject(v14);                                                                                            | 
  }                                                                                                                       | 
  return AnonymousToken;                                                                                                  | 
}                                                                                                                         | 
122 row(s)
