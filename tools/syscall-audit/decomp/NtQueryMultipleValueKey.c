line
__int64 __fastcall NtQueryMultipleValueKey(
        int a1,
        volatile void *a2,
        unsigned int a3,
        volatile void *a4,
        unsigned int *a5,
        _DWORD *a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  Address = a4;
  v6 = a3;
  v33[2] = a2;
  v27 = a3;
  v33[3] = a5;
  v42 = 0LL;
  v43 = 0LL;
  Length = 0;
  v30[0] = 0;
  v37 = 0;
  memset(&ApcState, 0, sizeof(ApcState));
  memset_0(&v51, 0, 0x58uLL);
  memset(v49, 0, sizeof(v49));
  v34 = 0LL;
  if ( CmpTraceRoutine )
  {
    EtwGetKernelTraceTimestamp(v49, 0x20000LL);
  }
  v23 = 0;
  v24 = 0;
  Object = 0LL;
  memset_0(v44, 0, 0x48uLL);
  v33[1] = v33;
  v33[0] = v33;
  CmpInitializeThreadInfo(&v42);
  v32 = 0LL;
  Privileges = 0LL;
  v35 = 0LL;
  v36 = 0LL;
  Length_4 = CmpAcquireShutdownRundown();
  if ( !Length_4 )
  {
    MultipleValueKey = -1073741431;
    goto LABEL_30;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  LOBYTE(v11) = PreviousMode;
  MultipleValueKey = CmObReferenceObjectByHandle(a1, 1, i, v11, (__int64)&Object, 0LL);
  if ( MultipleValueKey >= 0 )
  {
    if ( CmpTraceRoutine && Object )
    {
      v34 = *((_QWORD *)Object + 1);
    }
    if ( (_BYTE)PreviousMode == 1 )
    {
      v14 = 0x7FFFFFFF0000LL;
      v15 = 0x7FFFFFFF0000LL;
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )
      {
        v15 = (__int64)a5;
      }
      Length = *(_DWORD *)v15;
      if ( (unsigned int)v6 > 0x10000 )
      {
        RtlRaiseStatus(3221225626LL);
      }
      ProbeForWrite(a2, 24 * v6, 4u);
      if ( a6 )
      {
        if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )
        {
          v14 = (__int64)a6;
        }
        *(_DWORD *)v14 = *(_DWORD *)v14;
      }
      ProbeForWrite(Address, Length, 4u);
    }
    else
    {
      Length = *a5;
    }
    LOBYTE(i) = PreviousMode;
    MultipleValueKey = CmpCaptureKeyValueArray(
                         (_DWORD)a2,
                         v6,
                         i,
                         (unsigned int)&Privileges,
                         (__int64)&v35,
                         (__int64)&v36);
    if ( MultipleValueKey >= 0 )
    {
      CurrentThread = KeGetCurrentThread();
      --CurrentThread->KernelApcDisable;
      v24 = 1;
      if ( !CmpCallBackCount || (unsigned int)((__int64 (*)(void))CmpIsRegistryLockAcquired)() )
      {
LABEL_22:
        LOBYTE(v9) = PreviousMode;
        MultipleValueKey = CmKeyBodyRemapToVirtualForEnum(&Object, v9, 1LL, &v32);
        if ( MultipleValueKey >= 0 )
        {
          MultipleValueKey = CmpBounceContextStart(&v51, Address, Length, PreviousMode, 4);
          if ( MultipleValueKey >= 0 )
          {
            CmpAttachToRegistryProcess(&ApcState);
            MultipleValueKey = CmQueryMultipleValueKey(
                                 (_DWORD)Object,
                                 (_DWORD)v32,
                                 (_DWORD)Privileges,
                                 v6,
                                 (__int64)Src,
                                 (__int64)&Length,
                                 (__int64)v30);
            CmpDetachFromRegistryProcess(&ApcState);
            if ( a6 )
            {
              *a6 = v30[0];
            }
            if ( (int)(MultipleValueKey + 0x80000000) < 0 || MultipleValueKey == -2147483643 )
            {
              i = Length;
              v9 = (__int64)Src;
              if ( v51 != Src )
              {
                memmove(v51, Src, Length);
              }
            }
          }
        }
        goto LABEL_30;
      }
      v44[0] = Object;
      v44[1] = Privileges;
      v45 = v6;
      v46 = Address;
      p_Length = &Length;
      v48 = a6;
      LOBYTE(v17) = 1;
      v18 = CmpCallCallBacksEx(9, (unsigned int)v44, 0, v17, 24, (__int64)Object, (__int64)v33);
      if ( v18 >= 0 )
      {
        v23 = 1;
        goto LABEL_22;
      }
      MultipleValueKey = 0;
      if ( v18 != -1073740541 )
      {
        MultipleValueKey = v18;
      }
    }
  }
LABEL_30:
  v19 = v32;
  if ( v32 )
  {
    ObfDereferenceObject(v32);
  }
  if ( v23 && CmpCallBackCount && !(unsigned int)CmpIsRegistryLockAcquired(v19, v9, i) && (_QWORD *)v33[0] != v33 )
  {
    v40 = 0LL;
    v41 = 0LL;
    v38[0] = Object;
    v38[1] = (unsigned int)MultipleValueKey;
    v39 = (unsigned int)MultipleValueKey;
    v38[2] = v44;
    CmpCallCallBacksEx(24, (unsigned int)v38, 0, 0, 24, (__int64)Object, (__int64)v33);
    MultipleValueKey = v39;
  }
  if ( Privileges )
  {
    *a5 = Length;
    if ( ((MultipleValueKey + 0x80000000) & 0x80000000) != 0 || MultipleValueKey == -2147483643 )
    {
      for ( i = 0LL; ; i = (unsigned int)(i + 1) )
      {
        v30[1] = i;
        if ( (unsigned int)i >= (unsigned int)v6 )
        {
          break;
        }
        v9 = 3 * i;
        v21 = Privileges;
        *((_DWORD *)a2 + 2 * v9 + 2) = Privileges->Privilege[2 * i].Luid.LowPart;
        *((_DWORD *)a2 + 2 * v9 + 3) = v21->Privilege[2 * i].Luid.HighPart;
        *((_DWORD *)a2 + 2 * v9 + 4) = v21->Privilege[2 * i].Attributes;
      }
    }
  }
  if ( v24 )
  {
    KeLeaveCriticalRegion();
  }
  if ( Object )
  {
    ObfDereferenceObject(Object);
  }
  CmpBounceContextCleanup(&v51, v9, i);
  if ( CmpTraceRoutine )
  {
    v22 = v34;
    LOBYTE(v22) = 19;
    guard_dispatch_icall_no_overrides(v22, v49, (unsigned int)MultipleValueKey, (unsigned int)v6, v34, 0LL);
  }
  if ( Length_4 )
  {
    CmpReleaseShutdownRundown();
  }
  if ( Privileges )
  {
    CmSiFreeMemory(Privileges);
  }
  if ( v35 )
  {
    CmSiFreeMemory(v35);
  }
  if ( v36 )
  {
    CmSiFreeMemory(v36);
  }
  CmpCleanupThreadInfo(&v42);
  return (unsigned int)MultipleValueKey;
}
