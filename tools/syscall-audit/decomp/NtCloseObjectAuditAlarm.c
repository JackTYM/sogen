NTSTATUS __stdcall NtCloseObjectAuditAlarm(PUNICODE_STRING SubsystemName, PVOID HandleId, BOOLEAN GenerateOnClose) | 
{                                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                    | 
                                                                                                                   | 
  v3 = (int)HandleId;                                                                                              | 
  memset(&SubjectContext, 0, sizeof(SubjectContext));                                                              | 
  P = 0LL;                                                                                                         | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                               | 
  if ( !GenerateOnClose )                                                                                          | 
  {                                                                                                                | 
    return 0;                                                                                                      | 
  }                                                                                                                | 
  SeCaptureSubjectContext(&SubjectContext);                                                                        | 
  LOBYTE(v7) = PreviousMode;                                                                                       | 
  if ( (unsigned __int8)SeCheckAuditPrivilege(&SubjectContext, v7) )                                               | 
  {                                                                                                                | 
    v8 = SepProbeAndCaptureString_U(SubsystemName, &P);                                                            | 
    if ( v8 >= 0 )                                                                                                 | 
    {                                                                                                              | 
      SepAdtCloseObjectAuditAlarm((_DWORD)P, v3, (unsigned int)&SubjectContext, 0, 0);                             | 
    }                                                                                                              | 
  }                                                                                                                | 
  else                                                                                                             | 
  {                                                                                                                | 
    v8 = -1073741727;                                                                                              | 
  }                                                                                                                | 
  if ( P )                                                                                                         | 
  {                                                                                                                | 
    ExFreePoolWithTag(P, 0);                                                                                       | 
  }                                                                                                                | 
  SeReleaseSubjectContext(&SubjectContext);                                                                        | 
  if ( v8 == -1073741670 )                                                                                         | 
  {                                                                                                                | 
    SepAuditFailed();                                                                                              | 
  }                                                                                                                | 
  return v8;                                                                                                       | 
}                                                                                                                  | 
37 row(s)
