NTSTATUS __fastcall NtStartProfile(void *a1)                                                           | 
{                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                        | 
                                                                                                       | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                   | 
  Object = 0LL;                                                                                        | 
  result = ObReferenceObjectByHandle(a1, 1u, ExProfileObjectType, PreviousMode, &Object, 0LL);         | 
  v3 = (__int64 *)Object;                                                                              | 
  if ( result >= 0 )                                                                                   | 
  {                                                                                                    | 
    KeWaitForSingleObject(&ExpProfileStateMutex, Executive, 0, 0, 0LL);                                | 
    if ( v3[6] )                                                                                       | 
    {                                                                                                  | 
      v4 = -1073741640;                                                                                | 
LABEL_8:                                                                                               | 
      KeReleaseMutex(&ExpProfileStateMutex, 0);                                                        | 
      ObfDereferenceObject(v3);                                                                        | 
      return v4;                                                                                       | 
    }                                                                                                  | 
    if ( ExpCurrentProfileUsage == KeQueryActiveProcessorCountEx(0xFFFFu) << 13 )                      | 
    {                                                                                                  | 
      v4 = -1073741613;                                                                                | 
      goto LABEL_8;                                                                                    | 
    }                                                                                                  | 
    v5 = MmSizeOfMdl((PVOID)v3[3], *((unsigned int *)v3 + 8));                                         | 
    Pool2 = ExAllocatePool2(0x40uLL, v5 + 616, 0x666F7250u);                                           | 
    v7 = (char *)Pool2;                                                                                | 
    P = (PVOID)Pool2;                                                                                  | 
    if ( !Pool2 )                                                                                      | 
    {                                                                                                  | 
      v4 = -1073741670;                                                                                | 
      goto LABEL_8;                                                                                    | 
    }                                                                                                  | 
    v8 = (struct _MDL *)(Pool2 + 616);                                                                 | 
    v3[7] = Pool2 + 616;                                                                               | 
    v3[5] = Pool2;                                                                                     | 
    v9 = *((unsigned int *)v3 + 8);                                                                    | 
    v10 = v3[3];                                                                                       | 
    *(_QWORD *)(Pool2 + 616) = 0LL;                                                                    | 
    *(_WORD *)(Pool2 + 624) = 8 * ((((unsigned __int64)(v10 & 0xFFF) + v9 + 4095) >> 12) + 6);         | 
    *(_WORD *)(Pool2 + 626) = 0;                                                                       | 
    *(_QWORD *)(Pool2 + 648) = v10 & 0xFFFFFFFFFFFFF000uLL;                                            | 
    *(_DWORD *)(Pool2 + 660) = v10 & 0xFFF;                                                            | 
    *(_DWORD *)(Pool2 + 656) = v9;                                                                     | 
    MmProbeAndLockPages((PMDL)(Pool2 + 616), PreviousMode, IoWriteAccess);                             | 
    v11 = MmMapLockedPagesSpecifyCache((PMDL)v3[7], 0, MmCached, 0LL, 0, 0x40000010u);                 | 
    if ( v11 )                                                                                         | 
    {                                                                                                  | 
      v12 = *((_DWORD *)v3 + 17);                                                                      | 
      v13 = *((_DWORD *)v3 + 16);                                                                      | 
      v14 = *((_DWORD *)v3 + 9);                                                                       | 
      v15 = v3[2];                                                                                     | 
      v16 = v3[1];                                                                                     | 
      v17 = *v3;                                                                                       | 
      *(_DWORD *)v7 = 40370199;                                                                        | 
      *((_QWORD *)v7 + 3) = v17;                                                                       | 
      *((_QWORD *)v7 + 7) = v11;                                                                       | 
      *((_QWORD *)v7 + 4) = v16;                                                                       | 
      *((_QWORD *)v7 + 5) = v16 + v15;                                                                 | 
      *((_DWORD *)v7 + 12) = v14 - 2;                                                                  | 
      v7[610] = 0;                                                                                     | 
      *((_DWORD *)v7 + 16) = v13;                                                                      | 
      *((_WORD *)v7 + 304) = v12;                                                                      | 
      *((_QWORD *)v7 + 9) = 2097153LL;                                                                 | 
      memset_0(v7 + 80, 0, 0x100uLL);                                                                  | 
      if ( v3 == (__int64 *)-72LL )                                                                    | 
      {                                                                                                | 
        v18 = 0;                                                                                       | 
      }                                                                                                | 
      else                                                                                             | 
      {                                                                                                | 
        v18 = KeAndAffinityEx2(v3 + 9, &KeActiveProcessors, v7 + 72);                                  | 
      }                                                                                                | 
      if ( !v18 )                                                                                      | 
      {                                                                                                | 
        KiCopyAffinityEx((struct _KAFFINITY_EX *)(v7 + 72), *((_WORD *)v7 + 37), &KeActiveProcessors); | 
      }                                                                                                | 
      started = KeStartProfile(v7);                                                                    | 
      if ( started >= 0 )                                                                              | 
      {                                                                                                | 
        v3[6] = (__int64)v11;                                                                          | 
        ++ExpCurrentProfileUsage;                                                                      | 
        KeReleaseMutex(&ExpProfileStateMutex, 0);                                                      | 
        ObfDereferenceObject(v3);                                                                      | 
        return 0;                                                                                      | 
      }                                                                                                | 
      else                                                                                             | 
      {                                                                                                | 
        KeReleaseMutex(&ExpProfileStateMutex, 0);                                                      | 
        MmUnlockPages(v8);                                                                             | 
        ExFreePoolWithTag(v7, 0);                                                                      | 
        ObfDereferenceObject(v3);                                                                      | 
        return started;                                                                                | 
      }                                                                                                | 
    }                                                                                                  | 
    else                                                                                               | 
    {                                                                                                  | 
      KeReleaseMutex(&ExpProfileStateMutex, 0);                                                        | 
      MmUnlockPages(v8);                                                                               | 
      ExFreePoolWithTag(v7, 0);                                                                        | 
      ObfDereferenceObject(v3);                                                                        | 
      return -1073741670;                                                                              | 
    }                                                                                                  | 
  }                                                                                                    | 
  return result;                                                                                       | 
}                                                                                                      | 
106 row(s)
