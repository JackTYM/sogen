__int64 __fastcall NtGetNextThread(                                                                                    | 
        ULONG_PTR BugCheckParameter1,                                                                                  | 
        ULONG_PTR a2,                                                                                                  | 
        unsigned int a3,                                                                                               | 
        int a4,                                                                                                        | 
        int a5,                                                                                                        | 
        HANDLE *a6)                                                                                                    | 
{                                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                        | 
                                                                                                                       | 
  v19 = a3;                                                                                                            | 
  v23 = a6;                                                                                                            | 
  Object = 0LL;                                                                                                        | 
  v20 = 0LL;                                                                                                           | 
  memset_0(&PassedAccessState, 0, sizeof(PassedAccessState));                                                          | 
  memset_0(v26, 0, sizeof(v26));                                                                                       | 
  Handle = 0LL;                                                                                                        | 
  AccessMode = KeGetCurrentThread()->PreviousMode;                                                                     | 
  if ( AccessMode )                                                                                                    | 
  {                                                                                                                    | 
    v10 = 0x7FFFFFFF0000LL;                                                                                            | 
    if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )                                                                     | 
    {                                                                                                                  | 
      v10 = (__int64)a6;                                                                                               | 
    }                                                                                                                  | 
    *(_QWORD *)v10 = *(_QWORD *)v10;                                                                                   | 
  }                                                                                                                    | 
  *a6 = 0LL;                                                                                                           | 
  if ( a5 )                                                                                                            | 
  {                                                                                                                    | 
    return 3221225485LL;                                                                                               | 
  }                                                                                                                    | 
  result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x6E457350u, (__int64)&Object, 0LL, 0LL);             | 
  if ( (int)result >= 0 )                                                                                              | 
  {                                                                                                                    | 
    if ( a2 )                                                                                                          | 
    {                                                                                                                  | 
      v12 = ObpReferenceObjectByHandleWithTag(a2, 0x6E457350u, (__int64)&v20, 0LL, 0LL);                               | 
      if ( v12 >= 0 )                                                                                                  | 
      {                                                                                                                | 
        if ( *((PVOID *)v20 + 68) == Object )                                                                          | 
        {                                                                                                              | 
          goto LABEL_10;                                                                                               | 
        }                                                                                                              | 
        ObfDereferenceObjectWithTag(v20, 0x6E457350u);                                                                 | 
        v12 = -1073741811;                                                                                             | 
      }                                                                                                                | 
LABEL_31:                                                                                                              | 
      v16 = Object;                                                                                                    | 
LABEL_25:                                                                                                              | 
      ObfDereferenceObjectWithTag(v16, 0x6E457350u);                                                                   | 
      return (unsigned int)v12;                                                                                        | 
    }                                                                                                                  | 
LABEL_10:                                                                                                              | 
    NextProcessThread = (_DWORD *)PsGetNextProcessThread(Object, v20);                                                 | 
    v22 = NextProcessThread;                                                                                           | 
    if ( NextProcessThread )                                                                                           | 
    {                                                                                                                  | 
      v17 = SeSinglePrivilegeCheck(SeDebugPrivilege, AccessMode);                                                      | 
      CurrentThread = KeGetCurrentThread();                                                                            | 
      v24 = CurrentThread;                                                                                             | 
      v15 = Object;                                                                                                    | 
      while ( 1 )                                                                                                      | 
      {                                                                                                                | 
        if ( (NextProcessThread[360] & 2) != 0                                                                         | 
          || (PsSynchronizeWithThreadInsertion(NextProcessThread, CurrentThread), (NextProcessThread[360] & 2) != 0) ) | 
        {                                                                                                              | 
          v12 = SeCreateAccessState(&PassedAccessState, v26, v19, (char *)PsProcessType + 76);                         | 
          if ( v12 < 0 )                                                                                               | 
          {                                                                                                            | 
            goto LABEL_23;                                                                                             | 
          }                                                                                                            | 
          if ( v17 )                                                                                                   | 
          {                                                                                                            | 
            if ( (PassedAccessState.RemainingDesiredAccess & 0x2000000) != 0 )                                         | 
            {                                                                                                          | 
              PassedAccessState.PreviouslyGrantedAccess |= 0x1FFFFFu;                                                  | 
            }                                                                                                          | 
            else                                                                                                       | 
            {                                                                                                          | 
              PassedAccessState.PreviouslyGrantedAccess |= PassedAccessState.RemainingDesiredAccess;                   | 
            }                                                                                                          | 
            PassedAccessState.RemainingDesiredAccess = 0;                                                              | 
          }                                                                                                            | 
          v12 = ObOpenObjectByPointer(                                                                                 | 
                  NextProcessThread,                                                                                   | 
                  a4 & (AccessMode != 0 ? 7666 : 73714),                                                               | 
                  &PassedAccessState,                                                                                  | 
                  0,                                                                                                   | 
                  (POBJECT_TYPE)PsThreadType,                                                                          | 
                  AccessMode,                                                                                          | 
                  &Handle);                                                                                            | 
          SeDeleteAccessState(&PassedAccessState);                                                                     | 
          if ( v12 >= 0 )                                                                                              | 
          {                                                                                                            | 
            *v23 = Handle;                                                                                             | 
LABEL_23:                                                                                                              | 
            ObfDereferenceObjectWithTag(v15, 0x6E457350u);                                                             | 
            if ( NextProcessThread )                                                                                   | 
            {                                                                                                          | 
              v16 = NextProcessThread;                                                                                 | 
              goto LABEL_25;                                                                                           | 
            }                                                                                                          | 
            return (unsigned int)v12;                                                                                  | 
          }                                                                                                            | 
          if ( v12 != -1073741790 )                                                                                    | 
          {                                                                                                            | 
            goto LABEL_23;                                                                                             | 
          }                                                                                                            | 
        }                                                                                                              | 
        NextProcessThread = (_DWORD *)PsGetNextProcessThread(v15, NextProcessThread);                                  | 
        v22 = NextProcessThread;                                                                                       | 
        CurrentThread = v24;                                                                                           | 
        if ( !NextProcessThread )                                                                                      | 
        {                                                                                                              | 
          v12 = -2147483622;                                                                                           | 
          goto LABEL_23;                                                                                               | 
        }                                                                                                              | 
      }                                                                                                                | 
    }                                                                                                                  | 
    v12 = -2147483622;                                                                                                 | 
    goto LABEL_31;                                                                                                     | 
  }                                                                                                                    | 
  return result;                                                                                                       | 
}                                                                                                                      | 
