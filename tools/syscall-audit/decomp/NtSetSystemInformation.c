__int64 __fastcall NtSetSystemInformation(char *a1, __m128i *a2, __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v3 = (unsigned int)a3;
  v4 = a2;
  v5 = (int)a1;
  v145 = 0LL;
  v6 = 0LL;
  LOBYTE(v118[0]) = 0;
  *(_OWORD *)Buf1 = 0LL;
  v130 = 0LL;
  v7 = 1;
  v8 = 1LL;
  v134 = 0LL;
  v133 = 0LL;
  v131 = 0LL;
  v132 = 0LL;
  v127 = 0LL;
  v148 = 0LL;
  v149 = 0LL;
  v150 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v119 = PreviousMode;
  if ( PreviousMode )
  {
    if ( (_DWORD)a1 == 89 )
    {
      v10 = 1LL;
    }
    else if ( (_DWORD)a1 != 151 || (v10 = 0LL, (_DWORD)a3 != 1) )
    {
      v10 = 3LL;
    }
    if ( (_DWORD)a3 )
    {
      if ( (v10 & (unsigned __int64)a2) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      a1 = &a2->m128i_i8[(unsigned int)a3];
    }
  }
  updated = 0;
  if ( v5 > 113 )
  {
    if ( v5 > 194 )
    {
      if ( v5 > 223 )
      {
        if ( v5 > 233 )
        {
          v115 = v5 - 235;
          if ( !v115 )
          {
            return (unsigned int)HvlQuerySetBootPagesInfo(a2, 0LL);
          }
          v116 = v115 - 8;
          if ( !v116 )
          {
            return (unsigned int)-1073741637;
          }
          v117 = v116 - 2;
          if ( v117 )
          {
            if ( v117 == 1 )
            {
              if ( (_DWORD)a3 == 4 )
              {
                if ( SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
                {
                  PspBreakOnContextUnwindFailure = v4->m128i_i32[0];
                  return 0;
                }
                return 3221225569LL;
              }
              return 3221225476LL;
            }
            return (unsigned int)-1073741821;
          }
          if ( (_DWORD)a3 != 4 )
          {
            return (unsigned int)-1073741820;
          }
          if ( SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
          {
            ExResourceTimeoutCount = (v4->m128i_i32[0] + 3999) / 0xFA0u;
            return updated;
          }
          return (unsigned int)-1073741727;
        }
        if ( v5 == 233 )
        {
          return (unsigned int)PnpIommuBlockUnblockDevice(a2, (unsigned int)a3, a3, 1LL);
        }
        if ( v5 != 224 && v5 != 225 )
        {
          if ( v5 == 226 )
          {
            if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
            {
              return 3221225569LL;
            }
            LOBYTE(v114) = (_DWORD)v3 == 0;
            return (unsigned int)VfPtGenerateTraceInformation(v114);
          }
          else
          {
            if ( (unsigned int)(v5 - 228) > 1 )
            {
              return (unsigned int)-1073741821;
            }
            BYTE1(v118[0]) = 0;
            if ( (int)RtlCheckTokenMembership(0LL, SeAliasAdminsSid, (char *)v118 + 1, 1LL) < 0 || !BYTE1(v118[0]) )
            {
              return 3221225569LL;
            }
            return (unsigned int)KeUpdateDpcWatchdogConfiguration(v4, (unsigned int)v3);
          }
        }
      }
      else
      {
        if ( v5 == 223 )
        {
          return (unsigned int)ExPoolSetLimit(a2, (unsigned int)a3, PreviousMode);
        }
        if ( v5 > 212 )
        {
          if ( v5 == 217 )
          {
            if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
            {
              return 3221225506LL;
            }
            return (unsigned int)VfVolatileSetDifRuleClass(v4, (unsigned int)v3);
          }
          else
          {
            if ( v5 != 218 )
            {
              if ( (unsigned int)(v5 - 219) > 1 )
              {
                return (unsigned int)-1073741821;
              }
              if ( (_DWORD)a3 != 16 )
              {
                return 3221225476LL;
              }
              if ( !PreviousMode )
              {
                goto LABEL_413;
              }
              if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
              {
                return 3221225569LL;
              }
              result = VfProbeAndCaptureUnicodeString(Buf1, v4, 1LL);
              if ( (int)result < 0 )
              {
                return result;
              }
              v4 = (__m128i *)Buf1;
              if ( LOWORD(Buf1[0]) )
              {
LABEL_413:
                v113 = v5 - 219;
                if ( !v113 )
                {
                  v38 = VfVolatileApplyDifVerification(v4, a2, a3, v8);
                  goto LABEL_418;
                }
                if ( v113 == 1 )
                {
                  v38 = VfVolatileRemoveDifVerification(v4, a2, a3, v8);
                  goto LABEL_418;
                }
                goto LABEL_415;
              }
              return 3221225711LL;
            }
            if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
            {
              return 3221225506LL;
            }
            return (unsigned int)VfVolatileClearDifRuleClass();
          }
        }
        if ( v5 == 212 )
        {
          return (unsigned int)CmUpdateFeatureUsageSubscription(a2, (unsigned int)a3, PreviousMode);
        }
        if ( v5 != 199 )
        {
          if ( v5 != 204 )
          {
            v109 = (unsigned int)(v5 - 206);
            if ( v5 == 206 )
            {
              if ( (_DWORD)a3 != 8 )
              {
                return 3221225476LL;
              }
              if ( PreviousMode && !SeSinglePrivilegeCheck(SeSystemtimePrivilege, PreviousMode) )
              {
                return 3221225569LL;
              }
              if ( !(unsigned __int8)PsIsCurrentThreadInServerSilo(v109, a2, a3, v8) )
              {
                LOBYTE(v118[0]) = (unsigned __int8)v4->m128i_i64[0] != 0;
                result = ExSetLeapSecondEnabled();
                updated = result;
                if ( (int)result < 0 )
                {
                  return result;
                }
                *(_BYTE *)ExLeapSecondData = v118[0];
                return updated;
              }
              return 3221225506LL;
            }
            if ( v5 == 207 )
            {
              if ( (_DWORD)a3 != 4 )
              {
                return 3221225476LL;
              }
              if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
              {
                return 3221225506LL;
              }
              v126 = 0;
              if ( (v4->m128i_i32[0] & 0x2018) == 0 )
              {
                NtGlobalFlag2 = NtGlobalFlag2 & 0x2018 | v4->m128i_i32[0];
                v4->m128i_i32[0] = NtGlobalFlag2;
                return updated;
              }
              return 3221225485LL;
            }
            if ( v5 != 210 )
            {
              return (unsigned int)-1073741821;
            }
            return (unsigned int)CmUpdateFeatureConfiguration(a2, (unsigned int)a3, PreviousMode);
          }
          v139 = 0LL;
          if ( (unsigned int)a3 < 8 )
          {
            return 3221225476LL;
          }
          v110 = a3 - 8;
          if ( (((_BYTE)a3 - 8) & 7) != 0 || v110 > 0x100 )
          {
            return 3221225476LL;
          }
          v111 = a2->m128i_i64[0];
          memmove(v153, &a2->m128i_u64[1], v110);
          if ( v111 >= 2 )
          {
            return 3221225485LL;
          }
          LOBYTE(v112) = PreviousMode;
          updated = ExCpuSetResourceManagerAccessCheck(v112);
          if ( (updated & 0x80000000) != 0 )
          {
            return updated;
          }
          v98 = v110 >> 3;
          v99 = v111;
          v100 = v153;
          return (unsigned int)KeModifySystemAllowedCpuSets(v98, (_DWORD)v100, v97, v99);
        }
      }
    }
    else
    {
      if ( v5 == 194 )
      {
        if ( !(_DWORD)a3 )
        {
          if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )
          {
            return 3221225569LL;
          }
          updated = VslProvisionDumpEncryption();
          if ( (updated & 0x80000000) != 0 )
          {
            return updated;
          }
          return (unsigned int)IoProvisionCrashDumpKey();
        }
        return (unsigned int)-1073741811;
      }
      if ( v5 <= 155 )
      {
        if ( v5 == 155 )
        {
          return (unsigned int)CmReconcileAndValidateAllHives(a1, a2, a3, 1LL);
        }
        if ( v5 > 132 )
        {
          v84 = v5 - 134;
          if ( !v84 )
          {
            if ( (_DWORD)a3 != 32 )
            {
              return 3221225476LL;
            }
            return (unsigned int)sub_140A068A0(a1, a2, a3, 1LL);
          }
          v85 = v84 - 8;
          if ( !v85 )
          {
            *(_OWORD *)Src = 0LL;
            v90 = PsGetCurrentServerSiloGlobals(a1, a2, a3, 1LL) + 1368;
            PrivilegeValue = (LUID)v90;
            if ( (_DWORD)v3 == 48 )
            {
              if ( SeSinglePrivilegeCheck(SeShutdownPrivilege, PreviousMode) )
              {
                v148 = *v4;
                v149 = v4[1];
                v150 = v4[2];
                if ( _InterlockedCompareExchange((volatile signed __int32 *)v90, _mm_cvtsi128_si32(v148), 0) )
                {
                  return 0LL;
                }
                *(__m128i *)Src = v149;
                v149 = 0LL;
                v91 = _mm_cvtsi128_si32(*(__m128i *)Src);
                if ( v91 && (v91 & 1) == 0 )
                {
                  v92 = Src[1];
                  if ( ((__int64)Src[1] & 1) != 0 )
                  {
                    ExRaiseDatatypeMisalignment();
                  }
                  v93 = (char *)Src[1] + v91;
                  if ( (unsigned __int64)v93 > 0x7FFFFFFF0000LL || v93 < Src[1] )
                  {
                    v92 = Src[1];
                    v91 = (unsigned __int16)Src[0];
                  }
                  Pool2 = (void *)ExAllocatePool2(0x101uLL, v91, 0x50535845u);
                  v95 = (__int64)Pool2;
                  if ( Pool2 )
                  {
                    memmove(Pool2, v92, v91);
                    v149.m128i_i64[1] = v95;
                    v149.m128i_i16[0] = v91;
                    v149.m128i_i16[1] = v91;
                  }
                  v90 = (__int64)PrivilegeValue;
                }
                *(__m128i *)v90 = v148;
                *(__m128i *)(v90 + 16) = v149;
                *(__m128i *)(v90 + 32) = v150;
                return 0;
              }
              return 3221225569LL;
            }
            return 3221225476LL;
          }
          v86 = v85 - 8;
          if ( !v86 )
          {
            if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )
            {
              return 3221225569LL;
            }
            return (unsigned int)ExpSetBootLoaderMetadata(v4, (unsigned int)v3);
          }
          v87 = v86 - 1;
          if ( !v87 )
          {
            PrivilegeValue = (LUID)19LL;
            if ( !SeSinglePrivilegeCheck((LUID)19LL, PreviousMode) )
            {
              return 3221225569LL;
            }
            v88 = v3 - 1;
            if ( v88 )
            {
              if ( v88 != 3 )
              {
                return 3221225476LL;
              }
              v7 = v4->m128i_i32[0];
              v137 = v4->m128i_i32[0];
            }
            else
            {
              v89 = v4->m128i_i8[0];
              if ( v4->m128i_i8[0] && !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )
              {
                return 3221225569LL;
              }
              if ( !v89 )
              {
                v7 = 5;
              }
            }
            return (unsigned int)ExpSetSoftRebootFlags(v7);
          }
          if ( v87 != 1 )
          {
            return (unsigned int)-1073741821;
          }
          if ( (_DWORD)a3 != 8 )
          {
            return 3221225476LL;
          }
          if ( PreviousMode )
          {
            return (unsigned int)ExpQueryElamCertInfo(a2->m128i_i64[0], a2, a3, 1LL);
          }
          return 3221225485LL;
        }
        if ( v5 == 132 )
        {
          if ( (_DWORD)a3 != 4 )
          {
            return 3221225476LL;
          }
          *(_QWORD *)&DriverServiceName.Length = 7733364LL;
          DriverServiceName.Buffer = L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\condrv";
          if ( a2->m128i_i8[0] & 1 )
          {
            CurrentThread = KeGetCurrentThread();
            --CurrentThread->KernelApcDisable;
            v81 = KeAbPreAcquire(&ExpConDrvLoadLock, 0LL, 0LL, 1LL);
            v82 = v81;
            if ( _interlockedbittestandset64((volatile signed __int32 *)&ExpConDrvLoadLock, 0LL) )
            {
              ExfAcquirePushLockExclusiveEx(&ExpConDrvLoadLock, v81, &ExpConDrvLoadLock);
            }
            if ( v82 )
            {
              *(_BYTE *)(v82 + 10) = 1;
            }
            updated = ZwLoadDriver(&DriverServiceName);
            if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)&ExpConDrvLoadLock, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )
            {
              ExfTryToWakePushLock(&ExpConDrvLoadLock);
            }
            KeAbPostRelease((ULONG_PTR)&ExpConDrvLoadLock);
            KeLeaveCriticalRegion();
            return updated;
          }
          if ( !SeSinglePrivilegeCheck(SeLoadDriverPrivilege, PreviousMode) )
          {
            return 3221225569LL;
          }
          return (unsigned int)ZwUnloadDriver(&DriverServiceName);
        }
        else
        {
          v74 = v5 - 126;
          if ( v74 )
          {
            v75 = v74 - 1;
            if ( !v75 )
            {
              if ( (_DWORD)a3 == 16 )
              {
                if ( SeSinglePrivilegeCheck(SeProfileSingleProcessPrivilege, PreviousMode) )
                {
                  v138 = (HANDLE)v4->m128i_i64[0];
                  updated = MmScrubMemory(v79, v138, &v134);
                  PrivilegeValue.LowPart = updated;
                  v4->m128i_i64[1] = v134;
                  return updated;
                }
                return 3221225569LL;
              }
              return 3221225476LL;
            }
            v76 = v75 - 2;
            if ( v76 )
            {
              v77 = v76 - 1;
              if ( !v77 )
              {
                if ( (((_DWORD)a3 - 16) & 0xFFFFFFE7) == 0 && (_DWORD)a3 != 40 )
                {
                  if ( SeSinglePrivilegeCheck(SeProfileSingleProcessPrivilege, PreviousMode) )
                  {
                    memmove(&v148, v4, v3);
                    if ( (_DWORD)v3 == 32 )
                    {
                      v6 = v149.m128i_i64[1];
                    }
                    ThreadPartition = MiGetThreadPartition(KeGetCurrentThread());
                    updated = MiCombineIdenticalPages(
                                ThreadPartition,
                                v148.m128i_i64[0],
                                v149.m128i_u32[0],
                                v6,
                                KeGetCurrentThread()->PreviousMode,
                                &v133,
                                v118[0]);
                    PrivilegeValue.LowPart = updated;
                    v4->m128i_i64[1] = v133;
                    return updated;
                  }
                  return 3221225569LL;
                }
                return 3221225476LL;
              }
              if ( v77 != 1 )
              {
                return (unsigned int)-1073741821;
              }
              if ( PreviousMode )
              {
                return 3221225506LL;
              }
              if ( (_DWORD)a3 != 24 )
              {
                return 3221225476LL;
              }
              return (unsigned int)KeInitializeEntropySystem(
                                     a2->m128i_i64[0],
                                     a2->m128i_i64[1],
                                     a2[1].m128i_i64[0],
                                     1LL);
            }
            else
            {
              LOBYTE(a3) = PreviousMode;
              return (unsigned int)KeProcessorProfileControlArea(a2, (unsigned int)v3, a3, 1LL);
            }
          }
          else
          {
            if ( PreviousMode )
            {
              return 3221225569LL;
            }
            if ( (_DWORD)a3 != 32 )
            {
              return (unsigned int)-1073741820;
            }
            return (unsigned int)BgkSetBootGraphicsInformation(a1, a2, a3, 1LL);
          }
        }
      }
      if ( v5 > 176 )
      {
        if ( v5 == 177 )
        {
          v143 = 0LL;
          v144 = 0LL;
          if ( (_DWORD)a3 != 8 )
          {
            if ( (_DWORD)a3 )
            {
              return 3221225476LL;
            }
            updated = 0;
            Process = KeGetCurrentThread()->ApcState.Process;
            SessionId = PsGetSessionIdEx(Process, a2, a3, 1LL);
            if ( SessionId == -1 )
            {
              return updated;
            }
            v144 = Process;
            return (unsigned int)PsInvokeWin32Callout(32LL, &v143, 1LL, &SessionId);
          }
          if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )
          {
            return 3221225569LL;
          }
          v146 = (HANDLE)v4->m128i_i64[0];
          Object = 0LL;
          updated = ObReferenceObjectByHandle(v146, 0x1000u, (POBJECT_TYPE)PsProcessType, PreviousMode, &Object, 0LL);
          if ( (updated & 0x80000000) != 0 )
          {
            return updated;
          }
          v59 = (_KPROCESS *)Object;
          SessionId = PsGetSessionIdEx(Object, v105, v106, v107);
          if ( SessionId != -1 )
          {
            v144 = v59;
            updated = PsInvokeWin32Callout(32LL, &v143, 1LL, &SessionId);
          }
          goto LABEL_214;
        }
        if ( v5 == 187 )
        {
          if ( (_DWORD)a3 != 24 )
          {
            return 3221225476LL;
          }
          v148 = *a2;
          v149.m128i_i64[0] = a2[1].m128i_i64[0];
          result = VfProbeAndCaptureUnicodeStringBuffer(&v148, 1LL, a3, 1LL);
          if ( (int)result < 0 )
          {
            return result;
          }
          updated = PsSetExeModerationState(&v148, v149.m128i_u32[0], v149.m128i_u32[1]);
          v70 = &v148;
          goto LABEL_421;
        }
        v104 = (unsigned int)(v5 - 190);
        if ( v5 != 190 )
        {
          if ( v5 != 191 )
          {
            return (unsigned int)-1073741821;
          }
          if ( !(_DWORD)a3 )
          {
            if ( PreviousMode && !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
            {
              return 3221225569LL;
            }
            return (unsigned int)VslRelaxQuotas(v104, a2, a3, v8);
          }
          return (unsigned int)-1073741811;
        }
      }
      else
      {
        switch ( v5 )
        {
          case 176:
            if ( (unsigned int)a3 < 8 )
            {
              return 3221225476LL;
            }
            v101 = a3 - 8;
            if ( (((_BYTE)a3 - 8) & 7) != 0 || v101 > 0x100 )
            {
              return 3221225476LL;
            }
            v102 = a2->m128i_i64[0];
            v147 = a2->m128i_i64[0];
            memmove(v152, &a2->m128i_u64[1], v101);
            LOBYTE(v103) = PreviousMode;
            updated = ExCpuSetResourceManagerAccessCheck(v103);
            if ( (updated & 0x80000000) != 0 )
            {
              return updated;
            }
            return (unsigned int)KeSetTagCpuSets(v101 >> 3, v152, v102);
          case 159:
            return (unsigned int)-1073741637;
          case 161:
            if ( PreviousMode )
            {
              return 3221225506LL;
            }
            if ( (_DWORD)a3 != 8 )
            {
              return 3221225476LL;
            }
            return (unsigned int)KdInitialize(3LL, a2, (char *)&KdDebuggerNotPresent + 4, 1LL);
        }
        if ( v5 != 164 )
        {
          if ( v5 != 168 )
          {
            if ( v5 != 170 )
            {
              return (unsigned int)-1073741821;
            }
            v142 = 0LL;
            if ( (_DWORD)a3 != 16 )
            {
              return 3221225476LL;
            }
            if ( !SeSinglePrivilegeCheck(SeIncreaseBasePriorityPrivilege, PreviousMode) )
            {
              return 3221225569LL;
            }
            return (unsigned int)KeIntSteerAssignCpuSetForGsiv(
                                   v4->m128i_i64[0],
                                   HIDWORD(v4->m128i_i64[0]),
                                   v4->m128i_i64[1]);
          }
          if ( (a3 & 7) != 0 || (unsigned int)a3 > 0x100 )
          {
            return 3221225476LL;
          }
          memmove(v151, a2, (unsigned int)a3);
          LOBYTE(v96) = PreviousMode;
          result = ExCpuSetResourceManagerAccessCheck(v96);
          if ( (int)result < 0 )
          {
            return result;
          }
          v98 = (unsigned int)v3 >> 3;
          v99 = 0;
          v100 = v151;
          return (unsigned int)KeModifySystemAllowedCpuSets(v98, (_DWORD)v100, v97, v99);
        }
      }
    }
    if ( qword_140F04A00 )
    {
      return (unsigned int)guard_dispatch_icall_no_overrides((unsigned int)v5, a2, (unsigned int)a3, 1LL);
    }
    return (unsigned int)-1073741637;
  }
  if ( v5 == 113 )
  {
    LOBYTE(a3) = PreviousMode;
    return (unsigned int)PsSetCpuQuotaInformation(a2, (unsigned int)v3, a3, 1LL);
  }
  if ( v5 > 71 )
  {
    if ( v5 <= 92 )
    {
      if ( v5 == 92 )
      {
        if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
        {
          return 3221225506LL;
        }
        if ( (_DWORD)v3 != 40 )
        {
          return 3221225476LL;
        }
        return (unsigned int)VfSetVerifierInformationEx(v4);
      }
      if ( v5 <= 81 )
      {
        if ( v5 == 81 )
        {
LABEL_175:
          v120[0] = 0;
          if ( (unsigned int)a3 < 0x40 )
          {
            return 3221225476LL;
          }
          if ( v5 == 21 )
          {
            v50 = 0;
            v129 = 0;
          }
          else
          {
            v129 = a2[3].m128i_i32[3];
            v50 = v129;
            if ( (v129 & 0xFFFFFFF0) != 0 || (v129 & 0xC) == 0xC || (v129 & 3) == 3 )
            {
              return 3221225712LL;
            }
          }
          if ( SeSinglePrivilegeCheck(SeIncreaseQuotaPrivilege, PreviousMode) )
          {
            LOBYTE(v51) = 1;
            return MmAdjustWorkingSetSizeEx(v4[1].m128i_i64[1], v4[2].m128i_i64[0], 1, v51, v50, (__int64)v120);
          }
          return 3221225506LL;
        }
        if ( v5 != 72 )
        {
          switch ( v5 )
          {
            case 'J':
              return 3221225474LL;
            case 'K':
              LOBYTE(a3) = PreviousMode;
              return (unsigned int)ExpRegisterFirmwareTableInformationHandler(a2, (unsigned int)v3, a3, 1LL);
            case 'O':
              LOBYTE(v8) = PreviousMode;
              return (unsigned int)PfSetSuperfetchInformation(79LL, a2, (unsigned int)a3, v8);
            case 'P':
              if ( (unsigned int)a3 < 4 )
              {
                return (unsigned int)-1073741820;
              }
              v128 = 6;
              v47 = a2->m128i_u32[0];
              LOBYTE(a2) = PreviousMode;
              return (unsigned int)MmIssueMemoryListCommand(v47, a2, -1LL, 1LL);
            default:
              return (unsigned int)-1073741821;
          }
        }
        if ( PreviousMode || !a2 || (_DWORD)a3 != 8 )
        {
          return 3221225485LL;
        }
        if ( !a2->m128i_i32[0] )
        {
          return (unsigned int)-1073741637;
        }
        v48 = (unsigned int)(a2->m128i_i32[0] - 1);
        if ( a2->m128i_i32[0] == 1
          || (v48 = (unsigned int)(a2->m128i_i32[0] - 2), a2->m128i_i32[0] == 2)
          || (v48 = (unsigned int)(a2->m128i_i32[0] - 3), a2->m128i_i32[0] == 3) )
        {
          guard_dispatch_icall_no_overrides(v48, a2, a3, 1LL);
          return 0;
        }
        if ( a2->m128i_i32[0] == 4 )
        {
          return (unsigned int)-1073741637;
        }
        v49 = (unsigned int)(a2->m128i_i32[0] - 6);
        if ( a2->m128i_i32[0] == 6 )
        {
          LODWORD(v49) = 1;
          return (unsigned int)guard_dispatch_icall_no_overrides(v49, a2, a3, 1LL);
        }
        if ( a2->m128i_i32[0] == 7 )
        {
          updated = 0;
          if ( off_140E00B18[0] == xKdEnumerateDebuggingDevices )
          {
            return (unsigned int)-1073741822;
          }
          return updated;
        }
        return (unsigned int)-1073741811;
      }
      v52 = v5 - 82;
      if ( v52 )
      {
        v53 = v52 - 4;
        if ( !v53 )
        {
          return (unsigned int)ObSetRefTraceInformation(a2, (unsigned int)a3, a3, 1LL);
        }
        v54 = v53 - 1;
        if ( !v54 )
        {
          if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
          {
            return (unsigned int)-1073741790;
          }
          if ( (_DWORD)v3 == 8 )
          {
            v58 = v4->m128i_i64[0];
            MmSpecialPoolTag = v4->m128i_i64[0];
            MmSpecialPoolCatchOverruns = BYTE4(v58) & 1;
            return updated;
          }
          return (unsigned int)-1073741820;
        }
        v55 = v54 - 2;
        if ( v55 )
        {
          if ( v55 != 2 )
          {
            return (unsigned int)-1073741821;
          }
          if ( !PreviousMode )
          {
            if ( (_DWORD)a3 )
            {
              return (unsigned int)-1073741584;
            }
            else
            {
              return HvlHypervisorConnected == 0 ? 0xC0351000 : 0;
            }
          }
          return (unsigned int)-1073741790;
        }
        if ( PreviousMode != 1 )
        {
          return (unsigned int)-1073741637;
        }
        if ( SeSinglePrivilegeCheck(SeTcbPrivilege, 1) )
        {
          v56 = KeGetCurrentThread()->ApcState.Process;
          if ( !v56[1].ReadyTime )
          {
            return (unsigned int)DbgkRegisterErrorPort(v4, (unsigned int)v3);
          }
          v57 = WORD2(v56[3].PerProcessorCycleTimes);
          if ( v57 != 332 && v57 != 452 )
          {
            LOBYTE(v7) = 0;
          }
          if ( !(_BYTE)v7 )
          {
            return (unsigned int)DbgkRegisterErrorPort(v4, (unsigned int)v3);
          }
          return (unsigned int)-1073741637;
        }
        return (unsigned int)-1073741727;
      }
      if ( (unsigned int)a3 < 0x18 )
      {
        return 3221225476LL;
      }
      if ( !SeSinglePrivilegeCheck(SeIncreaseBasePriorityPrivilege, PreviousMode) )
      {
        return 3221225569LL;
      }
      if ( PreviousMode )
      {
        v148 = *v4;
        v149.m128i_i64[0] = v4[1].m128i_i64[0];
        v4 = &v148;
      }
      if ( (unsigned int)(v4[1].m128i_i32[0] - 1) > 0x1E )
      {
        return 3221225485LL;
      }
      v145 = *v4;
      updated = PsLookupProcessThreadByCid(&v145, 0LL, &v132);
      if ( (updated & 0x80000000) != 0 )
      {
        return updated;
      }
      v59 = (_KPROCESS *)v132;
      if ( *((_BYTE *)v132 + 4) )
      {
        updated = -1073741749;
      }
      else
      {
        KeSetActualBasePriorityThread((ULONG_PTR)v132);
        updated = 259;
      }
LABEL_214:
      ObfDereferenceObject(v59);
      return updated;
    }
    if ( v5 > 104 )
    {
      v71 = v5 - 106;
      if ( !v71 )
      {
        return (unsigned int)-1073741821;
      }
      v72 = v71 - 3;
      if ( v72 )
      {
        v73 = v72 - 1;
        if ( !v73 )
        {
          return (unsigned int)-1073741822;
        }
        if ( v73 != 1 )
        {
          return (unsigned int)-1073741821;
        }
        if ( (_DWORD)a3 != 4 )
        {
          return 3221225476LL;
        }
        if ( SeSinglePrivilegeCheck(SeProfileSingleProcessPrivilege, PreviousMode) )
        {
          *(_DWORD *)(MmWriteableSharedUserData + 584) = v4->m128i_i32[0];
          return 0;
        }
        return 3221225506LL;
      }
      LOBYTE(v8) = PreviousMode;
      return (unsigned int)SmSetStoreInformation(a1, a2, (unsigned int)a3, v8);
    }
    if ( v5 == 104 )
    {
      LOBYTE(a3) = PreviousMode;
      return (unsigned int)ExpSetProcessorMicrocodeUpdateInformation(a2, (unsigned int)v3, a3, 1LL);
    }
    v60 = v5 - 93;
    if ( !v60 )
    {
      v64 = 172;
LABEL_226:
      if ( (_DWORD)a3 != v64 )
      {
        return 3221225476LL;
      }
      if ( PreviousMode && !SeSinglePrivilegeCheck(SeTimeZonePrivilege, PreviousMode) )
      {
        return 3221225569LL;
      }
      return (unsigned int)ExpSetTimeZoneInformation(v4, v64, a3, v8);
    }
    v61 = v60 - 1;
    if ( !v61 )
    {
      if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )
      {
        return (unsigned int)-1073741790;
      }
      if ( (_DWORD)v3 == 8 )
      {
        v148.m128i_i64[0] = v4->m128i_i64[0];
        _InterlockedOr((volatile signed __int32 *)(MmWriteableSharedUserData + 928), v148.m128i_u32[0]);
        _InterlockedAnd((volatile signed __int32 *)(MmWriteableSharedUserData + 928), ~v148.m128i_i32[1]);
        return updated;
      }
      return (unsigned int)-1073741820;
    }
    v62 = v61 - 1;
    if ( !v62 )
    {
      return 3221225659LL;
    }
    v63 = v62 - 2;
    if ( v63 )
    {
      if ( v63 != 5 )
      {
        return (unsigned int)-1073741821;
      }
      v64 = 432;
      goto LABEL_226;
    }
    if ( (_DWORD)a3 != 40 )
    {
      return 3221225476LL;
    }
    if ( PreviousMode )
    {
      if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
      {
        return 3221225569LL;
      }
      v148 = *v4;
      v149 = v4[1];
      v150.m128i_i64[0] = v4[2].m128i_i64[0];
      result = VfProbeAndCaptureUnicodeStringBuffer(&v148.m128i_u64[1], 1LL, v65, v66);
      if ( (int)result < 0 )
      {
        return result;
      }
      v69 = VfProbeAndCaptureUnicodeStringBuffer(&v149.m128i_u64[1], 1LL, v67, v68);
      if ( v69 < 0 )
      {
        VfFreeCapturedUnicodeString(&v148.m128i_u64[1]);
        return (unsigned int)v69;
      }
      v4 = &v148;
    }
    updated = VfFaultsSetParameters(v4, a2, a3, v8);
    if ( !PreviousMode )
    {
      return updated;
    }
    VfFreeCapturedUnicodeString(&v4->m128i_u64[1]);
    v70 = (__m128i *)((char *)v4 + 24);
