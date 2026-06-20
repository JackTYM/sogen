NTSTATUS __stdcall NtAccessCheckAndAuditAlarm(                  | 
        PUNICODE_STRING SubsystemName,                          | 
        PVOID HandleId,                                         | 
        PUNICODE_STRING ObjectTypeName,                         | 
        PUNICODE_STRING ObjectName,                             | 
        PSECURITY_DESCRIPTOR SecurityDescriptor,                | 
        ACCESS_MASK DesiredAccess,                              | 
        PGENERIC_MAPPING GenericMapping,                        | 
        BOOLEAN ObjectCreation,                                 | 
        PACCESS_MASK GrantedAccess,                             | 
        PNTSTATUS AccessStatus,                                 | 
        PBOOLEAN GenerateOnClose)                               | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return SepAccessCheckAndAuditAlarm(                           | 
           (_DWORD)SubsystemName,                               | 
           (_DWORD)HandleId,                                    | 
           0,                                                   | 
           (_DWORD)ObjectTypeName,                              | 
           (__int64)ObjectName,                                 | 
           (__int64)SecurityDescriptor,                         | 
           0LL,                                                 | 
           DesiredAccess,                                       | 
           0,                                                   | 
           0,                                                   | 
           0LL,                                                 | 
           0,                                                   | 
           (__int64)GenericMapping,                             | 
           (__int64)GrantedAccess,                              | 
           (__int64)AccessStatus,                               | 
           (__int64)GenerateOnClose,                            | 
           0);                                                  | 
}                                                               | 
34 row(s)
