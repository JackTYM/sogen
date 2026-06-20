__int64 __fastcall NtProtectVirtualMemory(ULONG_PTR a1, PVOID *a2, __int64 *a3, unsigned int a4, _DWORD *a5) | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  v5 = a4;                                                                                                   | 
  v6 = a3;                                                                                                   | 
  v33 = a3;                                                                                                  | 
  v7 = a2;                                                                                                   | 
  v34 = a2;                                                                                                  | 
  v8 = a1;                                                                                                   | 
  v9 = (unsigned __int64)a5;                                                                                 | 
  v35 = a5;                                                                                                  | 
  memset(v36, 0, sizeof(v36));                                                                               | 
  v10 = 0;                                                                                                   | 
  Object = 0LL;                                                                                              | 
  BaseAddress = 0LL;                                                                                         | 
  *(_QWORD *)v29 = 0LL;                                                                                      | 
  v28 = 0;                                                                                                   | 
  if ( (_DWORD)v5 == 0x80000000 || (_DWORD)v5 == 0x10000000 )                                                | 
  {                                                                                                          | 
    ProtectionMask = 24;                                                                                     | 
  }                                                                                                          | 
  else                                                                                                       | 
  {                                                                                                          | 
    ProtectionMask = MiMakeProtectionMask(v5 & 0xFF807FF, a2, a2, v6);                                       | 
    if ( ProtectionMask == -1 )                                                                              | 
    {                                                                                                        | 
      return 3221225541LL;                                                                                   | 
    }                                                                                                        | 
  }                                                                                                          | 
  CurrentThread = KeGetCurrentThread();                                                                      | 
  Process = CurrentThread->ApcState.Process;                                                                 | 
  if ( CurrentThread->PreviousMode )                                                                         | 
  {                                                                                                          | 
    v13 = 0x7FFFFFFF0000LL;                                                                                  | 
    v14 = 0x7FFFFFFF0000LL;                                                                                  | 
    if ( (unsigned __int64)v7 < 0x7FFFFFFF0000LL )                                                           | 
    {                                                                                                        | 
      v14 = (__int64)v7;                                                                                     | 
    }                                                                                                        | 
    *(_QWORD *)v14 = *(_QWORD *)v14;                                                                         | 
    v15 = 0x7FFFFFFF0000LL;                                                                                  | 
    if ( (unsigned __int64)v6 < 0x7FFFFFFF0000LL )                                                           | 
    {                                                                                                        | 
      v15 = (__int64)v6;                                                                                     | 
    }                                                                                                        | 
    *(_QWORD *)v15 = *(_QWORD *)v15;                                                                         | 
    if ( v9 < 0x7FFFFFFF0000LL )                                                                             | 
    {                                                                                                        | 
      v13 = v9;                                                                                              | 
    }                                                                                                        | 
    *(_DWORD *)v13 = *(_DWORD *)v13;                                                                         | 
    v16 = (char *)*v7;                                                                                       | 
    BaseAddress = *v7;                                                                                       | 
    v17 = *v6;                                                                                               | 
    *(_QWORD *)v29 = *v6;                                                                                    | 
  }                                                                                                          | 
  else                                                                                                       | 
  {                                                                                                          | 
    v17 = *v6;                                                                                               | 
    *(_QWORD *)v29 = *v6;                                                                                    | 
    v16 = (char *)*v7;                                                                                       | 
    BaseAddress = *v7;                                                                                       | 
  }                                                                                                          | 
  if ( !v17 || &v16[v17 - 1] < v16 || (unsigned __int64)&v16[v17 - 1] > 0x7FFFFFFEFFFFLL )                   | 
  {                                                                                                          | 
    return 3221225485LL;                                                                                     | 
  }                                                                                                          | 
  result = ObpReferenceObjectByHandleWithTag(v8, 0x76506D4Du, (__int64)&Object, 0LL, 0LL);                   | 
  if ( (int)result >= 0 )                                                                                    | 
  {                                                                                                          | 
    v19 = Object;                                                                                            | 
    if ( Process != Object )                                                                                 | 
    {                                                                                                        | 
      KiStackAttachProcess((ULONG_PTR)Object);                                                               | 
      v10 = 1;                                                                                               | 
    }                                                                                                        | 
    v20 = v19[46];                                                                                           | 
    if ( (v20 & 1) != 0 )                                                                                    | 
    {                                                                                                        | 
      memset_0(v37, 0, 0x68uLL);                                                                             | 
      v38 = v20;                                                                                             | 
      v39 = Process;                                                                                         | 
      v40 = BaseAddress;                                                                                     | 
      v41 = *(_QWORD *)v29;                                                                                  | 
      v42 = v5;                                                                                              | 
      LOBYTE(v27) = 2;                                                                                       | 
      v22 = VslpEnterIumSecureMode(v27, 40LL, 0LL, v37);                                                     | 
      if ( v22 >= 0 )                                                                                        | 
      {                                                                                                      | 
        BaseAddress = v40;                                                                                   | 
        *(_QWORD *)v29 = v41;                                                                                | 
        v28 = v42;                                                                                           | 
      }                                                                                                      | 
    }                                                                                                        | 
    else                                                                                                     | 
    {                                                                                                        | 
      v22 = MmProtectVirtualMemory(                                                                          | 
              (_DWORD)Process,                                                                               | 
              (_DWORD)v19,                                                                                   | 
              (unsigned int)&BaseAddress,                                                                    | 
              (unsigned int)v29,                                                                             | 
              v5,                                                                                            | 
              (__int64)&v28);                                                                                | 
    }                                                                                                        | 
    LODWORD(Object) = v22;                                                                                   | 
    if ( v10 )                                                                                               | 
    {                                                                                                        | 
      KiUnstackDetachProcess(v36, 0LL);                                                                      | 
    }                                                                                                        | 
    if ( v22 >= 0 )                                                                                          | 
    {                                                                                                        | 
      v25 = MiMakeProtectionMask(v28 & 0xFFF807FF, v21, v23, v24);                                           | 
      if ( ((v25 | (unsigned __int8)ProtectionMask) & 2) != 0 )                                              | 
      {                                                                                                      | 
        if ( (unsigned int)PsIsProcessLoggingEnabled(Process, v19, 8LL) )                                    | 
        {                                                                                                    | 
          EtwTiLogProtectExecVm((ULONG_PTR)v19, BaseAddress, v29[0], v5, v26);                               | 
        }                                                                                                    | 
      }                                                                                                      | 
    }                                                                                                        | 
    ObfDereferenceObjectWithTag(v19, 0x76506D4Du);                                                           | 
    *v33 = *(_QWORD *)v29;                                                                                   | 
    *v34 = BaseAddress;                                                                                      | 
    *v35 = v28;                                                                                              | 
    return (unsigned int)v22;                                                                                | 
  }                                                                                                          | 
  return result;                                                                                             | 
}                                                                                                            | 
