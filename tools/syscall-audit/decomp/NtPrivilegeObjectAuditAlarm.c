NTSTATUS __stdcall NtPrivilegeObjectAuditAlarm(                                                                  | 
        PUNICODE_STRING SubsystemName,                                                                           | 
        PVOID HandleId,                                                                                          | 
        HANDLE ClientToken,                                                                                      | 
        ACCESS_MASK DesiredAccess,                                                                               | 
        PPRIVILEGE_SET Privileges,                                                                               | 
        BOOLEAN AccessGranted)                                                                                   | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  v8 = (int)HandleId;                                                                                            | 
  P = 0LL;                                                                                                       | 
  v25 = 0LL;                                                                                                     | 
  memset(&SubjectContext, 0, sizeof(SubjectContext));                                                            | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                             | 
  SeCaptureSubjectContext(&SubjectContext);                                                                      | 
  LOBYTE(v11) = PreviousMode;                                                                                    | 
  if ( !(unsigned __int8)SeCheckAuditPrivilege(&SubjectContext, v11) )                                           | 
  {                                                                                                              | 
    v12 = -1073741727;                                                                                           | 
LABEL_27:                                                                                                        | 
    SeReleaseSubjectContext(&SubjectContext);                                                                    | 
    return v12;                                                                                                  | 
  }                                                                                                              | 
  Object = 0LL;                                                                                                  | 
  v12 = ObReferenceObjectByHandle(ClientToken, 8u, (POBJECT_TYPE)SeTokenObjectType, PreviousMode, &Object, 0LL); | 
  if ( v12 < 0 )                                                                                                 | 
  {                                                                                                              | 
    SeReleaseSubjectContext(&SubjectContext);                                                                    | 
    if ( v12 == -1073741816 )                                                                                    | 
    {                                                                                                            | 
      return v12;                                                                                                | 
    }                                                                                                            | 
    v22 = (unsigned int)v12;                                                                                     | 
LABEL_33:                                                                                                        | 
    SepAuditFailed(v22);                                                                                         | 
    return v12;                                                                                                  | 
  }                                                                                                              | 
  v14 = Object;                                                                                                  | 
  if ( *((_DWORD *)Object + 48) == 2 && *((int *)Object + 49) < 1 )                                              | 
  {                                                                                                              | 
    ObfDereferenceObject(Object);                                                                                | 
    v12 = -1073741659;                                                                                           | 
    goto LABEL_27;                                                                                               | 
  }                                                                                                              | 
  v12 = SepProbeAndCaptureString_U(SubsystemName, &P);                                                           | 
  v23 = v12;                                                                                                     | 
  if ( v12 < 0 )                                                                                                 | 
  {                                                                                                              | 
    goto LABEL_17;                                                                                               | 
  }                                                                                                              | 
  if ( ((unsigned __int8)Privileges & 3) != 0 )                                                                  | 
  {                                                                                                              | 
    ExRaiseDatatypeMisalignment();                                                                               | 
  }                                                                                                              | 
  PrivilegeCount = Privileges->PrivilegeCount;                                                                   | 
  if ( Privileges->PrivilegeCount >= 0x43 )                                                                      | 
  {                                                                                                              | 
    v12 = -1073741811;                                                                                           | 
    v14 = Object;                                                                                                | 
LABEL_17:                                                                                                        | 
    v20 = v25;                                                                                                   | 
    goto LABEL_20;                                                                                               | 
  }                                                                                                              | 
  v16 = 12 * PrivilegeCount + 8;                                                                                 | 
  if ( 12 * PrivilegeCount != -8 )                                                                               | 
  {                                                                                                              | 
    v17 = (char *)Privileges + v16;                                                                              | 
    if ( (unsigned __int64)v17 > 0x7FFFFFFF0000LL || v17 < (char *)Privileges )                                  | 
    {                                                                                                            | 
      v16 = 12 * PrivilegeCount + 8;                                                                             | 
      PrivilegeCount = Privileges->PrivilegeCount;                                                               | 
    }                                                                                                            | 
  }                                                                                                              | 
  v18 = v16;                                                                                                     | 
  Pool2 = (ULONG *)ExAllocatePool2(0x100uLL, v16, 0x72506553u);                                                  | 
  v20 = Pool2;                                                                                                   | 
  v25 = Pool2;                                                                                                   | 
  if ( Pool2 )                                                                                                   | 
  {                                                                                                              | 
    memmove(Pool2, Privileges, v18);                                                                             | 
    *v20 = PrivilegeCount;                                                                                       | 
    v12 = v23;                                                                                                   | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    v12 = -1073741670;                                                                                           | 
  }                                                                                                              | 
  v14 = Object;                                                                                                  | 
LABEL_20:                                                                                                        | 
  if ( v12 < 0 )                                                                                                 | 
  {                                                                                                              | 
    if ( v20 )                                                                                                   | 
    {                                                                                                            | 
      ExFreePoolWithTag(v20, 0);                                                                                 | 
    }                                                                                                            | 
    if ( P )                                                                                                     | 
    {                                                                                                            | 
      ExFreePoolWithTag(P, 0);                                                                                   | 
    }                                                                                                            | 
    SeReleaseSubjectContext(&SubjectContext);                                                                    | 
    ObfDereferenceObject(v14);                                                                                   | 
    if ( v12 != -1073741670 )                                                                                    | 
    {                                                                                                            | 
      return v12;                                                                                                | 
    }                                                                                                            | 
    v22 = 3221225626LL;                                                                                          | 
    goto LABEL_33;                                                                                               | 
  }                                                                                                              | 
  v21 = P;                                                                                                       | 
  SepAdtPrivilegeObjectAuditAlarm(                                                                               | 
    (_DWORD)P,                                                                                                   | 
    0,                                                                                                           | 
    0,                                                                                                           | 
    v8,                                                                                                          | 
    (__int64)v14,                                                                                                | 
    (__int64)SubjectContext.PrimaryToken,                                                                        | 
    (__int64)SubjectContext.ProcessAuditId,                                                                      | 
    DesiredAccess,                                                                                               | 
    (__int64)v20,                                                                                                | 
    AccessGranted);                                                                                              | 
  if ( v20 )                                                                                                     | 
  {                                                                                                              | 
    ExFreePoolWithTag(v20, 0);                                                                                   | 
  }                                                                                                              | 
  if ( v21 )                                                                                                     | 
  {                                                                                                              | 
    ExFreePoolWithTag(v21, 0);                                                                                   | 
  }                                                                                                              | 
  SeReleaseSubjectContext(&SubjectContext);                                                                      | 
  ObfDereferenceObject(v14);                                                                                     | 
  return 0;                                                                                                      | 
}                                                                                                                | 
133 row(s)
