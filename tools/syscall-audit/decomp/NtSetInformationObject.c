__int64 __fastcall NtSetInformationObject(HANDLE Handle, int a2, __int16 *a3, int a4)                                 | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  v19 = 0;                                                                                                            | 
  v5 = -1073741821;                                                                                                   | 
  v6 = a2 - 4;                                                                                                        | 
  if ( v6 )                                                                                                           | 
  {                                                                                                                   | 
    v9 = v6 - 1;                                                                                                      | 
    if ( v9 )                                                                                                         | 
    {                                                                                                                 | 
      if ( v9 != 1 )                                                                                                  | 
      {                                                                                                               | 
        return (unsigned int)v5;                                                                                      | 
      }                                                                                                               | 
      PreviousMode = KeGetCurrentThread()->PreviousMode;                                                              | 
      if ( SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
      {                                                                                                               | 
        HandleInformation = 0LL;                                                                                      | 
        Object = 0LL;                                                                                                 | 
        v5 = ObReferenceObjectByHandle(Handle, 0, ObpDirectoryObjectType, PreviousMode, &Object, &HandleInformation); | 
        if ( v5 < 0 )                                                                                                 | 
        {                                                                                                             | 
          return (unsigned int)v5;                                                                                    | 
        }                                                                                                             | 
        CurrentProcessSessionId = PsGetCurrentProcessSessionId();                                                     | 
        SessionObjectById = (void *)PsGetSessionObjectById(CurrentProcessSessionId);                                  | 
        v13 = Object;                                                                                                 | 
        if ( !SessionObjectById )                                                                                     | 
        {                                                                                                             | 
          v5 = -1073740715;                                                                                           | 
LABEL_21:                                                                                                             | 
          ObfDereferenceObject(v13);                                                                                  | 
          return (unsigned int)v5;                                                                                    | 
        }                                                                                                             | 
        v17 = 0LL;                                                                                                    | 
        v18 = 0LL;                                                                                                    | 
        ObpLockDirectoryExclusive(&v17, Object);                                                                      | 
        if ( *((_QWORD *)v13 + 41) )                                                                                  | 
        {                                                                                                             | 
          v5 = -1073741790;                                                                                           | 
          ObfDereferenceObject(SessionObjectById);                                                                    | 
        }                                                                                                             | 
        else                                                                                                          | 
        {                                                                                                             | 
          *((_QWORD *)v13 + 41) = SessionObjectById;                                                                  | 
        }                                                                                                             | 
LABEL_20:                                                                                                             | 
        ObpUnlockDirectory(&v17);                                                                                     | 
        goto LABEL_21;                                                                                                | 
      }                                                                                                               | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      v14 = KeGetCurrentThread()->PreviousMode;                                                                       | 
      if ( SeSinglePrivilegeCheck(SeTcbPrivilege, v14) )                                                              | 
      {                                                                                                               | 
        HandleInformation = 0LL;                                                                                      | 
        Object = 0LL;                                                                                                 | 
        v5 = ObReferenceObjectByHandle(Handle, 0, ObpDirectoryObjectType, v14, &Object, &HandleInformation);          | 
        if ( v5 < 0 )                                                                                                 | 
        {                                                                                                             | 
          return (unsigned int)v5;                                                                                    | 
        }                                                                                                             | 
        v17 = 0LL;                                                                                                    | 
        v18 = 0LL;                                                                                                    | 
        v13 = Object;                                                                                                 | 
        ObpLockDirectoryExclusive(&v17, Object);                                                                      | 
        *((_DWORD *)v13 + 85) = PsGetCurrentProcessSessionId();                                                       | 
        goto LABEL_20;                                                                                                | 
      }                                                                                                               | 
    }                                                                                                                 | 
    return (unsigned int)-1073741727;                                                                                 | 
  }                                                                                                                   | 
  if ( a4 == 2 )                                                                                                      | 
  {                                                                                                                   | 
    v7 = KeGetCurrentThread()->PreviousMode;                                                                          | 
    v19 = *a3;                                                                                                        | 
    LOBYTE(a3) = v7;                                                                                                  | 
    return (unsigned int)ObSetHandleAttributes(Handle, &v19, a3);                                                     | 
  }                                                                                                                   | 
  return 3221225476LL;                                                                                                | 
}                                                                                                                     | 
