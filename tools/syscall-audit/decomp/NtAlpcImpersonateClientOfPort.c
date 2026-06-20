__int64 __fastcall NtAlpcImpersonateClientOfPort(HANDLE Handle, __int64 a2, unsigned __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v5 = 0LL;
  v51 = 0;
  v54 = 0;
  v6 = 0LL;
  v58 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( a2 )
  {
    v60 = KeGetCurrentThread();
    v8 = v60->PreviousMode;
    if ( v8 && (a2 & 3) != 0 )
    {
LABEL_4:
      ExRaiseDatatypeMisalignment();
    }
    if ( _bittest16((const signed __int16 *)(a2 + 4), 0xCu) )
    {
      v9 = *(_DWORD *)(a2 + 16);
      v51 = v9;
      v10 = *(_DWORD *)(a2 + 20);
    }
    else
    {
      if ( v8 && (a2 & 3) != 0 )
      {
        goto LABEL_4;
      }
      v9 = *(_DWORD *)(a2 + 24);
      v51 = v9;
      v10 = *(_DWORD *)(a2 + 32);
    }
    v54 = v10;
    if ( !v9 )
    {
LABEL_153:
      ClientSecurity = -1073741811;
      goto LABEL_59;
    }
  }
  else
  {
    v9 = v51;
  }
  v11 = a3 >> 2;
  v63 = a3 >> 2;
  if ( (unsigned int)(a3 >> 2) > 3 )
  {
    goto LABEL_153;
  }
  Process = (_KPROCESS *)(a3 & 1);
  v12 = (void *)((unsigned int)a3 & ((4 * (_DWORD)v11) | 2));
  v58 = v12;
  Object = 0LL;
  ClientSecurity = ObReferenceObjectByHandle(Handle, 1u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  v6 = Object;
  v53 = Object;
  if ( ClientSecurity < 0 )
  {
    goto LABEL_59;
  }
  if ( v9 )
  {
    if ( v9 < 0 )
    {
      if ( !Object )
      {
        ClientSecurity = -1073740030;
        goto LABEL_59;
      }
      v41 = 0LL;
      v42 = *((_QWORD *)Object + 2);
      if ( v42 )
      {
        v43 = (unsigned int)v9;
        LODWORD(v43) = v9 & 0x7FFFFFFF;
        v41 = AlpcReferenceBlobByHandle(v42 + 40, v43, AlpcReserveType);
      }
      if ( !v41 )
      {
        ClientSecurity = -1073741072;
        goto LABEL_59;
      }
      HandlePointer = *(_QWORD *)(v41 + 24);
      AlpcpLockForCachedReferenceBlob(HandlePointer);
      if ( _InterlockedCompareExchange((volatile signed __int32 *)(v41 + 40), 1, 0) )
      {
        AlpcpUnlockMessage(HandlePointer);
        AlpcpDereferenceBlobEx(v41);
        ClientSecurity = -1073740024;
        goto LABEL_59;
      }
      *(_DWORD *)(HandlePointer + 264) &= ~0x80000000;
      do
      {
        v44 = _InterlockedIncrement(&AlpcpNextCallbackId);
      }
      while ( !v44 );
      *(_DWORD *)(HandlePointer + 272) = v44;
      goto LABEL_120;
    }
    if ( (v9 & 0xFC000000) != 0 )
    {
      if ( AlpcpSecondaryMessageTables )
      {
        v14 = *(_QWORD *)(AlpcpSecondaryMessageTables + 8 * ((unsigned __int64)(unsigned int)v9 >> 26));
      }
      else
      {
        v14 = 0LL;
      }
    }
    else
    {
      v14 = AlpcMessageTable;
    }
    if ( !v14 )
    {
      ClientSecurity = -1073740030;
      goto LABEL_59;
    }
    v15 = v51 & 0x3FFFFFF;
    if ( (v51 & 0x3FC) != 0 )
    {
      v16 = (signed __int64 *)ExpLookupHandleTableEntry(v14, (unsigned int)v15);
      if ( v16 )
      {
        while ( 1 )
        {
          while ( 1 )
          {
            _m_prefetchw(v16);
            v17 = *v16;
            if ( (*v16 & 1) == 0 )
            {
              break;
            }
            if ( v17 == _InterlockedCompareExchange64(v16, v17 - 1, v17) )
            {
              goto LABEL_24;
            }
          }
          if ( !v17 )
          {
            break;
          }
          ExpBlockOnLockedHandleEntry(v14, v16);
        }
      }
    }
    v16 = 0LL;
    ExHandleLogBadReference(v14, v15);
LABEL_24:
    if ( !v16 )
    {
      ClientSecurity = -1073740030;
      v6 = v53;
      goto LABEL_59;
    }
    HandlePointer = ExGetHandlePointer(v16);
    if ( *(_DWORD *)(HandlePointer + 264) != v51 || v54 && *(_DWORD *)(HandlePointer + 272) != v54 )
    {
      _InterlockedIncrement64(v16);
      _InterlockedOr(v47, 0);
      if ( *(_QWORD *)(v14 + 48) )
      {
        ExfUnblockPushLock(v14 + 48, 0LL);
      }
LABEL_68:
      ClientSecurity = -1073740030;
      v6 = v53;
      goto LABEL_59;
    }
    _m_prefetchw((const void *)(HandlePointer - 24));
    v19 = *(_QWORD *)(HandlePointer - 24);
    while ( v19 > 0 )
    {
      v20 = v19;
      v19 = _InterlockedCompareExchange64((volatile signed __int64 *)(HandlePointer - 24), v19 + 1, v19);
      if ( v19 == v20 )
      {
        if ( v19 != -1 )
        {
          v21 = KeAbPreAcquire(HandlePointer - 16, 0LL, 0LL);
          v22 = v21;
          if ( _interlockedbittestandset64((volatile signed __int32 *)(HandlePointer - 16), 0LL) )
          {
            ExfAcquirePushLockExclusiveEx(HandlePointer - 16, v21, HandlePointer - 16);
          }
          if ( v22 )
          {
            *(_BYTE *)(v22 + 10) = 1;
          }
          *(_BYTE *)(HandlePointer - 32) |= 1u;
          v23 = _InterlockedExchangeAdd64((volatile signed __int64 *)(HandlePointer - 24), 0x10000uLL) + 0x10000;
          if ( v23 <= 0 )
          {
            KeBugCheckEx(0x18u, 0LL, HandlePointer, 0x26uLL, v23);
          }
          --*(_WORD *)(HandlePointer - 30);
          _InterlockedIncrement64(v16);
          _InterlockedOr(v47, 0);
          if ( *(_QWORD *)(v14 + 48) )
          {
            ExfUnblockPushLock(v14 + 48, 0LL);
          }
          if ( *(_DWORD *)(HandlePointer + 264) != v51 )
          {
            AlpcpUnlockMessage(HandlePointer);
            ClientSecurity = -1073740030;
            v6 = v53;
            goto LABEL_59;
          }
          if ( v54 && *(_DWORD *)(HandlePointer + 272) != v54
            || !*(_QWORD *)(HandlePointer + 24) && !*(_QWORD *)(HandlePointer + 16) )
          {
            AlpcpUnlockMessage(HandlePointer);
            goto LABEL_68;
          }
          v24 = (void ***)Object;
          if ( Object )
          {
            if ( *(_QWORD *)(HandlePointer + 184) == *((_QWORD *)Object + 2)
              || *(PVOID *)(HandlePointer + 192) == Object )
            {
LABEL_46:
              v65 = 0;
              memset_0(&v64, 0, 0x44uLL);
              v25 = 0;
              v50 = 0;
              v26 = 0LL;
              v27 = 0LL;
              if ( (*(_BYTE *)(HandlePointer + 40) & 7) != 3
                || _bittest16((const signed __int16 *)(HandlePointer + 244), 0xEu)
                || (v28 = *(void ****)(HandlePointer + 16), v28 != v24) && (((_BYTE)v24[52] & 6) != 6 || v28 != *v24[2]) )
              {
LABEL_51:
                ClientSecurity = -1073741790;
                goto LABEL_52;
              }
              v31 = *(_QWORD *)(HandlePointer + 136);
              if ( v31 )
              {
                AlpcpLockBlobExclusive(*(_QWORD *)(HandlePointer + 136));
                v38 = *(_DWORD *)(v31 + 104);
                if ( (v38 & 1) != 0 )
                {
                  AlpcpUnlockBlobUncachedExclusive(v31);
                  ClientSecurity = -1073741790;
                  goto LABEL_58;
                }
                *(_DWORD *)(v31 + 104) = v38 | 2;
                AlpcpUnlockBlobUncachedExclusive(v31);
                v27 = (__int64 *)(v31 + 32);
                v25 = 0;
              }
              else
              {
                v32 = (_DWORD *)AlpcpReferenceConnectedPort(v24);
                v26 = v32;
                if ( !v32 || (v32[64] & 0x10000) == 0 )
                {
                  goto LABEL_106;
                }
                if ( (v32[104] & 0x400) != 0 )
                {
                  v33 = *(_QWORD *)(HandlePointer + 32);
                  if ( v33 && (!v58 || v32[66] >= (int)v11) )
                  {
                    if ( (_DWORD)Process )
                    {
                      ClientSecurity = SeCreateClientSecurityEx(*(_QWORD *)(HandlePointer + 32));
                    }
                    else
                    {
                      v34 = 0;
                      v57 = 0;
                      v48 = 0;
                      v49[0] = 0;
                      SourceSid = 0LL;
                      v35 = KeGetCurrentThread();
                      if ( (struct _KTHREAD *)v33 == v35 )
                      {
                        Process = v35->ApcState.Process;
                      }
                      else
                      {
                        Process = *(_KPROCESS **)(v33 + 544);
                      }
                      if ( (*(_DWORD *)(v33 + 1440) & 8) == 0 )
                      {
                        goto LABEL_82;
                      }
                      Object = *(PVOID *)(v33 + 544);
                      v60 = KeGetCurrentThread();
                      --v60->KernelApcDisable;
                      v39 = (_BYTE *)KeAbPreAcquire(v33 + 1424, 0LL, 0LL);
                      v56 = v39;
                      if ( _InterlockedCompareExchange64((volatile signed __int64 *)(v33 + 1424), 17LL, 0LL) )
                      {
                        ExfAcquirePushLockSharedEx(v33 + 1424, 0LL, v39, v33 + 1424);
                        v39 = v56;
                      }
                      if ( v39 )
                      {
                        v39[10] = 1;
                      }
                      if ( (*(_DWORD *)(v33 + 1440) & 8) != 0 )
                      {
                        v56 = (PVOID)(*(_QWORD *)(v33 + 1336) & 0xFFFFFFFFFFFFFFF8uLL);
                        ObfReferenceObjectWithTag(v56, 0x63436553u);
                        v57 = *(_DWORD *)(v33 + 1336) & 3;
                        v48 = *((_BYTE *)Object + 1530);
                        v34 = (*(_BYTE *)(v33 + 1336) & 4) != 0;
                      }
                      else
                      {
                        v56 = 0LL;
                      }
                      PspUnlockThreadSecurityShared(v33, v60);
                      v36 = v56;
                      if ( v56 )
                      {
                        LODWORD(Object) = 2;
                      }
                      else
                      {
LABEL_82:
                        v36 = (_QWORD *)PsReferencePrimaryTokenWithTag(Process, 1665361235LL);
                        v56 = v36;
                        LODWORD(Object) = 1;
                        v34 = 0;
                        v48 = BYTE2(Process[3].ActiveGroupsMask.Masks[1]);
                      }
                      SepReconcileTrustSidWithProcessProtection(v36[138], &v48, v49, &SourceSid);
                      ClientSecurity = SepCreateClientSecurityEx(
                                         (int)v36,
                                         (int)v26 + 260,
                                         0,
                                         (int)Object,
                                         v34,
                                         v57,
                                         0,
                                         0LL,
                                         v49[0],
                                         SourceSid,
                                         (__int64)&v64);
                      if ( ClientSecurity < 0 || !v26[268] )
                      {
                        ObfDereferenceObjectWithTag(v56, 0x63436553u);
                      }
                      LODWORD(v11) = v63;
                    }
                    if ( ClientSecurity >= 0 )
                    {
                      v27 = &v64;
                      v25 = 1;
                      goto LABEL_88;
                    }
LABEL_107:
                    v25 = 0;
LABEL_52:
                    if ( v26 )
                    {
                      ObfDereferenceObject(v26);
                    }
                    if ( v25 )
                    {
                      v29 = (_DWORD *)v27[2];
                      if ( v29[48] == 1 || v29 )
                      {
                        ObfDereferenceObjectWithTag(v29, 0x63436553u);
                      }
                    }
LABEL_58:
                    AlpcpUnlockMessage(HandlePointer);
                    v6 = v53;
                    goto LABEL_59;
                  }
LABEL_106:
                  ClientSecurity = -1073741790;
                  goto LABEL_107;
                }
                v27 = (__int64 *)(v32 + 16);
                v25 = v50;
              }
LABEL_88:
              *(_OWORD *)ImpersonationLevel = *(_OWORD *)v27;
              *(_OWORD *)Token = *((_OWORD *)v27 + 1);
              if ( v58 )
              {
                if ( (int)v11 > *((_DWORD *)v27 + 1) )
                {
                  goto LABEL_51;
                }
                ImpersonationLevel[1] = v11;
              }
              v37 = BYTE1(Token[1]);
              if ( !LOBYTE(Token[1]) )
              {
                v37 = BYTE1(ImpersonationLevel[2]);
              }
              ClientSecurity = PsImpersonateClient(KeGetCurrentThread(), Token[0], 1u, v37, ImpersonationLevel[1]);
              goto LABEL_52;
            }
LABEL_111:
            AlpcpUnlockMessage(HandlePointer);
            ClientSecurity = -1073741790;
            v6 = v53;
            goto LABEL_59;
          }
          v40 = *(_QWORD *)(HandlePointer + 16);
          if ( !v40 || *(_KPROCESS **)(v40 + 24) != KeGetCurrentThread()->ApcState.Process )
          {
            goto LABEL_111;
          }
LABEL_120:
          v24 = (void ***)Object;
          goto LABEL_46;
        }
LABEL_121:
        ExUnlockHandleTableEntry(v14, v16);
        ClientSecurity = -1073740030;
        v6 = v53;
        goto LABEL_59;
      }
    }
    if ( v19 )
    {
      KeBugCheckEx(0x18u, 0LL, HandlePointer, 0x20uLL, v19);
    }
    goto LABEL_121;
  }
  v45 = AlpcpReferenceConnectedPort(Object);
  v5 = (void *)v45;
  if ( !v45 )
  {
    ClientSecurity = -1073741790;
    goto LABEL_59;
  }
  if ( (*(_DWORD *)(v45 + 256) & 0x10000) == 0 )
  {
    ClientSecurity = -1073741790;
    goto LABEL_59;
  }
  if ( (*(_DWORD *)(v45 + 416) & 0x400) != 0 )
  {
    ClientSecurity = -1073741790;
    goto LABEL_59;
  }
  *(_OWORD *)ImpersonationLevel = *(_OWORD *)(v45 + 64);
  *(_OWORD *)Token = *(_OWORD *)(v45 + 80);
  if ( v12 )
  {
    if ( (int)v11 > ImpersonationLevel[1] )
    {
      ClientSecurity = -1073741790;
      goto LABEL_59;
    }
    ImpersonationLevel[1] = v11;
  }
  v46 = BYTE1(Token[1]);
  if ( !LOBYTE(Token[1]) )
  {
    v46 = BYTE1(ImpersonationLevel[2]);
  }
  ClientSecurity = PsImpersonateClient(KeGetCurrentThread(), Token[0], 1u, v46, ImpersonationLevel[1]);
LABEL_59:
  if ( v5 )
  {
    ObfDereferenceObject(v5);
  }
  if ( v6 )
  {
    ObfDereferenceObject(v6);
  }
  KeLeaveCriticalRegionThread(KeGetCurrentThread());
  return (unsigned int)ClientSecurity;
}
484 row(s)
