__int64 __fastcall NtReplaceKey(__int64 a1, int a2, __int64 a3)                                                      | 
{                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      | 
                                                                                                                     | 
  Object = 0LL;                                                                                                      | 
  v29 = 0LL;                                                                                                         | 
  v28 = 0LL;                                                                                                         | 
  v6 = 0;                                                                                                            | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                 | 
  v25[1] = v25;                                                                                                      | 
  v25[0] = v25;                                                                                                      | 
  *(_OWORD *)v27 = 0LL;                                                                                              | 
  *(_OWORD *)P = 0LL;                                                                                                | 
  v30 = 0LL;                                                                                                         | 
  v31 = 0LL;                                                                                                         | 
  v32 = 0LL;                                                                                                         | 
  CmpInitializeThreadInfo(&v28);                                                                                     | 
  UnloadRundown = CmpAcquireHiveLoadUnloadRundown();                                                                 | 
  if ( UnloadRundown )                                                                                               | 
  {                                                                                                                  | 
    v9 = CmCheckNoTxContext();                                                                                       | 
    if ( v9 >= 0 )                                                                                                   | 
    {                                                                                                                | 
      if ( SeSinglePrivilegeCheck(SeRestorePrivilege, PreviousMode) )                                                | 
      {                                                                                                              | 
        CurrentThread = KeGetCurrentThread();                                                                        | 
        LOBYTE(v10) = PreviousMode;                                                                                  | 
        v6 = 1;                                                                                                      | 
        --CurrentThread->KernelApcDisable;                                                                           | 
        v9 = CmpNameFromAttributes(a1, v10, v27);                                                                    | 
        if ( v9 >= 0 )                                                                                               | 
        {                                                                                                            | 
          LOBYTE(v12) = PreviousMode;                                                                                | 
          v9 = CmpNameFromAttributes(a3, v12, P);                                                                    | 
          if ( v9 >= 0 )                                                                                             | 
          {                                                                                                          | 
            LOBYTE(v14) = PreviousMode;                                                                              | 
            v15 = CmObReferenceObjectByHandle(a2, 0, v13, v14, (__int64)&Object, 0LL);                               | 
            v16 = Object;                                                                                            | 
            v9 = v15;                                                                                                | 
            if ( v15 >= 0 )                                                                                          | 
            {                                                                                                        | 
              if ( (*(_DWORD *)(*((_QWORD *)Object + 1) + 8LL) & 0x80u) == 0 )                                       | 
              {                                                                                                      | 
                if ( !CmpCallBackCount                                                                               | 
                  || (unsigned int)CmpIsRegistryLockAcquired()                                                       | 
                  || (*(_QWORD *)&v30 = v16,                                                                         | 
                      *((_QWORD *)&v30 + 1) = P,                                                                     | 
                      LOBYTE(v17) = 1,                                                                               | 
                      *(_QWORD *)&v31 = v27,                                                                         | 
                      v18 = CmpCallCallBacksEx(45, (unsigned int)&v30, 0, v17, 46, 0LL, (__int64)v25),               | 
                      v18 >= 0) )                                                                                    | 
                {                                                                                                    | 
                  v19 = CmReplaceKey(*(_QWORD *)(v16[1] + 32LL));                                                    | 
                  v9 = CmPostCallbackNotificationEx(46, (_DWORD)v16, v19, (unsigned int)&v30, 0LL, (__int64)v25);    | 
                  if ( v9 == -1073740541 )                                                                           | 
                  {                                                                                                  | 
                    v9 = 0;                                                                                          | 
                  }                                                                                                  | 
                }                                                                                                    | 
                else                                                                                                 | 
                {                                                                                                    | 
                  v9 = 0;                                                                                            | 
                  if ( v18 != -1073740541 )                                                                          | 
                  {                                                                                                  | 
                    v9 = v18;                                                                                        | 
                  }                                                                                                  | 
                }                                                                                                    | 
              }                                                                                                      | 
              else                                                                                                   | 
              {                                                                                                      | 
                v9 = -1073741790;                                                                                    | 
              }                                                                                                      | 
            }                                                                                                        | 
            if ( v16 )                                                                                               | 
            {                                                                                                        | 
              ObfDereferenceObject(v16);                                                                             | 
            }                                                                                                        | 
          }                                                                                                          | 
        }                                                                                                            | 
      }                                                                                                              | 
      else                                                                                                           | 
      {                                                                                                              | 
        v9 = -1073741727;                                                                                            | 
      }                                                                                                              | 
    }                                                                                                                | 
  }                                                                                                                  | 
  else                                                                                                               | 
  {                                                                                                                  | 
    v9 = -1073741431;                                                                                                | 
  }                                                                                                                  | 
  if ( P[1] )                                                                                                        | 
  {                                                                                                                  | 
    ExFreePoolWithTag(P[1], 0);                                                                                      | 
  }                                                                                                                  | 
  if ( v27[1] )                                                                                                      | 
  {                                                                                                                  | 
    ExFreePoolWithTag(v27[1], 0);                                                                                    | 
  }                                                                                                                  | 
  if ( v6 )                                                                                                          | 
  {                                                                                                                  | 
    KeLeaveCriticalRegion();                                                                                         | 
  }                                                                                                                  | 
  if ( UnloadRundown )                                                                                               | 
  {                                                                                                                  | 
    CmpReleaseHiveLoadUnloadRundown();                                                                               | 
  }                                                                                                                  | 
  CmpCleanupThreadInfo(&v28);                                                                                        | 
  if ( v9 >= 0 )                                                                                                     | 
  {                                                                                                                  | 
    if ( (unsigned int)dword_140E09ED8 > 5 && (unsigned __int8)tlgKeywordOn(&dword_140E09ED8, 0x400000000000LL) )    | 
    {                                                                                                                | 
      v35 = 8LL;                                                                                                     | 
      p_Object = (PVOID *)&v24;                                                                                      | 
      v20 = &word_140054D1E;                                                                                         | 
      v22 = 3;                                                                                                       | 
      goto LABEL_36;                                                                                                 | 
    }                                                                                                                | 
  }                                                                                                                  | 
  else if ( (unsigned int)dword_140E09ED8 > 5 && (unsigned __int8)tlgKeywordOn(&dword_140E09ED8, 0x400000000000LL) ) | 
  {                                                                                                                  | 
    LODWORD(Object) = v9;                                                                                            | 
    p_Object = &Object;                                                                                              | 
    v20 = (__int16 *)&dword_140054D54;                                                                               | 
    v37 = 8LL;                                                                                                       | 
    v36 = &v24;                                                                                                      | 
    v35 = 4LL;                                                                                                       | 
    v22 = 4;                                                                                                         | 
LABEL_36:                                                                                                            | 
    v24 = 0x1000000LL;                                                                                               | 
    tlgWriteTransfer_EtwWriteTransfer(&dword_140E09ED8, v20, 0LL, 0LL, v22, v33);                                    | 
  }                                                                                                                  | 
  return (unsigned int)v9;                                                                                           | 
}                                                                                                                    | 
134 row(s)
