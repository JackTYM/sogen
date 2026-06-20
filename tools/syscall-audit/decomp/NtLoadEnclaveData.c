__int64 __fastcall NtLoadEnclaveData(                                                                                   | 
        ULONG_PTR BugCheckParameter1,                                                                                   | 
        __int64 a2,                                                                                                     | 
        void *a3,                                                                                                       | 
        __int64 a4,                                                                                                     | 
        int a5,                                                                                                         | 
        void *Src,                                                                                                      | 
        size_t Size,                                                                                                    | 
        unsigned __int64 a8,                                                                                            | 
        _DWORD *a9)                                                                                                     | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v9 = a4;                                                                                                              | 
  v42 = a4;                                                                                                             | 
  Base = a3;                                                                                                            | 
  v40 = a2;                                                                                                             | 
  v10 = BugCheckParameter1;                                                                                             | 
  v36 = BugCheckParameter1;                                                                                             | 
  v37 = a8;                                                                                                             | 
  v49 = BugCheckParameter1;                                                                                             | 
  v11 = Src;                                                                                                            | 
  v48 = Src;                                                                                                            | 
  v12 = Size;                                                                                                           | 
  v50 = a8;                                                                                                             | 
  v47 = a9;                                                                                                             | 
  memset(v51, 0, sizeof(v51));                                                                                          | 
  v33 = 0;                                                                                                              | 
  v43 = 0LL;                                                                                                            | 
  LODWORD(v39) = 0;                                                                                                     | 
  Pool = 0LL;                                                                                                           | 
  v14 = 0LL;                                                                                                            | 
  v35 = 0;                                                                                                              | 
  v41 = 0;                                                                                                              | 
  Object = 0LL;                                                                                                         | 
  v46 = 0LL;                                                                                                            | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                    | 
  v34 = PreviousMode;                                                                                                   | 
  if ( PreviousMode == 1 )                                                                                              | 
  {                                                                                                                     | 
    if ( a9 )                                                                                                           | 
    {                                                                                                                   | 
      v27 = 0x7FFFFFFF0000LL;                                                                                           | 
      if ( (unsigned __int64)a9 < 0x7FFFFFFF0000LL )                                                                    | 
      {                                                                                                                 | 
        v27 = (__int64)a9;                                                                                              | 
      }                                                                                                                 | 
      *(_DWORD *)v27 = *(_DWORD *)v27;                                                                                  | 
    }                                                                                                                   | 
    if ( a8 )                                                                                                           | 
    {                                                                                                                   | 
      v28 = 0x7FFFFFFF0000LL;                                                                                           | 
      if ( a8 < 0x7FFFFFFF0000LL )                                                                                      | 
      {                                                                                                                 | 
        v28 = a8;                                                                                                       | 
      }                                                                                                                 | 
      *(_QWORD *)v28 = *(_QWORD *)v28;                                                                                  | 
    }                                                                                                                   | 
  }                                                                                                                     | 
  if ( (_DWORD)Size )                                                                                                   | 
  {                                                                                                                     | 
    if ( (unsigned int)Size > 0xFFFF )                                                                                  | 
    {                                                                                                                   | 
      goto LABEL_40;                                                                                                    | 
    }                                                                                                                   | 
    if ( (unsigned int)Size > 0x20 )                                                                                    | 
    {                                                                                                                   | 
      Pool = (_BYTE *)MiAllocatePool(0x40uLL, (unsigned int)Size);                                                      | 
      v44 = Pool;                                                                                                       | 
      if ( !Pool )                                                                                                      | 
      {                                                                                                                 | 
        v33 = -1073741670;                                                                                              | 
        goto LABEL_41;                                                                                                  | 
      }                                                                                                                 | 
      v11 = v48;                                                                                                        | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      Pool = v52;                                                                                                       | 
      v44 = v52;                                                                                                        | 
    }                                                                                                                   | 
    memmove(Pool, v11, (unsigned int)Size);                                                                             | 
    PreviousMode = v34;                                                                                                 | 
    v9 = v42;                                                                                                           | 
    v10 = v36;                                                                                                          | 
  }                                                                                                                     | 
  if ( (v40 & 0xFFF) != 0 )                                                                                             | 
  {                                                                                                                     | 
    v33 = -1073741584;                                                                                                  | 
    goto LABEL_41;                                                                                                      | 
  }                                                                                                                     | 
  if ( (v9 & 0xFFF) != 0 )                                                                                              | 
  {                                                                                                                     | 
    goto LABEL_42;                                                                                                      | 
  }                                                                                                                     | 
  CurrentThread = KeGetCurrentThread();                                                                                 | 
  Process = CurrentThread->ApcState.Process;                                                                            | 
  if ( v10 == -1LL )                                                                                                    | 
  {                                                                                                                     | 
    Object = CurrentThread->ApcState.Process;                                                                           | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    v33 = ObpReferenceObjectByHandleWithTag(v10, 0x6D566D4Du, (__int64)&Object, 0LL, 0LL);                              | 
    if ( v33 < 0 )                                                                                                      | 
    {                                                                                                                   | 
      goto LABEL_41;                                                                                                    | 
    }                                                                                                                   | 
  }                                                                                                                     | 
  KiStackAttachProcess((ULONG_PTR)Object);                                                                              | 
  v35 = 1;                                                                                                              | 
  v18 = MiObtainReferencedVadEx(v40, 0LL, &v33);                                                                        | 
  v14 = v18;                                                                                                            | 
  if ( !v18                                                                                                             | 
    || ((v20 = *(_DWORD *)(v18 + 48), (v20 & 0x200000) == 0) || (v20 & 0xC000000) != 0x8000000 ? (v21 = 0) : (v21 = 1), | 
        !v21) )                                                                                                         | 
  {                                                                                                                     | 
    v33 = -1073741800;                                                                                                  | 
    goto LABEL_41;                                                                                                      | 
  }                                                                                                                     | 
  if ( (*(_DWORD *)(v14 + 64) & 1) == 0 )                                                                               | 
  {                                                                                                                     | 
    if ( *(_BYTE *)(*(_QWORD *)(v14 + 72) + 76LL) )                                                                     | 
    {                                                                                                                   | 
      LOBYTE(v19) = PreviousMode;                                                                                       | 
      DataIntoVsmEnclave = MiLoadDataIntoVsmEnclave(                                                                    | 
                             (int)Object,                                                                               | 
                             (int)Process,                                                                              | 
                             v14,                                                                                       | 
                             v19,                                                                                       | 
                             v40,                                                                                       | 
                             Size,                                                                                      | 
                             (__int64)Pool,                                                                             | 
                             Base,                                                                                      | 
                             v42,                                                                                       | 
                             a5,                                                                                        | 
                             (__int64)&v43);                                                                            | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v29 = (void *)PsReferencePrimaryTokenWithTag(Process, 1953261124LL);                                              | 
      Tag = v40;                                                                                                        | 
      LOBYTE(v30) = PreviousMode;                                                                                       | 
      DataIntoVsmEnclave = MiLoadSectionIntoVsmEnclave(Object, v14, v30, v29);                                          | 
      v14 = 0LL;                                                                                                        | 
      ObfDereferenceObject(v29);                                                                                        | 
    }                                                                                                                   | 
    v33 = DataIntoVsmEnclave;                                                                                           | 
    goto LABEL_41;                                                                                                      | 
  }                                                                                                                     | 
  if ( !(_DWORD)Size )                                                                                                  | 
  {                                                                                                                     | 
    v22 = v42;                                                                                                          | 
    if ( v42 )                                                                                                          | 
    {                                                                                                                   | 
      KiUnstackDetachProcess(v51, 0LL);                                                                                 | 
      v33 = MiCopyPagesIntoEnclave((ULONG_PTR)Object, (__int64)Base, v22, a5, (__int64)&v43, (__int64)&v39);            | 
      goto LABEL_15;                                                                                                    | 
    }                                                                                                                   | 
LABEL_42:                                                                                                               | 
    v33 = -1073741582;                                                                                                  | 
    goto LABEL_41;                                                                                                      | 
  }                                                                                                                     | 
