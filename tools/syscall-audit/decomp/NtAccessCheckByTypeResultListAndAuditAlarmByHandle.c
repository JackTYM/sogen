NTSTATUS __stdcall NtAccessCheckByTypeResultListAndAuditAlarmByHandle( | 
        PUNICODE_STRING SubsystemName,                                 | 
        PVOID HandleId,                                                | 
        HANDLE ClientToken,                                            | 
        PUNICODE_STRING ObjectTypeName,                                | 
        PUNICODE_STRING ObjectName,                                    | 
        PSECURITY_DESCRIPTOR SecurityDescriptor,                       | 
        PSID PrincipalSelfSid,                                         | 
        ACCESS_MASK DesiredAccess,                                     | 
        AUDIT_EVENT_TYPE AuditType,                                    | 
        ULONG Flags,                                                   | 
        POBJECT_TYPE_LIST ObjectTypeList,                              | 
        ULONG ObjectTypeListLength,                                    | 
        PGENERIC_MAPPING GenericMapping,                               | 
        BOOLEAN ObjectCreation,                                        | 
        PACCESS_MASK GrantedAccess,                                    | 
        PNTSTATUS AccessStatus,                                        | 
        PBOOLEAN GenerateOnClose)                                      | 
{                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]        | 
                                                                       | 
  v18 = ClientToken;                                                   | 
  return SepAccessCheckAndAuditAlarm(                                  | 
           (_DWORD)SubsystemName,                                      | 
           (_DWORD)HandleId,                                           | 
           (unsigned int)&v18,                                         | 
           (_DWORD)ObjectTypeName,                                     | 
           (__int64)ObjectName,                                        | 
           (__int64)SecurityDescriptor,                                | 
           (__int64)PrincipalSelfSid,                                  | 
           DesiredAccess,                                              | 
           AuditType,                                                  | 
           Flags,                                                      | 
           (__int64)ObjectTypeList,                                    | 
           ObjectTypeListLength,                                       | 
           (__int64)GenericMapping,                                    | 
           (__int64)GrantedAccess,                                     | 
           (__int64)AccessStatus,                                      | 
           (__int64)GenerateOnClose,                                   | 
           1);                                                         | 
}                                                                      | 
41 row(s)
