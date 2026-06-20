__int64 __fastcall NtQueryOpenSubKeysEx(int a1, unsigned int a2, _DWORD *a3, _DWORD *a4)                                | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v6 = a2;                                                                                                              | 
  v25 = 0LL;                                                                                                            | 
  v26 = 0LL;                                                                                                            | 
  memset_0(v28, 0, sizeof(v28));                                                                                        | 
  v24 = 0;                                                                                                              | 
  memset(&ApcState, 0, sizeof(ApcState));                                                                               | 
  Object = 0LL;                                                                                                         | 
  CmpInitializeParseContext(v28);                                                                                       | 
  *(_OWORD *)Src = 0LL;                                                                                                 | 
  v22 = 0LL;                                                                                                            | 
  v23 = 0LL;                                                                                                            | 
  v18 = 0;                                                                                                              | 
  CmpInitializeThreadInfo(&v25);                                                                                        | 
  v19 = CmpAcquireShutdownRundown();                                                                                    | 
  if ( !v19 )                                                                                                           | 
  {                                                                                                                     | 
    v8 = -1073741431;                                                                                                   | 
    goto LABEL_22;                                                                                                      | 
  }                                                                                                                     | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                    | 
  if ( !SeSinglePrivilegeCheck(SeRestorePrivilege, PreviousMode) )                                                      | 
  {                                                                                                                     | 
    v8 = -1073741727;                                                                                                   | 
    goto LABEL_22;                                                                                                      | 
  }                                                                                                                     | 
  if ( (unsigned int)v6 < 8 )                                                                                           | 
  {                                                                                                                     | 
    v8 = -1073741789;                                                                                                   | 
    goto LABEL_22;                                                                                                      | 
  }                                                                                                                     | 
  if ( (v6 & 1) != 0 )                                                                                                  | 
  {                                                                                                                     | 
    goto LABEL_8;                                                                                                       | 
  }                                                                                                                     | 
  if ( PreviousMode == 1 )                                                                                              | 
  {                                                                                                                     | 
    v10 = 0x7FFFFFFF0000LL;                                                                                             | 
    if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )                                                                      | 
    {                                                                                                                   | 
      v10 = (__int64)a4;                                                                                                | 
    }                                                                                                                   | 
    *(_DWORD *)v10 = *(_DWORD *)v10;                                                                                    | 
    ProbeForWrite(a3, v6, 4u);                                                                                          | 
  }                                                                                                                     | 
  v8 = ObReferenceObjectByNameEx(a1, 0, 131097, (_DWORD)CmKeyObjectType, PreviousMode, (__int64)v28, (__int64)&Object); | 
  if ( v8 >= 0 )                                                                                                        | 
  {                                                                                                                     | 
    LODWORD(Src[0]) = v6;                                                                                               | 
    v12 = v6;                                                                                                           | 
    Src[1] = (void *)CmpAllocateTransientPoolWithQuota(v11, v6, 825773379LL);                                           | 
    if ( Src[1] )                                                                                                       | 
    {                                                                                                                   | 
      CmpLockRegistryExclusive();                                                                                       | 
      v18 = 1;                                                                                                          | 
      v13 = Object;                                                                                                     | 
      v8 = CmpPerformKeyBodyDeletionCheck(Object, 0LL);                                                                 | 
      if ( v8 >= 0 )                                                                                                    | 
      {                                                                                                                 | 
        if ( (*(_DWORD *)(v13[1] + 184LL) & 0x40000) == 0 )                                                             | 
        {                                                                                                               | 
LABEL_8:                                                                                                                | 
          v8 = -1073741811;                                                                                             | 
          goto LABEL_22;                                                                                                | 
        }                                                                                                               | 
        *(_DWORD *)Src[1] = 0;                                                                                          | 
        LODWORD(v22) = 8;                                                                                               | 
        *((_QWORD *)&v23 + 1) = (char *)Src[1] + v12;                                                                   | 
        *(_QWORD *)&v23 = v13;                                                                                          | 
        *(_QWORD *)((char *)&v22 + 4) = 0x800000000LL;                                                                  | 
        CmpAttachToRegistryProcess(&ApcState);                                                                          | 
        CmpSearchForOpenSubKeys(v13[1], 0LL, Src);                                                                      | 
        CmpDetachFromRegistryProcess(&ApcState);                                                                        | 
        v8 = DWORD1(v22);                                                                                               | 
        CmpUnlockRegistry();                                                                                            | 
        v18 = 0;                                                                                                        | 
        *a4 = v22;                                                                                                      | 
        v14 = (char *)Src[1];                                                                                           | 
        *a3 = *(_DWORD *)Src[1];                                                                                        | 
        if ( v8 >= 0 )                                                                                                  | 
        {                                                                                                               | 
          v15 = v14 - (char *)a3;                                                                                       | 
          v16 = 0;                                                                                                      | 
          if ( *(_DWORD *)v14 )                                                                                         | 
          {                                                                                                             | 
            do                                                                                                          | 
            {                                                                                                           | 
              *(_QWORD *)&v14[24 * ++v16] -= v15;                                                                       | 
              v14 = (char *)Src[1];                                                                                     | 
            }                                                                                                           | 
            while ( v16 < *(_DWORD *)Src[1] );                                                                          | 
          }                                                                                                             | 
          memmove(a3, v14, v12);                                                                                        | 
          v8 = 0;                                                                                                       | 
        }                                                                                                               | 
      }                                                                                                                 | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v8 = -1073741670;                                                                                                 | 
    }                                                                                                                   | 
  }                                                                                                                     | 
LABEL_22:                                                                                                               | 
  if ( v18 )                                                                                                            | 
  {                                                                                                                     | 
    CmpUnlockRegistry();                                                                                                | 
  }                                                                                                                     | 
  CmpCleanupParseContext(v28, 0LL);                                                                                     | 
  if ( v19 )                                                                                                            | 
  {                                                                                                                     | 
    CmpReleaseShutdownRundown();                                                                                        | 
  }                                                                                                                     | 
  if ( Object )                                                                                                         | 
  {                                                                                                                     | 
    ObfDereferenceObject(Object);                                                                                       | 
  }                                                                                                                     | 
  if ( Src[1] )                                                                                                         | 
  {                                                                                                                     | 
    CmSiFreeMemory((PPRIVILEGE_SET)Src[1]);                                                                             | 
  }                                                                                                                     | 
  CmpCleanupThreadInfo(&v25);                                                                                           | 
  return (unsigned int)v8;                                                                                              | 
}                                                                                                                       | 
126 row(s)