LABEL_40:                                                                                                               | 
  v33 = -1073741820;                                                                                                    | 
LABEL_41:                                                                                                               | 
  v12 = v35;                                                                                                            | 
LABEL_15:                                                                                                               | 
  v23 = v36;                                                                                                            | 
  v24 = (_QWORD *)v37;                                                                                                  | 
  if ( v14 )                                                                                                            | 
  {                                                                                                                     | 
    MiUnlockAndDereferenceVad((PVOID)v14);                                                                              | 
  }                                                                                                                     | 
  if ( v12 )                                                                                                            | 
  {                                                                                                                     | 
    KiUnstackDetachProcess(v51, 0LL);                                                                                   | 
  }                                                                                                                     | 
  if ( Object && v23 != -1LL )                                                                                          | 
  {                                                                                                                     | 
    ObfDereferenceObjectWithTag(Object, 0x6D566D4Du);                                                                   | 
  }                                                                                                                     | 
  if ( Pool && Pool != v52 )                                                                                            | 
  {                                                                                                                     | 
    ExFreePoolWithTag(Pool, 0);                                                                                         | 
  }                                                                                                                     | 
  v25 = v46;                                                                                                            | 
  if ( v46 )                                                                                                            | 
  {                                                                                                                     | 
    DbgkMapViewOfSection(Object, Tag, *(_DWORD *)(v46 + 36), *(_DWORD *)(v46 + 40));                                    | 
    MiDereferenceEnclaveModule(v25);                                                                                    | 
  }                                                                                                                     | 
  if ( v24 )                                                                                                            | 
  {                                                                                                                     | 
    *v24 = v43;                                                                                                         | 
  }                                                                                                                     | 
  if ( a9 )                                                                                                             | 
  {                                                                                                                     | 
    *a9 = v39;                                                                                                          | 
  }                                                                                                                     | 
  return (unsigned int)v33;                                                                                             | 
}                                                                                                                       | 
202 row(s)
