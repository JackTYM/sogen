NTSTATUS __stdcall NtDeleteObjectAuditAlarm(PUNICODE_STRING SubsystemName, PVOID HandleId, BOOLEAN GenerateOnClose) | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  v3 = (int)HandleId;                                                                                               | 
  memset(&SubjectContext, 0, sizeof(SubjectContext));                                                               | 
  P = 0LL;                                                                                                          | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                | 
  if ( !GenerateOnClose )                                                                                           | 
  {                                                                                                                 | 
    return 0;                                                                                                       | 
  }                                                                                                                 | 
  SeCaptureSubjectContext(&SubjectContext);                                                                         | 
  if ( SubjectContext.PrimaryToken )                                                                                | 
  {                                                                                                                 | 
    LOBYTE(v7) = PreviousMode;                                                                                      | 
    if ( (unsigned __int8)SeCheckAuditPrivilege(&SubjectContext, v7) )                                              | 
    {                                                                                                               | 
      v8 = SepProbeAndCaptureString_U(SubsystemName, &P);                                                           | 
      if ( v8 >= 0 )                                                                                                | 
      {                                                                                                             | 
        SepAdtDeleteObjectAuditAlarm((_DWORD)P, v3, (unsigned int)&SubjectContext, 0, 0LL, 0);                      | 
      }                                                                                                             | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      v8 = -1073741727;                                                                                             | 
    }                                                                                                               | 
    SeReleaseSubjectContext(&SubjectContext);                                                                       | 
    if ( P )                                                                                                        | 
    {                                                                                                               | 
      ExFreePoolWithTag(P, 0);                                                                                      | 
    }                                                                                                               | 
    v9 = 3221225626LL;                                                                                              | 
    if ( v8 != -1073741670 )                                                                                        | 
    {                                                                                                               | 
      return v8;                                                                                                    | 
    }                                                                                                               | 
  }                                                                                                                 | 
  else                                                                                                              | 
  {                                                                                                                 | 
    v8 = -1073741700;                                                                                               | 
    v9 = 3221225596LL;                                                                                              | 
  }                                                                                                                 | 
  SepAuditFailed(v9);                                                                                               | 
  return v8;                                                                                                        | 
}                                                                                                                   | 
47 row(s)
