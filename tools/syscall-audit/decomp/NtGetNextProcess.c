__int64 __fastcall NtGetNextProcess(ULONG_PTR BugCheckParameter1, unsigned int a2, int a3, int a4, HANDLE *a5)       | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  Object = 0LL;                                                                                                      | 
  memset_0(&PassedAccessState, 0, sizeof(PassedAccessState));                                                        | 
  memset_0(v27, 0, sizeof(v27));                                                                                     | 
  Handle = 0LL;                                                                                                      | 
  AccessMode = KeGetCurrentThread()->PreviousMode;                                                                   | 
  v9 = a3 & (AccessMode != 0 ? 7666 : 73714);                                                                        | 
  if ( AccessMode )                                                                                                  | 
  {                                                                                                                  | 
    v10 = 0x7FFFFFFF0000LL;                                                                                          | 
    if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                   | 
    {                                                                                                                | 
      v10 = (__int64)a5;                                                                                             | 
    }                                                                                                                | 
    *(_QWORD *)v10 = *(_QWORD *)v10;                                                                                 | 
  }                                                                                                                  | 
  *a5 = 0LL;                                                                                                         | 
  if ( (a4 & 0xFFFFFFFE) != 0 )                                                                                      | 
  {                                                                                                                  | 
    return 3221225485LL;                                                                                             | 
  }                                                                                                                  | 
  if ( !BugCheckParameter1                                                                                           | 
    || (result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x6E457350u, (__int64)&Object, 0LL, 0LL),     | 
        (int)result >= 0) )                                                                                          | 
  {                                                                                                                  | 
    v12 = a4 & 1;                                                                                                    | 
    if ( v12 )                                                                                                       | 
    {                                                                                                                | 
      PreviousProcess = PsGetPreviousProcess(Object);                                                                | 
    }                                                                                                                | 
    else                                                                                                             | 
    {                                                                                                                | 
      PreviousProcess = PsGetNextProcess(Object);                                                                    | 
    }                                                                                                                | 
    v14 = (_DWORD *)PreviousProcess;                                                                                 | 
    if ( !PreviousProcess )                                                                                          | 
    {                                                                                                                | 
      return 2147483674LL;                                                                                           | 
    }                                                                                                                | 
    v20 = SeSinglePrivilegeCheck(SeDebugPrivilege, AccessMode);                                                      | 
    CurrentThread = KeGetCurrentThread();                                                                            | 
    v25 = CurrentThread;                                                                                             | 
    CurrentServerSilo = PsGetCurrentServerSilo();                                                                    | 
    v24 = CurrentServerSilo;                                                                                         | 
    while ( 1 )                                                                                                      | 
    {                                                                                                                | 
      if ( (v14[125] & 0x4000000) != 0 )                                                                             | 
      {                                                                                                              | 
        goto LABEL_13;                                                                                               | 
      }                                                                                                              | 
      PspLockUnlockProcessExclusive(v14, CurrentThread);                                                             | 
      if ( (v14[125] & 0x4000000) != 0 )                                                                             | 
      {                                                                                                              | 
        break;                                                                                                       | 
      }                                                                                                              | 
LABEL_27:                                                                                                            | 
      if ( v12 )                                                                                                     | 
      {                                                                                                              | 
        NextProcess = PsGetPreviousProcess(v14);                                                                     | 
      }                                                                                                              | 
      else                                                                                                           | 
      {                                                                                                              | 
        NextProcess = PsGetNextProcess(v14);                                                                         | 
      }                                                                                                              | 
      v14 = (_DWORD *)NextProcess;                                                                                   | 
      v19 = NextProcess == 0;                                                                                        | 
      CurrentServerSilo = v24;                                                                                       | 
      if ( v19 )                                                                                                     | 
      {                                                                                                              | 
        v17 = -2147483622;                                                                                           | 
LABEL_21:                                                                                                            | 
        if ( v14 )                                                                                                   | 
        {                                                                                                            | 
          ObfDereferenceObjectWithTag(v14, 0x6E457350u);                                                             | 
        }                                                                                                            | 
        return (unsigned int)v17;                                                                                    | 
      }                                                                                                              | 
    }                                                                                                                | 
    CurrentServerSilo = v24;                                                                                         | 
LABEL_13:                                                                                                            | 
    if ( (unsigned __int8)PsIsProcessInSilo(v14, CurrentServerSilo) )                                                | 
    {                                                                                                                | 
      v17 = SeCreateAccessState(&PassedAccessState, v27, a2, (char *)PsProcessType + 76);                            | 
      if ( v17 < 0 )                                                                                                 | 
      {                                                                                                              | 
        goto LABEL_21;                                                                                               | 
      }                                                                                                              | 
      if ( v20 )                                                                                                     | 
      {                                                                                                              | 
        if ( (PassedAccessState.RemainingDesiredAccess & 0x2000000) != 0 )                                           | 
        {                                                                                                            | 
          PassedAccessState.PreviouslyGrantedAccess |= 0x1FFFFFu;                                                    | 
        }                                                                                                            | 
        else                                                                                                         | 
        {                                                                                                            | 
          PassedAccessState.PreviouslyGrantedAccess |= PassedAccessState.RemainingDesiredAccess;                     | 
        }                                                                                                            | 
        PassedAccessState.RemainingDesiredAccess = 0;                                                                | 
      }                                                                                                              | 
      v17 = ObOpenObjectByPointer(v14, v9, &PassedAccessState, 0, (POBJECT_TYPE)PsProcessType, AccessMode, &Handle); | 
      SeDeleteAccessState(&PassedAccessState);                                                                       | 
      if ( v17 >= 0 )                                                                                                | 
      {                                                                                                              | 
        *a5 = Handle;                                                                                                | 
        goto LABEL_21;                                                                                               | 
      }                                                                                                              | 
      if ( v17 != -1073741790 )                                                                                      | 
      {                                                                                                              | 
        goto LABEL_21;                                                                                               | 
      }                                                                                                              | 
      CurrentThread = v25;                                                                                           | 
    }                                                                                                                | 
    goto LABEL_27;                                                                                                   | 
  }                                                                                                                  | 
  return result;                                                                                                     | 
}                                                                                                                    | 
119 row(s)
