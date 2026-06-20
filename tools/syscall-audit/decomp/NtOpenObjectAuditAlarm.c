NTSTATUS __stdcall NtOpenObjectAuditAlarm(                                                                                | 
        PUNICODE_STRING SubsystemName,                                                                                    | 
        PVOID HandleId,                                                                                                   | 
        PUNICODE_STRING ObjectTypeName,                                                                                   | 
        PUNICODE_STRING ObjectName,                                                                                       | 
        PSECURITY_DESCRIPTOR SecurityDescriptor,                                                                          | 
        HANDLE ClientToken,                                                                                               | 
        ACCESS_MASK DesiredAccess,                                                                                        | 
        ACCESS_MASK GrantedAccess,                                                                                        | 
        PPRIVILEGE_SET Privileges,                                                                                        | 
        BOOLEAN ObjectCreation,                                                                                           | 
        BOOLEAN AccessGranted,                                                                                            | 
        PBOOLEAN GenerateOnClose)                                                                                         | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  v12 = (__int64 *)HandleId;                                                                                              | 
  v55 = (__int64)HandleId;                                                                                                | 
  v52 = 0LL;                                                                                                              | 
  v51 = 0LL;                                                                                                              | 
  v53 = 0LL;                                                                                                              | 
  P = 0LL;                                                                                                                | 
  v54 = 0LL;                                                                                                              | 
  v44 = 0;                                                                                                                | 
  memset(&SubjectContext, 0, sizeof(SubjectContext));                                                                     | 
  GenerateAudit = 0;                                                                                                      | 
  GenerateAlarm[0] = 0;                                                                                                   | 
  v58 = 0LL;                                                                                                              | 
  v48 = 0;                                                                                                                | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                      | 
  GenerateAlarm[1] = PreviousMode;                                                                                        | 
  SeCaptureSubjectContext(&SubjectContext);                                                                               | 
  LOBYTE(v14) = PreviousMode;                                                                                             | 
  if ( !(unsigned __int8)SeCheckAuditPrivilege(&SubjectContext, v14) )                                                    | 
  {                                                                                                                       | 
    v35 = -1073741727;                                                                                                    | 
    goto LABEL_49;                                                                                                        | 
  }                                                                                                                       | 
  Token = 0LL;                                                                                                            | 
  v16 = ObReferenceObjectByHandle(ClientToken, 8u, (POBJECT_TYPE)SeTokenObjectType, PreviousMode, &Token, 0LL);           | 
  if ( v16 < 0 )                                                                                                          | 
  {                                                                                                                       | 
    SeReleaseSubjectContext(&SubjectContext);                                                                             | 
    if ( v16 == -1073741816 )                                                                                             | 
    {                                                                                                                     | 
      return v16;                                                                                                         | 
    }                                                                                                                     | 
    v36 = (unsigned int)v16;                                                                                              | 
    goto LABEL_60;                                                                                                        | 
  }                                                                                                                       | 
  v17 = Token;                                                                                                            | 
  if ( *((_DWORD *)Token + 48) == 2 && *((int *)Token + 49) < 1 )                                                         | 
  {                                                                                                                       | 
    ObfDereferenceObject(Token);                                                                                          | 
    v35 = -1073741659;                                                                                                    | 
LABEL_49:                                                                                                                 | 
    SeReleaseSubjectContext(&SubjectContext);                                                                             | 
    return v35;                                                                                                           | 
  }                                                                                                                       | 
  if ( !SecurityDescriptor )                                                                                              | 
  {                                                                                                                       | 
    ObfDereferenceObject(Token);                                                                                          | 
    v35 = -1073741703;                                                                                                    | 
    goto LABEL_49;                                                                                                        | 
  }                                                                                                                       | 
  LOBYTE(v15) = PreviousMode;                                                                                             | 
  v19 = SeCaptureSecurityDescriptor((_DWORD)SecurityDescriptor, v15, 1, 0, (__int64)&P);                                  | 
  v47 = v19;                                                                                                              | 
  if ( v19 >= 0 && P )                                                                                                    | 
  {                                                                                                                       | 
    if ( AccessGranted && Privileges )                                                                                    | 
    {                                                                                                                     | 
      if ( ((unsigned __int8)Privileges & 3) != 0 )                                                                       | 
      {                                                                                                                   | 
        goto LABEL_19;                                                                                                    | 
      }                                                                                                                   | 
      v22 = 0x7FFFFFFF0000LL;                                                                                             | 
      PrivilegeCount = Privileges->PrivilegeCount;                                                                        | 
      v57 = PrivilegeCount;                                                                                               | 
      if ( PrivilegeCount >= 0x43 )                                                                                       | 
      {                                                                                                                   | 
        v16 = -1073741811;                                                                                                | 
        v47 = -1073741811;                                                                                                | 
        v29 = GenerateOnClose;                                                                                            | 
LABEL_30:                                                                                                                 | 
        if ( v16 >= 0 )                                                                                                   | 
        {                                                                                                                 | 
          LOBYTE(v21) = AccessGranted == 0;                                                                               | 
          LOBYTE(v20) = AccessGranted;                                                                                    | 
          v31 = v51;                                                                                                      | 
          v32 = SepAdtAuditObjectAccessWithContext(0, (_DWORD)v51, v20, v21, (__int64)&SubjectContext, 0, (__int64)&v48); | 
          v33 = P;                                                                                                        | 
          if ( !v32 )                                                                                                     | 
          {                                                                                                               | 
LABEL_32:                                                                                                                 | 
            if ( GenerateAudit || GenerateAlarm[0] || !Privileges || !AccessGranted )                                     | 
            {                                                                                                             | 
              v34 = v44;                                                                                                  | 
            }                                                                                                             | 
            else                                                                                                          | 
            {                                                                                                             | 
              v31 = v51;                                                                                                  | 
              SepAdtPrivilegeObjectAuditAlarm(                                                                            | 
                (_DWORD)v52,                                                                                              | 
                (_DWORD)v51,                                                                                              | 
                (_DWORD)v53,                                                                                              | 
                v58,                                                                                                      | 
                (__int64)Token,                                                                                           | 
                (__int64)SubjectContext.PrimaryToken,                                                                     | 
                (__int64)KeGetCurrentThread()->ApcState.Process[1].Header.WaitListHead.Flink,                             | 
                DesiredAccess,                                                                                            | 
                (__int64)v54,                                                                                             | 
                AccessGranted);                                                                                           | 
              v34 = 0;                                                                                                    | 
              v33 = P;                                                                                                    | 
            }                                                                                                             | 
            if ( PreviousMode == 1 )                                                                                      | 
            {                                                                                                             | 
              ExFreePoolWithTag(v33, 0);                                                                                  | 
            }                                                                                                             | 
            if ( v52 )                                                                                                    | 
            {                                                                                                             | 
              ExFreePoolWithTag(v52, 0);                                                                                  | 
            }                                                                                                             | 
            if ( v31 )                                                                                                    | 
            {                                                                                                             | 
              ExFreePoolWithTag(v31, 0);                                                                                  | 
            }                                                                                                             | 
            if ( v53 )                                                                                                    | 
            {                                                                                                             | 
              ExFreePoolWithTag(v53, 0);                                                                                  | 
            }                                                                                                             | 
            if ( v54 )                                                                                                    | 
            {                                                                                                             | 
              ExFreePoolWithTag(v54, 0);                                                                                  | 
            }                                                                                                             | 
            ObfDereferenceObject(Token);                                                                                  | 
            SeReleaseSubjectContext(&SubjectContext);                                                                     | 
            *v29 = v34;                                                                                                   | 
            return 0;                                                                                                     | 
          }                                                                                                               | 
          v37 = *((_WORD *)P + 1);                                                                                        | 
          if ( (v37 & 0x10) == 0 )                                                                                        | 
          {                                                                                                               | 
            goto LABEL_67;                                                                                                | 
          }                                                                                                               | 
          if ( v37 >= 0 )                                                                                                 | 
          {                                                                                                               | 
            v38 = (ACL *)*((_QWORD *)P + 3);                                                                              | 
            goto LABEL_68;                                                                                                | 
          }                                                                                                               | 
          v40 = *((unsigned int *)P + 3);                                                                                 | 
          if ( (_DWORD)v40 )                                                                                              | 
          {                                                                                                               | 
            v38 = (ACL *)((char *)P + v40);                                                                               | 
          }                                                                                                               | 
          else                                                                                                            | 
          {                                                                                                               | 
LABEL_67:                                                                                                                 | 
            v38 = 0LL;                                                                                                    | 
          }                                                                                                               | 
          if ( (v37 & 0x10) == 0 )                                                                                        | 
          {                                                                                                               | 
            goto LABEL_69;                                                                                                | 
          }                                                                                                               | 
LABEL_68:                                                                                                                 | 
          if ( v37 >= 0 )                                                                                                 | 
          {                                                                                                               | 
            v39 = (ACL *)*((_QWORD *)P + 3);                                                                              | 
            goto LABEL_70;                                                                                                | 
          }                                                                                                               | 
          v41 = *((unsigned int *)P + 3);                                                                                 | 
          if ( (_DWORD)v41 )                                                                                              | 
          {                                                                                                               | 
            v39 = (ACL *)((char *)P + v41);                                                                               | 
          }                                                                                                               | 
          else                                                                                                            | 
          {                                                                                                               | 
LABEL_69:                                                                                                                 | 
            v39 = 0LL;                                                                                                    | 
          }                                                                                                               | 
LABEL_70:                                                                                                                 | 
          SeExamineSacl(v39, v38, Token, GrantedAccess | DesiredAccess, AccessGranted, &GenerateAudit, GenerateAlarm);    | 
          if ( GenerateAudit || GenerateAlarm[0] )                                                                        | 
          {                                                                                                               | 
            Process = KeGetCurrentThread()->ApcState.Process;                                                             | 
            v43 = v55 != 0;                                                                                               | 
            v55 = -v55;                                                                                                   | 
            v33 = P;                                                                                                      | 
            v31 = v51;                                                                                                    | 
            v44 = SepAdtOpenObjectAuditAlarm(                                                                             | 
                    v48,                                                                                                  | 
                    (int)v52,                                                                                             | 
                    (unsigned __int64)&v58 & -(__int64)v43,                                                               | 
                    (int)v51,                                                                                             | 
                    (__int64)v53,                                                                                         | 
                    P,                                                                                                    | 
                    (__int64)Token,                                                                                       | 
                    (__int64)SubjectContext.PrimaryToken,                                                                 | 
                    DesiredAccess,                                                                                        | 
                    GrantedAccess,                                                                                        | 
                    (__int64)v54,                                                                                         | 
                    AccessGranted,                                                                                        | 
                    (__int64)Process[1].Header.WaitListHead.Flink,                                                        | 
                    2,                                                                                                    | 
                    0LL,                                                                                                  | 
                    0,                                                                                                    | 
                    0LL,                                                                                                  | 
                    0LL,                                                                                                  | 
                    0LL);                                                                                                 | 
          }                                                                                                               | 
          goto LABEL_32;                                                                                                  | 
        }                                                                                                                 | 
        if ( v52 )                                                                                                        | 
        {                                                                                                                 | 
          ExFreePoolWithTag(v52, 0);                                                                                      | 
        }                                                                                                                 | 
        if ( v51 )                                                                                                        | 
        {                                                                                                                 | 
          ExFreePoolWithTag(v51, 0);                                                                                      | 
        }                                                                                                                 | 
        if ( v53 )                                                                                                        | 
        {                                                                                                                 | 
          ExFreePoolWithTag(v53, 0);                                                                                      | 
        }                                                                                                                 | 
        if ( v54 )                                                                                                        | 
        {                                                                                                                 | 
          ExFreePoolWithTag(v54, 0);                                                                                      | 
        }                                                                                                                 | 
        LOBYTE(v18) = PreviousMode;                                                                                       | 
        SeReleaseSecurityDescriptor(P, v18, 0LL);                                                                         | 
        ObfDereferenceObject(Token);                                                                                      | 
        SeReleaseSubjectContext(&SubjectContext);                                                                         | 
        if ( v16 != -1073741670 )                                                                                         | 
        {                                                                                                                 | 
          return v16;                                                                                                     | 
        }                                                                                                                 | 
        v36 = 3221225626LL;                                                                                               | 
LABEL_60:                                                                                                                 | 
        SepAuditFailed(v36);                                                                                              | 
        return v16;                                                                                                       | 
      }                                                                                                                   | 
      v24 = 12 * PrivilegeCount + 8;                                                                                      | 
      v56 = v24;                                                                                                          | 
      if ( 12 * PrivilegeCount != -8 )                                                                                    | 
      {                                                                                                                   | 
        v25 = (char *)Privileges + v24;                                                                                   | 
        if ( (unsigned __int64)v25 > 0x7FFFFFFF0000LL || v25 < (char *)Privileges )                                       | 
        {                                                                                                                 | 
          v24 = v56;                                                                                                      | 
          PrivilegeCount = v57;                                                                                           | 
        }                                                                                                                 | 
      }                                                                                                                   | 
      Size = v24;                                                                                                         | 
      Pool2 = (ULONG *)ExAllocatePool2(0x100uLL, v24, 0x72506553u);                                                       | 
      v28 = Pool2;                                                                                                        | 
      v54 = Pool2;                                                                                                        | 
      if ( !Pool2 )                                                                                                       | 
      {                                                                                                                   | 
        LOBYTE(v27) = PreviousMode;                                                                                       | 
        SeReleaseSecurityDescriptor(P, v27, 0LL);                                                                         | 
        ObfDereferenceObject(Token);                                                                                      | 
        SeReleaseSubjectContext(&SubjectContext);                                                                         | 
        v47 = -1073741670;                                                                                                | 
        SepAuditFailed(3221225626LL);                                                                                     | 
        return -1073741670;                                                                                               | 
      }                                                                                                                   | 
      memmove(Pool2, Privileges, Size);                                                                                   | 
      *v28 = PrivilegeCount;                                                                                              | 
      v12 = (__int64 *)v55;                                                                                               | 
    }                                                                                                                     | 
    else                                                                                                                  | 
    {                                                                                                                     | 
      v22 = 0x7FFFFFFF0000LL;                                                                                             | 
    }                                                                                                                     | 
    if ( v12 )                                                                                                            | 
    {                                                                                                                     | 
      if ( ((unsigned __int8)v12 & 7) != 0 )                                                                              | 
      {                                                                                                                   | 
LABEL_19:                                                                                                                 | 
        ExRaiseDatatypeMisalignment();                                                                                    | 
      }                                                                                                                   | 
      v58 = *v12;                                                                                                         | 
    }                                                                                                                     | 
    v29 = GenerateOnClose;                                                                                                | 
    if ( (unsigned __int64)GenerateOnClose < 0x7FFFFFFF0000LL )                                                           | 
    {                                                                                                                     | 
      v22 = (__int64)GenerateOnClose;                                                                                     | 
    }                                                                                                                     | 
    *(_BYTE *)v22 = *(_BYTE *)v22;                                                                                        | 
    v16 = SepProbeAndCaptureString_U(SubsystemName, &v52);                                                                | 
    v47 = v16;                                                                                                            | 
    if ( v16 >= 0 )                                                                                                       | 
    {                                                                                                                     | 
      v16 = SepProbeAndCaptureString_U(ObjectTypeName, &v51);                                                             | 
      v47 = v16;                                                                                                          | 
      if ( v16 >= 0 )                                                                                                     | 
      {                                                                                                                   | 
        v16 = SepProbeAndCaptureString_U(ObjectName, &v53);                                                               | 
        v47 = v16;                                                                                                        | 
      }                                                                                                                   | 
    }                                                                                                                     | 
    goto LABEL_30;                                                                                                        | 
  }                                                                                                                       | 
  ObfDereferenceObject(v17);                                                                                              | 
  SeReleaseSubjectContext(&SubjectContext);                                                                               | 
  if ( v19 == -1073741670 )                                                                                               | 
  {                                                                                                                       | 
    SepAuditFailed(3221225626LL);                                                                                         | 
  }                                                                                                                       | 
  return v19;                                                                                                             | 
}                                                                                                                         | 
312 row(s)