LABEL_421:
    VfFreeCapturedUnicodeString(v70);
    return updated;
  }
  if ( v5 == 71 )
  {
    return 3221225659LL;
  }
  if ( v5 > 39 )
  {
    if ( v5 <= 51 )
    {
      if ( v5 != 51 )
      {
        if ( v5 != 40 && v5 != 41 )
        {
          v35 = (unsigned int)(v5 - 46);
          if ( v5 == 46 )
          {
            if ( (_DWORD)a3 != 8 )
            {
              return 3221225476LL;
            }
            if ( PreviousMode && !SeSinglePrivilegeCheck(SeSystemtimePrivilege, PreviousMode) )
            {
              return 3221225569LL;
            }
            if ( !(unsigned __int8)PsIsCurrentThreadInServerSilo(v35, a2, a3, v8) )
            {
              v138 = (HANDLE)v4->m128i_i64[0];
              if ( v138 )
              {
                Object = 0LL;
                updated = ObReferenceObjectByHandle(
                            v138,
                            2u,
                            (POBJECT_TYPE)ExEventObjectType,
                            PreviousMode,
                            &Object,
                            0LL);
                v36 = Object;
                if ( (updated & 0x80000000) != 0 )
                {
                  return updated;
                }
              }
              else
              {
                v36 = 0LL;
                updated = 0;
              }
              KdUpdateTimeSlipEvent(v36);
              return updated;
            }
            return 3221225506LL;
          }
          if ( (unsigned int)(v5 - 47) >= 2 )
          {
            return (unsigned int)-1073741821;
          }
          return 3221225474LL;
        }
        if ( (_DWORD)a3 != 16 )
        {
          return 3221225476LL;
        }
        if ( !PreviousMode )
        {
          goto LABEL_115;
        }
        if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
        {
          return 3221225569LL;
        }
        result = VfProbeAndCaptureUnicodeString(Buf1, v4, 1LL);
        if ( (int)result < 0 )
        {
          return result;
        }
        v4 = (__m128i *)Buf1;
        if ( LOWORD(Buf1[0]) )
        {
LABEL_115:
          v37 = v5 - 40;
          if ( !v37 )
          {
            v38 = VfAddVerifierEntry((PCUNICODE_STRING)v4);
            goto LABEL_418;
          }
          if ( v37 == 1 )
          {
            v38 = VfRemoveVerifierEntry(v4, a2, a3, v8);
LABEL_418:
            updated = v38;
            goto LABEL_419;
          }
LABEL_415:
          updated = -1073741821;
LABEL_419:
          if ( !PreviousMode )
          {
            return updated;
          }
          v70 = v4;
          goto LABEL_421;
        }
        return 3221225711LL;
      }
      if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
      {
        return 3221225506LL;
      }
      return (unsigned int)VfSetVerifierInformation(v4, (unsigned int)v3, 0LL);
    }
    switch ( v5 )
    {
      case '6':
        LODWORD(v8) = 0;
        goto LABEL_131;
      case '8':
        LOBYTE(v8) = PreviousMode;
        return (unsigned int)PfSnSetPrefetcherInformation(a1, a2, (unsigned int)a3, v8);
      case ';':
        if ( (_DWORD)a3 == 4 )
        {
          v39 = a2->m128i_i32[0];
          updated = ExpUpdateComPlusPackage(a2->m128i_u32[0], a2, a3, 1LL);
          if ( (updated & 0x80000000) == 0 )
          {
            *(_DWORD *)(MmWriteableSharedUserData + 736) = v39;
          }
          return updated;
        }
        return 3221225476LL;
    }
    if ( v5 != 69 )
    {
      return (unsigned int)-1073741821;
    }
    return (unsigned int)-1073741637;
  }
  if ( v5 == 39 )
  {
    if ( (_DWORD)a3 == 4 )
    {
      if ( SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )
      {
        v141 = v4->m128i_i32[0];
        LOBYTE(v34) = 1;
        PsChangeQuantumTable(v34);
        return 0;
      }
      return 3221225569LL;
    }
    return 3221225476LL;
  }
  if ( v5 <= 28 )
  {
    if ( v5 != 28 )
    {
      if ( v5 != 9 )
      {
        if ( v5 != 21 )
        {
          if ( v5 == 24 )
          {
            if ( (_DWORD)a3 == 20 )
            {
              if ( !PreviousMode || SeSinglePrivilegeCheck(SeLoadDriverPrivilege, PreviousMode) )
              {
                v148 = *v4;
                v149.m128i_i32[0] = v4[1].m128i_i32[0];
                KiMinimumDpcRate = v148.m128i_i32[2];
                KiMaximumDpcQueueDepth = v148.m128i_i32[1];
                KiAdjustDpcThreshold = v148.m128i_i32[3];
                KiIdealDpcRate = v149.m128i_i32[0];
                KeSynchronizeWithDynamicProcessors(v149.m128i_u32[0], a2, a3, v8);
                ActiveProcessorCount = KeQueryActiveProcessorCountEx(0xFFFFu);
                if ( ActiveProcessorCount )
                {
                  v13 = &KiProcessorBlock;
                  v14 = ActiveProcessorCount;
                  do
                  {
                    v15 = *v13;
                    *(_DWORD *)(v15 + 14504) = KiMaximumDpcQueueDepth;
                    *(_DWORD *)(v15 + 14512) = KiMinimumDpcRate;
                    ++v13;
                    --v14;
                  }
                  while ( v14 );
                }
                return updated;
              }
              return 3221225569LL;
            }
            return 3221225476LL;
          }
          if ( v5 != 26 )
          {
            if ( v5 == 27 )
            {
              if ( (_DWORD)a3 == 8 )
              {
                if ( !PreviousMode )
                {
                  MmUnloadSystemImage(a2->m128i_i64[0]);
                  return 0;
                }
                return 3221225569LL;
              }
              return 3221225476LL;
            }
            return (unsigned int)-1073741821;
          }
LABEL_131:
          if ( (_DWORD)a3 == 48 )
          {
            v40 = 0;
          }
          else
          {
            if ( (_DWORD)a3 != 56 )
            {
              return 3221225476LL;
            }
            v40 = 1;
          }
          if ( !PreviousMode )
          {
            *(__m128i *)Buf1 = *a2;
            SystemImage = MmLoadSystemImage((unsigned int)Buf1, 0, 0, v8, (__int64)&v131, (__int64)&v130);
            updated = SystemImage;
            if ( SystemImage < 0 )
            {
              if ( SystemImage == -1073741411 )
              {
                return (unsigned int)-1073741554;
              }
            }
            else
            {
              v43 = v130;
              if ( v40 )
              {
                LOBYTE(v42) = 1;
                v44 = RtlImageDirectoryEntryToData(v130, v42, 0LL, &Object);
              }
              else
              {
                v44 = 0LL;
              }
              v45 = RtlImageNtHeader(v43);
              v46 = &v43[*(unsigned int *)(v45 + 40)];
              v4[1].m128i_i64[0] = (__int64)v43;
              v4[1].m128i_i64[1] = v131;
              v4[2].m128i_i64[0] = (__int64)v46;
              if ( v40 )
              {
                v4[2].m128i_i64[1] = v44;
                v4[3].m128i_i32[0] = *(_DWORD *)(v45 + 80);
              }
              else
              {
                v4[2].m128i_i32[2] = *(_DWORD *)(v45 + 80);
              }
            }
            return updated;
          }
          return 3221225569LL;
        }
        goto LABEL_175;
      }
      if ( (_DWORD)a3 == 4 )
      {
        if ( SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )
        {
          NtGlobalFlag = NtGlobalFlag & 0x6DCE640F | v4->m128i_i32[0] & 0x92319BF0;
          v4->m128i_i32[0] = NtGlobalFlag;
          return updated;
        }
        return (unsigned int)-1073741790;
      }
      return (unsigned int)-1073741820;
    }
    if ( (((_DWORD)a3 - 8) & 0xFFFFFFF7) != 0 )
    {
      return 3221225476LL;
    }
    if ( PreviousMode && !SeSinglePrivilegeCheck(SeSystemtimePrivilege, PreviousMode) )
    {
      return 3221225569LL;
    }
    if ( !(unsigned __int8)PsIsCurrentThreadInServerSilo(a1, a2, a3, v8) )
    {
      if ( (_DWORD)v3 == 16 )
      {
        LOBYTE(v17) = v4->m128i_i8[8];
        LOBYTE(v118[0]) = v17;
        v127 = v4->m128i_i64[0];
      }
      else
      {
        LOBYTE(v17) = v4->m128i_i8[4];
        LOBYTE(v118[0]) = v17;
        v18 = v4->m128i_u32[0];
        v127 = v18;
        if ( v18 )
        {
          v16 = MEMORY[0xFFFFF78000000300] * (unsigned __int64)(unsigned int)KeMaximumIncrement % v18;
          v127 = MEMORY[0xFFFFF78000000300] * (unsigned __int64)(unsigned int)KeMaximumIncrement / v18;
        }
      }
      if ( (_BYTE)v17 )
      {
        v127 = MEMORY[0xFFFFF78000000300];
      }
      ExAcquireTimeRefreshLockExclusive(v17, v16, 0xFFFFF78000000300uLL);
      updated = ExpUpdateTimerConfiguration(0LL, &v127, v118);
      ExReleaseTimeRefreshLockExclusive();
      return updated;
    }
    return 3221225506LL;
  }
  v19 = v5 - 30;
  if ( !v19 )
  {
    return (unsigned int)MmCreateMirror();
  }
  v20 = v19 - 1;
  if ( !v20 )
  {
    return (unsigned int)EtwSetPerformanceTraceInformation(a2, (unsigned int)a3, PreviousMode);
  }
  v21 = v20 - 3;
  if ( !v21 )
  {
    if ( PreviousMode && !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode)
      || (unsigned __int8)PsIsCurrentThreadInServerSilo(a1, a2, a3, v8) )
    {
      return 3221225569LL;
    }
    if ( (_DWORD)v3 != 4 )
    {
      goto LABEL_78;
    }
    if ( PreviousMode )
    {
      if ( ((unsigned __int8)v4 & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v32 = v4->m128i_i32[0];
      v140 = v4->m128i_i32[0];
    }
    else
    {
      v32 = v4->m128i_i32[0];
    }
    if ( !v32 )
    {
      v31 = 0LL;
      return (unsigned int)IoConfigureCrashDump(v31, 0LL);
    }
    v33 = v32 - 1;
    if ( !v33 )
    {
LABEL_78:
      v31 = 1LL;
      return (unsigned int)IoConfigureCrashDump(v31, 0LL);
    }
    if ( v33 == 1 )
    {
      return (unsigned int)WheaCrashDumpInitializationComplete();
    }
    return 3221225485LL;
  }
  v22 = v21 - 3;
  if ( !v22 )
  {
    if ( (_DWORD)a3 != 16 )
    {
      return 3221225476LL;
    }
    if ( PreviousMode && !SeSinglePrivilegeCheck(SeIncreaseQuotaPrivilege, PreviousMode) )
    {
      return 3221225569LL;
    }
    return (unsigned int)CmSetRegistryQuotaInformation(v4, a2, a3, v8);
  }
  if ( v22 != 1 )
  {
    return (unsigned int)-1073741821;
  }
  if ( (_DWORD)a3 != 16 )
  {
    return 3221225476LL;
  }
  if ( !PreviousMode )
  {
    *(__m128i *)Buf1 = *a2;
    if ( !memcmp((const void *)a2->m128i_i64[1], L"\\SystemRoot\\System32\\win32k.sys", 0x3EuLL) )
    {
      v25 = *(_QWORD *)(PsWin32kDataTableEntry + 48);
      v26 = RtlImageNtHeader(v25);
      if ( !v26 )
      {
        MmUnloadSystemImage(PsWin32kDataTableEntry);
        return 3221225595LL;
      }
      updated = ExpInitializeSessionDriver(v25 + *(unsigned int *)(v26 + 40), v25);
      if ( (updated & 0x80000000) != 0
        && !(unsigned int)PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process, v27, v28, v29) )
      {
        MmUnloadSystemImage(PsWin32kDataTableEntry);
      }
      return updated;
    }
    return 3221225659LL;
  }
  if ( (HIDWORD(KeGetCurrentThread()->ApcState.Process[4].IdealProcessorAssignmentBlock) & 8) == 0
    || !SeSinglePrivilegeCheck(SeLoadDriverPrivilege, 1) )
  {
    return 3221225569LL;
  }
  v23 = 0x7FFFFFFF0000LL;
  if ( (unsigned __int64)v4 < 0x7FFFFFFF0000LL )
  {
    v23 = (__int64)v4;
  }
  LODWORD(Buf1[0]) = *(_DWORD *)v23;
  Buf1[1] = *(void **)(v23 + 8);
  if ( LOWORD(Buf1[0]) != 62 )
  {
    return 3221225569LL;
  }
  if ( memcmp(Buf1[1], L"\\SystemRoot\\System32\\win32k.sys", 0x3EuLL) )
  {
    return 3221225569LL;
  }
  Buf1[1] = L"\\SystemRoot\\System32\\win32k.sys";
  WORD1(Buf1[0]) = 62;
  return ZwSetSystemInformation(38LL, Buf1);
}
