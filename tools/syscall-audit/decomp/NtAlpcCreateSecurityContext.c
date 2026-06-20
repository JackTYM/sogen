__int64 __fastcall NtAlpcCreateSecurityContext(HANDLE Handle, int a2, __m128i *a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v31 = 0LL;
  LODWORD(v32) = 0;
  *(_QWORD *)&ClientSecurityQos.Length = 0LL;
  *(_DWORD *)&ClientSecurityQos.ContextTrackingMode = 0;
  v5 = 0LL;
  v28 = 0LL;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  if ( a2 )
  {
    v12 = -1073741811;
    goto LABEL_36;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v25 = PreviousMode;
  if ( PreviousMode )
  {
    if ( ((unsigned __int8)a3 & 7) != 0 )
    {
      ExRaiseDatatypeMisalignment();
    }
    v8 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )
    {
      v8 = (__int64)a3;
    }
    *(_BYTE *)v8 = *(_BYTE *)v8;
    *(_BYTE *)(v8 + 23) = *(_BYTE *)(v8 + 23);
    v31 = *a3;
    v32 = a3[1].m128i_i64[0];
    v9 = _mm_srli_si128(v31, 8).m128i_u64[0];
    v10 = (struct _SECURITY_QUALITY_OF_SERVICE *)v9;
    if ( v9 )
    {
      v11 = v9;
      if ( v9 >= 0x7FFFFFFF0000LL )
      {
        v11 = 0x7FFFFFFF0000LL;
      }
      RtlCopyVolatileMemory(&ClientSecurityQos, (const void *)v11, 0xCuLL);
      PreviousMode = v25;
    }
  }
  else
  {
    v31 = *a3;
    v32 = a3[1].m128i_i64[0];
    v10 = (struct _SECURITY_QUALITY_OF_SERVICE *)a3->m128i_i64[1];
    if ( v10 )
    {
      ClientSecurityQos = *v10;
    }
  }
  Object = 0LL;
  v12 = ObReferenceObjectByHandle(Handle, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  v13 = (volatile signed __int64 *)Object;
  if ( v12 >= 0 )
  {
    if ( !v10 )
    {
      ClientSecurityQos = *(struct _SECURITY_QUALITY_OF_SERVICE *)((char *)Object + 260);
    }
    ClientThread = KeGetCurrentThread();
    v14 = IoThreadToProcess(ClientThread);
    v27 = v14;
    Pool2 = ExAllocatePool2(0x100uLL, 0xA0uLL, 0x65536C41u);
    if ( !Pool2 )
    {
      goto LABEL_37;
    }
    *(_WORD *)(Pool2 + 16) = 768;
    *(_DWORD *)(Pool2 + 18) = 0;
    *(_WORD *)(Pool2 + 22) = 0;
    *(_QWORD *)(Pool2 + 40) = 0LL;
    *(_QWORD *)(Pool2 + 32) = 0LL;
    *(_QWORD *)(Pool2 + 8) = Pool2;
    *(_QWORD *)Pool2 = Pool2;
    *(_BYTE *)(Pool2 + 16) &= ~2u;
    *(_QWORD *)(Pool2 + 24) = 1LL;
    v16 = Pool2 + 48;
    if ( Pool2 == -48 )
    {
      goto LABEL_37;
    }
    memset_0((void *)(Pool2 + 48), 0, 0x70uLL);
    *(_QWORD *)(v16 + 8) = -1LL;
    _m_prefetchw(&v14[3].ProcessLock);
    while ( 1 )
    {
      v17 = *(_QWORD *)&v14[3].ProcessLock;
      if ( v17 < 0xA0 )
      {
        break;
      }
      v18 = *(_QWORD *)&v14[3].ProcessLock;
      if ( v18 == _InterlockedCompareExchange64((volatile signed __int64 *)&v14[3].ProcessLock, v17 - 160, v17) )
      {
        v12 = 0;
        goto LABEL_20;
      }
    }
    v12 = PsChargeProcessPagedPoolQuota(v14, 160LL);
LABEL_20:
    if ( v12 >= 0 )
    {
      v12 = SeCreateClientSecurity(ClientThread, &ClientSecurityQos, 0, (PSECURITY_CLIENT_CONTEXT)(v16 + 32));
      if ( v12 >= 0 )
      {
        v19 = v27;
        ObfReferenceObjectWithTag(v27, 0x63706C41u);
        *(_QWORD *)(v16 + 16) = v19;
        v20 = v13 + 44;
        v21 = KeAbPreAcquire(v13 + 44, 0LL, 0LL);
        if ( _InterlockedCompareExchange64(v13 + 44, 17LL, 0LL) )
        {
          ExfAcquirePushLockSharedEx(v13 + 44, 0LL, v21, v13 + 44);
        }
        if ( v21 )
        {
          *(_BYTE *)(v21 + 10) = 1;
        }
        if ( (v13[52] & 0x20) != 0 )
        {
          if ( _InterlockedCompareExchange64(v20, 0LL, 17LL) != 17 )
          {
            ExfReleasePushLockShared(v13 + 44);
          }
          KeAbPostRelease((ULONG_PTR)(v13 + 44));
        }
        else
        {
          AlpcpStartInitialization(v16);
          AlpcpReferenceBlob(v16);
          v22 = *((_QWORD *)v13 + 2) + 40LL;
          *(_QWORD *)v16 = v22;
          v30 = v16;
          v23 = AlpcAddHandleTableEntry(v22, &v30);
          *(_QWORD *)(v16 + 8) = v23;
          if ( v23 != -1 )
          {
            PsReferenceSiloContext(v13);
            *(_QWORD *)(v16 + 24) = v13;
            AlpcpInsertResourcePort(v13, v16);
            if ( _InterlockedCompareExchange64(v20, 0LL, 17LL) != 17 )
            {
              ExfReleasePushLockShared(v13 + 44);
            }
            KeAbPostRelease((ULONG_PTR)(v13 + 44));
            AlpcpEndInitialization(v16);
            v5 = v16;
            v28 = v16;
            v12 = 0;
            goto LABEL_33;
          }
          if ( _InterlockedCompareExchange64(v20, 0LL, 17LL) != 17 )
          {
            ExfReleasePushLockShared(v13 + 44);
          }
          KeAbPostRelease((ULONG_PTR)(v13 + 44));
          *(_QWORD *)v16 = 0LL;
          AlpcpEndInitialization(v16);
        }
        AlpcpDereferenceBlobEx(v16);
LABEL_37:
        v12 = -1073741670;
LABEL_35:
        ObfDereferenceObject((PVOID)v13);
        goto LABEL_36;
      }
      AlpcpReleasePagedPoolQuota(v27, 160LL);
    }
    AlpcpDereferenceBlobEx(v16);
LABEL_33:
    if ( v12 >= 0 )
    {
      a3[1].m128i_i64[0] = *(_QWORD *)(v5 + 8);
      AlpcpDereferenceBlobEx(v5);
    }
    goto LABEL_35;
  }
LABEL_36:
  KeLeaveCriticalRegionThread(KeGetCurrentThread());
  return (unsigned int)v12;
}
188 row(s)
