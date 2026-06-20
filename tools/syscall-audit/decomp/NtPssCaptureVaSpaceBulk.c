__int64 __fastcall NtPssCaptureVaSpaceBulk(ULONG_PTR a1, unsigned __int64 a2, __int128 *a3, SIZE_T a4, __int64 *a5)   | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  MappedSystemVa = a3;                                                                                                | 
  v17 = a2;                                                                                                           | 
  BugCheckParameter1 = a1;                                                                                            | 
  v25 = a1;                                                                                                           | 
  v26 = a2;                                                                                                           | 
  v27 = a3;                                                                                                           | 
  v28 = a4;                                                                                                           | 
  v20 = a5;                                                                                                           | 
  Object = 0LL;                                                                                                       | 
  memset(v30, 0, sizeof(v30));                                                                                        | 
  v23 = 0LL;                                                                                                          | 
  v18 = 0LL;                                                                                                          | 
  v7 = 0;                                                                                                             | 
  v8 = 0LL;                                                                                                           | 
  MemoryDescriptorList = 0LL;                                                                                         | 
  VirtualMemory = 0;                                                                                                  | 
  if ( a4 < 0x10 )                                                                                                    | 
  {                                                                                                                   | 
    return 3221225476LL;                                                                                              | 
  }                                                                                                                   | 
  CurrentThread = KeGetCurrentThread();                                                                               | 
  PreviousMode = CurrentThread->PreviousMode;                                                                         | 
  if ( PreviousMode )                                                                                                 | 
  {                                                                                                                   | 
    v24 = 0LL;                                                                                                        | 
    ProbeForWrite(a3, a4, 8u);                                                                                        | 
    if ( a5 )                                                                                                         | 
    {                                                                                                                 | 
      v12 = 0x7FFFFFFF0000LL;                                                                                         | 
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                  | 
      {                                                                                                               | 
        v12 = (__int64)a5;                                                                                            | 
      }                                                                                                               | 
      *(_QWORD *)v12 = *(_QWORD *)v12;                                                                                | 
    }                                                                                                                 | 
    v24 = *MappedSystemVa;                                                                                            | 
    a2 = v17;                                                                                                         | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v24 = *a3;                                                                                                        | 
  }                                                                                                                   | 
  if ( !(_DWORD)v24 )                                                                                                 | 
  {                                                                                                                   | 
    goto LABEL_11;                                                                                                    | 
  }                                                                                                                   | 
  if ( (v24 & 0xFFFFFFFC) != 0 )                                                                                      | 
  {                                                                                                                   | 
    VirtualMemory = -1073741637;                                                                                      | 
    goto LABEL_46;                                                                                                    | 
  }                                                                                                                   | 
  if ( a2 > 0x7FFFFFFEFFFFLL )                                                                                        | 
  {                                                                                                                   | 
    VirtualMemory = -1073741503;                                                                                      | 
    goto LABEL_46;                                                                                                    | 
  }                                                                                                                   | 
  if ( !PreviousMode )                                                                                                | 
  {                                                                                                                   | 
    v8 = 0LL;                                                                                                         | 
    goto LABEL_27;                                                                                                    | 
  }                                                                                                                   | 
  if ( !(unsigned int)Feature_1462962491__private_IsEnabledDeviceUsageNoInline() || a4 <= 0xFFFFFFFF )                | 
  {                                                                                                                   | 
    Mdl = IoAllocateMdl(MappedSystemVa, a4, 0, 0, 0LL);                                                               | 
    v8 = Mdl;                                                                                                         | 
    MemoryDescriptorList = Mdl;                                                                                       | 
    if ( !Mdl                                                                                                         | 
      || ((MmProbeAndLockPages(Mdl, 0, IoWriteAccess), v7 = 2, (v8->MdlFlags & 5) == 0)                               | 
        ? (MappedSystemVa = (__int128 *)MmMapLockedPagesSpecifyCache(v8, 0, MmCached, 0LL, 0, 0x40000010u))           | 
        : (MappedSystemVa = (__int128 *)v8->MappedSystemVa),                                                          | 
          !MappedSystemVa) )                                                                                          | 
    {                                                                                                                 | 
      VirtualMemory = -1073741670;                                                                                    | 
      goto LABEL_46;                                                                                                  | 
    }                                                                                                                 | 
LABEL_27:                                                                                                             | 
    if ( BugCheckParameter1 != -1LL )                                                                                 | 
    {                                                                                                                 | 
      VirtualMemory = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x41737350u, (__int64)&Object, 0LL, 0LL); | 
      if ( VirtualMemory < 0 )                                                                                        | 
      {                                                                                                               | 
        goto LABEL_46;                                                                                                | 
      }                                                                                                               | 
      if ( (*((_DWORD *)Object + 383) & 0x1000) != 0 )                                                                | 
      {                                                                                                               | 
        ObfDereferenceObjectWithTag(Object, 0x41737350u);                                                             | 
        VirtualMemory = -1073741790;                                                                                  | 
        goto LABEL_46;                                                                                                | 
      }                                                                                                               | 
      KiStackAttachProcess((ULONG_PTR)Object);                                                                        | 
      v7 |= 1u;                                                                                                       | 
    }                                                                                                                 | 
    if ( PreviousMode )                                                                                               | 
    {                                                                                                                 | 
      VirtualMemory = ZwPssCaptureVaSpaceBulk(-1LL, v17, MappedSystemVa, a4, &v18);                                   | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      *((_DWORD *)MappedSystemVa + 1) = 0;                                                                            | 
      v18 = 16LL;                                                                                                     | 
      v14 = a4 - 16;                                                                                                  | 
      v15 = MappedSystemVa + 1;                                                                                       | 
      while ( v14 >= 0x30 )                                                                                           | 
      {                                                                                                               | 
        if ( (*(_DWORD *)(&CurrentThread[1].SwapListEntry + 1) & 1) != 0 )                                            | 
        {                                                                                                             | 
          VirtualMemory = -1073741749;                                                                                | 
          goto LABEL_46;                                                                                              | 
        }                                                                                                             | 
        VirtualMemory = MmQueryVirtualMemory(0xFFFFFFFFFFFFFFFFuLL, 48, (__int64)&v23, 2);                            | 
        if ( VirtualMemory < 0 )                                                                                      | 
        {                                                                                                             | 
          if ( *((_DWORD *)MappedSystemVa + 1) )                                                                      | 
          {                                                                                                           | 
            VirtualMemory = 0;                                                                                        | 
          }                                                                                                           | 
          break;                                                                                                      | 
        }                                                                                                             | 
        v14 -= v23;                                                                                                   | 
        v18 += v23;                                                                                                   | 
        v16 = *v15 + v15[3];                                                                                          | 
        v15 += 6;                                                                                                     | 
        ++*((_DWORD *)MappedSystemVa + 1);                                                                            | 
        *((_QWORD *)MappedSystemVa + 1) = v16;                                                                        | 
      }                                                                                                               | 
      if ( VirtualMemory >= 0 && *((_QWORD *)MappedSystemVa + 1) != 0x7FFFFFFF0000LL )                                | 
      {                                                                                                               | 
        VirtualMemory = 261;                                                                                          | 
      }                                                                                                               | 
    }                                                                                                                 | 
    goto LABEL_46;                                                                                                    | 
  }                                                                                                                   | 
LABEL_11:                                                                                                             | 
  VirtualMemory = -1073741811;                                                                                        | 
LABEL_46:                                                                                                             | 
  if ( (v7 & 1) != 0 )                                                                                                | 
  {                                                                                                                   | 
    KiUnstackDetachProcess(v30, 0LL);                                                                                 | 
  }                                                                                                                   | 
  if ( v8 )                                                                                                           | 
  {                                                                                                                   | 
    if ( v7 >= 2 )                                                                                                    | 
    {                                                                                                                 | 
      MmUnlockPages(v8);                                                                                              | 
    }                                                                                                                 | 
    IoFreeMdl(v8);                                                                                                    | 
  }                                                                                                                   | 
  if ( a5 )                                                                                                           | 
  {                                                                                                                   | 
    *a5 = v18;                                                                                                        | 
  }                                                                                                                   | 
  return (unsigned int)VirtualMemory;                                                                                 | 
}                                                                                                                     | 
157 row(s)
