NTSTATUS __stdcall NtPrivilegedServiceAuditAlarm(                                                                | 
        PUNICODE_STRING SubsystemName,                                                                           | 
        PUNICODE_STRING ServiceName,                                                                             | 
        HANDLE ClientToken,                                                                                      | 
        PPRIVILEGE_SET Privileges,                                                                               | 
        BOOLEAN AccessGranted)                                                                                   | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  v26 = 0LL;                                                                                                     | 
  memset(&SubjectContext, 0, sizeof(SubjectContext));                                                            | 
  P = 0LL;                                                                                                       | 
  v29 = 0LL;                                                                                                     | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                             | 
  SeCaptureSubjectContext(&SubjectContext);                                                                      | 
  LOBYTE(v10) = PreviousMode;                                                                                    | 
  if ( !(unsigned __int8)SeCheckAuditPrivilege(&SubjectContext, v10) )                                           | 
  {                                                                                                              | 
    v22 = -1073741727;                                                                                           | 
LABEL_33:                                                                                                        | 
    SeReleaseSubjectContext(&SubjectContext);                                                                    | 
    return v22;                                                                                                  | 
  }                                                                                                              | 
  Object = 0LL;                                                                                                  | 
  v11 = ObReferenceObjectByHandle(ClientToken, 8u, (POBJECT_TYPE)SeTokenObjectType, PreviousMode, &Object, 0LL); | 
  v24 = v11;                                                                                                     | 
  if ( v11 < 0 )                                                                                                 | 
  {                                                                                                              | 
    SeReleaseSubjectContext(&SubjectContext);                                                                    | 
    if ( v11 == -1073741816 )                                                                                    | 
    {                                                                                                            | 
      return v11;                                                                                                | 
    }                                                                                                            | 
    v23 = (unsigned int)v11;                                                                                     | 
LABEL_38:                                                                                                        | 
    SepAuditFailed(v23);                                                                                         | 
    return v11;                                                                                                  | 
  }                                                                                                              | 
  v12 = Object;                                                                                                  | 
  if ( *((_DWORD *)Object + 48) == 2 && *((int *)Object + 49) < 1 )                                              | 
  {                                                                                                              | 
    ObfDereferenceObject(Object);                                                                                | 
    v22 = -1073741659;                                                                                           | 
    goto LABEL_33;                                                                                               | 
  }                                                                                                              | 
  if ( SubsystemName && (v11 = SepProbeAndCaptureString_U(SubsystemName, &P), v24 = v11, v11 < 0)                | 
    || ServiceName && (v11 = SepProbeAndCaptureString_U(ServiceName, &v29), v24 = v11, v11 < 0) )                | 
  {                                                                                                              | 
    v13 = v26;                                                                                                   | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    if ( ((unsigned __int8)Privileges & 3) != 0 )                                                                | 
    {                                                                                                            | 
      ExRaiseDatatypeMisalignment();                                                                             | 
    }                                                                                                            | 
    PrivilegeCount = Privileges->PrivilegeCount;                                                                 | 
    v28 = PrivilegeCount;                                                                                        | 
    if ( PrivilegeCount >= 0x43 )                                                                                | 
    {                                                                                                            | 
      v11 = -1073741811;                                                                                         | 
      v13 = v26;                                                                                                 | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v15 = 12 * PrivilegeCount + 8;                                                                             | 
      v27 = v15;                                                                                                 | 
      if ( 12 * PrivilegeCount != -8 )                                                                           | 
      {                                                                                                          | 
        v16 = (char *)Privileges + v15;                                                                          | 
        if ( (unsigned __int64)v16 > 0x7FFFFFFF0000LL || v16 < (char *)Privileges )                              | 
        {                                                                                                        | 
          v15 = v27;                                                                                             | 
          PrivilegeCount = v28;                                                                                  | 
        }                                                                                                        | 
      }                                                                                                          | 
      v17 = v15;                                                                                                 | 
      Pool2 = (_DWORD *)ExAllocatePool2(0x100uLL, v15, 0x72506553u);                                             | 
      v13 = Pool2;                                                                                               | 
      v26 = Pool2;                                                                                               | 
      if ( Pool2 )                                                                                               | 
      {                                                                                                          | 
        memmove(Pool2, Privileges, v17);                                                                         | 
        *v13 = PrivilegeCount;                                                                                   | 
        v11 = v24;                                                                                               | 
      }                                                                                                          | 
      else                                                                                                       | 
      {                                                                                                          | 
        v11 = -1073741670;                                                                                       | 
      }                                                                                                          | 
    }                                                                                                            | 
    v12 = Object;                                                                                                | 
  }                                                                                                              | 
  if ( v11 < 0 )                                                                                                 | 
  {                                                                                                              | 
    if ( P )                                                                                                     | 
    {                                                                                                            | 
      ExFreePoolWithTag(P, 0);                                                                                   | 
    }                                                                                                            | 
    if ( v29 )                                                                                                   | 
    {                                                                                                            | 
      ExFreePoolWithTag(v29, 0);                                                                                 | 
    }                                                                                                            | 
    if ( v13 )                                                                                                   | 
    {                                                                                                            | 
      ExFreePoolWithTag(v13, 0);                                                                                 | 
    }                                                                                                            | 
    SeReleaseSubjectContext(&SubjectContext);                                                                    | 
    ObfDereferenceObject(v12);                                                                                   | 
    if ( v11 != -1073741670 )                                                                                    | 
    {                                                                                                            | 
      return v11;                                                                                                | 
    }                                                                                                            | 
    v23 = 3221225626LL;                                                                                          | 
    goto LABEL_38;                                                                                               | 
  }                                                                                                              | 
  v19 = v29;                                                                                                     | 
  v20 = P;                                                                                                       | 
  SepAdtPrivilegedServiceAuditAlarm(                                                                             | 
    (unsigned int)&SubjectContext,                                                                               | 
    (_DWORD)P,                                                                                                   | 
    (_DWORD)v29,                                                                                                 | 
    (_DWORD)v12,                                                                                                 | 
    (__int64)SubjectContext.PrimaryToken,                                                                        | 
    (__int64)v13,                                                                                                | 
    AccessGranted);                                                                                              | 
  if ( v20 )                                                                                                     | 
  {                                                                                                              | 
    ExFreePoolWithTag(v20, 0);                                                                                   | 
  }                                                                                                              | 
  if ( v19 )                                                                                                     | 
  {                                                                                                              | 
    ExFreePoolWithTag(v19, 0);                                                                                   | 
  }                                                                                                              | 
  if ( v13 )                                                                                                     | 
  {                                                                                                              | 
    ExFreePoolWithTag(v13, 0);                                                                                   | 
  }                                                                                                              | 
  ObfDereferenceObject(v12);                                                                                     | 
  SeReleaseSubjectContext(&SubjectContext);                                                                      | 
  return 0;                                                                                                      | 
}                                                                                                                | 
142 row(s)
