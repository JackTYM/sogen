__int64 __fastcall NtTraceControl(__int64 a1, unsigned int *a2, __int64 a3, void *a4, unsigned int Length, void *a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v7 = a1;
  v101 = a1;
  LODWORD(Size) = a3;
  v112[1] = a4;
  v110[1] = a6;
  v8 = 0LL;
  v9 = 0LL;
  Src = 0LL;
  v103 = 0;
  v11 = *(_QWORD *)(PsGetCurrentServerSiloGlobals(
                      a1,
                      (unsigned __int8)KeGetCurrentThread()->PreviousMode,
                      a3,
                      (unsigned int)a1)
                  + 832);
  started = 0;
  v106 = 0;
  LOBYTE(v105) = 0;
  if ( (_BYTE)v10 )
  {
    v105 = (unsigned int)v12 >> 31;
    v7 &= ~0x80000000;
    if ( v12 >= 0 )
    {
      v7 = v12;
    }
    v101 = v7;
    v15 = 0x7FFFFFFF0000LL;
    if ( !a2 )
    {
      LODWORD(Size) = 0;
    }
    if ( v13 )
    {
      ProbeForWrite(v13, Length, 1u);
    }
    else
    {
      Length = 0;
    }
    if ( !a6 )
    {
      started = -1073741811;
      v106 = -1073741811;
      goto LABEL_344;
    }
    if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )
    {
      v15 = (__int64)a6;
    }
    *(_DWORD *)v15 = *(_DWORD *)v15;
    started = v106;
  }
  if ( v7 <= 0x1B && (v16 = 134238208, _bittest(&v16, v7)) )
  {
    v17 = Length;
    v18 = Size;
  }
  else
  {
    v18 = Size;
    v17 = Length;
    if ( (_DWORD)Size || Length )
    {
      v19 = Length;
      if ( (unsigned int)Size > Length )
      {
        v19 = (unsigned int)Size;
      }
      Pool2 = (unsigned int *)ExAllocatePool2(0x101uLL, v19, 0x50777445u);
      v9 = Pool2;
      Src = Pool2;
      if ( !Pool2 )
      {
        started = -1073741801;
        goto LABEL_344;
      }
      if ( a2 )
      {
        memmove(Pool2, a2, (unsigned int)Size);
      }
    }
  }
  v21 = 0x140000000uLL;
  switch ( v101 )
  {
    case 1u:
    case 2u:
    case 3u:
    case 4u:
    case 5u:
    case 6u:
    case 7u:
      if ( v18 < 0xB0 || v17 < 0xB0 )
      {
        goto LABEL_35;
      }
      if ( v9 )
      {
        if ( *v9 >= 0xB0 )
        {
          if ( (v9[11] & 0x20000) != 0 )
          {
            if ( *v9 > v18 )
            {
LABEL_35:
              started = -1073741306;
              goto LABEL_330;
            }
            started = 0;
          }
          else
          {
            started = -1073741811;
          }
        }
        else
        {
          started = -1073741306;
        }
      }
      else
      {
        started = -1073741811;
      }
      if ( started >= 0 )
      {
        switch ( v101 )
        {
          case 1u:
            *((_QWORD *)v9 + 10) = (unsigned __int8)v105 | *((_QWORD *)v9 + 10) & 0xFFFFFFFFFFFFFFFEuLL;
            started = EtwpStartTrace(v11, v9);
            v103 = 176;
            break;
          case 2u:
            started = EtwpStopTrace(v11, v9, 0LL);
            v103 = 176;
            break;
          case 3u:
            started = EtwpQueryTrace(v11, v9);
            v103 = 176;
            break;
          case 4u:
            started = EtwpUpdateTrace(v11, v9);
            v103 = 176;
            break;
          case 5u:
            started = EtwpFlushTrace(v11, v9);
            v103 = 176;
            break;
          case 6u:
            Object[0] = 0LL;
            if ( v9 )
            {
              if ( *v9 >= 0xB0 )
              {
                if ( (v9[11] & 0x20000) != 0 )
                {
                  CurrentThread = KeGetCurrentThread();
                  --CurrentThread->KernelApcDisable;
                  started = EtwpAcquireLoggerContext(v11, v9, Object);
                  if ( started >= 0 )
                  {
                    v23 = (unsigned int *)Object[0];
                    if ( (*((_DWORD *)Object[0] + 3) & 8) != 0 )
                    {
                      v23 = (unsigned int *)Object[0];
                      if ( (*((_DWORD *)Object[0] + 206) & 1) == 0 )
                      {
                        EtwpSynchronizeWithLogger(Object[0], 4LL);
                        EtwpGenerateFileName(v23 + 42, v23 + 74, v23 + 46);
                        started = EtwpSynchronizeWithLogger(v23, 1LL);
                      }
                      if ( started >= 0 )
                      {
                        started = EtwpGetLoggerInfoFromContext(v9, v23);
                      }
                    }
                    else
                    {
                      started = -1073741811;
                    }
                    KeReleaseMutex((PRKMUTEX)(v23 + 158), 0);
                    ExReleaseRundownProtectionCacheAwareEx(
                      *(PEX_RUNDOWN_REF_CACHE_AWARE *)(*(_QWORD *)(*((_QWORD *)v23 + 170) + 704LL) + 8LL * *v23),
                      1u);
                  }
                  KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
                  v17 = Length;
                  v103 = 176;
                }
                else
                {
                  started = -1073741811;
                  v103 = 176;
                }
              }
              else
              {
                started = -1073741306;
                v103 = 176;
              }
            }
            else
            {
              started = -1073741811;
              v103 = 176;
            }
            break;
          case 7u:
            started = EtwpTransitionToRealtime(v11, v9);
            goto LABEL_60;
          default:
LABEL_60:
            v103 = 176;
            break;
        }
      }
      goto LABEL_330;
    case 0xBu:
      if ( v18 != 96 || v17 != 96 )
      {
        goto LABEL_125;
      }
      started = EtwpRealtimeConnect(v9);
      v103 = 96;
      goto LABEL_330;
    case 0xCu:
      if ( v17 != 16 )
      {
        goto LABEL_125;
      }
      started = 0;
      EtwpCreateActivityId(a4);
      v46 = v101;
      v47 = (unsigned int *)a6;
      goto LABEL_332;
    case 0xDu:
      if ( v18 != 48 || v17 )
      {
        goto LABEL_125;
      }
      *(_OWORD *)Object = 0LL;
      if ( !v9 )
      {
        started = -1073741811;
        goto LABEL_330;
      }
      LOBYTE(v10) = 1;
      ProviderIdFromHandle = EtwGetProviderIdFromHandle(*v9, v10, Object);
      if ( ProviderIdFromHandle < 0 )
      {
        goto LABEL_155;
      }
      if ( v9 == (unsigned int *)-24LL )
      {
        started = -1073741811;
      }
      else if ( v9 == (unsigned int *)-8LL )
      {
        started = -1073741811;
      }
      else if ( v9[6]
             || *((_WORD *)v9 + 14)
             || *((_WORD *)v9 + 15)
             || *((_BYTE *)v9 + 32)
             || *((_BYTE *)v9 + 33)
             || *((_BYTE *)v9 + 34)
             || *((_BYTE *)v9 + 35)
             || *((_BYTE *)v9 + 36)
             || *((_BYTE *)v9 + 37)
             || *((_BYTE *)v9 + 38)
             || *((_BYTE *)v9 + 39) )
      {
        v49 = *((unsigned __int16 *)v9 + 4);
        if ( v9[10] == 10 )
        {
          if ( WdipSemEnabled )
          {
            started = WdipSemEnableScenario(Object, v49);
          }
          else
          {
            started = -1073741823;
          }
        }
        else if ( WdipSemEnabled )
        {
          ProviderIdFromHandle = WdipSemDisableScenario(Object, v49);
LABEL_155:
          started = ProviderIdFromHandle;
        }
        else
        {
          started = -1073741823;
        }
      }
      else
      {
        started = -1073741811;
      }
      goto LABEL_330;
    case 0xEu:
      if ( v18 != 8 || v17 )
      {
        goto LABEL_125;
      }
      Handle = (HANDLE)*a2;
      PreviousMode = KeGetCurrentThread()->PreviousMode;
      Object[0] = 0LL;
      v44 = ObReferenceObjectByHandle(Handle, 0x400u, EtwpRealTimeConnectionObjectType, PreviousMode, Object, 0LL);
      if ( v44 >= 0 )
      {
        EtwpRealtimeDisconnectConsumer(Object[0]);
        ObfDereferenceObject(Object[0]);
      }
      started = v44;
      v17 = Length;
      goto LABEL_330;
    case 0xFu:
      if ( v18 != 160 || v17 - 160 > 0xFF60 )
      {
        goto LABEL_125;
      }
      started = EtwpRegisterUMProvider(v11, (_DWORD)v9, v17, (unsigned __int8)v105, (__int64)&v103);
      goto LABEL_330;
    case 0x10u:
      if ( v18 || v17 - 72 > 0xFFB8 )
      {
        goto LABEL_125;
      }
      started = EtwpReceiveNotification(v9);
      goto LABEL_330;
    case 0x11u:
      if ( v18 < 0x48 || v17 != 72 || v9[1] != v18 )
      {
        goto LABEL_125;
      }
      if ( *v9 == 3 )
      {
        if ( v18 < 0x78 )
        {
          goto LABEL_125;
        }
        LOBYTE(v21) = 1;
        started = EtwpEnableGuid(v11, v9, v21);
        v103 = 72;
      }
      else
      {
        LOBYTE(v21) = 1;
        started = EtwpNotifyGuid(v11, v9, v21);
        v103 = 72;
      }
      goto LABEL_330;
    case 0x12u:
      if ( v18 < 0x48 || v9[1] != v18 )
      {
        goto LABEL_125;
      }
      v9[9] = PsGetCurrentThreadProcessId();
      v28 = (void *)*((_QWORD *)v9 + 3);
      v29 = v9[4];
      v30 = KeGetCurrentThread();
      --v30->KernelApcDisable;
      v109 = 0LL;
      started = ObReferenceObjectByHandle(v28, 4u, EtwpRegistrationObjectType, 1, &v109, 0LL);
      if ( started >= 0 )
      {
        if ( (*((_BYTE *)v109 + 98) & 2) != 0 )
        {
          if ( (unsigned int)v29 < 4 )
          {
            v31 = _InterlockedExchange64((volatile __int64 *)v109 + v29 + 6, 0LL);
            if ( v31 )
            {
              v32 = *(_QWORD *)(v31 + 32);
              if ( (*(_BYTE *)(v32 + 98) & 0x20) != 0 )
              {
                started = -1073741055;
              }
              else
              {
                started = EtwpQueueReply(*(_QWORD *)(v32 + 48), v9);
              }
              ObfDereferenceObject(*(PVOID *)(v31 + 32));
              v33 = *(_QWORD *)(v31 + 40);
              if ( v33 )
              {
                PspChargeProcessWakeCounter((PVOID)(v33 & 0xFFFFFFFFFFFFFFF8uLL), -1, 1, 0LL);
              }
              _m_prefetchw((const void *)(v31 + 52));
              v34 = *(_DWORD *)(v31 + 52);
              do
              {
                v35 = v34;
                v34 = _InterlockedCompareExchange((volatile signed __int32 *)(v31 + 52), v34 & 0xFFFFFFFD, v34);
              }
              while ( v35 != v34 );
              if ( (v34 & 0xFFFFFFFD) == 0 )
              {
                ExFreePoolWithTag((PVOID)v31, 0);
              }
            }
            else
            {
              started = -1073741811;
            }
          }
          else
          {
            started = -1073741811;
          }
        }
        else
        {
          started = -1073741816;
        }
        ObfDereferenceObject(v109);
      }
      KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
      v17 = Length;
      goto LABEL_330;
    case 0x13u:
      if ( v18 != 8 )
      {
        goto LABEL_125;
      }
      v36 = (void *)*v9;
      Timeout.QuadPart = EtwpOneMs * v9[1];
      v111 = 0LL;
      started = ObReferenceObjectByHandle(v36, 4u, EtwpRegistrationObjectType, 1, &v111, 0LL);
      if ( started >= 0 )
      {
        v37 = v111;
        if ( (*((_BYTE *)v111 + 98) & 4) != 0 )
        {
          v38 = (struct _KQUEUE *)*((_QWORD *)v111 + 6);
          EntryArray = 0LL;
          KeRemoveQueueEx(v38, 1, 0, &Timeout, &EntryArray, 1u);
          started = (int)EntryArray;
          if ( EntryArray != (PLIST_ENTRY)258 && EntryArray != (PLIST_ENTRY)192 )
          {
            v39 = EntryArray;
            Flink = EntryArray[1].Flink;
            Flink_high = HIDWORD(Flink->Flink);
            if ( Flink_high > v17 )
            {
              started = -1073741789;
            }
            else
            {
              memmove(v9, EntryArray[1].Flink, Flink_high);
              started = 0;
            }
            v42 = HIDWORD(Flink->Flink);
            if ( _InterlockedExchangeAdd((volatile signed __int32 *)&Flink->Blink, 0xFFFFFFFF) == 1 )
            {
              ExFreePoolWithTag(Flink, 0);
            }
            ExFreePoolWithTag(v39, 0);
            v17 = Length;
            ObfDereferenceObject(v111);
            v103 = v42;
            goto LABEL_330;
          }
        }
        else
        {
          started = -1073741816;
        }
        ObfDereferenceObject(v37);
        v103 = 0;
        goto LABEL_330;
      }
      v103 = 0;
      goto LABEL_330;
    case 0x14u:
      if ( v18 || v17 )
      {
        goto LABEL_125;
      }
      started = WdiUpdateSem();
      goto LABEL_330;
    case 0x15u:
      v50 = v17 >> 4;
      LODWORD(Object[0]) = 0;
      started = EtwpGetGuidList(v11, 0, (_DWORD)v9, v17 >> 4, (__int64)Object);
      v53 = (unsigned int)Object[0];
      if ( started == -2147483643 )
      {
        goto LABEL_173;
      }
      v54 = 0;
      v55 = &EtwpUmglProviders;
      do
      {
        if ( ++v53 > 0xFFFFFFF )
        {
          goto LABEL_172;
        }
        if ( v53 <= v50 )
        {
          v51 = *v55;
          v52 = 2LL * (v53 - 1);
          *(_OWORD *)&v9[4 * v53 - 4] = *(_OWORD *)*v55;
        }
        ++v54;
        v55 += 2;
      }
      while ( v54 < 0xA );
      if ( v50 >= v53 )
      {
        EtwpCopyMetaProviderGuids(&v9[4 * v53], v50 - v53, v55);
      }
      v56 = EtwpMetaProviderCount(v52, v51, v55);
      if ( v57 + v56 < v57 )
      {
        LODWORD(v8) = -1073741675;
        v53 = -1;
      }
      else
      {
        v53 = v57 + v56;
      }
      if ( (int)v8 < 0 || v53 > 0xFFFFFFF )
      {
LABEL_172:
        started = -2147483643;
      }
LABEL_173:
      if ( started >= 0 && v53 > v50 )
      {
        started = -1073741789;
      }
      v103 = 16 * v53;
      goto LABEL_330;
    case 0x16u:
      if ( v18 != 16 )
      {
        goto LABEL_125;
      }
      v103 = v17;
      started = EtwpGetTraceGuidInfo(v11, v9, v9, &v103);
      goto LABEL_330;
    case 0x17u:
      v63 = v17 / 0x24;
      v64 = 0;
      NextGuidEntry = EtwpGetNextGuidEntry(v11, 0LL, 0LL);
      if ( !NextGuidEntry )
      {
        goto LABEL_205;
      }
      do
      {
        v66 = v64++;
        if ( v64 > 0x71C71C7 )
        {
LABEL_215:
          LODWORD(v8) = -2147483643;
          goto LABEL_216;
        }
        if ( v64 <= v63 )
        {
          v67 = 9 * v66;
          *(_OWORD *)&v9[v67] = *(_OWORD *)(NextGuidEntry + 40);
          v9[v67 + 7] = *(_DWORD *)(NextGuidEntry + 80);
          v9[v67 + 5] = *(unsigned __int16 *)(NextGuidEntry + 88);
          v9[v67 + 6] = *(unsigned __int8 *)(NextGuidEntry + 90);
          LOBYTE(v9[v67 + 8]) = *(_BYTE *)(NextGuidEntry + 91) & 1;
        }
        NextGuidEntry = EtwpGetNextGuidEntry(v11, NextGuidEntry, 0LL);
      }
      while ( NextGuidEntry );
LABEL_205:
      if ( v11 == EtwpHostSiloState )
      {
        v68 = 2147353472LL;
      }
      else
      {
        v68 = *(_QWORD *)(*(_QWORD *)(v11 + 8) + 1288LL) + 550LL;
      }
      v69 = 0;
      v70 = &EtwpUmglProviders;
      do
      {
        v71 = v64;
        v72 = (unsigned __int8 *)(v68 + 2LL * *((unsigned __int8 *)v70 + 8));
        if ( *v72 )
        {
          if ( ++v64 > 0x71C71C7 )
          {
            goto LABEL_215;
          }
          if ( v64 <= v63 )
          {
            v73 = 9 * v71;
            *(_OWORD *)&v9[v73] = *(_OWORD *)*v70;
            v9[v73 + 5] = *v72;
            v9[v73 + 7] = v72[1];
            v9[v73 + 6] = 0;
            LOBYTE(v9[v73 + 8]) = 1;
          }
        }
        ++v69;
        v70 += 2;
      }
      while ( v69 < 0xA );
LABEL_216:
      if ( (int)v8 >= 0 && v64 > v63 )
      {
        LODWORD(v8) = -1073741789;
      }
      started = (int)v8;
      v103 = 36 * v64;
      goto LABEL_330;
    case 0x18u:
      if ( v18 || v17 )
      {
        goto LABEL_125;
      }
      if ( *(_DWORD *)(v11 + 4344) )
      {
        started = -1073741790;
      }
      else
      {
        *(_DWORD *)(v11 + 4344) = PsGetCurrentThreadProcessId();
LABEL_225:
        started = (int)v8;
      }
      goto LABEL_330;
    case 0x19u:
      if ( v18 != 4 || v17 != 16 )
      {
        goto LABEL_125;
      }
      v74 = *v9;
      if ( (_DWORD)v74 == 0xFFFF )
      {
        v74 = *(unsigned __int8 *)(v11 + 4504);
      }
      v75 = KeGetCurrentThread();
      --v75->KernelApcDisable;
      v76 = (unsigned int *)EtwpAcquireLoggerContextByLoggerId(v11, v74, 0LL);
      if ( v76 )
      {
        *(_OWORD *)v9 = *((_OWORD *)v76 + 19);
        ExReleaseRundownProtectionCacheAwareEx(
          *(PEX_RUNDOWN_REF_CACHE_AWARE *)(*(_QWORD *)(*((_QWORD *)v76 + 170) + 704LL) + 8LL * *v76),
          1u);
      }
      else
      {
        LODWORD(v8) = -1073741162;
      }
      KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
      started = (int)v8;
      v103 = 16;
      v17 = Length;
      goto LABEL_330;
    case 0x1Au:
      goto LABEL_65;
    case 0x1Bu:
      if ( v18 != 4 )
      {
        goto LABEL_125;
      }
      v77 = (void *)*a2;
      v116 = v77;
      if ( !v77 )
      {
        goto LABEL_125;
      }
      v112[0] = 0LL;
      started = ObReferenceObjectByHandle(v77, 2u, (POBJECT_TYPE)ExEventObjectType, 1, v112, 0LL);
      if ( started < 0 )
      {
        goto LABEL_243;
      }
      v78 = EtwpAddDataSource(KeGetCurrentThread()->ApcState.Process);
      v79 = (volatile signed __int64 *)v78;
      if ( v78 )
      {
        if ( (_BYTE)v105 )
        {
          v79 = (volatile signed __int64 *)(v78 + 8);
        }
        v17 = Length;
        if ( !_InterlockedCompareExchange64(v79, (signed __int64)v112[0], 0LL) )
        {
          goto LABEL_330;
        }
        started = -1073740008;
      }
      else
      {
        started = -1073741801;
        v17 = Length;
      }
LABEL_243:
      if ( v112[0] )
      {
        ObfDereferenceObject(v112[0]);
      }
      goto LABEL_330;
    case 0x1Cu:
      if ( v18 < 8 )
      {
        goto LABEL_125;
      }
      v80 = v9[1];
      if ( 16 * v80 + 8 != v18 )
      {
        goto LABEL_125;
      }
      if ( (_DWORD)v80 )
      {
        v8 = v9 + 2;
      }
      started = EtwpUpdateDisallowList(v11, *v9, v80, v8);
      goto LABEL_330;
    case 0x1Eu:
      if ( v18 != 24 || v17 - 120 > 0xFF88 )
      {
        goto LABEL_125;
      }
      started = EtwpSetProviderTraitsUm((_DWORD)v9, v17, (unsigned int)&v103);
      goto LABEL_330;
    case 0x1Fu:
      if ( v18 != 16 || v17 )
      {
        goto LABEL_125;
      }
      v81 = *(void **)v9;
      v110[0] = 0LL;
      v82 = ObReferenceObjectByHandle(v81, 0x800u, EtwpRegistrationObjectType, 1, v110, 0LL);
      v83 = v82;
      if ( v82 >= 0 )
      {
        v84 = *((_BYTE *)v9 + 8);
        if ( v84 == 1 )
        {
          _InterlockedOr16((volatile signed __int16 *)v110[0] + 49, 0x100u);
          v17 = Length;
          ObfDereferenceObject(v110[0]);
          started = v83;
        }
        else if ( v84 )
        {
          ObfDereferenceObject(v110[0]);
          started = -1073741811;
        }
        else
        {
          _InterlockedAnd16((volatile signed __int16 *)v110[0] + 49, 0xFEFFu);
          v17 = Length;
          ObfDereferenceObject(v110[0]);
          started = v83;
        }
      }
      else
      {
        started = v82;
      }
      goto LABEL_330;
    case 0x20u:
      started = EtwpGetGuidList(v11, 2, (_DWORD)v9, v17 >> 4, (__int64)&v103);
      v103 *= 16;
      goto LABEL_330;
    case 0x21u:
      if ( v18 != 16 )
      {
        goto LABEL_125;
      }
      v103 = v17;
      started = EtwpGetTraceGroupInfo(v11, v9, v9, &v103);
      goto LABEL_330;
    case 0x22u:
      if ( v18 != 8 )
      {
        goto LABEL_125;
      }
      v103 = v17;
      v58 = 0;
      v104 = 0;
      v59 = (unsigned int *)EtwpAcquireLoggerContextByLoggerId(v11, *(unsigned __int16 *)v9, 0LL);
      if ( v59 )
      {
        memset_0(v9, 0, v103);
        v60 = (ULONG_PTR)(v59 + 172);
        v61 = KeAbPreAcquire(v59 + 172, 0LL, 0LL);
        if ( _InterlockedCompareExchange64((volatile signed __int64 *)v59 + 86, 17LL, 0LL) )
        {
          ExfAcquirePushLockSharedEx(v59 + 172, 0LL, v61, v59 + 172);
        }
        if ( v61 )
        {
          *(_BYTE *)(v61 + 10) = 1;
        }
        v104 = 1;
        v62 = 16 * *((unsigned __int16 *)v59 + 664);
        v105 = v62;
        v17 = Length;
        if ( v62 > Length )
        {
          v58 = -1073741789;
        }
        else
        {
          memmove(v9, *((const void **)v59 + 167), v62);
          v62 = v105;
        }
        v103 = v62;
      }
      else
      {
        v58 = -1073741811;
        v60 = 688LL;
      }
      if ( v104 )
      {
        if ( _InterlockedCompareExchange64((volatile signed __int64 *)v60, 0LL, 17LL) != 17 )
        {
          ExfReleasePushLockShared(v60);
        }
        KeAbPostRelease(v60);
        v17 = Length;
      }
      if ( v59 )
      {
        ExReleaseRundownProtectionCacheAwareEx(
          *(PEX_RUNDOWN_REF_CACHE_AWARE *)(*(_QWORD *)(*((_QWORD *)v59 + 170) + 704LL) + 8LL * *v59),
          1u);
      }
      started = v58;
      goto LABEL_330;
    case 0x23u:
      if ( v18 != 16 || v17 )
      {
        goto LABEL_125;
      }
      started = EtwpSetCompressionSettings(v9);
      goto LABEL_330;
    case 0x24u:
      if ( v18 != 8 || v17 != 16 )
      {
        goto LABEL_125;
      }
      v103 = 16;
      started = EtwpGetCompressionSettings(v9, v9);
      goto LABEL_330;
    case 0x25u:
      if ( v18 < 0xC )
      {
        goto LABEL_125;
      }
      v85 = *((unsigned __int16 *)v9 + 4);
      if ( (unsigned int)v85 <= 0x10 )
      {
        v86 = v9[1];
        if ( (unsigned int)(v86 - 1) <= 3 )
        {
          started = -1073741811;
          goto LABEL_330;
        }
        if ( (_WORD)v85 )
        {
          if ( !(_DWORD)v86 )
          {
            started = -1073741811;
            goto LABEL_330;
          }
        }
        else if ( (_DWORD)v86 )
        {
          goto LABEL_125;
        }
        if ( 16 * v85 + 12 == v18 )
        {
          if ( (_WORD)v85 )
          {
            v8 = v9 + 3;
          }
          started = EtwpUpdatePeriodicCaptureState(*v9, v86, v85, v8);
          goto LABEL_330;
        }
LABEL_125:
        started = -1073741811;
        goto LABEL_330;
      }
      started = -1073741811;
LABEL_330:
      v45 = a4;
      v46 = v101;
LABEL_331:
      v47 = (unsigned int *)a6;
LABEL_332:
      if ( started >= 0 )
      {
        if ( v103 )
        {
          if ( v103 > v17 )
          {
            started = -1073741789;
            v106 = -1073741789;
          }
          else
          {
            memmove(v45, v9, v103);
          }
        }
        *v47 = v103;
      }
      if ( started == -1073741789 && ((unsigned int)(v46 - 15) <= 1 || (unsigned int)(v46 - 21) <= 1)
        || (unsigned int)v46 <= 0x2E && (v99 = 0x41D700880000LL, _bittest64(&v99, v46)) )
      {
        *v47 = v103;
      }
LABEL_344:
      if ( v9 )
      {
        ExFreePoolWithTag(v9, 0);
      }
      return (unsigned int)started;
    case 0x26u:
      if ( v18 < 8 || (v18 & 7) != 0 || v17 < 2 )
      {
        goto LABEL_125;
      }
      LOWORD(Object[0]) = 0;
      started = EtwpGetPrivateSessionTraceHandle(v9, v18 >> 3, Object);
      if ( !started )
      {
        v103 = 2;
        *(_WORD *)v9 = Object[0];
      }
      goto LABEL_330;
    case 0x27u:
      if ( v18 < 2 || v17 < 8 )
      {
        goto LABEL_125;
      }
      started = EtwpRegisterPrivateSession(v11, *(unsigned __int16 *)v9, v9 + 1, v9);
      if ( !started )
      {
        v103 = 8;
      }
      goto LABEL_330;
    case 0x28u:
      if ( v18 < 8 || v17 < 2 )
      {
        goto LABEL_125;
      }
      v87 = *(void **)v9;
      Object[0] = 0LL;
      v88 = ObReferenceObjectByHandle(v87, 0, EtwpSessionDemuxObjectType, 1, Object, 0LL);
      v89 = Object[0];
      if ( v88 >= 0 )
      {
        v90 = *((_WORD *)Object[0] + 9);
        if ( v90 )
        {
          *(_WORD *)v9 = v90;
          v88 = 0;
        }
        else
        {
          v88 = -1073741162;
        }
      }
      if ( v89 )
      {
        ObfDereferenceObject(v89);
      }
      started = v88;
      if ( !v88 )
      {
        v103 = 2;
      }
      goto LABEL_330;
    case 0x29u:
      if ( v18 < 8 )
      {
        goto LABEL_125;
      }
      v91 = *((_BYTE *)v9 + 4);
      v92 = EtwpAcquireLoggerContextByLoggerId(v11, *v9, 0LL);
      v93 = (unsigned int *)v92;
      if ( v92 )
      {
        if ( v91 )
        {
          _InterlockedOr((volatile signed __int32 *)(v92 + 816), 0x2000000u);
          for ( i = EtwpGetNextGuidEntry(v11, 0LL, 0LL); i; i = EtwpGetNextGuidEntry(v11, i, 0LL) )
          {
            EtwpTrackGuidEntryRegistrations(v93, i, 0LL);
          }
          for ( j = EtwpGetNextGuidEntry(v11, 0LL, 2LL); j; j = EtwpGetNextGuidEntry(v11, j, 2LL) )
          {
            EtwpTrackGuidEntryRegistrations(v93, j, 2LL);
          }
        }
        else
        {
          _InterlockedAnd((volatile signed __int32 *)(v92 + 816), 0xFDFFFFFF);
        }
        v17 = Length;
      }
      else
      {
        LODWORD(v8) = -1073741811;
      }
      if ( !v93 )
      {
        goto LABEL_225;
      }
      ExReleaseRundownProtectionCacheAwareEx(
        *(PEX_RUNDOWN_REF_CACHE_AWARE *)(*(_QWORD *)(*((_QWORD *)v93 + 170) + 704LL) + 8LL * *v93),
        1u);
      started = (int)v8;
      goto LABEL_330;
    case 0x2Au:
      v46 = v101;
      v45 = a4;
      if ( v17 < 4 )
      {
        started = -1073741811;
      }
      else
      {
        *v9 = *(_DWORD *)(v11 + 16);
        v103 = 4;
        started = 0;
      }
      goto LABEL_331;
    case 0x2Bu:
      v46 = v101;
      v45 = a4;
      if ( v17 < 4 )
      {
        started = -1073741811;
      }
      else
      {
        *v9 = EtwpMaxPmcCounter;
        v103 = 4;
        started = 0;
      }
      goto LABEL_331;
    case 0x2Cu:
      if ( v18 != 8 || v17 < 4 )
      {
        goto LABEL_125;
      }
      v96 = *(unsigned __int16 *)v9;
      if ( (_DWORD)v96 == 0xFFFF )
      {
        v96 = *(unsigned __int8 *)(v11 + 4504);
      }
      v97 = EtwpAcquireLoggerContextByLoggerId(v11, v96, 0LL);
      if ( v97 )
      {
        v103 = 4;
        *v9 = EtwpQueryUsedProcessorCount(v97);
        ExReleaseRundownProtectionCacheAwareEx(
          *(PEX_RUNDOWN_REF_CACHE_AWARE *)(*(_QWORD *)(*((_QWORD *)v98 + 170) + 704LL) + 8LL * *v98),
          1u);
      }
      else
      {
        started = -1073741811;
      }
      goto LABEL_330;
    case 0x2Du:
      v103 = v17;
      started = EtwpGetPmcOwnership(v9, &v103);
      goto LABEL_330;
    case 0x2Eu:
      v103 = v17;
      started = EtwpGetPmcSessions(v9, &v103);
      goto LABEL_330;
    case 0x2Fu:
      if ( !(unsigned int)Feature_ModernWpp__private_IsEnabledDeviceUsageNoInline() )
      {
        goto LABEL_329;
      }
LABEL_65:
      if ( v18 != 8 )
      {
        goto LABEL_125;
      }
      if ( (unsigned int)Feature_ModernWpp__private_IsEnabledDeviceUsageNoInline() )
      {
        v24 = *(void **)v9;
        Object[0] = 0LL;
        started = ObReferenceObjectByHandle(v24, 0x800u, EtwpRegistrationObjectType, 1, Object, 0LL);
        v27 = Object[0];
        if ( started >= 0 )
        {
          LOBYTE(v26) = v101 == 47;
          started = EtwpTrackProviderBinaryKm(Object[0], v25, v26);
        }
        if ( v27 )
        {
          ObfDereferenceObject(v27);
        }
      }
      else
      {
        started = EtwpTrackProviderBinaryOld(v11, v9);
      }
      goto LABEL_330;
    default:
LABEL_329:
      started = -1073741808;
      goto LABEL_330;
  }
}
1124 row(s)
