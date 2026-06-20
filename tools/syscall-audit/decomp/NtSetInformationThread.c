NTSTATUS __stdcall NtSetInformationThread(                                                                                                                               | 
        HANDLE ThreadHandle,                                                                                                                                             | 
        THREADINFOCLASS ThreadInformationClass,                                                                                                                          | 
        PVOID ThreadInformation,                                                                                                                                         | 
        ULONG ThreadInformationLength)                                                                                                                                   | 
{                                                                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                                          | 
                                                                                                                                                                         | 
  v4 = ThreadInformationLength;                                                                                                                                          | 
  BugCheckParameter1 = (ULONG_PTR)ThreadHandle;                                                                                                                          | 
  v7 = 0LL;                                                                                                                                                              | 
  Object = 0LL;                                                                                                                                                          | 
  v127 = 0LL;                                                                                                                                                            | 
  memset_0(&v151, 0, 0x108uLL);                                                                                                                                          | 
  v117 = 0;                                                                                                                                                              | 
  v123 = 0;                                                                                                                                                              | 
  v115 = 0;                                                                                                                                                              | 
  v125 = 0;                                                                                                                                                              | 
  v124 = 0LL;                                                                                                                                                            | 
  v144 = 0LL;                                                                                                                                                            | 
  v128 = 0LL;                                                                                                                                                            | 
  v129 = 0LL;                                                                                                                                                            | 
  v146 = 0LL;                                                                                                                                                            | 
  v118 = 0;                                                                                                                                                              | 
  v130 = 0LL;                                                                                                                                                            | 
  *(_OWORD *)Src = 0LL;                                                                                                                                                  | 
  v132 = 0LL;                                                                                                                                                            | 
  CurrentThread = KeGetCurrentThread();                                                                                                                                  | 
  v116 = CurrentThread;                                                                                                                                                  | 
  PreviousMode = (unsigned __int8)CurrentThread->PreviousMode;                                                                                                           | 
  v109 = PreviousMode;                                                                                                                                                   | 
  if ( (_BYTE)PreviousMode )                                                                                                                                             | 
  {                                                                                                                                                                      | 
    if ( ThreadInformationClass == ThreadPriority )                                                                                                                      | 
    {                                                                                                                                                                    | 
LABEL_3:                                                                                                                                                                 | 
      v11 = 4;                                                                                                                                                           | 
      v115 = 4;                                                                                                                                                          | 
      v12 = 8;                                                                                                                                                           | 
    }                                                                                                                                                                    | 
    else                                                                                                                                                                 | 
    {                                                                                                                                                                    | 
      switch ( ThreadInformationClass )                                                                                                                                  | 
      {                                                                                                                                                                  | 
        case ThreadAffinityMask:                                                                                                                                         | 
        case ThreadGroupInformation:                                                                                                                                     | 
        case ThreadCpuAccountingInformation:                                                                                                                             | 
        case ThreadCpuAccountingInformation|ThreadAffinityMask:                                                                                                          | 
        case ThreadCounterProfiling|ThreadIsIoPending:                                                                                                                   | 
        case ThreadCounterProfiling|ThreadPagePriority:                                                                                                                  | 
          v12 = 8;                                                                                                                                                       | 
          v11 = 8;                                                                                                                                                       | 
          v115 = 8;                                                                                                                                                      | 
          break;                                                                                                                                                         | 
        case ThreadEnableAlignmentFaultFixup:                                                                                                                            | 
        case ThreadCounterProfiling:                                                                                                                                     | 
          v11 = 1;                                                                                                                                                       | 
          v115 = 1;                                                                                                                                                      | 
          v12 = 8;                                                                                                                                                       | 
          break;                                                                                                                                                         | 
        default:                                                                                                                                                         | 
          goto LABEL_3;                                                                                                                                                  | 
      }                                                                                                                                                                  | 
    }                                                                                                                                                                    | 
    if ( (_DWORD)v4 )                                                                                                                                                    | 
    {                                                                                                                                                                    | 
      if ( ((v11 - 1) & (unsigned int)ThreadInformation) != 0 )                                                                                                          | 
      {                                                                                                                                                                  | 
        ExRaiseDatatypeMisalignment();                                                                                                                                   | 
      }                                                                                                                                                                  | 
      v8 = (char *)ThreadInformation + v4;                                                                                                                               | 
      if ( (unsigned __int64)ThreadInformation + v4 > 0x7FFFFFFF0000LL || v8 < ThreadInformation )                                                                       | 
      {                                                                                                                                                                  | 
        v11 = v115;                                                                                                                                                      | 
      }                                                                                                                                                                  | 
    }                                                                                                                                                                    | 
    if ( ThreadInformationClass == (ThreadActualGroupAffinity|ThreadIsIoPending)                                                                                         | 
      || ThreadInformationClass == (ThreadCounterProfiling|ThreadIsIoPending) )                                                                                          | 
    {                                                                                                                                                                    | 
      ProbeForWrite(ThreadInformation, v4, v11);                                                                                                                         | 
      PreviousMode = v109;                                                                                                                                               | 
    }                                                                                                                                                                    | 
    CurrentThread = v116;                                                                                                                                                | 
  }                                                                                                                                                                      | 
  else                                                                                                                                                                   | 
  {                                                                                                                                                                      | 
    v12 = 8;                                                                                                                                                             | 
  }                                                                                                                                                                      | 
  if ( ThreadInformationClass == ThreadImpersonationToken )                                                                                                              | 
  {                                                                                                                                                                      | 
    if ( (_DWORD)v4 == 8 )                                                                                                                                               | 
    {                                                                                                                                                                    | 
      Handle = *(HANDLE *)ThreadInformation;                                                                                                                             | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                           | 
      if ( result >= 0 )                                                                                                                                                 | 
      {                                                                                                                                                                  | 
        v16 = KeGetCurrentThread();                                                                                                                                      | 
        if ( Handle )                                                                                                                                                    | 
        {                                                                                                                                                                | 
          v25 = v16->PreviousMode;                                                                                                                                       | 
          Token[0] = 0LL;                                                                                                                                                | 
          LODWORD(v7) = ObReferenceObjectByHandle(Handle, 4u, (POBJECT_TYPE)SeTokenObjectType, v25, Token, 0LL);                                                         | 
          if ( (int)v7 >= 0 )                                                                                                                                            | 
          {                                                                                                                                                              | 
            v26 = Token[0];                                                                                                                                              | 
            if ( *((_DWORD *)Token[0] + 48) == 2 )                                                                                                                       | 
            {                                                                                                                                                            | 
              LODWORD(v7) = PsImpersonateClient(                                                                                                                         | 
                              (PETHREAD)Object,                                                                                                                          | 
                              Token[0],                                                                                                                                  | 
                              0,                                                                                                                                         | 
                              0,                                                                                                                                         | 
                              *((SECURITY_IMPERSONATION_LEVEL *)Token[0] + 49));                                                                                         | 
              ObfDereferenceObject(v26);                                                                                                                                 | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              ObfDereferenceObject(Token[0]);                                                                                                                            | 
              LODWORD(v7) = -1073741656;                                                                                                                                 | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
        }                                                                                                                                                                | 
        else                                                                                                                                                             | 
        {                                                                                                                                                                | 
          PsImpersonateClient((PETHREAD)Object, 0LL, 0, 0, SecurityImpersonation);                                                                                       | 
        }                                                                                                                                                                | 
LABEL_34:                                                                                                                                                                | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                | 
        return (int)v7;                                                                                                                                                  | 
      }                                                                                                                                                                  | 
      return result;                                                                                                                                                     | 
    }                                                                                                                                                                    | 
    return -1073741820;                                                                                                                                                  | 
  }                                                                                                                                                                      | 
  if ( ThreadInformationClass == (ThreadCounterProfiling|ThreadAmILastThread) )                                                                                          | 
  {                                                                                                                                                                      | 
    if ( BugCheckParameter1 != -2LL )                                                                                                                                    | 
    {                                                                                                                                                                    | 
      return -1073741811;                                                                                                                                                | 
    }                                                                                                                                                                    | 
    if ( (_DWORD)v4 == 8 )                                                                                                                                               | 
    {                                                                                                                                                                    | 
      v19 = *(HANDLE *)ThreadInformation;                                                                                                                                | 
      v132 = *(HANDLE *)ThreadInformation;                                                                                                                               | 
      result = ObpReferenceObjectByHandleWithTag(0xFFFFFFFFFFFFFFFEuLL, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                        | 
      v13 = result;                                                                                                                                                      | 
      if ( result >= 0 )                                                                                                                                                 | 
      {                                                                                                                                                                  | 
        Token[0] = 0LL;                                                                                                                                                  | 
        if ( v19 && (v13 = PspThreadFromTicket(&v132, Token), v7 = Token[0], v13 < 0) )                                                                                  | 
        {                                                                                                                                                                | 
          v20 = Object;                                                                                                                                                  | 
        }                                                                                                                                                                | 
        else                                                                                                                                                             | 
        {                                                                                                                                                                | 
          v20 = Object;                                                                                                                                                  | 
          PspUpdateContainerImpersonation((ULONG_PTR)Object, v7);                                                                                                        | 
        }                                                                                                                                                                | 
        if ( v7 )                                                                                                                                                        | 
        {                                                                                                                                                                | 
          ObfDereferenceObject(v7);                                                                                                                                      | 
        }                                                                                                                                                                | 
LABEL_48:                                                                                                                                                                | 
        v21 = v20;                                                                                                                                                       | 
        goto LABEL_49;                                                                                                                                                   | 
      }                                                                                                                                                                  | 
      return result;                                                                                                                                                     | 
    }                                                                                                                                                                    | 
    return -1073741820;                                                                                                                                                  | 
  }                                                                                                                                                                      | 
  if ( ThreadInformationClass != ThreadPriority )                                                                                                                        | 
  {                                                                                                                                                                      | 
    if ( ThreadInformationClass != ThreadQuerySetWin32StartAddress )                                                                                                     | 
    {                                                                                                                                                                    | 
      switch ( ThreadInformationClass )                                                                                                                                  | 
      {                                                                                                                                                                  | 
        case ThreadBasePriority:                                                                                                                                         | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v32 = *(_DWORD *)ThreadInformation;                                                                                                                            | 
          Increment = *(_DWORD *)ThreadInformation;                                                                                                                      | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v111 = result;                                                                                                                                                 | 
          if ( result < 0 )                                                                                                                                              | 
          {                                                                                                                                                              | 
            return result;                                                                                                                                               | 
          }                                                                                                                                                              | 
          v33 = Object;                                                                                                                                                  | 
          v34 = *((_QWORD *)Object + 68);                                                                                                                                | 
          v35 = (unsigned int)(v32 + 16);                                                                                                                                | 
          if ( (unsigned int)v35 > 0x20 || (v36 = 0x10007C001LL, !_bittest64(&v36, v35)) )                                                                               | 
          {                                                                                                                                                              | 
            ServerSiloGlobals = PsGetServerSiloGlobals(0LL, v34);                                                                                                        | 
            if ( v116->ApcState.Process != *(_KPROCESS **)(ServerSiloGlobals + 848) && *(_BYTE *)(v34 + 839) != 4 )                                                      | 
            {                                                                                                                                                            | 
              ObfDereferenceObjectWithTag(v33, 0x79517350u);                                                                                                             | 
              return -1073741811;                                                                                                                                        | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
          v37 = *(_QWORD *)(v34 + 672);                                                                                                                                  | 
          if ( v37 && (*(_DWORD *)(v37 + 1056) & 0x20) != 0 && *(_BYTE *)(v34 + 839) != 4 )                                                                              | 
          {                                                                                                                                                              | 
            v38 = Increment;                                                                                                                                             | 
            if ( Increment > 0 )                                                                                                                                         | 
            {                                                                                                                                                            | 
              v39 = Object;                                                                                                                                              | 
              goto LABEL_82;                                                                                                                                             | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
          else                                                                                                                                                           | 
          {                                                                                                                                                              | 
            v38 = Increment;                                                                                                                                             | 
          }                                                                                                                                                              | 
          v39 = Object;                                                                                                                                                  | 
          KeSetBasePriorityThread((PKTHREAD)Object, v38);                                                                                                                | 
LABEL_82:                                                                                                                                                                | 
          v40 = v111;                                                                                                                                                    | 
          ObfDereferenceObjectWithTag(v39, 0x79517350u);                                                                                                                 | 
          return v40;                                                                                                                                                    | 
        case ThreadAffinityMask:                                                                                                                                         | 
          if ( (_DWORD)v4 != 8 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          *(_QWORD *)&v127 = *(_QWORD *)ThreadInformation;                                                                                                               | 
          v53 = v127;                                                                                                                                                    | 
          if ( !(_QWORD)v127 )                                                                                                                                           | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v54 = Object;                                                                                                                                                | 
            v55 = (struct _EX_RUNDOWN_REF *)*((_QWORD *)Object + 68);                                                                                                    | 
            if ( ExAcquireRundownProtection_0(v55 + 61) )                                                                                                                | 
            {                                                                                                                                                            | 
              v56 = KiSetLegacyAffinityThread(v54, v53);                                                                                                                 | 
              ExReleaseRundownProtection_0(v55 + 61);                                                                                                                    | 
              if ( !v56 )                                                                                                                                                | 
              {                                                                                                                                                          | 
                LODWORD(v7) = -1073741811;                                                                                                                               | 
              }                                                                                                                                                          | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              LODWORD(v7) = -1073741558;                                                                                                                                 | 
            }                                                                                                                                                            | 
            ObfDereferenceObjectWithTag(v54, 0x79517350u);                                                                                                               | 
            return (int)v7;                                                                                                                                              | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadEnableAlignmentFaultFixup:                                                                                                                            | 
          if ( (_DWORD)v4 != 1 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v89 = *(_BYTE *)ThreadInformation;                                                                                                                             | 
          v120 = *(_BYTE *)ThreadInformation;                                                                                                                            | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v111 = result;                                                                                                                                                 | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            if ( v89 )                                                                                                                                                   | 
            {                                                                                                                                                            | 
              _interlockedbittestandset((volatile signed __int32 *)Object + 30, 2u);                                                                                     | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              _interlockedbittestandreset((volatile signed __int32 *)Object + 30, 2u);                                                                                   | 
            }                                                                                                                                                            | 
            goto LABEL_57;                                                                                                                                               | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadZeroTlsCell:                                                                                                                                          | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v69 = *(_DWORD *)ThreadInformation;                                                                                                                            | 
          v123 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v70 = result;                                                                                                                                                  | 
          v111 = result;                                                                                                                                                 | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v71 = (struct _KTHREAD *)Object;                                                                                                                             | 
            ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                            | 
            if ( v71 != v116 )                                                                                                                                           | 
            {                                                                                                                                                            | 
              return -1073741811;                                                                                                                                        | 
            }                                                                                                                                                            | 
            Process = v71->Process;                                                                                                                                      | 
            v116 = (struct _KTHREAD *)Process;                                                                                                                           | 
            for ( i = (struct _EX_RUNDOWN_REF *)PsGetNextProcessThread(Process, 0LL);                                                                                    | 
                  ;                                                                                                                                                      | 
                  i = (struct _EX_RUNDOWN_REF *)PsGetNextProcessThread(Process, v74) )                                                                                   | 
            {                                                                                                                                                            | 
              Object = i;                                                                                                                                                | 
              v74 = i;                                                                                                                                                   | 
              if ( !i )                                                                                                                                                  | 
              {                                                                                                                                                          | 
                break;                                                                                                                                                   | 
              }                                                                                                                                                          | 
              if ( ExAcquireRundownProtection_0(i + 177) )                                                                                                               | 
              {                                                                                                                                                          | 
                Count = v74[30].Count;                                                                                                                                   | 
                if ( Count )                                                                                                                                             | 
                {                                                                                                                                                        | 
                  v76 = 0LL;                                                                                                                                             | 
                  ReadyTime = Process[1].ReadyTime;                                                                                                                      | 
                  if ( ReadyTime )                                                                                                                                       | 
                  {                                                                                                                                                      | 
                    v78 = WORD2(Process[3].PerProcessorCycleTimes);                                                                                                      | 
                    if ( v78 == 332 || v78 == 452 )                                                                                                                      | 
                    {                                                                                                                                                    | 
                      v76 = Count + 0x2000;                                                                                                                              | 
                    }                                                                                                                                                    | 
                  }                                                                                                                                                      | 
                  if ( v69 < 0x40 )                                                                                                                                      | 
                  {                                                                                                                                                      | 
                    if ( ReadyTime && ((v83 = WORD2(Process[3].PerProcessorCycleTimes), v83 == 332) || v83 == 452) )                                                     | 
                    {                                                                                                                                                    | 
                      if ( v76 )                                                                                                                                         | 
                      {                                                                                                                                                  | 
                        *(_DWORD *)(v76 + 4LL * v69 + 3600) = 0;                                                                                                         | 
                      }                                                                                                                                                  | 
                    }                                                                                                                                                    | 
                    else                                                                                                                                                 | 
                    {                                                                                                                                                    | 
                      *(_QWORD *)(Count + 8LL * v69 + 5248) = 0LL;                                                                                                       | 
                    }                                                                                                                                                    | 
                  }                                                                                                                                                      | 
                  else if ( v69 < 0x440 )                                                                                                                                | 
                  {                                                                                                                                                      | 
                    if ( ReadyTime && ((v81 = WORD2(Process[3].PerProcessorCycleTimes), v81 == 332) || v81 == 452) )                                                     | 
                    {                                                                                                                                                    | 
                      if ( v76 )                                                                                                                                         | 
                      {                                                                                                                                                  | 
                        v82 = *(unsigned int *)(v76 + 3988);                                                                                                             | 
                        if ( (_DWORD)v82 )                                                                                                                               | 
                        {                                                                                                                                                | 
                          *(_DWORD *)(v82 + 4LL * (v69 - 64)) = 0;                                                                                                       | 
                        }                                                                                                                                                | 
                      }                                                                                                                                                  | 
                    }                                                                                                                                                    | 
                    else                                                                                                                                                 | 
                    {                                                                                                                                                    | 
                      v79 = *(_QWORD *)(Count + 6016);                                                                                                                   | 
                      v149[1] = v79;                                                                                                                                     | 
                      if ( v79 )                                                                                                                                         | 
                      {                                                                                                                                                  | 
                        v80 = v79 + 8LL * (v69 - 64);                                                                                                                    | 
                        if ( v80 >= 0x7FFFFFFF0000LL )                                                                                                                   | 
                        {                                                                                                                                                | 
                          v80 = 0x7FFFFFFF0000LL;                                                                                                                        | 
                        }                                                                                                                                                | 
                        *(_QWORD *)v80 = 0LL;                                                                                                                            | 
                      }                                                                                                                                                  | 
                    }                                                                                                                                                    | 
                  }                                                                                                                                                      | 
                }                                                                                                                                                        | 
                ExReleaseRundownProtection_0(v74 + 177);                                                                                                                 | 
              }                                                                                                                                                          | 
            }                                                                                                                                                            | 
            return v70;                                                                                                                                                  | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadIdealProcessor:                                                                                                                                       | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v125 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          v57 = v125;                                                                                                                                                    | 
          if ( v125 > 0x40 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v58 = KeSetIdealProcessorThread((PKTHREAD)Object, v57);                                                                                                      | 
            goto LABEL_138;                                                                                                                                              | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadPriorityBoost:                                                                                                                                        | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v60 = *(_DWORD *)ThreadInformation;                                                                                                                            | 
          v134 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v61 = result;                                                                                                                                                  | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            LOBYTE(v7) = v60 != 0;                                                                                                                                       | 
            KeSetDisableBoostThread(Object, (unsigned int)v7);                                                                                                           | 
            ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                            | 
            return v61;                                                                                                                                                  | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadSetTlsArrayAddress:                                                                                                                                   | 
          return -1073741822;                                                                                                                                            | 
        case ThreadHideFromDebugger:                                                                                                                                     | 
          if ( (_DWORD)v4 )                                                                                                                                              | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v111 = result;                                                                                                                                                 | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            _InterlockedOr((volatile signed __int32 *)Object + 360, 4u);                                                                                                 | 
            goto LABEL_230;                                                                                                                                              | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadBreakOnTermination:                                                                                                                                   | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v88 = *(_DWORD *)ThreadInformation;                                                                                                                            | 
          v135 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )                                                                                                 | 
          {                                                                                                                                                              | 
            return -1073741727;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v111 = result;                                                                                                                                                 | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            if ( v88 )                                                                                                                                                   | 
            {                                                                                                                                                            | 
              _InterlockedOr((volatile signed __int32 *)Object + 360, 0x20u);                                                                                            | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              _InterlockedAnd((volatile signed __int32 *)Object + 360, 0xFFFFFFDF);                                                                                      | 
            }                                                                                                                                                            | 
            goto LABEL_57;                                                                                                                                               | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadSwitchLegacyState:                                                                                                                                    | 
          if ( BugCheckParameter1 != -2LL )                                                                                                                              | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(0xFFFFFFFFFFFFFFFEuLL, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                    | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v90 = Object;                                                                                                                                                | 
            *((_QWORD *)Object + 74) |= MEMORY[0xFFFFF780000003D8] | 3LL;                                                                                                | 
            ObfDereferenceObjectWithTag(v90, 0x79517350u);                                                                                                               | 
            return 0;                                                                                                                                                    | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadIoPriority:                                                                                                                                           | 
          if ( (((_DWORD)v4 - 4) & 0xFFFFFFFB) != 0 )                                                                                                                    | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (_DWORD)v4 == 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            LODWORD(v17) = *(_DWORD *)ThreadInformation;                                                                                                                 | 
            v136 = *(_DWORD *)ThreadInformation;                                                                                                                         | 
            LOBYTE(v18) = 0;                                                                                                                                             | 
          }                                                                                                                                                              | 
          else                                                                                                                                                           | 
          {                                                                                                                                                              | 
            v17 = *(_QWORD *)ThreadInformation;                                                                                                                          | 
            v144 = v17;                                                                                                                                                  | 
            v18 = HIDWORD(v17);                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (unsigned int)v17 >= 4 )                                                                                                                                  | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (unsigned int)v17 >= 3                                                                                                                                    | 
            && !(unsigned __int8)((__int64 (__fastcall *)(_QWORD, _QWORD, _QWORD, _QWORD))SeCheckPrivilegedObject)(                                                      | 
                                   SeIncreaseBasePriorityPrivilege,                                                                                                      | 
                                   BugCheckParameter1,                                                                                                                   | 
                                   32LL,                                                                                                                                 | 
                                   PreviousMode) )                                                                                                                       | 
          {                                                                                                                                                              | 
            return -1073741727;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v62 = Object;                                                                                                                                                | 
            if ( (_BYTE)v18 == 1 && ((*((_DWORD *)Object + 360) >> 9) & 7) < (int)v17 )                                                                                  | 
            {                                                                                                                                                            | 
              IoBoostThreadIoPriority(Object, (unsigned int)v17, 0LL);                                                                                                   | 
            }                                                                                                                                                            | 
            PsSetIoPriorityThread(v62, (unsigned int)v17);                                                                                                               | 
            ObfDereferenceObjectWithTag(v62, 0x79517350u);                                                                                                               | 
            return 0;                                                                                                                                                    | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadPagePriority:                                                                                                                                         | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v137 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          v45 = v137;                                                                                                                                                    | 
          if ( v137 > (unsigned int)MmGetDefaultPagePriority(v8, 0x7FFFFFFF0000LL, CurrentThread, PreviousMode)                                                          | 
            || v137 < (unsigned int)MmGetMinWsPagePriority() )                                                                                                           | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            PsSetPagePriorityThread(Object, v45);                                                                                                                        | 
            goto LABEL_57;                                                                                                                                               | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadActualBasePriority:                                                                                                                                   | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          Increment = *(_DWORD *)ThreadInformation;                                                                                                                      | 
          v27 = Increment;                                                                                                                                               | 
          if ( (unsigned int)(Increment - 1) > 0x1E )                                                                                                                    | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v28 = result;                                                                                                                                                  | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v29 = Object;                                                                                                                                                | 
            if ( v27 < 16                                                                                                                                                | 
              || *(_BYTE *)(*((_QWORD *)Object + 68) + 839LL) == 4                                                                                                       | 
              || SeSinglePrivilegeCheck(SeIncreaseBasePriorityPrivilege, v109) )                                                                                         | 
            {                                                                                                                                                            | 
              KeSetActualBasePriorityThread((ULONG_PTR)v29);                                                                                                             | 
              ObfDereferenceObjectWithTag(v29, 0x79517350u);                                                                                                             | 
              return v28;                                                                                                                                                | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              ObfDereferenceObjectWithTag(v29, 0x79517350u);                                                                                                             | 
              return -1073741727;                                                                                                                                        | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadWow64Context:                                                                                                                                         | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v30 = Object;                                                                                                                                                | 
            if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 177) )                                                                                           | 
            {                                                                                                                                                            | 
              v31 = PspWow64SetContextThread((PETHREAD)v30);                                                                                                             | 
              ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)v30 + 177);                                                                                                  | 
              ObfDereferenceObjectWithTag(v30, 0x79517350u);                                                                                                             | 
              return v31;                                                                                                                                                | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              ObfDereferenceObjectWithTag(v30, 0x79517350u);                                                                                                             | 
              return -1073741749;                                                                                                                                        | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadGroupInformation:                                                                                                                                     | 
          if ( (_DWORD)v4 != 16 )                                                                                                                                        | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v127 = *(_OWORD *)ThreadInformation;                                                                                                                           | 
          if ( WORD4(v127) >= KeQueryActiveGroupCount() )                                                                                                                | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v63 = v127;                                                                                                                                                    | 
          if ( ((unsigned __int64)v127 & KeActiveProcessors.Bitmap[WORD4(v127)]) != (_QWORD)v127 )                                                                       | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v64 = WORD4(v127);                                                                                                                                             | 
          if ( WORD5(v127) | (unsigned __int16)(WORD6(v127) | HIWORD(v127)) )                                                                                            | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result < 0 )                                                                                                                                              | 
          {                                                                                                                                                              | 
            return result;                                                                                                                                               | 
          }                                                                                                                                                              | 
          v151 = 2097153LL;                                                                                                                                              | 
          memset_0(v152, 0, 0x100uLL);                                                                                                                                   | 
          if ( (unsigned __int16)v151 > v64 )                                                                                                                            | 
          {                                                                                                                                                              | 
            goto LABEL_169;                                                                                                                                              | 
          }                                                                                                                                                              | 
          if ( WORD1(v151) > v64 )                                                                                                                                       | 
          {                                                                                                                                                              | 
            LOWORD(v151) = v64 + 1;                                                                                                                                      | 
LABEL_169:                                                                                                                                                               | 
            v152[v64] |= v63;                                                                                                                                            | 
          }                                                                                                                                                              | 
          v65 = *((_QWORD *)Object + 68);                                                                                                                                | 
          v66 = v116;                                                                                                                                                    | 
          PspLockProcessShared(v65, v116);                                                                                                                               | 
          v67 = *(char **)(v65 + 672);                                                                                                                                   | 
          v124 = v67;                                                                                                                                                    | 
          if ( v67                                                                                                                                                       | 
            && (ExAcquireResourceSharedLite((PERESOURCE)(v67 + 56), 1u), (*((_DWORD *)v67 + 264) & 0x10) != 0)                                                           | 
            && !(unsigned int)KeIsSubsetAffinityEx(&v151, v67 + 728) )                                                                                                   | 
          {                                                                                                                                                              | 
            LODWORD(v7) = -1073741823;                                                                                                                                   | 
          }                                                                                                                                                              | 
          else                                                                                                                                                           | 
          {                                                                                                                                                              | 
            KeSetUserAffinityThread(Object, &v151);                                                                                                                      | 
            KeQueryPrimaryGroupProcess(v65, &v117);                                                                                                                      | 
            for ( j = 0; ; ++j )                                                                                                                                         | 
            {                                                                                                                                                            | 
              if ( j >= (unsigned __int16)v151 )                                                                                                                         | 
              {                                                                                                                                                          | 
                goto LABEL_220;                                                                                                                                          | 
              }                                                                                                                                                          | 
              if ( v152[j] )                                                                                                                                             | 
              {                                                                                                                                                          | 
                break;                                                                                                                                                   | 
              }                                                                                                                                                          | 
            }                                                                                                                                                            | 
            v111 = 0;                                                                                                                                                    | 
            if ( v117 != WORD4(v127) )                                                                                                                                   | 
            {                                                                                                                                                            | 
              _interlockedbittestandset((volatile signed __int32 *)(v65 + 136), 0xCu);                                                                                   | 
              v67 = (char *)v124;                                                                                                                                        | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
LABEL_220:                                                                                                                                                               | 
          if ( v67 )                                                                                                                                                     | 
          {                                                                                                                                                              | 
            ExReleaseResourceLite((PERESOURCE)(v67 + 56));                                                                                                               | 
          }                                                                                                                                                              | 
          PspUnlockProcessShared(v65, v66);                                                                                                                              | 
          goto LABEL_34;                                                                                                                                                 | 
        case ThreadCounterProfiling:                                                                                                                                     | 
          if ( (_DWORD)v4 != 24 )                                                                                                                                        | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v128 = *(_OWORD *)ThreadInformation;                                                                                                                           | 
          v129 = *((_QWORD *)ThreadInformation + 2);                                                                                                                     | 
          v93 = v129;                                                                                                                                                    | 
          if ( (v129 & 3) != 0 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            ExRaiseDatatypeMisalignment();                                                                                                                               | 
          }                                                                                                                                                              | 
          if ( v129 >= 0x7FFFFFFF0000LL )                                                                                                                                | 
          {                                                                                                                                                              | 
            v93 = 0x7FFFFFFF0000LL;                                                                                                                                      | 
          }                                                                                                                                                              | 
          *(_BYTE *)v93 = *(_BYTE *)v93;                                                                                                                                 | 
          *(_BYTE *)(v93 + 447) = *(_BYTE *)(v93 + 447);                                                                                                                 | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result < 0 )                                                                                                                                              | 
          {                                                                                                                                                              | 
            return result;                                                                                                                                               | 
          }                                                                                                                                                              | 
          v20 = Object;                                                                                                                                                  | 
          if ( Object == KeGetCurrentThread() )                                                                                                                          | 
          {                                                                                                                                                              | 
            if ( HIDWORD(v128) )                                                                                                                                         | 
            {                                                                                                                                                            | 
              v94 = KeEnableProfiling(Object, DWORD2(v128), v128, v129);                                                                                                 | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              v94 = KeDisableProfiling(Object, v129);                                                                                                                    | 
            }                                                                                                                                                            | 
            v13 = v94;                                                                                                                                                   | 
          }                                                                                                                                                              | 
          else                                                                                                                                                           | 
          {                                                                                                                                                              | 
            v13 = -1073741637;                                                                                                                                           | 
          }                                                                                                                                                              | 
          goto LABEL_48;                                                                                                                                                 | 
        case ThreadIdealProcessorEx:                                                                                                                                     | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v118 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v84 = Object;                                                                                                                                                | 
            v85 = KeSetIdealProcessorThreadByNumber(Object, &v118, &v118);                                                                                               | 
            if ( v85 >= 0 )                                                                                                                                              | 
            {                                                                                                                                                            | 
              *(_DWORD *)ThreadInformation = v118;                                                                                                                       | 
            }                                                                                                                                                            | 
            ObfDereferenceObjectWithTag(v84, 0x79517350u);                                                                                                               | 
            return v85;                                                                                                                                                  | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadCpuAccountingInformation:                                                                                                                             | 
          if ( BugCheckParameter1 != -2LL )                                                                                                                              | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (_DWORD)v4 != 8 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v150 = *(HANDLE *)ThreadInformation;                                                                                                                           | 
          if ( v150 )                                                                                                                                                    | 
          {                                                                                                                                                              | 
            result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v150, 0x79517350u, (__int64)&v130, 0LL, 0LL);                                                          | 
            if ( result < 0 )                                                                                                                                            | 
            {                                                                                                                                                            | 
              return result;                                                                                                                                             | 
            }                                                                                                                                                            | 
            v95 = (struct _LIST_ENTRY *)v130;                                                                                                                            | 
            v96 = *((_QWORD *)v130 + 4);                                                                                                                                 | 
            if ( !v96 )                                                                                                                                                  | 
            {                                                                                                                                                            | 
              ObfDereferenceObjectWithTag(v130, 0x79517350u);                                                                                                            | 
              return -1073740715;                                                                                                                                        | 
            }                                                                                                                                                            | 
            if ( !(unsigned __int8)KeSetThreadChargeOnlySchedulingGroup(v116, v96) )                                                                                     | 
            {                                                                                                                                                            | 
              ObfDereferenceObjectWithTag(v95, 0x79517350u);                                                                                                             | 
              return -1073740714;                                                                                                                                        | 
            }                                                                                                                                                            | 
            v116[1].ApcState.ApcListHead[1].Flink = v95;                                                                                                                 | 
            return 0;                                                                                                                                                    | 
          }                                                                                                                                                              | 
          if ( CurrentThread[1].ApcState.ApcListHead[1].Flink )                                                                                                          | 
          {                                                                                                                                                              | 
            KeSetThreadChargeOnlySchedulingGroup(CurrentThread, 0LL);                                                                                                    | 
            ObfDereferenceObject(v116[1].ApcState.ApcListHead[1].Flink);                                                                                                 | 
            v116[1].ApcState.ApcListHead[1].Flink = 0LL;                                                                                                                 | 
            return 0;                                                                                                                                                    | 
          }                                                                                                                                                              | 
          return -1073740713;                                                                                                                                            | 
        case ThreadCounterProfiling|ThreadAffinityMask:                                                                                                                  | 
        case ThreadCounterProfiling|ThreadIsIoPending:                                                                                                                   | 
          return -1073741637;                                                                                                                                            | 
        case ThreadCpuAccountingInformation|ThreadAffinityMask:                                                                                                          | 
          v114 = 0;                                                                                                                                                      | 
          v46 = 0LL;                                                                                                                                                     | 
          v122 = 0LL;                                                                                                                                                    | 
          P = 0LL;                                                                                                                                                       | 
          v113 = 0;                                                                                                                                                      | 
          if ( (_DWORD)v4 == 16 )                                                                                                                                        | 
          {                                                                                                                                                              | 
            v13 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                        | 
            v111 = v13;                                                                                                                                                  | 
            if ( v13 >= 0 )                                                                                                                                              | 
            {                                                                                                                                                            | 
              v114 = 1;                                                                                                                                                  | 
              if ( v109 )                                                                                                                                                | 
              {                                                                                                                                                          | 
                v147 = 0LL;                                                                                                                                              | 
                v47 = 0x7FFFFFFF0000LL;                                                                                                                                  | 
                if ( (unsigned __int64)ThreadInformation < 0x7FFFFFFF0000LL )                                                                                            | 
                {                                                                                                                                                        | 
                  v47 = (__int64)ThreadInformation;                                                                                                                      | 
                }                                                                                                                                                        | 
                v147.m128i_i32[0] = *(_DWORD *)v47;                                                                                                                      | 
                v48 = *(_QWORD *)(v47 + 8);                                                                                                                              | 
                v147.m128i_i64[1] = v48;                                                                                                                                 | 
                *(__m128i *)Src = v147;                                                                                                                                  | 
                if ( (unsigned __int16)_mm_cvtsi128_si32(v147) && (v48 & 1) != 0 )                                                                                       | 
                {                                                                                                                                                        | 
                  ExRaiseDatatypeMisalignment();                                                                                                                         | 
                }                                                                                                                                                        | 
                v13 = v111;                                                                                                                                              | 
                v46 = v122;                                                                                                                                              | 
              }                                                                                                                                                          | 
              else                                                                                                                                                       | 
              {                                                                                                                                                          | 
                *(_OWORD *)Src = *(_OWORD *)ThreadInformation;                                                                                                           | 
              }                                                                                                                                                          | 
              v49 = (unsigned __int16)Src[0];                                                                                                                            | 
              if ( ((__int64)Src[0] & 1) == 0 && LOWORD(Src[0]) <= WORD1(Src[0]) )                                                                                       | 
              {                                                                                                                                                          | 
                Pool2 = (unsigned __int16 *)ExAllocatePool2(0x40uLL, LOWORD(Src[0]) + 16LL, 0x6D4E6854u);                                                                | 
                v46 = Pool2;                                                                                                                                             | 
                v122 = Pool2;                                                                                                                                            | 
                if ( Pool2 )                                                                                                                                             | 
                {                                                                                                                                                        | 
                  *((_QWORD *)Pool2 + 1) = Pool2 + 8;                                                                                                                    | 
                  *Pool2 = v49;                                                                                                                                          | 
                  Pool2[1] = v49;                                                                                                                                        | 
                  memmove(Pool2 + 8, Src[1], v49);                                                                                                                       | 
                  v51 = v116;                                                                                                                                            | 
                  v52 = Object;                                                                                                                                          | 
                  PspLockThreadSecurityExclusive(Object, v116);                                                                                                          | 
                  v113 = 1;                                                                                                                                              | 
                  P = (PVOID)*((_QWORD *)v52 + 212);                                                                                                                     | 
                  *((_QWORD *)v52 + 212) = v46;                                                                                                                          | 
                  v46 = 0LL;                                                                                                                                             | 
                  v122 = 0LL;                                                                                                                                            | 
                  EtwTraceThreadSetName(v52);                                                                                                                            | 
                }                                                                                                                                                        | 
                else                                                                                                                                                     | 
                {                                                                                                                                                        | 
                  v13 = -1073741670;                                                                                                                                     | 
                  v111 = -1073741670;                                                                                                                                    | 
                  v51 = v116;                                                                                                                                            | 
                }                                                                                                                                                        | 
LABEL_116:                                                                                                                                                               | 
                if ( v113 )                                                                                                                                              | 
                {                                                                                                                                                        | 
                  PspUnlockThreadSecurityExclusive(Object, v51);                                                                                                         | 
                }                                                                                                                                                        | 
                if ( v114 )                                                                                                                                              | 
                {                                                                                                                                                        | 
                  ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                      | 
                }                                                                                                                                                        | 
                if ( P )                                                                                                                                                 | 
                {                                                                                                                                                        | 
                  ExFreePoolWithTag(P, 0x6D4E6854u);                                                                                                                     | 
                }                                                                                                                                                        | 
                if ( v46 )                                                                                                                                               | 
                {                                                                                                                                                        | 
                  ExFreePoolWithTag(v46, 0x6D4E6854u);                                                                                                                   | 
                  return v13;                                                                                                                                            | 
                }                                                                                                                                                        | 
                return v13;                                                                                                                                              | 
              }                                                                                                                                                          | 
              v13 = -1073741811;                                                                                                                                         | 
              v111 = -1073741811;                                                                                                                                        | 
            }                                                                                                                                                            | 
            v51 = v116;                                                                                                                                                  | 
            goto LABEL_116;                                                                                                                                              | 
          }                                                                                                                                                              | 
          v13 = -1073741820;                                                                                                                                             | 
          v111 = -1073741820;                                                                                                                                            | 
          v51 = v116;                                                                                                                                                    | 
          goto LABEL_116;                                                                                                                                                | 
        case ThreadSuspendCount|ThreadAffinityMask:                                                                                                                      | 
          if ( (v4 & 7) != 0 || (unsigned int)v4 > 0x100 )                                                                                                               | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          memmove(v153, ThreadInformation, v4);                                                                                                                          | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v58 = KeSetSelectedCpuSetsThread((_DWORD)Object);                                                                                                            | 
LABEL_138:                                                                                                                                                               | 
            ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                            | 
            return v58;                                                                                                                                                  | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadDynamicCodePolicyInfo:                                                                                                                                | 
          if ( BugCheckParameter1 != -2LL )                                                                                                                              | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v143 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          if ( v143 == 1 )                                                                                                                                               | 
          {                                                                                                                                                              | 
            if ( ((__int64)CurrentThread->Process[4].ThreadListHead.Flink & 0x200) != 0 )                                                                                | 
            {                                                                                                                                                            | 
              _InterlockedOr((volatile signed __int32 *)&CurrentThread[1].SwapListEntry + 2, 0x40000u);                                                                  | 
              return 0;                                                                                                                                                  | 
            }                                                                                                                                                            | 
            return -1073741790;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( v143 )                                                                                                                                                    | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          _InterlockedAnd((volatile signed __int32 *)&CurrentThread[1].SwapListEntry + 2, 0xFFFBFFFF);                                                                   | 
          return 0;                                                                                                                                                      | 
        case ThreadDynamicCodePolicyInfo|ThreadTimes:                                                                                                                    | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v86 = *(_DWORD *)ThreadInformation;                                                                                                                            | 
          v142 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          if ( !(_BYTE)PreviousMode )                                                                                                                                    | 
          {                                                                                                                                                              | 
            goto LABEL_227;                                                                                                                                              | 
          }                                                                                                                                                              | 
          if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )                                                                                                 | 
          {                                                                                                                                                              | 
            return -1073741727;                                                                                                                                          | 
          }                                                                                                                                                              | 
          LOBYTE(v92) = 81;                                                                                                                                              | 
          if ( !(unsigned __int8)RtlTestProtectedAccess(BYTE2(v116->Process[3].ActiveGroupsMask.Masks[1]), v92) )                                                        | 
          {                                                                                                                                                              | 
            return -1073741790;                                                                                                                                          | 
          }                                                                                                                                                              | 
LABEL_227:                                                                                                                                                               | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v111 = result;                                                                                                                                                 | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            if ( v86 )                                                                                                                                                   | 
            {                                                                                                                                                            | 
              _InterlockedOr((volatile signed __int32 *)Object + 360, 0x80000u);                                                                                         | 
            }                                                                                                                                                            | 
            else                                                                                                                                                         | 
            {                                                                                                                                                            | 
              _InterlockedAnd((volatile signed __int32 *)Object + 360, 0xFFF7FFFF);                                                                                      | 
            }                                                                                                                                                            | 
LABEL_230:                                                                                                                                                               | 
            ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                            | 
            return v111;                                                                                                                                                 | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadDynamicCodePolicyInfo|ThreadAffinityMask:                                                                                                             | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v91 = *(_DWORD *)ThreadInformation;                                                                                                                            | 
          v141 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          v111 = result;                                                                                                                                                 | 
          if ( result < 0 )                                                                                                                                              | 
          {                                                                                                                                                              | 
            return result;                                                                                                                                               | 
          }                                                                                                                                                              | 
          if ( v91 )                                                                                                                                                     | 
          {                                                                                                                                                              | 
            _InterlockedOr((volatile signed __int32 *)Object + 360, 0x200000u);                                                                                          | 
          }                                                                                                                                                              | 
          else                                                                                                                                                           | 
          {                                                                                                                                                              | 
            _InterlockedAnd((volatile signed __int32 *)Object + 360, 0xFFDFFFFF);                                                                                        | 
          }                                                                                                                                                              | 
          goto LABEL_230;                                                                                                                                                | 
        case ThreadSubsystemInformation|ThreadPriority:                                                                                                                  | 
          if ( BugCheckParameter1 != -2LL )                                                                                                                              | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (_DWORD)v4 != 8 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v131 = *(_QWORD *)ThreadInformation;                                                                                                                           | 
          v97 = KeGetCurrentThread();                                                                                                                                    | 
          if ( !v131 )                                                                                                                                                   | 
          {                                                                                                                                                              | 
            if ( v97[1].WaitBlock[3].WaitListEntry.Blink == (struct _LIST_ENTRY *)-3LL )                                                                                 | 
            {                                                                                                                                                            | 
              return -1073741811;                                                                                                                                        | 
            }                                                                                                                                                            | 
            v98 = PsAttachSiloToCurrentThread(-3LL);                                                                                                                     | 
            v24 = 1833530192;                                                                                                                                            | 
            v23 = (PVOID)v98;                                                                                                                                            | 
            goto LABEL_59;                                                                                                                                               | 
          }                                                                                                                                                              | 
          Object = v97;                                                                                                                                                  | 
          if ( v97[1].WaitBlock[3].WaitListEntry.Blink != (struct _LIST_ENTRY *)-3LL )                                                                                   | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(v131, 0x6D497350u, (__int64)&v124, 0LL, 0LL);                                                                       | 
          if ( result >= 0 )                                                                                                                                             | 
          {                                                                                                                                                              | 
            v99 = v124;                                                                                                                                                  | 
            if ( (*((_DWORD *)v124 + 389) & 2) == 0 )                                                                                                                    | 
            {                                                                                                                                                            | 
              ObfDereferenceObjectWithTag(v124, 0x6D497350u);                                                                                                            | 
              return -1073741811;                                                                                                                                        | 
            }                                                                                                                                                            | 
            v100 = IoThreadToProcess((PETHREAD)Object);                                                                                                                  | 
            ProcessSilo = PsGetProcessSilo(v100);                                                                                                                        | 
            IsSiloInSilo = PspIsSiloInSilo(v99, ProcessSilo);                                                                                                            | 
            v103 = v99;                                                                                                                                                  | 
            if ( IsSiloInSilo )                                                                                                                                          | 
            {                                                                                                                                                            | 
              EffectiveServerSilo = PsGetEffectiveServerSilo(v99);                                                                                                       | 
              ProcessServerSilo = PsGetProcessServerSilo(v100);                                                                                                          | 
              v103 = v99;                                                                                                                                                | 
              if ( ProcessServerSilo == EffectiveServerSilo )                                                                                                            | 
              {                                                                                                                                                          | 
                PsAttachSiloToCurrentThread(v99);                                                                                                                        | 
                return 0;                                                                                                                                                | 
              }                                                                                                                                                          | 
            }                                                                                                                                                            | 
            ObfDereferenceObjectWithTag(v103, 0x6D497350u);                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          return result;                                                                                                                                                 | 
        case ThreadIdealProcessorEx|ThreadIsIoPending:                                                                                                                   | 
          if ( (_DWORD)v4 != 12 )                                                                                                                                        | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          Token[0] = *(PVOID *)ThreadInformation;                                                                                                                        | 
          v59 = *((_DWORD *)ThreadInformation + 2);                                                                                                                      | 
          if ( LODWORD(Token[0]) != 1 || (HIDWORD(Token[0]) & 0xFFFFFFFE) != 0 || (~HIDWORD(Token[0]) & v59) != 0 )                                                      | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (BYTE4(Token[0]) & 1) != 0 )                                                                                                                              | 
          {                                                                                                                                                              | 
            if ( (v59 & 1) == 0 )                                                                                                                                        | 
            {                                                                                                                                                            | 
              v12 = 3;                                                                                                                                                   | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
          else                                                                                                                                                           | 
          {                                                                                                                                                              | 
            v12 = 0;                                                                                                                                                     | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                       | 
          if ( result < 0 )                                                                                                                                              | 
          {                                                                                                                                                              | 
            return result;                                                                                                                                               | 
          }                                                                                                                                                              | 
          PspSetThreadPpmPolicy(Object, v12);                                                                                                                            | 
          goto LABEL_57;                                                                                                                                                 | 
        case ThreadCpuAccountingInformation|ThreadIsIoPending:                                                                                                           | 
          if ( BugCheckParameter1 != -2LL || (_BYTE)PreviousMode )                                                                                                       | 
          {                                                                                                                                                              | 
            return -1073741790;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v140 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          if ( v140 >= 2 )                                                                                                                                               | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v106 = KeGetCurrentThread();                                                                                                                                   | 
          v107 = *((_DWORD *)&v106[1].SwapListEntry + 3) ^ ((unsigned __int16)*((_DWORD *)&v106[1].SwapListEntry + 3) ^ (unsigned __int16)((_WORD)v140 << 11)) & 0x800u; | 
          *((_DWORD *)&v106[1].SwapListEntry + 3) = v107;                                                                                                                | 
          KeUpdateThreadCpuSets(v106, v107, CurrentThread);                                                                                                              | 
          return 0;                                                                                                                                                      | 
        case ThreadIdealProcessorEx|ThreadIsTerminated:                                                                                                                  | 
          if ( (_DWORD)v4 != 4 )                                                                                                                                         | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          if ( BugCheckParameter1 != -2LL )                                                                                                                              | 
          {                                                                                                                                                              | 
            return -1073741790;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v139 = *(_DWORD *)ThreadInformation;                                                                                                                           | 
          v108 = v139;                                                                                                                                                   | 
          if ( v139 > 1 )                                                                                                                                                | 
          {                                                                                                                                                              | 
            return -1073741811;                                                                                                                                          | 
          }                                                                                                                                                              | 
          result = ObpReferenceObjectByHandleWithTag(0xFFFFFFFFFFFFFFFEuLL, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                    | 
          if ( result < 0 )                                                                                                                                              | 
          {                                                                                                                                                              | 
            return result;                                                                                                                                               | 
          }                                                                                                                                                              | 
          v23 = Object;                                                                                                                                                  | 
          *((_DWORD *)Object + 361) ^= ((unsigned __int16)*((_DWORD *)Object + 361) ^ (unsigned __int16)(v108 << 12)) & 0x1000;                                          | 
          goto LABEL_58;                                                                                                                                                 | 
        case ThreadCounterProfiling|ThreadPagePriority:                                                                                                                  | 
          if ( (_DWORD)v4 != 16 )                                                                                                                                        | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          v146 = *(_OWORD *)ThreadInformation;                                                                                                                           | 
          if ( DWORD2(v146) )                                                                                                                                            | 
          {                                                                                                                                                              | 
            v13 = -1073741811;                                                                                                                                           | 
          }                                                                                                                                                              | 
          else                                                                                                                                                           | 
          {                                                                                                                                                              | 
            v13 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                        | 
            v111 = v13;                                                                                                                                                  | 
            if ( v13 >= 0 )                                                                                                                                              | 
            {                                                                                                                                                            | 
              v14 = Object;                                                                                                                                              | 
              if ( Object == KeGetCurrentThread() )                                                                                                                      | 
              {                                                                                                                                                          | 
                KeAbUserModeEntryFree(Object, v146);                                                                                                                     | 
                v13 = v111;                                                                                                                                              | 
              }                                                                                                                                                          | 
              else                                                                                                                                                       | 
              {                                                                                                                                                          | 
                v13 = -1073741637;                                                                                                                                       | 
              }                                                                                                                                                          | 
LABEL_28:                                                                                                                                                                | 
              if ( v14 )                                                                                                                                                 | 
              {                                                                                                                                                          | 
                ObfDereferenceObjectWithTag(v14, 0x79517350u);                                                                                                           | 
                return v13;                                                                                                                                              | 
              }                                                                                                                                                          | 
              return v13;                                                                                                                                                | 
            }                                                                                                                                                            | 
          }                                                                                                                                                              | 
          v14 = Object;                                                                                                                                                  | 
          goto LABEL_28;                                                                                                                                                 | 
        case ThreadActualGroupAffinity|ThreadIsIoPending:                                                                                                                | 
          if ( (_DWORD)v4 != 24 )                                                                                                                                        | 
          {                                                                                                                                                              | 
            return -1073741820;                                                                                                                                          | 
          }                                                                                                                                                              | 
          Token[0] = 0LL;                                                                                                                                                | 
          v41 = *(__m128i *)ThreadInformation;                                                                                                                           | 
          *(_OWORD *)v148 = *(_OWORD *)ThreadInformation;                                                                                                                | 
          v42 = *((_QWORD *)ThreadInformation + 2);                                                                                                                      | 
          v149[0] = v42;                                                                                                                                                 | 
          v13 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
          if ( v13 < 0 )                                                                                                                                                 | 
          {                                                                                                                                                              | 
            goto LABEL_90;                                                                                                                                               | 
          }                                                                                                                                                              | 
          v43 = _mm_cvtsi128_si32(v41);                                                                                                                                  | 
          if ( !v43 )                                                                                                                                                    | 
          {                                                                                                                                                              | 
            v13 = ObpReferenceObjectByHandleWithTag(v148[1], 0x79517350u, (__int64)Token, 0LL, 0LL);                                                                     | 
            if ( v13 < 0 )                                                                                                                                               | 
            {                                                                                                                                                            | 
              goto LABEL_90;                                                                                                                                             | 
            }                                                                                                                                                            | 
            v44 = PspSchedulerSharedDataRegionSlotAllocate(Object, Token[0], v149);                                                                                      | 
            v42 = v149[0];                                                                                                                                               | 
            v41 = *(__m128i *)v148;                                                                                                                                      | 
            goto LABEL_88;                                                                                                                                               | 
          }                                                                                                                                                              | 
          if ( v43 == 1 )                                                                                                                                                | 
          {                                                                                                                                                              | 
            v44 = PspSchedulerSharedDataRegionSlotFree(Object);                                                                                                          | 
LABEL_88:                                                                                                                                                                | 
            v13 = v44;                                                                                                                                                   | 
            goto LABEL_89;                                                                                                                                               | 
          }                                                                                                                                                              | 
          v13 = -1073741637;                                                                                                                                             | 
LABEL_89:                                                                                                                                                                | 
          *(__m128i *)ThreadInformation = v41;                                                                                                                           | 
          *((_QWORD *)ThreadInformation + 2) = v42;                                                                                                                      | 
LABEL_90:                                                                                                                                                                | 
          if ( Token[0] )                                                                                                                                                | 
          {                                                                                                                                                              | 
            ObfDereferenceObjectWithTag(Token[0], 0x79517350u);                                                                                                          | 
          }                                                                                                                                                              | 
          v21 = Object;                                                                                                                                                  | 
          if ( !Object )                                                                                                                                                 | 
          {                                                                                                                                                              | 
            return v13;                                                                                                                                                  | 
          }                                                                                                                                                              | 
          break;                                                                                                                                                         | 
        default:                                                                                                                                                         | 
          return -1073741821;                                                                                                                                            | 
      }                                                                                                                                                                  | 
LABEL_49:                                                                                                                                                                | 
      ObfDereferenceObjectWithTag(v21, 0x79517350u);                                                                                                                     | 
      return v13;                                                                                                                                                        | 
    }                                                                                                                                                                    | 
    return -1073741811;                                                                                                                                                  | 
  }                                                                                                                                                                      | 
  if ( (_DWORD)v4 != 4 )                                                                                                                                                 | 
  {                                                                                                                                                                      | 
    return -1073741820;                                                                                                                                                  | 
  }                                                                                                                                                                      | 
  v138 = *(_DWORD *)ThreadInformation;                                                                                                                                   | 
  v22 = v138;                                                                                                                                                            | 
  if ( (unsigned int)(v138 - 1) > 0x1E )                                                                                                                                 | 
  {                                                                                                                                                                      | 
    return -1073741811;                                                                                                                                                  | 
  }                                                                                                                                                                      | 
  if ( v138 >= 16                                                                                                                                                        | 
    && !(unsigned __int8)((__int64 (__fastcall *)(_QWORD, _QWORD, _QWORD, _QWORD))SeCheckPrivilegedObject)(                                                              | 
                           SeIncreaseBasePriorityPrivilege,                                                                                                              | 
                           BugCheckParameter1,                                                                                                                           | 
                           1024LL,                                                                                                                                       | 
                           PreviousMode) )                                                                                                                               | 
  {                                                                                                                                                                      | 
    return -1073741727;                                                                                                                                                  | 
  }                                                                                                                                                                      | 
  result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                               | 
  if ( result >= 0 )                                                                                                                                                     | 
  {                                                                                                                                                                      | 
    KeSetPriorityThread((PKTHREAD)Object, v22);                                                                                                                          | 
LABEL_57:                                                                                                                                                                | 
    v23 = Object;                                                                                                                                                        | 
LABEL_58:                                                                                                                                                                | 
    v24 = 2035381072;                                                                                                                                                    | 
LABEL_59:                                                                                                                                                                | 
    ObfDereferenceObjectWithTag(v23, v24);                                                                                                                               | 
    return 0;                                                                                                                                                            | 
  }                                                                                                                                                                      | 
  return result;                                                                                                                                                         | 
}                                                                                                                                                                        | 
