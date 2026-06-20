__int64 __fastcall NtResetWriteWatch(ULONG_PTR a1, unsigned __int64 a2, unsigned __int64 a3)                              | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  BugCheckParameter1 = 0LL;                                                                                               | 
  memset(v18, 0, sizeof(v18));                                                                                            | 
  if ( a2 > 0x7FFFFFFEFFFFLL )                                                                                            | 
  {                                                                                                                       | 
    return 3221225712LL;                                                                                                  | 
  }                                                                                                                       | 
  if ( 0x7FFFFFFF0000LL - a2 < a3 || !a3 )                                                                                | 
  {                                                                                                                       | 
    return 3221225713LL;                                                                                                  | 
  }                                                                                                                       | 
  CurrentThread = KeGetCurrentThread();                                                                                   | 
  v7 = a2 + a3 - 1;                                                                                                       | 
  Process = CurrentThread->ApcState.Process;                                                                              | 
  if ( a1 == -1LL )                                                                                                       | 
  {                                                                                                                       | 
    v9 = CurrentThread->ApcState.Process;                                                                                 | 
    v10 = 0;                                                                                                              | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    result = ObpReferenceObjectByHandleWithTag(a1, 0x77576D4Du, (__int64)&BugCheckParameter1, 0LL, 0LL);                  | 
    if ( (int)result < 0 )                                                                                                | 
    {                                                                                                                     | 
      return result;                                                                                                      | 
    }                                                                                                                     | 
    v9 = (void *)BugCheckParameter1;                                                                                      | 
    v10 = 0;                                                                                                              | 
    if ( Process != (_KPROCESS *)BugCheckParameter1 )                                                                     | 
    {                                                                                                                     | 
      KiStackAttachProcess(BugCheckParameter1);                                                                           | 
      v10 = 1;                                                                                                            | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  v11 = MiObtainReferencedVadEx(a2, 0LL, &BugCheckParameter1);                                                            | 
  v12 = (void *)v11;                                                                                                      | 
  if ( !v11 )                                                                                                             | 
  {                                                                                                                       | 
    v13 = -1073741585;                                                                                                    | 
    goto LABEL_13;                                                                                                        | 
  }                                                                                                                       | 
  if ( (*(_DWORD *)(v11 + 48) & 0x600000) == 0x600000                                                                     | 
    && v7 <= (((*(unsigned int *)(v11 + 28) | ((unsigned __int64)*(unsigned __int8 *)(v11 + 33) << 32)) << 12) | 0xFFF) ) | 
  {                                                                                                                       | 
    VadMandatoryPageSize = MiGetVadMandatoryPageSize(v11);                                                                | 
    if ( VadMandatoryPageSize <= 1 )                                                                                      | 
    {                                                                                                                     | 
LABEL_11:                                                                                                                 | 
      MiMoveDirtyBitsToPfns(a2, v7, v12, 1LL);                                                                            | 
      v13 = 0;                                                                                                            | 
      goto LABEL_12;                                                                                                      | 
    }                                                                                                                     | 
    v16 = (VadMandatoryPageSize << 12) - 1;                                                                               | 
    if ( (v16 & a2) == 0 )                                                                                                | 
    {                                                                                                                     | 
      if ( (v16 & a3) != 0 )                                                                                              | 
      {                                                                                                                   | 
        v13 = -1073741583;                                                                                                | 
        goto LABEL_12;                                                                                                    | 
      }                                                                                                                   | 
      goto LABEL_11;                                                                                                      | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  v13 = -1073741585;                                                                                                      | 
LABEL_12:                                                                                                                 | 
  MiUnlockAndDereferenceVad(v12);                                                                                         | 
LABEL_13:                                                                                                                 | 
  if ( v10 )                                                                                                              | 
  {                                                                                                                       | 
    KiUnstackDetachProcess(v18, 0LL);                                                                                     | 
  }                                                                                                                       | 
  if ( a1 != -1LL )                                                                                                       | 
  {                                                                                                                       | 
    ObfDereferenceObjectWithTag(v9, 0x77576D4Du);                                                                         | 
  }                                                                                                                       | 
  return v13;                                                                                                             | 
}                                                                                                                         | 
80 row(s)
