NTSTATUS __stdcall NtPowerInformation(
        POWER_INFORMATION_LEVEL InformationLevel,
        PVOID InputBuffer,
        ULONG InputBufferLength,
        PVOID OutputBuffer,
        ULONG OutputBufferLength)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v169 = InformationLevel;
  v166 = 0;
  memset_0(&v180, 0, 0x600uLL);
  LODWORD(v171) = 0;
  v170 = 0LL;
  Src = 0LL;
  Length_4 = 0;
  Pool2 = 0LL;
  v167 = 0LL;
  P[0] = 0LL;
  LOBYTE(v11) = 0;
  v165 = 0;
  v162[0] = 0;
  LODWORD(v168) = 0;
  v163 = 0;
  if ( (unsigned int)InformationLevel > (PlatformInformation|ProcessorPerfStates) )
  {
    goto LABEL_278;
  }
  LOBYTE(v9) = KeGetCurrentThread()->PreviousMode;
  PreviousMode = v9;
  v12 = InputBuffer != 0LL ? InputBufferLength : 0;
  v13 = (void *)((unsigned __int64)InputBuffer & -(__int64)(v12 != 0));
  P[1] = v13;
  Length = OutputBuffer != 0LL ? OutputBufferLength : 0;
  v14 = (volatile void *)((unsigned __int64)OutputBuffer & -(__int64)(Length != 0));
  if ( (_BYTE)v9 )
  {
    v15 = v169;
    if ( (unsigned int)v169 <= ProcessorCap && (v11 = 0x7C30200C0LL, _bittest64(&v11, v169))
      || (v16 = (unsigned int)(v169 - 39), (unsigned int)v16 <= 0x3A)
      && (v11 = 0x4104F0071C2E50FLL, _bittest64(&v11, v16)) )
    {
      IsAppContainerOrIdentifyLevelContext = -1073741790;
      InputBuffer = v13;
      Pool2 = v167;
      goto LABEL_279;
    }
    v18 = (unsigned int)(v169 - 37);
    if ( (unsigned int)v18 <= 0x3A && (v11 = 0x608801808940903LL, _bittest64(&v11, v18))
      || (unsigned int)(v169 - 2) <= 1 )
    {
      IsAppContainerOrIdentifyLevelContext = SeIsAppContainerOrIdentifyLevelContext(0LL);
      v163 = IsAppContainerOrIdentifyLevelContext;
      if ( IsAppContainerOrIdentifyLevelContext < 0 )
      {
        InputBuffer = v13;
        Pool2 = v167;
        goto LABEL_279;
      }
      LOBYTE(v9) = PreviousMode;
    }
    else
    {
      IsAppContainerOrIdentifyLevelContext = v163;
    }
    if ( (unsigned int)(v169 - 92) <= 1 )
    {
      IsAppContainerOrIdentifyLevelContext = ExCheckFullProcessInformationAccess(v9);
      v163 = IsAppContainerOrIdentifyLevelContext;
      if ( IsAppContainerOrIdentifyLevelContext < 0 )
      {
        InputBuffer = v13;
        Pool2 = v167;
        goto LABEL_279;
      }
    }
    v19 = (unsigned int)(v169 - 28);
    if ( (unsigned int)v19 <= 0x32 )
    {
      v9 = 0x4000000420001LL;
      if ( _bittest64(&v9, v19) )
      {
        if ( !(unsigned __int8)PspIsContextAdmin() )
        {
          IsAppContainerOrIdentifyLevelContext = -1073741790;
          v163 = -1073741790;
          InputBuffer = v13;
          Pool2 = v167;
          goto LABEL_279;
        }
      }
    }
    if ( v169 == SuspendResumeInvocation && !(unsigned __int8)PopIsRunningAsLocalSystem(v9) )
    {
      IsAppContainerOrIdentifyLevelContext = -1073741790;
      v163 = -1073741790;
      InputBuffer = v13;
      Pool2 = v167;
      goto LABEL_279;
    }
    if ( v13 )
    {
      v20 = (unsigned int)(v169 - 11);
      if ( (unsigned int)v20 > 0x3D || (v9 = 0x2001800900000001LL, !_bittest64(&v9, v20)) )
      {
        if ( (unsigned __int8)PsIsCurrentThreadInServerSilo(v9) )
        {
          IsAppContainerOrIdentifyLevelContext = -1073741637;
          v163 = -1073741637;
          InputBuffer = v13;
          Pool2 = v167;
          goto LABEL_279;
        }
      }
      if ( v169 == AdministratorPowerPolicy
        || (v21 = (unsigned int)(v169 - 38), (unsigned int)v21 <= 0x39)
        && (v22 = 0x218000004422401LL, _bittest64(&v22, v21)) )
      {
        if ( !(unsigned __int8)PspIsContextAdmin() )
        {
          IsAppContainerOrIdentifyLevelContext = -1073741790;
          v163 = -1073741790;
          InputBuffer = v13;
          Pool2 = v167;
          goto LABEL_279;
        }
      }
      if ( (unsigned int)(v169 - 2) > 1 )
      {
        v23 = (unsigned int)(v169 - 37);
        if ( (unsigned int)v23 > 0x3D || (v24 = 0x2FAE005C28F40BC3LL, !_bittest64(&v24, v23)) )
        {
          if ( v169 != ProcessorInformation )
          {
            v25 = SeShutdownPrivilege;
            if ( v169 == SystemReserveHiberFile )
            {
              v25 = SeCreatePagefilePrivilege;
            }
            v179 = v25;
            if ( !SeSinglePrivilegeCheck(v25, PreviousMode) )
            {
              IsAppContainerOrIdentifyLevelContext = -1073741727;
              v163 = -1073741727;
              InputBuffer = v13;
              Pool2 = v167;
              goto LABEL_279;
            }
          }
        }
      }
      if ( v12 )
      {
        v26 = (unsigned __int64)v13 + v12;
        if ( v26 > 0x7FFFFFFF0000LL || v26 < (unsigned __int64)v13 )
        {
          IsAppContainerOrIdentifyLevelContext = v163;
        }
      }
      if ( v12 > 0x40 )
      {
        Pool2 = (_QWORD *)ExAllocatePool2(0x100uLL, v12, 0x206D654Du);
        v167 = Pool2;
        if ( !Pool2 )
        {
          IsAppContainerOrIdentifyLevelContext = -1073741670;
          v163 = -1073741670;
          InputBuffer = v13;
          goto LABEL_279;
        }
      }
      else
      {
        Pool2 = psz;
        v167 = psz;
      }
      memmove(Pool2, v13, v12);
    }
    else
    {
      Pool2 = v167;
    }
    if ( v14 )
    {
      ProbeForWrite(v14, Length, 1u);
    }
    v27 = 1LL;
    LOBYTE(v11) = v165;
  }
  else
  {
    Pool2 = (_QWORD *)((unsigned __int64)InputBuffer & -(__int64)(v12 != 0));
    v167 = Pool2;
    v27 = 1LL;
    IsAppContainerOrIdentifyLevelContext = v163;
    v15 = v169;
  }
  if ( (unsigned int)v15 > 0x1F || (v28 = -234812256, !_bittest(&v28, v15)) )
  {
    v29 = (unsigned int)(v15 - 38);
    if ( (unsigned int)v29 > 0x3C || (v30 = 0x1FFFE3BEF5BE36EDLL, !_bittest64(&v30, v29)) )
    {
      PopAcquirePolicyLock(1LL, v11);
      v27 = 1LL;
      LOBYTE(v11) = 1;
      v165 = 1;
    }
  }
  if ( v15 > 50 )
  {
    if ( v15 > 75 )
    {
      if ( v15 <= 87 )
      {
        if ( v15 == 87 )
        {
          if ( !Pool2 )
          {
            goto LABEL_277;
          }
          if ( v12 < 8 )
          {
            goto LABEL_277;
          }
          v150 = *(_DWORD *)Pool2;
          if ( (unsigned int)(*(_DWORD *)Pool2 - 100) <= 0xF9B || v150 >= 4107 )
          {
            goto LABEL_277;
          }
          if ( (unsigned int)(v150 - 4096) > 0xA )
          {
            WakeSource = PopPowerInformationInternal(
                           v150,
                           *((_DWORD *)Pool2 + 1),
                           (_DWORD)Pool2,
                           v12,
                           (__int64)v14,
                           Length,
                           (__int64)&Src,
                           (__int64)&Length_4,
                           (__int64)v162);
          }
          else
          {
            if ( !SeSinglePrivilegeCheck(SeShutdownPrivilege, PreviousMode)
              && !(unsigned __int8)PopCapabilityCheck(L"terminalPowerManagement") )
            {
              IsAppContainerOrIdentifyLevelContext = -1073741727;
              goto LABEL_100;
            }
            WakeSource = TtmDispatchApi(
                           v150,
                           (_DWORD)Pool2,
                           v12,
                           (_DWORD)v14,
                           Length,
                           (__int64)&Src,
                           (__int64)&Length_4,
                           (__int64)v162);
          }
          goto LABEL_262;
        }
        if ( v15 > 81 )
        {
          v145 = v15 - 82;
          if ( !v145 )
          {
            goto LABEL_496;
          }
          v146 = v145 - 1;
          if ( !v146 )
          {
            if ( !Pool2 && v14 )
            {
              PopCurrentPowerStatePrecise(&v180, 0LL);
              goto LABEL_79;
            }
            goto LABEL_277;
          }
          v147 = v146 - 1;
          if ( !v147 )
          {
            if ( !Pool2 || v14 )
            {
              goto LABEL_277;
            }
            if ( v12 < 0x10 || v12 < 2 * (unsigned int)*((unsigned __int16 *)Pool2 + 6) + 14 )
            {
              goto LABEL_99;
            }
            v68 = PopThermalProcessUsermodeEvent(Pool2, v11);
            goto LABEL_618;
          }
          v148 = v147 - 1;
          if ( v148 )
          {
            if ( v148 == 1 && Pool2 && v14 )
            {
              IsAppContainerOrIdentifyLevelContext = RtlStringCbLengthW((STRSAFE_PCNZWCH)Pool2, v12, 0LL);
              InputBuffer = v13;
              if ( IsAppContainerOrIdentifyLevelContext < 0 )
              {
                goto LABEL_279;
              }
              IsAppContainerOrIdentifyLevelContext = PopBatteryDeviceState(Pool2, &v180);
              if ( IsAppContainerOrIdentifyLevelContext < 0 )
              {
                goto LABEL_279;
              }
              Src = &v180;
              Length_4 = 52;
              goto LABEL_620;
            }
            goto LABEL_277;
          }
          v149 = (void *)*Pool2;
          if ( *((_BYTE *)Pool2 + 12) )
          {
            inserted = PopPowerRequestReferenceAcquire(v149);
          }
          else
          {
            inserted = PopPowerRequestReferenceRelease(v149);
          }
          goto LABEL_325;
        }
        if ( v15 == 81 )
        {
          goto LABEL_496;
        }
        v136 = v15 - 76;
        if ( !v136 )
        {
          if ( Pool2 || !v14 )
          {
            goto LABEL_277;
          }
          LODWORD(v180) = qword_140F0B6F0;
          v181 = (char *)qword_140F0B888;
          v182 = qword_140F0B890;
          if ( !PopSleepStats )
          {
            IsAppContainerOrIdentifyLevelContext = -1073741823;
            goto LABEL_100;
          }
          v58 = (__int128 *)&v180;
          goto LABEL_151;
        }
        v137 = v136 - 1;
        if ( !v137 )
        {
          if ( !Pool2 && !v14 )
          {
            v139 = PopErrataReportingIncorrectLidState;
            if ( !(unsigned __int8)TtmIsEnabled(1LL, v11) )
            {
              v144 = 0;
              if ( !PopLidOpened )
              {
                v141 = 1LL;
                v144 = PopConsoleExternalDisplayConnected == 0;
              }
              if ( !v144 )
              {
                PopEventCalloutDispatch(10LL);
              }
              if ( !v139 )
              {
                LOBYTE(v142) = v144;
                PopDiagTraceDisplayBurstWin32kCallout(v141, v140, v142);
              }
              goto LABEL_620;
            }
            SessionId = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);
            if ( SessionId == -1 )
            {
              IsAppContainerOrIdentifyLevelContext = -1073740715;
            }
            else
            {
              IsAppContainerOrIdentifyLevelContext = TtmNotifySessionDisplayBurst(SessionId, 6LL);
            }
            goto LABEL_88;
          }
          goto LABEL_277;
        }
        v138 = v137 - 1;
        if ( !v138 )
        {
          if ( PreviousMode != 1 )
          {
            goto LABEL_242;
          }
          if ( v14 || Length )
          {
            goto LABEL_277;
          }
          if ( !Pool2 )
          {
            PpmClearExitLatencySamplingPercentage(1LL, v11);
            goto LABEL_620;
          }
          if ( v12 != 4 )
          {
            goto LABEL_277;
          }
          inserted = PpmSetExitLatencySamplingPercentage(Pool2, v11);
          goto LABEL_325;
        }
        v99 = v138 - 1;
        if ( !v99 )
        {
          goto LABEL_496;
        }
LABEL_495:
        if ( v99 != 1 )
        {
          goto LABEL_277;
        }
        goto LABEL_496;
      }
      if ( v15 > 93 )
      {
        v155 = v15 - 94;
        if ( !v155 )
        {
          if ( !Pool2 || v14 || Length )
          {
            goto LABEL_277;
          }
          if ( v12 != 32 )
          {
            goto LABEL_99;
          }
          LOBYTE(v11) = PreviousMode;
          v68 = PopBlackBoxUpdate(Pool2, v11);
          goto LABEL_618;
        }
        v156 = v155 - 1;
        if ( !v156 )
        {
          if ( Pool2 && v12 == 1 && !v14 )
          {
            ExNotifyCallback(PipCslCallbackObject, Pool2, 0LL);
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        v157 = v156 - 1;
        if ( !v157 )
        {
          if ( !Pool2 || v14 )
          {
            goto LABEL_277;
          }
          if ( !v12 )
          {
            goto LABEL_99;
          }
          LOBYTE(v27) = *(_BYTE *)Pool2;
          v68 = PopSendSuspendResumeNotifications(v27, v11);
          goto LABEL_618;
        }
        v158 = v157 - 1;
        if ( !v158 )
        {
          if ( !Pool2 || !v12 || !v14 || !Length )
          {
            goto LABEL_277;
          }
          if ( v12 >= 0x20 )
          {
            v86 = Length;
            if ( Length >= 0x10 )
            {
              IsAppContainerOrIdentifyLevelContext = PopBlackBoxDirectAccess(Pool2, &v180);
              InputBuffer = v13;
              if ( IsAppContainerOrIdentifyLevelContext < 0 )
              {
                goto LABEL_279;
              }
              Src = &v180;
              Length_4 = 16;
              goto LABEL_621;
            }
          }
          goto LABEL_99;
        }
        if ( v158 != 1 )
        {
          goto LABEL_277;
        }
        if ( (unsigned int)Feature_EE_MPTF_Functionality__private_IsEnabledDeviceUsageNoInline(1LL, v11) )
        {
          if ( !Pool2 && v14 )
          {
            PopCurrentSystemPowerSourceState(&v180);
            Src = &v180;
            Length_4 = 68;
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        goto LABEL_453;
      }
      if ( v15 == 93 )
      {
        if ( !Pool2 || !v14 )
        {
          goto LABEL_277;
        }
        if ( v12 == 8 )
        {
          v86 = Length;
          if ( Length >= 0xC )
          {
            IsAppContainerOrIdentifyLevelContext = PopEtEnergyTrackerQuery(*Pool2, v14, Length);
            InputBuffer = v13;
            if ( IsAppContainerOrIdentifyLevelContext < 0 )
            {
              goto LABEL_279;
            }
            goto LABEL_621;
          }
        }
        goto LABEL_99;
      }
      if ( v15 != 88 )
      {
        if ( v15 == 89 )
        {
          if ( !Pool2 || !v14 )
          {
            goto LABEL_277;
          }
          if ( v12 < 4 )
          {
            goto LABEL_99;
          }
          PopAcquireTransitionLock(6LL, v11);
          PopAcquirePolicyLock(v153, v152);
          IsAppContainerOrIdentifyLevelContext = PopSetHiberFileType(*(unsigned int *)Pool2, &v170, &v168);
          ((void (*)(void))PopReleasePolicyLock)();
          v112 = 6LL;
          goto LABEL_391;
        }
        v151 = (unsigned int)(v15 - 90);
        if ( v15 == 90 )
        {
          if ( Pool2 && v12 >= 4 && !v14 )
          {
            LOBYTE(v151) = *(_DWORD *)Pool2 != 0;
            PopUpdatePowerButtonHoldState(v151, v11);
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        if ( v15 != 91 )
        {
          if ( !Pool2 || !v14 )
          {
            goto LABEL_277;
          }
          if ( v12 != 12 )
          {
            goto LABEL_99;
          }
          v86 = Length;
          if ( Length != 8 )
          {
            goto LABEL_99;
          }
          v135 = PopEtEnergyTrackerCreate(Pool2, &v180);
          goto LABEL_481;
        }
        if ( (unsigned __int8)SSHSupportIsPlatformAoAc((unsigned int)(v15 - 91), v11) )
        {
          if ( Pool2 && v12 == 336 && v14 )
          {
            LOBYTE(v180) = PopFxIsDevicePotentialDripsConstraint(Pool2);
            Src = &v180;
            Length_4 = 1;
            IsAppContainerOrIdentifyLevelContext = 0;
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        goto LABEL_453;
      }
    }
    else
    {
      if ( v15 == 75 )
      {
        if ( Pool2 || !v14 )
        {
          goto LABEL_277;
        }
        LODWORD(v180) = PopCapturePlatformRole(1LL, v11);
        goto LABEL_180;
      }
      if ( v15 <= 63 )
      {
        if ( v15 == 63 )
        {
          if ( !Pool2 && !v14 )
          {
            v123 = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);
            PopPrintEx(3LL, "PopAdaptive: Session %u is closed\n", v123);
            PopDiagTraceSessionStates(&POP_ETW_ADPM_SESSION_CLOSED);
            PopFreeSessionState(v123);
            if ( (unsigned __int8)TtmIsEnabled(v125, v124) )
            {
              PopPowerRequestNotifyTtmSessionUninitialized();
              TtmCleanupCurrentSession();
            }
            goto LABEL_87;
          }
          goto LABEL_277;
        }
        if ( v15 > 57 )
        {
          v113 = v15 - 58;
          if ( v113 )
          {
            v114 = v113 - 1;
            if ( v114 )
            {
              v115 = v114 - 1;
              if ( v115 )
              {
                v116 = v115 - 1;
                if ( v116 )
                {
                  if ( v116 == 1
                    && !(unsigned int)Feature_Servicing_PDCAgentSessions__private_IsEnabledDeviceUsageNoInline(1LL, v11) )
                  {
                    if ( Pool2 )
                    {
                      if ( v12 == 40 )
                      {
                        if ( v14 )
                        {
                          v86 = Length;
                          if ( Length == 16 )
                          {
                            v117 = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);
                            PopSessionConnectionChange(v117, v167, v14);
                            v118 = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);
                            Feature_Servicing_PDCAgentSessions__private_IsEnabledDeviceUsageNoInline(v120, v119);
                            Pool2 = v167;
                            if ( qword_140E67578 )
                            {
                              LOBYTE(v121) = *((_BYTE *)v167 + 1);
                              LOBYTE(v122) = *(_BYTE *)v167;
                              guard_dispatch_icall_no_overrides(v118, v121, v122, 3LL);
                            }
                            IsAppContainerOrIdentifyLevelContext = v163;
                            goto LABEL_621;
                          }
                        }
                      }
                    }
                  }
                }
                else if ( Pool2 )
                {
                  if ( v12 == 16 )
                  {
                    if ( v14 )
                    {
                      v86 = Length;
                      if ( Length == 8 )
                      {
                        IsAppContainerOrIdentifyLevelContext = -1073741637;
                        goto LABEL_621;
                      }
                    }
                  }
                }
                goto LABEL_277;
              }
              if ( !Pool2 || v14 )
              {
LABEL_277:
                InputBuffer = v13;
LABEL_278:
                IsAppContainerOrIdentifyLevelContext = -1073741811;
                goto LABEL_279;
              }
              if ( v12 >= 8 )
              {
                PopEnforceResiliencyScenarios(Pool2, v11);
                goto LABEL_620;
              }
LABEL_99:
              IsAppContainerOrIdentifyLevelContext = -1073741789;
LABEL_100:
              InputBuffer = v13;
              goto LABEL_279;
            }
            if ( !Pool2 || !v14 )
            {
              goto LABEL_277;
            }
            if ( v12 != 16 || Length < 4 )
            {
              goto LABEL_99;
            }
            SettingValue = PopGetSettingValue(Pool2, v14, Length);
          }
          else
          {
            if ( !v14 )
            {
              goto LABEL_277;
            }
            if ( v13 && v12 != 20 || Length != 8 )
            {
              goto LABEL_99;
            }
            SettingValue = PopGetSettingNotificationName(Pool2, v14);
          }
          goto LABEL_421;
        }
        if ( v15 == 57 )
        {
          goto LABEL_496;
        }
        v105 = v15 - 51;
        if ( v105 )
        {
          v106 = v105 - 1;
          if ( !v106 )
          {
            goto LABEL_496;
          }
          v107 = v106 - 1;
          if ( !v107 )
          {
            goto LABEL_496;
          }
          v108 = v107 - 1;
          if ( !v108 )
          {
            goto LABEL_496;
          }
          v109 = v108 - 1;
          if ( v109 )
          {
            if ( v109 != 1 || !Pool2 || !v14 )
            {
              goto LABEL_277;
            }
            if ( v12 == 8 )
            {
              v86 = Length;
              if ( Length == 4 )
              {
                IsAppContainerOrIdentifyLevelContext = PpmParkSetLpiCap(
                                                         *(unsigned int *)Pool2,
                                                         *((unsigned int *)Pool2 + 1),
                                                         &v180);
                if ( IsAppContainerOrIdentifyLevelContext < 0 )
                {
                  goto LABEL_100;
                }
                Src = &v180;
                Length_4 = Length;
                goto LABEL_621;
              }
            }
            goto LABEL_99;
          }
          if ( !(_BYTE)KdDebuggerEnabled )
          {
            goto LABEL_242;
          }
          if ( !Pool2 || v14 )
          {
            goto LABEL_277;
          }
          if ( v12 == 8 )
          {
            WakeSource = PpmSetSimulatedIdle(Pool2, v11);
            goto LABEL_262;
          }
          if ( v12 == 4 )
          {
            WakeSource = PpmClearSimulatedIdle(Pool2, v11);
            goto LABEL_262;
          }
          goto LABEL_269;
        }
        if ( !Pool2 || !v14 )
        {
          goto LABEL_277;
        }
        if ( v12 < 4 )
        {
          goto LABEL_99;
        }
        PopAcquireTransitionLock(5LL, v11);
        PopAcquirePolicyLock(v111, v110);
        IsAppContainerOrIdentifyLevelContext = PopSetHiberFileSize(*(unsigned int *)Pool2, &v170, &v168);
        ((void (*)(void))PopReleasePolicyLock)();
        v112 = 5LL;
LABEL_391:
        PopReleaseTransitionLock(v112);
        if ( IsAppContainerOrIdentifyLevelContext < 0 )
        {
          v170 = (unsigned int)v168;
        }
        v63 = &v170;
        goto LABEL_184;
      }
      if ( v15 <= 69 )
      {
        if ( v15 == 69 )
        {
          if ( !Pool2 && !v14 )
          {
            IsAppContainerOrIdentifyLevelContext = PopInitPlatformSettings(1LL, v11);
            byte_140F0BE74 = SSHSupportIsPlatformAoAc(v134, v133);
            goto LABEL_100;
          }
          goto LABEL_277;
        }
        v126 = v15 - 64;
        if ( !v126 )
        {
          if ( Pool2 && v12 == 8 && !v14 )
          {
            v132 = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);
            Pool2 = v167;
            PopSessionWinlogonNotification(v132, v167);
            IsAppContainerOrIdentifyLevelContext = v163;
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        v127 = v126 - 1;
        if ( v127 )
        {
          v128 = v127 - 1;
          if ( v128 )
          {
            v129 = v128 - 1;
            if ( v129 )
            {
              if ( v129 != 1 || !Pool2 || v12 != 8 || v14 || Length )
              {
                goto LABEL_277;
              }
              SettingValue = PopMonitorInvocation(Pool2, v11);
            }
            else
            {
              if ( !Pool2 || v12 != 224 || v14 && Length != 168 )
              {
                goto LABEL_277;
              }
              SettingValue = PopPdcInvocation(Pool2, v14);
            }
            goto LABEL_421;
          }
          if ( Pool2 || !v14 )
          {
            goto LABEL_277;
          }
          LOBYTE(v180) = SSHSupportIsPlatformAoAc(1LL, v11);
          v131 = &v180;
LABEL_451:
          Src = v131;
          Length_4 = v130;
          goto LABEL_620;
        }
        if ( !(unsigned __int8)PsIsCurrentThreadInServerSilo(1LL) )
        {
          if ( Pool2 || !v14 )
          {
            goto LABEL_277;
          }
          PopFilterCapabilities(&PopCapabilities, &v180);
          if ( BYTE2(v182) )
          {
            PopReadHiberbootPolicy((char *)&v182 + 2);
          }
          v131 = (char **)((char *)&v182 + 2);
          v130 = 1;
          goto LABEL_451;
        }
LABEL_453:
        IsAppContainerOrIdentifyLevelContext = -1073741637;
        goto LABEL_100;
      }
      switch ( v15 )
      {
        case 'F':
          PopShutdownButtonPressTime = KeQueryPerformanceCounter(0LL).QuadPart;
          goto LABEL_620;
        case 'G':
          goto LABEL_453;
        case 'H':
LABEL_476:
          if ( !Pool2 || !v14 )
          {
            goto LABEL_277;
          }
          if ( v12 != 40 )
          {
            goto LABEL_99;
          }
          v86 = Length;
          if ( Length != 8 )
          {
            goto LABEL_99;
          }
          LOBYTE(v11) = v15 == 72;
          v135 = PopPowerRequestCreateUserModeRequest(Pool2, v11, &v180);
LABEL_481:
          IsAppContainerOrIdentifyLevelContext = v135;
          InputBuffer = v13;
          if ( v135 < 0 )
          {
            goto LABEL_279;
          }
          Src = &v180;
          Length_4 = 8;
          goto LABEL_621;
      }
      if ( v15 != 73 )
      {
        if ( !PreviousMode )
        {
          if ( !Pool2 || v12 != 16 || v14 || Length )
          {
            goto LABEL_277;
          }
          SettingValue = PopPdcCsDeviceNotification(Pool2, v11);
          goto LABEL_421;
        }
LABEL_242:
        IsAppContainerOrIdentifyLevelContext = -1073741790;
        goto LABEL_100;
      }
    }
    if ( !Pool2 && !v14 )
    {
      v154 = 23LL;
      if ( v15 != 88 )
      {
        v154 = 11LL;
      }
      PopScreenOff(v154, v11);
      goto LABEL_581;
    }
    goto LABEL_277;
  }
  if ( v15 == 50 )
  {
    if ( Pool2 || !v14 )
    {
      goto LABEL_277;
    }
    WakeTimerList = ExGetWakeTimerList(&Src, &Length_4);
    goto LABEL_357;
  }
  if ( v15 <= 24 )
  {
    if ( v15 != 24 )
    {
      if ( v15 <= 12 )
      {
        if ( v15 == 12 )
        {
          if ( !Pool2 && v14 )
          {
            HIDWORD(v180) = dword_140F0C224;
            LODWORD(v181) = dword_140F0C22C - dword_140F0C228;
            BYTE4(v181) = PopCoolingMode;
            Src = &v180;
            Length_4 = 16;
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        if ( v15 <= 6 )
        {
          if ( v15 != 6 )
          {
            if ( v15 && (v31 = v15 - 1) != 0 )
            {
              v32 = v31 - 1;
              if ( v32 )
              {
                v33 = v32 - 1;
                if ( v33 )
                {
                  v34 = v33 - 1;
                  if ( v34 )
                  {
                    if ( v34 == 1 && !Pool2 && v14 )
                    {
                      PopCurrentPowerState(&v180, v11);
LABEL_79:
                      Src = &v180;
                      Length_4 = 32;
LABEL_620:
                      v86 = Length;
LABEL_621:
                      if ( !Src || !v14 )
                      {
LABEL_625:
                        InputBuffer = v13;
                        if ( v169 != SystemHiberFileSize && v169 != SystemHiberFileType )
                        {
LABEL_581:
                          IsAppContainerOrIdentifyLevelContext = 0;
                          goto LABEL_100;
                        }
                        goto LABEL_279;
                      }
                      if ( v86 >= Length_4 )
                      {
                        memmove((void *)v14, Src, Length_4);
                        goto LABEL_625;
                      }
                      goto LABEL_99;
                    }
                    goto LABEL_277;
                  }
                  if ( !Pool2 )
                  {
                    if ( v14 )
                    {
LABEL_86:
                      byte_140F0BE69 = PopFullWake & 1;
                      byte_140F0BE75 = PopDisksRegisteredForIdle(1LL, v11);
                      PopFilterCapabilities(&PopCapabilities, &v180);
                      Src = &v180;
                      Length_4 = 76;
LABEL_87:
                      IsAppContainerOrIdentifyLevelContext = v163;
LABEL_88:
                      Pool2 = v167;
                      goto LABEL_620;
                    }
                    goto LABEL_277;
                  }
                  if ( (PopSimulate & 1) == 0 )
                  {
                    goto LABEL_277;
                  }
                  if ( v12 >= 0x4C )
                  {
                    PopCapabilities = *(_OWORD *)Pool2;
                    unk_140F0BE70 = *((_OWORD *)Pool2 + 1);
                    xmmword_140F0BE80 = *((_OWORD *)Pool2 + 2);
                    unk_140F0BE90 = *((_OWORD *)Pool2 + 3);
                    qword_140F0BEA0 = Pool2[8];
                    dword_140F0BEA8 = *((_DWORD *)Pool2 + 18);
                    IsAppContainerOrIdentifyLevelContext = PopResetCurrentPolicies(1LL, v11);
                    v163 = IsAppContainerOrIdentifyLevelContext;
                    InputBuffer = v13;
                    if ( IsAppContainerOrIdentifyLevelContext < 0 )
                    {
                      goto LABEL_279;
                    }
                    goto LABEL_86;
                  }
                  goto LABEL_99;
                }
              }
              if ( !Pool2 || !v14 )
              {
                goto LABEL_277;
              }
              if ( v12 < 0xE8 )
              {
                goto LABEL_99;
              }
              IsAppContainerOrIdentifyLevelContext = PopVerifySystemPowerPolicy(Pool2, &v180);
              InputBuffer = v13;
              if ( IsAppContainerOrIdentifyLevelContext < 0 )
              {
                goto LABEL_279;
              }
              v35 = &v180;
            }
            else
            {
              if ( Pool2 )
              {
                if ( v12 < 0xE8 )
                {
                  goto LABEL_99;
                }
                IsAppContainerOrIdentifyLevelContext = PopApplyPolicy(0LL, 0LL, Pool2, v12);
                InputBuffer = v13;
                if ( IsAppContainerOrIdentifyLevelContext < 0 )
                {
                  goto LABEL_279;
                }
              }
              else if ( !v14 )
              {
                goto LABEL_277;
              }
              v35 = (char **)&unk_140F0BAE4;
            }
LABEL_94:
            Src = v35;
            Length_4 = 232;
            goto LABEL_620;
          }
          if ( !Pool2 || v14 )
          {
            goto LABEL_277;
          }
          if ( v12 < 0x18 )
          {
            goto LABEL_99;
          }
          v36 = *(unsigned int *)Pool2;
          if ( (unsigned int)v36 >= 7 )
          {
            goto LABEL_277;
          }
          v37 = 3 * v36;
          if ( *((_QWORD *)&PopPowerStateHandlers + 3 * v36 + 1) )
          {
            if ( (_DWORD)v36 != 5
              && ((_DWORD)v36 != 4 || (void (__fastcall __noreturn *)())qword_140F0BE08 != PopShutdownHandler) )
            {
              goto LABEL_277;
            }
          }
          *(_OWORD *)((char *)&PopPowerStateHandlers + 24 * v36) = *(_OWORD *)Pool2;
          *((_QWORD *)&PopPowerStateHandlers + 3 * v36 + 2) = Pool2[2];
          *(_WORD *)((char *)&PopPowerStateHandlers + 24 * v36 + 5) = 0;
          *((_BYTE *)&PopPowerStateHandlers + 24 * v36 + 7) = 0;
          v38 = 0LL;
          v39 = 0;
          if ( (_DWORD)v36 )
          {
            v40 = v36 - 1;
            if ( v40 )
            {
              v41 = v40 - 1;
              if ( v41 )
              {
                v42 = v41 - 1;
                if ( v42 )
                {
                  v43 = v42 - 1;
                  if ( v43 )
                  {
                    if ( v43 == 2 )
                    {
                      v39 = 4;
                      v44 = (PopSimulate & 0x2000) == 0;
                      v38 = (char *)&unk_140F0BE71;
                      goto LABEL_123;
                    }
                  }
                  else
                  {
                    v38 = &byte_140F0BE67;
                  }
LABEL_125:
                  v45 = v39 & (unsigned int)-(*((_BYTE *)&PopPowerStateHandlers + 8 * v37 + 4) != 0);
                  if ( (int)v45 > (int)qword_140F0BEA0 )
                  {
                    LODWORD(qword_140F0BEA0) = *((_BYTE *)&PopPowerStateHandlers + 8 * v37 + 4) != 0 ? v39 : 0;
                  }
                  if ( v38 )
                  {
                    LOBYTE(v45) = 1;
                    PopChangeCapability(v38, v45);
                  }
                  goto LABEL_620;
                }
                v39 = 5;
                v44 = (PopSimulate & 0x2000) == 0;
                v38 = &byte_140F0BE66;
              }
              else
              {
                v39 = 4;
                v44 = (PopSimulate & 0x20) == 0;
                v38 = (char *)&word_140F0BE64 + 1;
              }
            }
            else
            {
              v39 = 3;
              v44 = (PopSimulate & 0x40) == 0;
              v38 = (char *)&word_140F0BE64;
            }
          }
          else
          {
            v39 = 2;
            v44 = (PopSimulate & 8) == 0;
            v38 = &byte_140F0BE63;
          }
LABEL_123:
          if ( !v44 )
          {
            v38 = 0LL;
          }
          goto LABEL_125;
        }
        v46 = v15 - 7;
        if ( !v46 )
        {
          if ( !Pool2 && v14 )
          {
            Src = &PpmProcessorDriverDispatchTable;
            Length_4 = 280;
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        v47 = v46 - 1;
        if ( !v47 )
        {
          if ( Pool2 || !v14 )
          {
            goto LABEL_277;
          }
          v35 = (char **)PopPolicy;
          goto LABEL_94;
        }
        v48 = v47 - 1;
        if ( v48 )
        {
          v49 = v48 - 1;
          if ( v49 )
          {
            if ( v49 != 1 || Pool2 || !v14 )
            {
              goto LABEL_277;
            }
            PrimaryGroupThread = KeQueryPrimaryGroupThread(KeGetCurrentThread(), v11);
            v52 = PopProcessorInformation(&v180, v51, PrimaryGroupThread, &Length_4);
            Pool2 = v167;
LABEL_137:
            InputBuffer = v13;
            IsAppContainerOrIdentifyLevelContext = v52;
            if ( v52 < 0 )
            {
              goto LABEL_279;
            }
            Src = &v180;
            goto LABEL_620;
          }
          if ( !Pool2 || v14 )
          {
            goto LABEL_277;
          }
          if ( !v12 )
          {
            goto LABEL_99;
          }
          PopAcquireTransitionLock(3LL, v11);
          PopAcquirePolicyLock(v54, v53);
          v55 = *(_BYTE *)Pool2 != 0;
          PopHiberEnabledReg = *(_BYTE *)Pool2 != 0;
          PopSetHiberPersistedRegValue(0LL, v55, 0LL);
          IsAppContainerOrIdentifyLevelContext = PopHibernateEvaluation(0LL, 0LL, 0LL);
          PopReleasePolicyLock(v56);
          PopReleaseTransitionLock(3LL);
          goto LABEL_143;
        }
        if ( Pool2 )
        {
          if ( v12 < 0x18 )
          {
            goto LABEL_99;
          }
          IsAppContainerOrIdentifyLevelContext = PopApplyAdminPolicy(1LL, Pool2);
          InputBuffer = v13;
          if ( IsAppContainerOrIdentifyLevelContext < 0 )
          {
            goto LABEL_279;
          }
          IsAppContainerOrIdentifyLevelContext = ((__int64 (*)(void))PopResetCurrentPolicies)();
          if ( IsAppContainerOrIdentifyLevelContext < 0 )
          {
            goto LABEL_279;
          }
        }
        else if ( !v14 )
        {
          goto LABEL_277;
        }
        v58 = &PopAdminPolicy;
LABEL_151:
        Src = v58;
        Length_4 = 24;
        goto LABEL_620;
      }
      if ( v15 > 18 )
      {
        v64 = v15 - 19;
        if ( v64 )
        {
          v65 = v64 - 1;
          if ( v65 )
          {
            v66 = v65 - 1;
            if ( v66 )
            {
              v67 = v66 - 1;
              if ( v67 )
              {
                if ( v67 == 1 && !Pool2 && v14 )
                {
                  IsAppContainerOrIdentifyLevelContext = PopLoggingInformation(P, &v171);
                  InputBuffer = v13;
                  if ( IsAppContainerOrIdentifyLevelContext < 0 )
                  {
                    goto LABEL_279;
                  }
                  Src = P[0];
                  Length_4 = (unsigned int)v171;
                  goto LABEL_620;
                }
                goto LABEL_277;
              }
            }
          }
        }
      }
      else if ( v15 != 18 )
      {
        v59 = v15 - 13;
        if ( v59 )
        {
          v60 = v59 - 1;
          if ( v60 )
          {
            v61 = v60 - 1;
            if ( v61 )
            {
              v62 = v61 - 1;
              if ( v62 )
              {
                if ( v62 != 1 || !Pool2 || v14 )
                {
                  goto LABEL_277;
                }
                if ( v12 >= 0x10 )
                {
                  if ( !(_QWORD)PopPowerStateNotifyHandler || !*Pool2 )
                  {
                    PopPowerStateNotifyHandler = *(_OWORD *)Pool2;
                    goto LABEL_620;
                  }
                  goto LABEL_277;
                }
                goto LABEL_99;
              }
              if ( Pool2 || !v14 )
              {
                goto LABEL_277;
              }
              if ( dword_140E019D8 )
              {
                LODWORD(v180) = (unsigned int)v180 | 1;
              }
              if ( dword_140E019A8 )
              {
                LODWORD(v180) = (unsigned int)v180 | 2;
              }
LABEL_180:
              Src = &v180;
              Length_4 = 4;
              goto LABEL_620;
            }
            if ( Pool2 || !v14 )
            {
              goto LABEL_277;
            }
            v63 = &qword_140F0B0A0;
          }
          else
          {
            if ( Pool2 || !v14 )
            {
              goto LABEL_277;
            }
            v63 = &qword_140F0B098;
          }
LABEL_184:
          Src = v63;
          Length_4 = 8;
          goto LABEL_620;
        }
      }
LABEL_496:
      IsAppContainerOrIdentifyLevelContext = -1073741822;
      goto LABEL_100;
    }
    if ( !Pool2 || v14 )
    {
      goto LABEL_277;
    }
    if ( v12 < 8 )
    {
      goto LABEL_99;
    }
    v68 = PopLogSleepDisabled(*(unsigned int *)Pool2, *((unsigned int *)Pool2 + 1), 0LL, 0LL);
LABEL_618:
    v57 = v68 < 0;
    IsAppContainerOrIdentifyLevelContext = v68;
LABEL_619:
    InputBuffer = v13;
    if ( v57 )
    {
      goto LABEL_279;
    }
    goto LABEL_620;
  }
  if ( v15 > 38 )
  {
    if ( v15 <= 44 )
    {
      if ( v15 == 44 )
      {
        if ( !Pool2 || v14 )
        {
          goto LABEL_277;
        }
        if ( v12 != 24 )
        {
          goto LABEL_99;
        }
        v68 = PopPowerRequestActionInfo(Pool2, v11);
        goto LABEL_618;
      }
      if ( v15 != 39 )
      {
        if ( v15 == 40 )
        {
          if ( Pool2 && v12 == 4 && !v14 )
          {
            LODWORD(v168) = *(_DWORD *)Pool2 != 0;
            PopSetPowerSettingValueAcDc(&GUID_VIDEO_BRIGHTNESS_CAPABLE, 4LL, &v168);
            goto LABEL_620;
          }
          goto LABEL_277;
        }
        if ( v15 == 41 )
        {
          if ( !Pool2 )
          {
            if ( v14 )
            {
              v86 = Length;
              if ( Length == 64 )
              {
                PopAcquireRwLockExclusive(&PopEsLock);
                v87 = PopEsState;
                PopReleaseRwLock((ULONG_PTR)&PopEsLock);
                v89 = v188;
                if ( v87 == 1 )
                {
                  v89 = 1;
                }
                v188 = v89;
                IsEnabled = TtmIsEnabled(1LL, v88);
                if ( (unsigned __int8)TtmIsEnabled(v91, v90) )
                {
                  IsAppContainerOrIdentifyLevelContext = TtmInitCurrentSession();
                  v163 = IsAppContainerOrIdentifyLevelContext;
                  InputBuffer = v13;
                  if ( IsAppContainerOrIdentifyLevelContext < 0 )
                  {
                    goto LABEL_279;
                  }
                  PopPowerRequestNotifyTtmSessionInitialized();
                }
                PopAcquirePolicyLock(v93, v92);
                LODWORD(v182) = 0;
                v180 = &PopNoMoreInput;
                v181 = &PopHiberBootForceMonitorOff;
                v183 = byte_140F0BBD1 != 0;
                v184 = dword_140F0BBCC;
                HIDWORD(v182) = dword_140F0BBEC;
                v185 = qword_140F0BBF0;
                v190 = HIDWORD(qword_140F0BBF8);
                v186 = qword_140F0BBF8;
                v187 = dword_140F0BC00;
                v189 = PopLidOpened;
                _m_prefetchw(&PopLidStateIsReliable);
                v192 = _InterlockedOr(&PopLidStateIsReliable, 0) != 0;
                Src = &v180;
                Length_4 = 64;
                v94 = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);
                PopPrintEx(3LL, "PopAdaptive: Session %u is started\n", v94);
                PopDiagTraceSessionStates(&POP_ETW_ADPM_SESSION_CREATED);
                ((void (*)(void))PopReleasePolicyLock)();
                IsAppContainerOrIdentifyLevelContext = v163;
                Pool2 = v167;
                goto LABEL_621;
              }
            }
          }
          goto LABEL_277;
        }
        v11 = (unsigned int)(v15 - 42);
        if ( v15 != 42 )
        {
          goto LABEL_476;
        }
        if ( !Pool2 || v12 != 16 || v14 )
        {
          goto LABEL_277;
        }
        if ( !*((_BYTE *)Pool2 + 13) || !*((_BYTE *)Pool2 + 12) )
        {
LABEL_303:
          if ( *((_BYTE *)Pool2 + 14) )
          {
            if ( *((_BYTE *)Pool2 + 12) )
            {
              ZwUpdateWnfStateData(&WNF_PO_PRIMARY_DISPLAY_VISIBLE_STATE, (char *)Pool2 + 4, 4LL);
            }
            if ( *((_BYTE *)Pool2 + 14) )
            {
              LOBYTE(v27) = *((_DWORD *)Pool2 + 1) == 0;
              PopDiagTraceSessionDisplayStateChange(
                v27,
                *(unsigned int *)Pool2,
                *((unsigned __int8 *)Pool2 + 12),
                *((unsigned int *)Pool2 + 2));
            }
          }
          goto LABEL_620;
        }
        if ( (unsigned __int8)SSHSupportIsPlatformAoAc(1LL, v11) && !PopConsoleDisplayState )
        {
          if ( *((_DWORD *)Pool2 + 1) != 2 )
          {
LABEL_300:
            LOBYTE(v84) = *((_BYTE *)Pool2 + 15);
            PopProcessSessionDisplayStateChange(v84, *((unsigned int *)Pool2 + 2));
LABEL_301:
            PopSetDisplayStatus(*((unsigned int *)Pool2 + 1));
            PopUpdateConsoleDisplayState(*((unsigned int *)Pool2 + 1));
            v27 = 1LL;
            if ( *((_DWORD *)Pool2 + 1) == 1 )
            {
              PopPowerAggregatorNotifyDisplayPoweredOn();
            }
            goto LABEL_303;
          }
          v85 = *(unsigned int *)Pool2;
          v177 = 0LL;
          v178 = 0;
          LODWORD(v173) = v84;
          HIDWORD(v173) = 40;
          v174 = 0LL;
          v175 = 0LL;
          v176 = 0;
          IsAppContainerOrIdentifyLevelContext = DbgkWerCaptureLiveKernelDump2(
                                                   L"InvalidDisplayStateTransition",
                                                   v85,
                                                   0LL,
                                                   (__int64)&v173);
        }
        if ( *((_DWORD *)Pool2 + 1) == 2 )
        {
          goto LABEL_301;
        }
        goto LABEL_300;
      }
      if ( !Pool2 || v14 || v12 != 24 )
      {
        goto LABEL_277;
      }
      inserted = PopShutdownListenerInsertCallback(Pool2, (unsigned int)(v15 - 39));
LABEL_325:
      IsAppContainerOrIdentifyLevelContext = inserted;
      goto LABEL_620;
    }
    v96 = v15 - 45;
    if ( v96 )
    {
      v97 = v96 - 1;
      if ( !v97 )
      {
        if ( !Pool2 )
        {
          goto LABEL_277;
        }
        if ( v12 < 2 )
        {
          goto LABEL_277;
        }
        if ( !v14 )
        {
          goto LABEL_277;
        }
        v102 = *(_WORD *)Pool2;
        if ( v102 >= KeQueryActiveGroupCount() )
        {
          goto LABEL_277;
        }
        v52 = PopProcessorInformation(&v180, v103, v102, &Length_4);
        goto LABEL_137;
      }
      v98 = v97 - 1;
      if ( !v98 )
      {
        if ( Pool2 && v12 == 16 )
        {
          v101 = PopUmpoSendLegacyEvent(Pool2, v11);
          IsAppContainerOrIdentifyLevelContext = v101;
          if ( v101 < 0 )
          {
            InputBuffer = v13;
            if ( v101 != -1073741536 )
            {
              goto LABEL_279;
            }
          }
          goto LABEL_620;
        }
        goto LABEL_277;
      }
      v99 = v98 - 1;
      if ( !v99 )
      {
        if ( !(_BYTE)KdDebuggerEnabled )
        {
          goto LABEL_242;
        }
        if ( !Pool2 || v14 )
        {
          goto LABEL_277;
        }
        switch ( v12 )
        {
          case 0x10u:
            v100 = 0LL;
            break;
          case 0x18u:
            v100 = Pool2 + 2;
            break;
          case 2u:
            WakeSource = PpmParkClearForcedMask(Pool2, v11);
            goto LABEL_262;
          default:
            goto LABEL_269;
        }
        WakeSource = PpmParkApplyForcedMask(Pool2, v100);
        goto LABEL_262;
      }
      goto LABEL_495;
    }
    if ( Pool2 || !v14 )
    {
      goto LABEL_277;
    }
    WakeTimerList = PopGetPowerRequestListInfo(&Src, &Length_4);
LABEL_357:
    IsAppContainerOrIdentifyLevelContext = WakeTimerList;
    InputBuffer = v13;
    if ( WakeTimerList < 0 )
    {
      goto LABEL_279;
    }
    v162[0] = 1;
    goto LABEL_620;
  }
  if ( v15 == 38 )
  {
    if ( !Pool2 || v14 )
    {
      goto LABEL_277;
    }
    if ( v12 == 8 )
    {
      WakeSource = PpmSetSimulatedLoad((PPROCESSOR_NUMBER)Pool2);
      goto LABEL_262;
    }
    if ( v12 == 4 )
    {
      WakeSource = PpmClearSimulatedLoad((PPROCESSOR_NUMBER)Pool2);
      goto LABEL_262;
    }
LABEL_269:
    IsAppContainerOrIdentifyLevelContext = -1073741811;
    goto LABEL_143;
  }
  if ( v15 > 32 )
  {
    v74 = v15 - 33;
    if ( !v74 )
    {
      goto LABEL_496;
    }
    v75 = v74 - 1;
    if ( !v75 )
    {
      goto LABEL_496;
    }
    v76 = v75 - 1;
    if ( v76 )
    {
      v77 = v76 - 1;
      if ( !v77 )
      {
        if ( !v14 )
        {
          goto LABEL_277;
        }
        if ( !(_DWORD)dword_140F0B260 || !qword_140F0B258 )
        {
          IsAppContainerOrIdentifyLevelContext = -1073741275;
          goto LABEL_100;
        }
        Length_4 = dword_140F0B260 + 8;
        v78 = ExAllocatePool2(0x100uLL, (unsigned int)(dword_140F0B260 + 8), 0x206D654Du);
        v79 = (_DWORD *)v78;
        if ( v78 )
        {
          Src = (PVOID)v78;
          v162[0] = 1;
          memmove((void *)(v78 + 8), qword_140F0B258, (unsigned int)dword_140F0B260);
          *v79 = (unsigned int)dword_140F0B260 >> 4;
          goto LABEL_620;
        }
        goto LABEL_218;
      }
      if ( v77 != 1 || !Pool2 || v14 )
      {
        goto LABEL_277;
      }
      if ( PreviousMode == 1 )
      {
        if ( v12 >= 4 )
        {
          IsAppContainerOrIdentifyLevelContext = RtlStringCbLengthW((STRSAFE_PCNZWCH)Pool2 + 2, v12 - 4, 0LL);
          if ( IsAppContainerOrIdentifyLevelContext >= 0 )
          {
            IsAppContainerOrIdentifyLevelContext = 0;
          }
        }
        else
        {
          IsAppContainerOrIdentifyLevelContext = -1073741811;
        }
        InputBuffer = v13;
        if ( IsAppContainerOrIdentifyLevelContext < 0 )
        {
          goto LABEL_279;
        }
        PopDiagTraceServiceNotification(Pool2, v11);
        goto LABEL_620;
      }
      goto LABEL_242;
    }
    if ( Pool2 || !v14 )
    {
      goto LABEL_277;
    }
    if ( (_BYTE)v11 )
    {
      PopReleasePolicyLock(1LL);
      v165 = 0;
    }
    IsAppContainerOrIdentifyLevelContext = PopGetWakeSource(0LL, &Length_4);
    if ( IsAppContainerOrIdentifyLevelContext != -1073741789 )
    {
LABEL_143:
      v57 = IsAppContainerOrIdentifyLevelContext < 0;
      goto LABEL_619;
    }
    v80 = (void *)ExAllocatePool2(0x40uLL, Length_4, 0x206D654Du);
    Src = v80;
    if ( !v80 )
    {
      goto LABEL_218;
    }
    v162[0] = 1;
    WakeSource = PopGetWakeSource(v80, &Length_4);
LABEL_262:
    IsAppContainerOrIdentifyLevelContext = WakeSource;
    goto LABEL_143;
  }
  if ( v15 == 32 )
  {
    goto LABEL_496;
  }
  if ( v15 != 25 )
  {
    if ( v15 != 28 )
    {
      if ( v15 != 29 )
      {
        if ( (unsigned int)(v15 - 30) > 1 || !Pool2 || v14 )
        {
          goto LABEL_277;
        }
        if ( v12 >= 8 )
        {
          if ( v15 == 30 )
          {
            PopDiagTraceAppPowerMessage(Pool2);
          }
          else
          {
            PopDiagTraceAppPowerMessageEnd(Pool2);
          }
          goto LABEL_620;
        }
        goto LABEL_99;
      }
      if ( Pool2 || !v14 )
      {
        goto LABEL_277;
      }
      Length_4 = 4;
      Src = (PVOID)ExAllocatePool2(0x100uLL, 4uLL, 0x206D654Du);
      if ( Src )
      {
        v162[0] = 1;
        *(_DWORD *)Src = PopConsoleDisplayState;
        goto LABEL_620;
      }
LABEL_218:
      IsAppContainerOrIdentifyLevelContext = -1073741670;
      goto LABEL_100;
    }
    PopSuspendResumePdc(1LL);
    PopHiberBootForceMonitorOff = 1;
    SettingValue = PoPowerOffMonitor();
LABEL_421:
    IsAppContainerOrIdentifyLevelContext = SettingValue;
    goto LABEL_100;
  }
  if ( !Pool2 )
  {
    goto LABEL_277;
  }
  if ( v14 )
  {
    goto LABEL_277;
  }
  if ( Length )
  {
    goto LABEL_277;
  }
  if ( v12 < 0x1C )
  {
    goto LABEL_277;
  }
  v171 = (unsigned int *)(Pool2 + 3);
  v70 = *((_DWORD *)Pool2 + 6);
  if ( !v70 )
  {
    goto LABEL_277;
  }
  v168 = (unsigned int *)Pool2 + 5;
  if ( *((_DWORD *)Pool2 + 5) > 2u )
  {
    goto LABEL_277;
  }
  InputBuffer = v13;
  if ( *(_DWORD *)Pool2 != 1 )
  {
    IsAppContainerOrIdentifyLevelContext = -1073741736;
    goto LABEL_279;
  }
  v71 = v70 + 28;
  v72 = -1;
  if ( v71 >= 0x1C )
  {
    v72 = v71;
  }
  Length_4 = v72;
  IsAppContainerOrIdentifyLevelContext = v71 < 0x1C ? 0xC0000095 : 0;
  if ( v71 >= 0x1C )
  {
    if ( v12 < v72 )
    {
      goto LABEL_277;
    }
    v73 = PsGetSessionIdEx(KeGetCurrentThread()->ApcState.Process);
    Pool2 = v167;
    v68 = PopSetPowerSettingValue((char *)v167 + 4, v73, *v168, *v171, (char *)v167 + 28);
    goto LABEL_618;
  }
LABEL_279:
  v82 = P[0];
  if ( P[0] )
  {
    ExFreePoolWithTag(P[0], 0);
  }
  if ( v162[0] )
  {
    ExFreePoolWithTag(Src, 0x206D654Du);
  }
  if ( v165 )
  {
    PopReleasePolicyLock(v82);
  }
  if ( Pool2 && Pool2 != InputBuffer && Pool2 != (_QWORD *)psz )
  {
    ExFreePoolWithTag(Pool2, 0x206D654Du);
  }
  return IsAppContainerOrIdentifyLevelContext;
}
