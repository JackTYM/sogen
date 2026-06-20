NTSTATUS __fastcall NtQueryInformationWorkerFactory(HANDLE Handle, int a2, unsigned __int64 a3, int a4, _DWORD *a5) | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  memset(&LockHandle, 0, sizeof(LockHandle));                                                                       | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                | 
  if ( a2 != 7 )                                                                                                    | 
  {                                                                                                                 | 
    return -1073741821;                                                                                             | 
  }                                                                                                                 | 
  if ( PreviousMode )                                                                                               | 
  {                                                                                                                 | 
    if ( (a3 & 3) != 0 )                                                                                            | 
    {                                                                                                               | 
      ExRaiseDatatypeMisalignment();                                                                                | 
    }                                                                                                               | 
    v10 = 0x7FFFFFFF0000LL;                                                                                         | 
    v11 = 0x7FFFFFFF0000LL;                                                                                         | 
    if ( a3 < 0x7FFFFFFF0000LL )                                                                                    | 
    {                                                                                                               | 
      v11 = a3;                                                                                                     | 
    }                                                                                                               | 
    *(_BYTE *)v11 = *(_BYTE *)v11;                                                                                  | 
    *(_BYTE *)(v11 + 119) = *(_BYTE *)(v11 + 119);                                                                  | 
    if ( a5 )                                                                                                       | 
    {                                                                                                               | 
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                | 
      {                                                                                                             | 
        v10 = (__int64)a5;                                                                                          | 
      }                                                                                                             | 
      *(_DWORD *)v10 = 120;                                                                                         | 
    }                                                                                                               | 
  }                                                                                                                 | 
  else if ( a5 )                                                                                                    | 
  {                                                                                                                 | 
    *a5 = 120;                                                                                                      | 
  }                                                                                                                 | 
  if ( a4 != 120 )                                                                                                  | 
  {                                                                                                                 | 
    return -1073741820;                                                                                             | 
  }                                                                                                                 | 
  Object = 0LL;                                                                                                     | 
  result = ObReferenceObjectByHandle(Handle, 8u, ExpWorkerFactoryObjectType, PreviousMode, &Object, 0LL);           | 
  if ( result >= 0 )                                                                                                | 
  {                                                                                                                 | 
    HIBYTE(v16) = 0;                                                                                                | 
    HIDWORD(v18) = 0;                                                                                               | 
    HIDWORD(v22) = 0;                                                                                               | 
    v12 = (char *)Object;                                                                                           | 
    KeAcquireInStackQueuedSpinLock(*((PKSPIN_LOCK *)Object + 2), &LockHandle);                                      | 
    *(_QWORD *)&v16 = *((_QWORD *)v12 + 14);                                                                        | 
    WORD4(v16) = 0;                                                                                                 | 
    BYTE10(v16) = (v12[408] & 8) != 0;                                                                              | 
    v13 = *((_QWORD *)v12 + 2);                                                                                     | 
    v14 = *(_DWORD *)(v13 + 28);                                                                                    | 
    BYTE11(v16) = v14 == 0;                                                                                         | 
    BYTE12(v16) = *((_DWORD *)v12 + 100) != 0;                                                                      | 
    *(_WORD *)((char *)&v16 + 13) = *(_WORD *)(v13 + 32);                                                           | 
    LODWORD(v17) = *((_DWORD *)v12 + 101);                                                                          | 
    *(_QWORD *)((char *)&v17 + 4) = *((_QWORD *)v12 + 47);                                                          | 
    HIDWORD(v17) = *((_DWORD *)v12 + 98);                                                                           | 
    LODWORD(v18) = v14;                                                                                             | 
    DWORD1(v18) = *((_DWORD *)v12 + 97);                                                                            | 
    DWORD2(v18) = *(_DWORD *)(v13 + 24);                                                                            | 
    *(_QWORD *)&v19 = 0LL;                                                                                          | 
    *((_QWORD *)&v19 + 1) = *((_QWORD *)v12 + 3);                                                                   | 
    *(_QWORD *)&v20 = *((_QWORD *)v12 + 4);                                                                         | 
    *((_QWORD *)&v20 + 1) = *(_QWORD *)(*((_QWORD *)v12 + 6) + 464LL);                                              | 
    v21 = *(_OWORD *)(v12 + 56);                                                                                    | 
    LODWORD(v22) = *((_DWORD *)v12 + 104);                                                                          | 
    KeReleaseInStackQueuedSpinLock(&LockHandle);                                                                    | 
    ObfDereferenceObjectWithTag(v12, 0x746C6644u);                                                                  | 
    *(_OWORD *)a3 = 0LL;                                                                                            | 
    *(_OWORD *)(a3 + 16) = v16;                                                                                     | 
    *(_OWORD *)(a3 + 32) = v17;                                                                                     | 
    *(_OWORD *)(a3 + 48) = v18;                                                                                     | 
    *(_OWORD *)(a3 + 64) = v19;                                                                                     | 
    *(_OWORD *)(a3 + 80) = v20;                                                                                     | 
    *(_OWORD *)(a3 + 96) = v21;                                                                                     | 
    *(_QWORD *)(a3 + 112) = v22;                                                                                    | 
    return 0;                                                                                                       | 
  }                                                                                                                 | 
  return result;                                                                                                    | 
}                                                                                                                   | 
84 row(s)
