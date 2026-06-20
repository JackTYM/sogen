__int64 __fastcall NtAlpcOpenSenderProcess(_QWORD *a1, void *a2, unsigned __int64 a3, int a4, int a5, __int128 *Src)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v28 = 0LL;
  v29 = 0LL;
  v30 = 0LL;
  v32 = 0LL;
  v33 = 0LL;
  *(_QWORD *)&v34 = 0LL;
  DWORD2(v34) = 0;
  v31 = 0LL;
  BugCheckParameter2 = 0LL;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v11 = ObReferenceObjectByHandle(a2, 0x20000u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v11 < 0 )
  {
    goto LABEL_24;
  }
  if ( PreviousMode )
  {
    v13 = 0x7FFFFFFF0000LL;
    v14 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )
    {
      v14 = (__int64)a1;
    }
    *(_QWORD *)v14 = *(_QWORD *)v14;
    v35 = 0LL;
    v36 = 0LL;
    v15 = 0x7FFFFFFF0000LL;
    if ( (a4 & 0xC0000000) == 0x80000000 )
    {
      if ( a3 < 0x7FFFFFFF0000LL )
      {
        v15 = a3;
      }
      RtlCopyVolatileMemory(&v35, (const void *)v15, 0x18uLL);
      LOWORD(v28) = v35;
      WORD1(v28) = v35 + 40;
      DWORD1(v28) = DWORD1(v35);
      *((_QWORD *)&v28 + 1) = DWORD2(v35);
      *(_QWORD *)&v29 = HIDWORD(v35);
      DWORD2(v29) = v36;
      v30 = HIDWORD(v36);
    }
    else
    {
      if ( a3 < 0x7FFFFFFF0000LL )
      {
        v15 = a3;
      }
      RtlCopyVolatileMemory(&v28, (const void *)v15, 0x28uLL);
    }
    if ( (unsigned __int64)Src < 0x7FFFFFFF0000LL )
    {
      v13 = (__int64)Src;
    }
    RtlCopyVolatileMemory(&v32, (const void *)v13, 0x30uLL);
  }
  else
  {
    v28 = *(_OWORD *)a3;
    v29 = *(_OWORD *)(a3 + 16);
    v30 = *(_QWORD *)(a3 + 32);
    v32 = *Src;
    v33 = Src[1];
    v34 = Src[2];
  }
  v16 = Object;
  v11 = AlpcpLookupMessage((_DWORD)Object, DWORD2(v29), v30, v12, (__int64)&BugCheckParameter2);
  if ( v11 < 0 )
  {
    ObfDereferenceObject(v16);
    goto LABEL_24;
  }
  v17 = BugCheckParameter2;
  if ( (*(_DWORD *)(BugCheckParameter2 + 40) & 0x80u) != 0 )
  {
    AlpcpUnlockMessage(BugCheckParameter2);
    ObfDereferenceObject(v16);
    v11 = -1073740029;
    goto LABEL_24;
  }
  v18 = *(_QWORD **)(BugCheckParameter2 + 32);
  if ( v18 )
  {
    v19 = v18[161] - *((_QWORD *)&v28 + 1);
    if ( !v19 )
    {
      v19 = v18[162] - v29;
    }
    if ( v19 )
    {
      AlpcpUnlockMessage(BugCheckParameter2);
      ObfDereferenceObject(v16);
      v11 = -1073741813;
      goto LABEL_24;
    }
    v20 = (_QWORD *)v18[68];
    ObfReferenceObjectWithTag(v20, 0x63706C41u);
    goto LABEL_22;
  }
  v22 = *(_QWORD *)(BugCheckParameter2 + 24);
  if ( !v22 )
  {
    AlpcpUnlockMessage(BugCheckParameter2);
    v25 = v16;
    goto LABEL_38;
  }
  v23 = (volatile signed __int64 *)(v22 + 352);
  v24 = KeAbPreAcquire(v22 + 352, 0LL, 0LL);
  if ( _InterlockedCompareExchange64((volatile signed __int64 *)(v22 + 352), 17LL, 0LL) )
  {
    ExfAcquirePushLockSharedEx(v22 + 352, 0LL, v24, v22 + 352);
  }
  if ( v24 )
  {
    *(_BYTE *)(v24 + 10) = 1;
  }
  if ( (*(_DWORD *)(v22 + 416) & 0x40) == 0 )
  {
    v20 = *(_QWORD **)(v22 + 24);
    if ( v20[58] == *((_QWORD *)&v28 + 1) )
    {
      ObfReferenceObjectWithTag(v20, 0x63706C41u);
      if ( _InterlockedCompareExchange64(v23, 0LL, 17LL) != 17 )
      {
        ExfReleasePushLockShared(v23);
      }
      KeAbPostRelease((ULONG_PTR)v23);
      v17 = BugCheckParameter2;
      v16 = Object;
LABEL_22:
      AlpcpUnlockMessage(v17);
      *(_QWORD *)&v29 = 0LL;
      v11 = PsOpenProcess((unsigned int)&v31, a5, (unsigned int)&v32, (unsigned int)&v28 + 8, 0, PreviousMode);
      ObfDereferenceObjectWithTag(v20, 0x63706C41u);
      ObfDereferenceObject(v16);
      if ( v11 >= 0 )
      {
        *a1 = v31;
      }
      goto LABEL_24;
    }
    if ( _InterlockedCompareExchange64(v23, 0LL, 17LL) != 17 )
    {
      ExfReleasePushLockShared(v23);
    }
    KeAbPostRelease((ULONG_PTR)v23);
    AlpcpUnlockMessage(BugCheckParameter2);
    v25 = Object;
LABEL_38:
    ObfDereferenceObject(v25);
    v11 = -1073741790;
    goto LABEL_24;
  }
  if ( _InterlockedCompareExchange64(v23, 0LL, 17LL) != 17 )
  {
    ExfReleasePushLockShared(v22 + 352);
  }
  KeAbPostRelease(v22 + 352);
  AlpcpUnlockMessage(BugCheckParameter2);
  ObfDereferenceObject(Object);
  v11 = -1073741769;
LABEL_24:
  KeLeaveCriticalRegionThread(KeGetCurrentThread());
  return (unsigned int)v11;
}
172 row(s)
