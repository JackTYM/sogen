__int64 __fastcall NtAlpcSetInformation(void *a1, int a2, __int64 *a3, unsigned int a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v4 = a4;
  Handle = a1;
  v39 = a3;
  memset_0(&v42, 0, 0x48uLL);
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v8 = 0;
  if ( !a1 || !v39 && a2 != 7 && a2 != 10 )
  {
    v12 = -1073741811;
    goto LABEL_16;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v37 = PreviousMode;
  v10 = (__int64)v39;
  v11 = v39;
  v41 = (unsigned int *)v39;
  if ( (_DWORD)v4 && PreviousMode )
  {
    if ( (unsigned int)v4 > 0x48 )
    {
      v12 = -1073741820;
      goto LABEL_16;
    }
    if ( (unsigned __int64)v39 >= 0x7FFFFFFF0000LL )
    {
      v10 = 0x7FFFFFFF0000LL;
    }
    v39 = (__int64 *)v10;
    memmove(&v42, (const void *)v10, v4);
    v11 = &v42;
    v41 = (unsigned int *)&v42;
  }
  Object = 0LL;
  v12 = ObReferenceObjectByHandle(Handle, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v12 >= 0 )
  {
    v13 = a2 - 1;
    if ( !v13 )
    {
      if ( (_DWORD)v4 != 72 )
      {
        goto LABEL_63;
      }
      if ( (*(_DWORD *)v11 & 0xFC00FFFF) == 0 )
      {
        *((_DWORD *)Object + 64) ^= (*(_DWORD *)v11 ^ *((_DWORD *)Object + 64)) & 0x20000;
        goto LABEL_15;
      }
      goto LABEL_39;
    }
    v14 = v13 - 1;
    if ( !v14 )
    {
      if ( (_DWORD)v4 == 16 )
      {
        v15 = AlpcpAssociateIoCompletionPort(Object, v11[1], *v11);
LABEL_14:
        v12 = v15;
LABEL_15:
        ObfDereferenceObject(Object);
        goto LABEL_16;
      }
LABEL_63:
      v12 = -1073741820;
      goto LABEL_15;
    }
    v17 = v14 - 3;
    if ( !v17 )
    {
      v12 = (_DWORD)v4 != 16 ? 0xC000000D : 0;
      goto LABEL_15;
    }
    v18 = v17 - 1;
    if ( v18 )
    {
      v19 = v18 - 1;
      if ( v19 )
      {
        v20 = v19 - 1;
        if ( !v20 )
        {
          if ( (_DWORD)v4 == 4 && *(_DWORD *)v11 )
          {
            v21 = (volatile signed __int64 *)((char *)Object + 352);
            v22 = KeAbPreAcquire((char *)Object + 352, 0LL, 0LL);
            if ( _InterlockedCompareExchange64(v21, 17LL, 0LL) )
            {
              ExfAcquirePushLockSharedEx(v21, 0LL, v22, v21);
            }
            if ( v22 )
            {
              *(_BYTE *)(v22 + 10) = 1;
            }
            if ( *((_QWORD *)Object + 45) )
            {
              AlpcpAdjustCompletionListConcurrencyCount(Object, *v41);
              v12 = 0;
            }
            else
            {
              v12 = -1073741811;
            }
            if ( _InterlockedCompareExchange64(v21, 0LL, 17LL) != 17 )
            {
              ExfReleasePushLockShared(v21);
            }
            v28 = (ULONG_PTR)v21;
LABEL_52:
            KeAbPostRelease(v28);
            goto LABEL_15;
          }
          goto LABEL_39;
        }
        v24 = v20 - 1;
        if ( v24 )
        {
          if ( v24 == 1 && !(_DWORD)v4 )
          {
            v25 = (volatile signed __int32 *)((char *)Object + 352);
            v26 = KeAbPreAcquire((char *)Object + 352, 0LL, 0LL);
            v27 = v26;
            if ( _interlockedbittestandset64(v25, 0LL) )
            {
              ExfAcquirePushLockExclusiveEx(v25, v26, v25);
            }
            if ( v27 )
            {
              *(_BYTE *)(v27 + 10) = 1;
            }
            if ( *((_QWORD *)Object + 45) )
            {
              *((_DWORD *)Object + 104) &= ~0x10000u;
              v12 = 0;
            }
            else
            {
              v12 = -1073741811;
            }
            if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v25, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )
            {
              ExfTryToWakePushLock(v25);
            }
            v28 = (ULONG_PTR)v25;
            goto LABEL_52;
          }
          goto LABEL_39;
        }
        if ( v37 )
        {
          goto LABEL_39;
        }
        PsReferenceSiloContext(*(_QWORD *)v10);
        v29 = (volatile signed __int32 *)((char *)Object + 352);
        v32 = KeAbPreAcquire((char *)Object + 352, 0LL, 0LL);
        v33 = v32;
        if ( _interlockedbittestandset64(v29, 0LL) )
        {
          ExfAcquirePushLockExclusiveEx(v29, v32, v29);
        }
        if ( v33 )
        {
          *(_BYTE *)(v33 + 10) = 1;
        }
        v35 = v39;
        v36 = Object;
        *((_QWORD *)Object + 46) = *v39;
        v36[47] = v35[1];
      }
      else
      {
        if ( (_DWORD)v4 )
        {
          goto LABEL_39;
        }
        v29 = (volatile signed __int32 *)((char *)Object + 352);
        v30 = KeAbPreAcquire((char *)Object + 352, 0LL, 0LL);
        v31 = v30;
        if ( _interlockedbittestandset64(v29, 0LL) )
        {
          ExfAcquirePushLockExclusiveEx(v29, v30, v29);
        }
        if ( v31 )
        {
          *(_BYTE *)(v31 + 10) = 1;
        }
        if ( *((_QWORD *)Object + 45) )
        {
          AlpcpFreeCompletionList();
        }
      }
      if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v29, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )
      {
        ExfTryToWakePushLock(v29);
      }
      KeAbPostRelease((ULONG_PTR)v29);
      v12 = 0;
      goto LABEL_15;
    }
    if ( (*((_DWORD *)Object + 104) & 6) == 2 )
    {
      if ( (_DWORD)v4 == 24 )
      {
        v23 = *v11;
      }
      else
      {
        if ( (_DWORD)v4 != 16 )
        {
          goto LABEL_39;
        }
        v34 = *(__m128i *)v11;
        v11 = &v42;
        v23 = v34.m128i_u32[0];
        v42 = v34.m128i_u32[0];
        v43 = v34.m128i_i32[1];
        v44 = _mm_srli_si128(v34, 8).m128i_u64[0];
        v8 = 1;
      }
      v15 = AlpcpInitializeCompletionList(
              Object,
              v23,
              *((unsigned int *)v11 + 2),
              *((unsigned int *)v11 + 3),
              *((_DWORD *)v11 + 4),
              v8);
      goto LABEL_14;
    }
LABEL_39:
    v12 = -1073741811;
    goto LABEL_15;
  }
LABEL_16:
  KeLeaveCriticalRegion();
  return (unsigned int)v12;
}
240 row(s)
