NTSTATUS __fastcall NtSetInformationProcess(ULONG_PTR BugCheckParameter1, unsigned int a2, unsigned int *a3, ULONG a4)   | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  v4 = a4;                                                                                                               | 
  v5 = (__int128 *)a3;                                                                                                   | 
  v7 = (HANDLE)BugCheckParameter1;                                                                                       | 
  v242 = (HANDLE)BugCheckParameter1;                                                                                     | 
  P = a3;                                                                                                                | 
  Alignment = a4;                                                                                                        | 
  v8 = 0LL;                                                                                                              | 
  Object = 0LL;                                                                                                          | 
  v307 = 0LL;                                                                                                            | 
  v241 = 0;                                                                                                              | 
  v249 = 0;                                                                                                              | 
  v259 = 0;                                                                                                              | 
  v254 = 0LL;                                                                                                            | 
  v274 = 0LL;                                                                                                            | 
  v291 = 0LL;                                                                                                            | 
  LODWORD(v262) = 0;                                                                                                     | 
  v308 = 0LL;                                                                                                            | 
  v247 = 0;                                                                                                              | 
  CurrentThread = KeGetCurrentThread();                                                                                  | 
  v266 = (char *)CurrentThread;                                                                                          | 
  PreviousMode = CurrentThread->PreviousMode;                                                                            | 
  v246 = PreviousMode;                                                                                                   | 
  if ( PreviousMode )                                                                                                    | 
  {                                                                                                                      | 
    switch ( a2 )                                                                                                        | 
    {                                                                                                                    | 
      case 5u:                                                                                                           | 
        v22 = 4;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x11u:                                                                                                        | 
        v22 = 1;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x19u:                                                                                                        | 
        v22 = 1;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x12u:                                                                                                        | 
        v22 = 1;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x6Cu:                                                                                                        | 
        v22 = 2;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x15u:                                                                                                        | 
        v11 = 8LL;                                                                                                       | 
        v22 = 8;                                                                                                         | 
        break;                                                                                                           | 
      case 0x21u:                                                                                                        | 
        v22 = 4;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x27u:                                                                                                        | 
        v22 = 4;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x23u:                                                                                                        | 
        v11 = 8LL;                                                                                                       | 
        v22 = 8;                                                                                                         | 
        break;                                                                                                           | 
      case 8u:                                                                                                           | 
        v11 = 8LL;                                                                                                       | 
        v22 = 8;                                                                                                         | 
        break;                                                                                                           | 
      case 0x28u:                                                                                                        | 
        v11 = 8LL;                                                                                                       | 
        v22 = 8;                                                                                                         | 
        break;                                                                                                           | 
      case 0x29u:                                                                                                        | 
        v11 = 8LL;                                                                                                       | 
        v22 = 8;                                                                                                         | 
        break;                                                                                                           | 
      case 0x62u:                                                                                                        | 
        v11 = 8LL;                                                                                                       | 
        v22 = 8;                                                                                                         | 
        break;                                                                                                           | 
      case 0x63u:                                                                                                        | 
        v11 = 8LL;                                                                                                       | 
        v22 = 8;                                                                                                         | 
        break;                                                                                                           | 
      case 0x2Du:                                                                                                        | 
        v22 = 4;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      case 0x2Eu:                                                                                                        | 
        v22 = 4;                                                                                                         | 
        v11 = 8LL;                                                                                                       | 
        break;                                                                                                           | 
      default:                                                                                                           | 
        v11 = 8LL;                                                                                                       | 
        switch ( a2 )                                                                                                    | 
        {                                                                                                                | 
          case '1':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case '5':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case '8':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case '>':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case 'A':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case 'F':                                                                                                      | 
            v22 = 1;                                                                                                     | 
            break;                                                                                                       | 
          case 'J':                                                                                                      | 
            v22 = 1;                                                                                                     | 
            break;                                                                                                       | 
          case 'S':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case 'Z':                                                                                                      | 
            v22 = 1;                                                                                                     | 
            break;                                                                                                       | 
          case '[':                                                                                                      | 
            v22 = 4;                                                                                                     | 
            break;                                                                                                       | 
          case '_':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case 'W':                                                                                                      | 
            v22 = 1;                                                                                                     | 
            break;                                                                                                       | 
          case 'd':                                                                                                      | 
            v22 = 4;                                                                                                     | 
            break;                                                                                                       | 
          case 'e':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case 'f':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          case 'i':                                                                                                      | 
            v22 = 8;                                                                                                     | 
            break;                                                                                                       | 
          default:                                                                                                       | 
            v22 = 4;                                                                                                     | 
            if ( a2 == 107 )                                                                                             | 
            {                                                                                                            | 
              v22 = 8;                                                                                                   | 
            }                                                                                                            | 
            break;                                                                                                       | 
        }                                                                                                                | 
        break;                                                                                                           | 
    }                                                                                                                    | 
    if ( a4 )                                                                                                            | 
    {                                                                                                                    | 
      if ( ((v22 - 1) & (unsigned int)a3) != 0 )                                                                         | 
      {                                                                                                                  | 
        ExRaiseDatatypeMisalignment();                                                                                   | 
      }                                                                                                                  | 
      v11 = 8LL;                                                                                                         | 
    }                                                                                                                    | 
  }                                                                                                                      | 
  else                                                                                                                   | 
  {                                                                                                                      | 
    v11 = 8LL;                                                                                                           | 
  }                                                                                                                      | 
  if ( a2 != 96 )                                                                                                        | 
  {                                                                                                                      | 
    switch ( a2 )                                                                                                        | 
    {                                                                                                                    | 
      case 1u:                                                                                                           | 
        return PspSetQuotaLimits((ULONG_PTR)v7);                                                                         | 
      case 5u:                                                                                                           | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v278 = *a3;                                                                                                      | 
        v146 = v278;                                                                                                     | 
        if ( v278 < 0 )                                                                                                  | 
        {                                                                                                                | 
          v146 = v278 & 0x7FFFFFFF;                                                                                      | 
        }                                                                                                                | 
        v158 = v278 < 0 ? 2 : 0;                                                                                         | 
        if ( (unsigned int)(v146 - 1) > 0x1E )                                                                           | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        LODWORD(v92) = (_DWORD)Object;                                                                                   | 
        if ( v146 > *((char *)Object + 144) )                                                                            | 
        {                                                                                                                | 
          v159 = ((__int64 (__fastcall *)(_QWORD, _QWORD, _QWORD, _QWORD))SeCheckPrivilegedObject)(                      | 
                   SeIncreaseBasePriorityPrivilege,                                                                      | 
                   v242,                                                                                                 | 
                   512LL,                                                                                                | 
                   PreviousMode);                                                                                        | 
          v92 = Object;                                                                                                  | 
          if ( !v159 )                                                                                                   | 
          {                                                                                                              | 
            goto LABEL_676;                                                                                              | 
          }                                                                                                              | 
        }                                                                                                                | 
        KeSetPriorityAndQuantumProcess((_DWORD)v92, v146, 0, 0, 0);                                                      | 
        MmSetMemoryPriorityProcess(Object, v158);                                                                        | 
        goto LABEL_24;                                                                                                   | 
      case 6u:                                                                                                           | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v162 = *a3;                                                                                                      | 
        v279 = *a3;                                                                                                      | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        v47 = result;                                                                                                    | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( !ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                               | 
        {                                                                                                                | 
          goto LABEL_287;                                                                                                | 
        }                                                                                                                | 
        for ( i = PsGetNextProcessThread(Object, 0LL); i; i = PsGetNextProcessThread(Object, i) )                        | 
        {                                                                                                                | 
          KeBoostPriorityThread(i, v162);                                                                                | 
        }                                                                                                                | 
        goto LABEL_419;                                                                                                  | 
      case 8u:                                                                                                           | 
        if ( a4 == 8 )                                                                                                   | 
        {                                                                                                                | 
          v61 = 0;                                                                                                       | 
          v270 = 0;                                                                                                      | 
          v60 = *(void **)a3;                                                                                            | 
          v290 = *(_QWORD *)a3;                                                                                          | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          if ( a4 != 16 )                                                                                                | 
          {                                                                                                              | 
            return -1073741820;                                                                                          | 
          }                                                                                                              | 
          v60 = *(void **)a3;                                                                                            | 
          v290 = *(_QWORD *)a3;                                                                                          | 
          v270 = a3[2];                                                                                                  | 
          v61 = v270;                                                                                                    | 
          if ( (v270 & 0xFFFFFFF8) != 0 )                                                                                | 
          {                                                                                                              | 
            return -1073741811;                                                                                          | 
          }                                                                                                              | 
        }                                                                                                                | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        v276 = 0LL;                                                                                                      | 
        v25 = ObReferenceObjectByHandle(v60, 0, LpcPortObjectType, PreviousMode, &v276, 0LL);                            | 
        v252 = v276;                                                                                                     | 
        v269 = (char *)v276;                                                                                             | 
        if ( v25 < 0 )                                                                                                   | 
        {                                                                                                                | 
          return v25;                                                                                                    | 
        }                                                                                                                | 
        v62 = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);               | 
        if ( v62 < 0 )                                                                                                   | 
        {                                                                                                                | 
          ObfDereferenceObject(v252);                                                                                    | 
          return v62;                                                                                                    | 
        }                                                                                                                | 
        v63 = v252;                                                                                                      | 
        v64 = (unsigned __int64)v252 | v61;                                                                              | 
        v268 = v64;                                                                                                      | 
        v65 = (signed __int64 *)((char *)Object + 576);                                                                  | 
        _m_prefetchw((char *)Object + 576);                                                                              | 
        v66 = *v65;                                                                                                      | 
        do                                                                                                               | 
        {                                                                                                                | 
          v252 = (PVOID)v66;                                                                                             | 
          if ( (_DWORD)v4 == 16 )                                                                                        | 
          {                                                                                                              | 
            *((_DWORD *)v5 + 2) = v66 & 7;                                                                               | 
          }                                                                                                              | 
          else if ( (v66 & 7) != 0 )                                                                                     | 
          {                                                                                                              | 
            ObfDereferenceObject(v63);                                                                                   | 
            goto LABEL_470;                                                                                              | 
          }                                                                                                              | 
          v67 = _InterlockedCompareExchange64((volatile signed __int64 *)Object + 72, v64, v66);                         | 
          v192 = v66 == v67;                                                                                             | 
          v66 = v67;                                                                                                     | 
        }                                                                                                                | 
        while ( !v192 );                                                                                                 | 
        if ( v67 )                                                                                                       | 
        {                                                                                                                | 
          memset(&SubjectContext[8], 0, 32);                                                                             | 
          v137 = (void *)(v67 & 0xFFFFFFFFFFFFFFF8uLL);                                                                  | 
          *(_QWORD *)SubjectContext = 0xD00300008LL;                                                                     | 
          v314 = *((_QWORD *)Object + 58);                                                                               | 
          while ( 1 )                                                                                                    | 
          {                                                                                                              | 
            v138 = LpcRequestPort(v137, SubjectContext);                                                                 | 
            if ( v138 != -1073741801 && v138 != -1073741670 )                                                            | 
            {                                                                                                            | 
              break;                                                                                                     | 
            }                                                                                                            | 
            KeDelayExecutionThread(0, 0, (PLARGE_INTEGER)&PspShortTime);                                                 | 
          }                                                                                                              | 
          PspLockUnlockProcessExclusive(Object, CurrentThread);                                                          | 
          ObfDereferenceObject(v137);                                                                                    | 
        }                                                                                                                | 
        goto LABEL_218;                                                                                                  | 
      case 9u:                                                                                                           | 
        if ( a4 != 16 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v302 = *(_QWORD *)a3;                                                                                            | 
        return PspAssignPrimaryToken(CurrentThread, PreviousMode, v7);                                                   | 
      case 0xAu:                                                                                                         | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result >= 0 )                                                                                               | 
        {                                                                                                                | 
          goto LABEL_513;                                                                                                | 
        }                                                                                                                | 
        return result;                                                                                                   | 
      case 0xBu:                                                                                                         | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result >= 0 )                                                                                               | 
        {                                                                                                                | 
          goto LABEL_513;                                                                                                | 
        }                                                                                                                | 
        return result;                                                                                                   | 
      case 0xCu:                                                                                                         | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v21 = *a3;                                                                                                       | 
        v280 = *a3;                                                                                                      | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        PspSetProcessDefaultHardErrorMode(Object, CurrentThread, v21);                                                   | 
        goto LABEL_24;                                                                                                   | 
      case 0xDu:                                                                                                         | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
LABEL_513:                                                                                                               | 
        v47 = xKdEnumerateDebuggingDevices(Object, v5, (unsigned int)v4);                                                | 
        goto LABEL_157;                                                                                                  | 
      case 0xFu:                                                                                                         | 
      case 0x2Au:                                                                                                        | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v47 = PsChargeProcessNonPagedPoolQuota(Object, 24616LL);                                                         | 
        if ( v47 < 0 )                                                                                                   | 
        {                                                                                                                | 
          goto LABEL_157;                                                                                                | 
        }                                                                                                                | 
        Pool2 = ExAllocatePool2(0x40uLL, 0x6028uLL, 0x73577350u);                                                        | 
        v157 = (void *)Pool2;                                                                                            | 
        if ( Pool2 )                                                                                                     | 
        {                                                                                                                | 
          PsWatchEnabled = 1;                                                                                            | 
          *(_DWORD *)Pool2 = 0;                                                                                          | 
          *(_QWORD *)(Pool2 + 8) = 0LL;                                                                                  | 
          KeInitializeGate(Pool2 + 16, 0LL);                                                                             | 
          if ( !_InterlockedCompareExchange64((volatile signed __int64 *)Object + 88, (signed __int64)v157, 0LL) )       | 
          {                                                                                                              | 
LABEL_218:                                                                                                               | 
            v47 = 0;                                                                                                     | 
            ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                            | 
            return v47;                                                                                                  | 
          }                                                                                                              | 
          ExFreePoolWithTag(v157, 0);                                                                                    | 
          v47 = -1073741752;                                                                                             | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v47 = -1073741801;                                                                                             | 
        }                                                                                                                | 
        PsReturnProcessNonPagedPoolQuota((ULONG_PTR)Object, 0x6028uLL);                                                  | 
        goto LABEL_157;                                                                                                  | 
      case 0x10u:                                                                                                        | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                | 
        return -1073741822;                                                                                              | 
      case 0x11u:                                                                                                        | 
        if ( a4 != 1 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v164 = *(_BYTE *)a3;                                                                                             | 
        v256 = *(_BYTE *)a3;                                                                                             | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        PspLockProcessExclusive(Object, CurrentThread);                                                                  | 
        if ( v164 )                                                                                                      | 
        {                                                                                                                | 
          *((_DWORD *)Object + 226) |= 4u;                                                                               | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          *((_DWORD *)Object + 226) &= ~4u;                                                                              | 
        }                                                                                                                | 
        if ( *((_QWORD *)Object + 98) )                                                                                  | 
        {                                                                                                                | 
          v165 = *((_WORD *)Object + 886);                                                                               | 
          if ( v165 == 332 || v165 == 452 )                                                                              | 
          {                                                                                                              | 
            v164 = 1;                                                                                                    | 
          }                                                                                                              | 
        }                                                                                                                | 
        KeSetAutoAlignmentProcess(Object, v164);                                                                         | 
        PspUnlockProcessExclusive(Object, CurrentThread);                                                                | 
        goto LABEL_24;                                                                                                   | 
      case 0x12u:                                                                                                        | 
        if ( a4 != 2 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v160 = *(_WORD *)a3;                                                                                             | 
        v249 = *(_WORD *)a3;                                                                                             | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v99 = PspSetProcessPriorityClass(Object, HIBYTE(v249), v242, PreviousMode);                                      | 
        if ( v99 >= 0 )                                                                                                  | 
        {                                                                                                                | 
          if ( (unsigned int)dword_140E085C0 > 5 && (unsigned __int8)tlgKeywordOn(&dword_140E085C0, 0x400000000000LL) )  | 
          {                                                                                                              | 
            v239[0] = v160;                                                                                              | 
            *(_QWORD *)&SubjectContext[32] = v239;                                                                       | 
            v314 = 1LL;                                                                                                  | 
            v305 = (PVOID)0x1000000;                                                                                     | 
            v315 = &v305;                                                                                                | 
            v316 = 8;                                                                                                    | 
            v317 = 0;                                                                                                    | 
            tlgWriteTransfer_EtwWriteTransfer(&dword_140E085C0, &byte_140051A27, 0LL, 0LL, 4, SubjectContext);           | 
          }                                                                                                              | 
          MmGetMinWsPagePriority();                                                                                      | 
          if ( (unsigned int)Feature_ForegroundBoost_ApiRemoval__private_IsEnabledDeviceUsageNoInline() )                | 
          {                                                                                                              | 
            KeSetPriorityAndQuantumProcess(                                                                              | 
              (_DWORD)Object,                                                                                            | 
              *(_DWORD *)&PspPriorityTable[2 * *((unsigned __int8 *)Object + 839)],                                      | 
              0,                                                                                                         | 
              0,                                                                                                         | 
              0);                                                                                                        | 
          }                                                                                                              | 
          else                                                                                                           | 
          {                                                                                                              | 
            PsSetProcessPriorityByClass(Object, 1LL);                                                                    | 
          }                                                                                                              | 
        }                                                                                                                | 
        goto LABEL_330;                                                                                                  | 
      case 0x13u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v166 = *a3;                                                                                                      | 
        v281 = *a3;                                                                                                      | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        v47 = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);               | 
        if ( v47 < 0 )                                                                                                   | 
        {                                                                                                                | 
          return v47;                                                                                                    | 
        }                                                                                                                | 
        v20 = Object;                                                                                                    | 
        if ( *((_QWORD *)Object + 200) )                                                                                 | 
        {                                                                                                                | 
          v47 = -1073741790;                                                                                             | 
          goto LABEL_158;                                                                                                | 
        }                                                                                                                | 
        if ( v166 )                                                                                                      | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)Object + 125, 0x1000000u);                                           | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          _InterlockedAnd((volatile signed __int32 *)Object + 125, 0xFEFFFFFF);                                          | 
        }                                                                                                                | 
        goto LABEL_157;                                                                                                  | 
      case 0x15u:                                                                                                        | 
        if ( a4 == 8 )                                                                                                   | 
        {                                                                                                                | 
          *(_QWORD *)&v307 = *(_QWORD *)a3;                                                                              | 
          if ( !(_QWORD)v307 )                                                                                           | 
          {                                                                                                              | 
            return -1073741811;                                                                                          | 
          }                                                                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          if ( a4 != 16 )                                                                                                | 
          {                                                                                                              | 
            return -1073741820;                                                                                          | 
          }                                                                                                              | 
          v307 = *(_OWORD *)a3;                                                                                          | 
          if ( !(unsigned __int8)KeVerifyGroupAffinity(&v307, 0LL) )                                                     | 
          {                                                                                                              | 
            return -1073741811;                                                                                          | 
          }                                                                                                              | 
          v7 = v242;                                                                                                     | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v20 = Object;                                                                                                    | 
        if ( (*((_DWORD *)Object + 34) & 0x1000) != 0 )                                                                  | 
        {                                                                                                                | 
          v47 = -1073741811;                                                                                             | 
          goto LABEL_158;                                                                                                | 
        }                                                                                                                | 
        KeQueryPrimaryGroupProcess(Object, &v247);                                                                       | 
        if ( (_DWORD)v4 == 8 )                                                                                           | 
        {                                                                                                                | 
          v147 = v307 & KeActiveProcessors.Bitmap[v247];                                                                 | 
          if ( v147 != (_QWORD)v307 )                                                                                    | 
          {                                                                                                              | 
LABEL_470:                                                                                                               | 
            v47 = -1073741811;                                                                                           | 
            ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                            | 
            return v47;                                                                                                  | 
          }                                                                                                              | 
          WORD4(v307) = v247;                                                                                            | 
          *(_QWORD *)&v307 = v147;                                                                                       | 
        }                                                                                                                | 
        --CurrentThread->KernelApcDisable;                                                                               | 
        if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                                | 
        {                                                                                                                | 
          v100 = (volatile signed __int64 *)((char *)Object + 456);                                                      | 
          v101 = KeAbPreAcquire((char *)Object + 456, 0LL, 0LL);                                                         | 
          if ( _InterlockedCompareExchange64(v100, 17LL, 0LL) )                                                          | 
          {                                                                                                              | 
            ExfAcquirePushLockSharedEx(v100, 0LL, v101, v100);                                                           | 
          }                                                                                                              | 
          if ( v101 )                                                                                                    | 
          {                                                                                                              | 
            *(_BYTE *)(v101 + 10) = 1;                                                                                   | 
          }                                                                                                              | 
          v47 = PspSetProcessAffinitySafe((struct _KPROCESS *)Object, (__int64)&v262);                                   | 
          v102 = (char *)Object;                                                                                         | 
          if ( _InterlockedCompareExchange64((volatile signed __int64 *)Object + 57, 0LL, 17LL) != 17 )                  | 
          {                                                                                                              | 
            ExfReleasePushLockShared(v102 + 456);                                                                        | 
          }                                                                                                              | 
          KeAbPostRelease((ULONG_PTR)(v102 + 456));                                                                      | 
          ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)Object + 61);                                                    | 
          if ( v47 >= 0 )                                                                                                | 
          {                                                                                                              | 
            if ( (_DWORD)v262 )                                                                                          | 
            {                                                                                                            | 
              PspUpdatePebForAffinityChange(CurrentThread, Object);                                                      | 
            }                                                                                                            | 
            _InterlockedOr((volatile signed __int32 *)Object + 124, 0x200000u);                                          | 
          }                                                                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v47 = -1073741558;                                                                                             | 
        }                                                                                                                | 
        KeLeaveCriticalRegionThread(CurrentThread);                                                                      | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                | 
        return v47;                                                                                                      | 
      case 0x16u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v282 = *a3;                                                                                                      | 
        v140 = v282 != 0;                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        v47 = result;                                                                                                    | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( !ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                               | 
        {                                                                                                                | 
          goto LABEL_287;                                                                                                | 
        }                                                                                                                | 
        KeSetDisableBoostProcess(Object, v140);                                                                          | 
        goto LABEL_419;                                                                                                  | 
      case 0x17u:                                                                                                        | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v167 = *(_QWORD *)a3;                                                                                            | 
        v303 = *(_QWORD *)a3;                                                                                            | 
        if ( (unsigned __int8)RtlIsSandboxedToken(0LL) )                                                                 | 
        {                                                                                                                | 
          return -1073741790;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v47 = ObSetProcessDeviceMap(Object, v167, PreviousMode);                                                         | 
        goto LABEL_157;                                                                                                  | 
      case 0x18u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v168 = *a3;                                                                                                      | 
        v286 = *a3;                                                                                                      | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( v168 != (unsigned int)PsGetSessionId(Object) )                                                              | 
        {                                                                                                                | 
          LODWORD(v8) = -1073741790;                                                                                     | 
        }                                                                                                                | 
        goto LABEL_207;                                                                                                  | 
      case 0x19u:                                                                                                        | 
        if ( a4 != 1 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v161 = *(_BYTE *)a3;                                                                                             | 
        v255 = *(_BYTE *)a3;                                                                                             | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( (unsigned int)dword_140E085C0 > 5 && (unsigned __int8)tlgKeywordOn(&dword_140E085C0, 0x400000000000LL) )    | 
        {                                                                                                                | 
          v239[0] = v161;                                                                                                | 
          *(_QWORD *)&SubjectContext[32] = v239;                                                                         | 
          v314 = 1LL;                                                                                                    | 
          v305 = (PVOID)0x1000000;                                                                                       | 
          v315 = &v305;                                                                                                  | 
          v316 = 8;                                                                                                      | 
          v317 = 0;                                                                                                      | 
          tlgWriteTransfer_EtwWriteTransfer(&dword_140E085C0, &byte_140051967, 0LL, 0LL, 4, SubjectContext);             | 
        }                                                                                                                | 
        MmGetMinWsPagePriority();                                                                                        | 
        if ( !(unsigned int)Feature_ForegroundBoost_ApiRemoval__private_IsEnabledDeviceUsageNoInline() )                 | 
        {                                                                                                                | 
          LOBYTE(v8) = v161 != 0;                                                                                        | 
          PsSetProcessPriorityByClass(Object, (unsigned int)v8);                                                         | 
        }                                                                                                                | 
        goto LABEL_24;                                                                                                   | 
      case 0x1Du:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v169 = *a3;                                                                                                      | 
        v295 = *a3;                                                                                                      | 
        if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )                                                   | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( v169 )                                                                                                      | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)Object + 125, 0x2000u);                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          _InterlockedAnd((volatile signed __int32 *)Object + 125, 0xFFFFDFFF);                                          | 
        }                                                                                                                | 
        goto LABEL_24;                                                                                                   | 
      case 0x1Fu:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        v47 = result;                                                                                                    | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v271 = *(_DWORD *)v5;                                                                                            | 
        if ( (v271 & 0xFFFFFFFE) != 0 )                                                                                  | 
        {                                                                                                                | 
          goto LABEL_470;                                                                                                | 
        }                                                                                                                | 
        if ( (v271 & 1) != 0 )                                                                                           | 
        {                                                                                                                | 
          _InterlockedAnd((volatile signed __int32 *)Object + 125, 0xFFFFFFFD);                                          | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)Object + 125, 2u);                                                   | 
        }                                                                                                                | 
        goto LABEL_157;                                                                                                  | 
      case 0x20u:                                                                                                        | 
        Handle = 0LL;                                                                                                    | 
        if ( !a4 )                                                                                                       | 
        {                                                                                                                | 
          goto LABEL_752;                                                                                                | 
        }                                                                                                                | 
        if ( ((a4 - 4) & 0xFFFFFFFB) != 0 )                                                                              | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v170 = *a3;                                                                                                      | 
        LODWORD(Handle) = *a3;                                                                                           | 
        if ( a4 == 8 )                                                                                                   | 
        {                                                                                                                | 
          HIDWORD(Handle) = a3[1];                                                                                       | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          HIDWORD(Handle) = 0;                                                                                           | 
        }                                                                                                                | 
        if ( v170 && (v170 & 0xFFFFFFF0) != 0 )                                                                          | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
LABEL_752:                                                                                                               | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( (_DWORD)v4 )                                                                                                | 
        {                                                                                                                | 
          p_Handle = &Handle;                                                                                            | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          p_Handle = 0LL;                                                                                                | 
        }                                                                                                                | 
        v47 = PsSetProcessHandleTracingInformation(Object, p_Handle);                                                    | 
        goto LABEL_157;                                                                                                  | 
      case 0x21u:                                                                                                        | 
        if ( ((a4 - 4) & 0xFFFFFFFB) != 0 )                                                                              | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( a4 == 4 )                                                                                                   | 
        {                                                                                                                | 
          LODWORD(v68) = *a3;                                                                                            | 
          v260 = *a3;                                                                                                    | 
          LOBYTE(v69) = 0;                                                                                               | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v68 = *(_QWORD *)a3;                                                                                           | 
          v291 = v68;                                                                                                    | 
          v69 = HIDWORD(v68);                                                                                            | 
          v260 = v68;                                                                                                    | 
        }                                                                                                                | 
        if ( (unsigned int)v68 >= 4 )                                                                                    | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( (unsigned int)v68 < 3 )                                                                                     | 
        {                                                                                                                | 
          goto LABEL_353;                                                                                                | 
        }                                                                                                                | 
        if ( !(unsigned __int8)((__int64 (__fastcall *)(_QWORD, _QWORD, _QWORD, _QWORD))SeCheckPrivilegedObject)(        | 
                                 SeIncreaseBasePriorityPrivilege,                                                        | 
                                 v7,                                                                                     | 
                                 512LL,                                                                                  | 
                                 PreviousMode) )                                                                         | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        v7 = v242;                                                                                                       | 
LABEL_353:                                                                                                               | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        v47 = result;                                                                                                    | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                                | 
        {                                                                                                                | 
          PspLockProcessExclusive(Object, CurrentThread);                                                                | 
          v105 = (volatile signed __int32 *)Object;                                                                      | 
          v106 = *((_DWORD *)Object + 125);                                                                              | 
          v107 = v260 << 27;                                                                                             | 
          do                                                                                                             | 
          {                                                                                                              | 
            v108 = v106;                                                                                                 | 
            v106 = _InterlockedCompareExchange(v105 + 125, v107 | v106 & 0xC7FFFFFF, v106);                              | 
          }                                                                                                              | 
          while ( v106 != v108 );                                                                                        | 
          v109 = Object;                                                                                                 | 
          v110 = (char *)*((_QWORD *)Object + 110);                                                                      | 
          if ( v110 != (char *)Object + 880 )                                                                            | 
          {                                                                                                              | 
            v111 = v260;                                                                                                 | 
            do                                                                                                           | 
            {                                                                                                            | 
              if ( (_BYTE)v69 == 1 && ((*((_DWORD *)v110 + 10) >> 9) & 7) < (int)v111 )                                  | 
              {                                                                                                          | 
                IoBoostThreadIoPriority(v110 - 1400, v111, 0LL);                                                         | 
              }                                                                                                          | 
              PsSetIoPriorityThread(v110 - 1400, v111);                                                                  | 
              v110 = *(char **)v110;                                                                                     | 
              v109 = Object;                                                                                             | 
            }                                                                                                            | 
            while ( v110 != (char *)Object + 880 );                                                                      | 
          }                                                                                                              | 
LABEL_418:                                                                                                               | 
          PspUnlockProcessExclusive(v109, CurrentThread);                                                                | 
LABEL_419:                                                                                                               | 
          ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)Object + 61);                                                    | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
LABEL_287:                                                                                                               | 
          v47 = -1073741558;                                                                                             | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
        }                                                                                                                | 
        return v47;                                                                                                      | 
      case 0x22u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        v122 = *a3;                                                                                                      | 
        v71 = KeSetExecuteOptions(KeGetCurrentThread()->ApcState.Process, v122);                                         | 
        if ( v71 >= 0 && (v122 & 3) == 1 )                                                                               | 
        {                                                                                                                | 
          MmRemoveExecuteGrants();                                                                                       | 
        }                                                                                                                | 
        return v71;                                                                                                      | 
      case 0x23u:                                                                                                        | 
        memset(v310, 0, sizeof(v310));                                                                                   | 
        v311 = 0LL;                                                                                                      | 
        v261 = 0LL;                                                                                                      | 
        v265 = 0LL;                                                                                                      | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( PreviousMode != 1 )                                                                                         | 
        {                                                                                                                | 
          return -1073741823;                                                                                            | 
        }                                                                                                                | 
        if ( a4 < 0x28 || a4 - 16 != 24 * ((a4 - 16) / 0x18uLL) )                                                        | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( a4 == 40 )                                                                                                  | 
        {                                                                                                                | 
          P = v310;                                                                                                      | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          P = (PVOID)ExAllocatePool2(0x101uLL, a4, 0x736C5450u);                                                         | 
          if ( !P )                                                                                                      | 
          {                                                                                                              | 
            return -1073741670;                                                                                          | 
          }                                                                                                              | 
        }                                                                                                                | 
        v266 = (char *)P;                                                                                                | 
        memmove(P, v5, v4);                                                                                              | 
        v293 = (char *)P + 4;                                                                                            | 
        if ( *((_DWORD *)P + 1) >= 2u                                                                                    | 
          || (*(_DWORD *)P & 0xFFFFFFFE) != 0                                                                            | 
          || (v292 = (unsigned int *)((char *)P + 8), v26 = *((unsigned int *)P + 2), !(_DWORD)v26)                      | 
          || (unsigned int)(v4 - 16) / 0x18uLL != v26 )                                                                  | 
        {                                                                                                                | 
          v47 = -1073741820;                                                                                             | 
          goto LABEL_136;                                                                                                | 
        }                                                                                                                | 
        v27 = 0;                                                                                                         | 
        while ( 2 )                                                                                                      | 
        {                                                                                                                | 
          v241 = v27;                                                                                                    | 
          if ( v27 < (unsigned int)v26 )                                                                                 | 
          {                                                                                                              | 
            if ( !*((_DWORD *)P + 6 * v27 + 4) )                                                                         | 
            {                                                                                                            | 
              ++v27;                                                                                                     | 
              continue;                                                                                                  | 
            }                                                                                                            | 
            goto LABEL_220;                                                                                              | 
          }                                                                                                              | 
          break;                                                                                                         | 
        }                                                                                                                | 
        Process = KeGetCurrentThread()->ApcState.Process;                                                                | 
        Object = Process;                                                                                                | 
        v250 = 0;                                                                                                        | 
        if ( (*(_DWORD *)P & 1) != 0 )                                                                                   | 
        {                                                                                                                | 
          if ( !Process[1].ReadyTime || (v88 = WORD2(Process[3].PerProcessorCycleTimes), v88 != 332) && v88 != 452 )     | 
          {                                                                                                              | 
LABEL_220:                                                                                                               | 
            v47 = -1073741811;                                                                                           | 
            goto LABEL_136;                                                                                              | 
          }                                                                                                              | 
          v250 = 1;                                                                                                      | 
        }                                                                                                                | 
        v29 = v250 ^ 1u;                                                                                                 | 
        Alignment = 4 * v29 + 4;                                                                                         | 
        v268 = 4 * v29 + 4;                                                                                              | 
        v269 = (char *)v5;                                                                                               | 
        v241 = 0;                                                                                                        | 
        v240 = 0;                                                                                                        | 
        v243 = 0;                                                                                                        | 
        for ( j = PsGetNextProcessThread(Process, 0LL); ; j = PsGetNextProcessThread(Object, v252) )                     | 
        {                                                                                                                | 
          v252 = (PVOID)j;                                                                                               | 
          v31 = (volatile void **)v241;                                                                                  | 
          LODWORD(v242) = v241;                                                                                          | 
          v32 = j;                                                                                                       | 
          if ( !j || v241 >= *v292 )                                                                                     | 
          {                                                                                                              | 
            break;                                                                                                       | 
          }                                                                                                              | 
          if ( (*(_DWORD *)(j + 116) & 0x400) == 0 )                                                                     | 
          {                                                                                                              | 
            RunRef = (PEX_RUNDOWN_REF)(j + 1416);                                                                        | 
            if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)(j + 1416)) )                                             | 
            {                                                                                                            | 
              v33 = *(_QWORD *)(v32 + 240);                                                                              | 
              if ( v250 )                                                                                                | 
              {                                                                                                          | 
                v34 = (char *)(v33 + 8236);                                                                              | 
                v265 = v33 + 8236;                                                                                       | 
                v35 = (char *)*(unsigned int *)(v33 + 8236);                                                             | 
              }                                                                                                          | 
              else                                                                                                       | 
              {                                                                                                          | 
                v34 = (char *)(v33 + 88);                                                                                | 
                v265 = v33 + 88;                                                                                         | 
                v35 = *(char **)(v33 + 88);                                                                              | 
              }                                                                                                          | 
              v261 = v35;                                                                                                | 
              if ( v35 )                                                                                                 | 
              {                                                                                                          | 
                if ( *v293 == 1 )                                                                                        | 
                {                                                                                                        | 
                  if ( v35 == v34 )                                                                                      | 
                  {                                                                                                      | 
                    v261 = 0LL;                                                                                          | 
                    v49 = v31;                                                                                           | 
                  }                                                                                                      | 
                  else                                                                                                   | 
                  {                                                                                                      | 
                    v48 = v268 * *((unsigned int *)P + 3);                                                               | 
                    if ( v48 )                                                                                           | 
                    {                                                                                                    | 
                      if ( ((Alignment - 1) & (unsigned int)v35) != 0 )                                                  | 
                      {                                                                                                  | 
                        ExRaiseDatatypeMisalignment();                                                                   | 
                      }                                                                                                  | 
                      if ( (unsigned __int64)&v35[v48] > 0x7FFFFFFF0000LL || &v35[v48] < v35 )                           | 
                      {                                                                                                  | 
                        v35 = v261;                                                                                      | 
                      }                                                                                                  | 
                    }                                                                                                    | 
                    v264 = (char *)v241;                                                                                 | 
                    v31 = (volatile void **)((char *)P + 24 * v241 + 24);                                                | 
                    ProbeForWrite(*v31, v268 * *((unsigned int *)P + 3), Alignment);                                     | 
                    memmove((void *)*v31, v35, v268 * *((unsigned int *)P + 3));                                         | 
                    _InterlockedOr(v237, 0);                                                                             | 
                    LODWORD(v31) = (_DWORD)v242;                                                                         | 
                    v49 = (volatile void **)v264;                                                                        | 
                  }                                                                                                      | 
                  v50 = &v269[24 * (_QWORD)v49];                                                                         | 
                  v242 = v50;                                                                                            | 
                  v51 = v50 + 16;                                                                                        | 
                  v264 = v50 + 16;                                                                                       | 
                  *((_DWORD *)v50 + 4) |= 1u;                                                                            | 
                  v52 = &v266[24 * (unsigned int)v31 + 24];                                                              | 
                  if ( v250 )                                                                                            | 
                  {                                                                                                      | 
                    v294 = *(_DWORD *)v52;                                                                               | 
                    *(_DWORD *)(v33 + 8236) = v294;                                                                      | 
                  }                                                                                                      | 
                  else                                                                                                   | 
                  {                                                                                                      | 
                    *(_QWORD *)(v33 + 88) = *(_QWORD *)v52;                                                              | 
                  }                                                                                                      | 
                  if ( v240 < 0 )                                                                                        | 
                  {                                                                                                      | 
                    *v51 &= ~1u;                                                                                         | 
                  }                                                                                                      | 
                  else                                                                                                   | 
                  {                                                                                                      | 
                    *((_QWORD *)v50 + 4) = *((_QWORD *)v252 + 162);                                                      | 
                    *((_QWORD *)v50 + 3) = v261;                                                                         | 
                    *v51 ^= 3u;                                                                                          | 
                    ++v241;                                                                                              | 
                  }                                                                                                      | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  v36 = 24LL * v241;                                                                                     | 
                  v37 = &v269[v36];                                                                                      | 
                  v264 = &v269[v36];                                                                                     | 
                  v38 = &v269[v36 + 16];                                                                                 | 
                  v242 = v38;                                                                                            | 
                  *(_DWORD *)v38 |= 1u;                                                                                  | 
                  v39 = *((unsigned int *)v266 + 3);                                                                     | 
                  if ( v250 )                                                                                            | 
                  {                                                                                                      | 
                    v43 = (unsigned __int64)&v35[4 * v39];                                                               | 
                    if ( v43 >= 0x7FFFFFFF0000LL )                                                                       | 
                    {                                                                                                    | 
                      v43 = 0x7FFFFFFF0000LL;                                                                            | 
                    }                                                                                                    | 
                    v44 = *(unsigned int *)v43;                                                                          | 
                    v41 = (unsigned int)v44;                                                                             | 
                    v265 = v44;                                                                                          | 
                    v45 = *(_DWORD *)&v266[v36 + 24];                                                                    | 
                    v289 = v45;                                                                                          | 
                    v46 = (unsigned __int64)&v261[4 * *((unsigned int *)v266 + 3)];                                      | 
                    if ( v46 >= 0x7FFFFFFF0000LL )                                                                       | 
                    {                                                                                                    | 
                      v46 = 0x7FFFFFFF0000LL;                                                                            | 
                    }                                                                                                    | 
                    *(_DWORD *)v46 = v45;                                                                                | 
                  }                                                                                                      | 
                  else                                                                                                   | 
                  {                                                                                                      | 
                    v40 = (unsigned __int64)&v35[8 * v39];                                                               | 
                    if ( v40 >= 0x7FFFFFFF0000LL )                                                                       | 
                    {                                                                                                    | 
                      v40 = 0x7FFFFFFF0000LL;                                                                            | 
                    }                                                                                                    | 
                    v41 = *(_QWORD *)v40;                                                                                | 
                    v265 = *(_QWORD *)v40;                                                                               | 
                    v42 = (unsigned __int64)&v261[8 * *((unsigned int *)v266 + 3)];                                      | 
                    if ( v42 >= 0x7FFFFFFF0000LL )                                                                       | 
                    {                                                                                                    | 
                      v42 = 0x7FFFFFFF0000LL;                                                                            | 
                    }                                                                                                    | 
                    RtlCopyVolatileMemory((void *)v42, (char *)P + v36 + 24, 8uLL);                                      | 
                  }                                                                                                      | 
                  if ( v240 < 0 )                                                                                        | 
                  {                                                                                                      | 
                    *(_DWORD *)v38 &= ~1u;                                                                               | 
                  }                                                                                                      | 
                  else                                                                                                   | 
                  {                                                                                                      | 
                    *((_QWORD *)v37 + 3) = v41;                                                                          | 
                    *(_DWORD *)v38 ^= 3u;                                                                                | 
                    ++v241;                                                                                              | 
                  }                                                                                                      | 
                }                                                                                                        | 
              }                                                                                                          | 
              ExReleaseRundownProtection_0(RunRef);                                                                      | 
              if ( v240 < 0 )                                                                                            | 
              {                                                                                                          | 
                break;                                                                                                   | 
              }                                                                                                          | 
            }                                                                                                            | 
          }                                                                                                              | 
        }                                                                                                                | 
        if ( v252 )                                                                                                      | 
        {                                                                                                                | 
          ObfDereferenceObjectWithTag(v252, 0x6E457350u);                                                                | 
        }                                                                                                                | 
        v47 = v240;                                                                                                      | 
LABEL_136:                                                                                                               | 
        if ( P != v310 )                                                                                                 | 
        {                                                                                                                | 
          ExFreePoolWithTag(P, 0);                                                                                       | 
        }                                                                                                                | 
        return v47;                                                                                                      | 
      case 0x27u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v85 = *a3;                                                                                                       | 
        v272 = v85;                                                                                                      | 
        if ( v85 > (unsigned int)MmGetDefaultPagePriority(8LL) || v85 < (unsigned int)MmGetMinWsPagePriority() )         | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        v47 = result;                                                                                                    | 
        v240 = result;                                                                                                   | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( !ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                               | 
        {                                                                                                                | 
          goto LABEL_287;                                                                                                | 
        }                                                                                                                | 
        PspLockProcessExclusive(Object, CurrentThread);                                                                  | 
        v123 = v85 << 12;                                                                                                | 
        v124 = (volatile signed __int32 *)Object;                                                                        | 
        v125 = *((_DWORD *)Object + 124);                                                                                | 
        do                                                                                                               | 
        {                                                                                                                | 
          v126 = v125;                                                                                                   | 
          v125 = _InterlockedCompareExchange(v124 + 124, v123 | v125 & 0xFFFF8FFF, v125);                                | 
        }                                                                                                                | 
        while ( v125 != v126 );                                                                                          | 
        v109 = Object;                                                                                                   | 
        v127 = (_QWORD *)*((_QWORD *)Object + 110);                                                                      | 
        if ( v127 != (_QWORD *)((char *)Object + 880) )                                                                  | 
        {                                                                                                                | 
          v128 = v272;                                                                                                   | 
          do                                                                                                             | 
          {                                                                                                              | 
            PsSetPagePriorityThread(v127 - 175, v128);                                                                   | 
            v127 = (_QWORD *)*v127;                                                                                      | 
            v109 = Object;                                                                                               | 
          }                                                                                                              | 
          while ( v127 != (_QWORD *)((char *)Object + 880) );                                                            | 
          v47 = v240;                                                                                                    | 
        }                                                                                                                | 
        goto LABEL_418;                                                                                                  | 
      case 0x28u:                                                                                                        | 
        memset(&ApcState, 0, sizeof(ApcState));                                                                          | 
        if ( ((a4 - 8) & 0xFFFFFFF7) != 0 )                                                                              | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( a4 == 8 )                                                                                                   | 
        {                                                                                                                | 
          v267.m128i_i64[0] = 0LL;                                                                                       | 
          v89 = *(_QWORD *)a3;                                                                                           | 
          v267.m128i_i64[1] = *(_QWORD *)a3;                                                                             | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v267 = *(__m128i *)a3;                                                                                         | 
          v89 = _mm_srli_si128(v267, 8).m128i_u64[0];                                                                    | 
        }                                                                                                                | 
        if ( v267.m128i_i32[1] )                                                                                         | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( v267.m128i_i32[0] )                                                                                         | 
        {                                                                                                                | 
          return -1073741736;                                                                                            | 
        }                                                                                                                | 
        if ( v89 != (__int64)(v89 << 16) >> 16 )                                                                         | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v90 = KeGetCurrentThread()->ApcState.Process;                                                                    | 
        v91 = SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode);                                                    | 
        v92 = Object;                                                                                                    | 
        if ( !v91 && Object != v90 )                                                                                     | 
        {                                                                                                                | 
LABEL_676:                                                                                                               | 
          ObfDereferenceObjectWithTag(v92, 0x79517350u);                                                                 | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        v93 = ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61);                                                | 
        v94 = (struct _EX_RUNDOWN_REF *)Object;                                                                          | 
        if ( !v93 )                                                                                                      | 
        {                                                                                                                | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
          return -1073741558;                                                                                            | 
        }                                                                                                                | 
        v95 = 0;                                                                                                         | 
        if ( *((_QWORD *)Object + 98) )                                                                                  | 
        {                                                                                                                | 
          v141 = *((_WORD *)Object + 886);                                                                               | 
          if ( v141 == 332 || v141 == 452 )                                                                              | 
          {                                                                                                              | 
            v95 = 1;                                                                                                     | 
          }                                                                                                              | 
        }                                                                                                                | 
        ReadyTime = v90[1].ReadyTime;                                                                                    | 
        if ( v95 )                                                                                                       | 
        {                                                                                                                | 
          if ( ReadyTime )                                                                                               | 
          {                                                                                                              | 
            v148 = WORD2(v90[3].PerProcessorCycleTimes);                                                                 | 
            if ( v148 == 332 || v148 == 452 )                                                                            | 
            {                                                                                                            | 
              KeStackAttachProcess((PRKPROCESS)Object, &ApcState);                                                       | 
              v99 = 0;                                                                                                   | 
              if ( v267.m128i_i64[1] < (unsigned __int64)MmGetMaximumUserAddress()                                       | 
                && (unsigned int)MmValidateUserCallTarget(v267.m128i_i64[1], 1LL) )                                      | 
              {                                                                                                          | 
                v172 = (HANDLE *)*((_QWORD *)Object + 98);                                                               | 
                if ( v172 )                                                                                              | 
                {                                                                                                        | 
                  v8 = *v172;                                                                                            | 
                }                                                                                                        | 
                *((_DWORD *)v8 + 292) = v267.m128i_i32[2];                                                               | 
              }                                                                                                          | 
              else                                                                                                       | 
              {                                                                                                          | 
                v99 = -1073741811;                                                                                       | 
              }                                                                                                          | 
              KiUnstackDetachProcess(&ApcState, 0LL);                                                                    | 
              v94 = (struct _EX_RUNDOWN_REF *)Object;                                                                    | 
              goto LABEL_329;                                                                                            | 
            }                                                                                                            | 
          }                                                                                                              | 
        }                                                                                                                | 
        else if ( !ReadyTime || (v142 = WORD2(v90[3].PerProcessorCycleTimes), v142 != 332) && v142 != 452 )              | 
        {                                                                                                                | 
          KeStackAttachProcess((PRKPROCESS)Object, &ApcState);                                                           | 
          v97 = v267.m128i_i64[1];                                                                                       | 
          v98 = MmValidateUserCallTarget(v267.m128i_i64[1], 0LL);                                                        | 
          v99 = -1073741811;                                                                                             | 
          if ( v98 )                                                                                                     | 
          {                                                                                                              | 
            v99 = 0;                                                                                                     | 
          }                                                                                                              | 
          KiUnstackDetachProcess(&ApcState, 0LL);                                                                        | 
          v94 = (struct _EX_RUNDOWN_REF *)Object;                                                                        | 
          if ( v98 )                                                                                                     | 
          {                                                                                                              | 
            PspLockProcessExclusive(Object, CurrentThread);                                                              | 
            *((_QWORD *)Object + 45) = v97;                                                                              | 
            v144 = Object;                                                                                               | 
            v145 = (volatile signed __int32 *)*((_QWORD *)Object + 110);                                                 | 
            if ( v145 != (volatile signed __int32 *)((char *)Object + 880) )                                             | 
            {                                                                                                            | 
              while ( 1 )                                                                                                | 
              {                                                                                                          | 
                if ( v97 )                                                                                               | 
                {                                                                                                        | 
                  _interlockedbittestandset(v145 - 350, 0x19u);                                                          | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  _interlockedbittestandreset(v145 - 350, 0x19u);                                                        | 
                }                                                                                                        | 
                v145 = *(volatile signed __int32 **)v145;                                                                | 
                v144 = Object;                                                                                           | 
                if ( v145 == (volatile signed __int32 *)((char *)Object + 880) )                                         | 
                {                                                                                                        | 
                  break;                                                                                                 | 
                }                                                                                                        | 
                v97 = v267.m128i_i64[1];                                                                                 | 
              }                                                                                                          | 
            }                                                                                                            | 
            PspUnlockProcessExclusive(v144, CurrentThread);                                                              | 
            v94 = (struct _EX_RUNDOWN_REF *)Object;                                                                      | 
          }                                                                                                              | 
          goto LABEL_329;                                                                                                | 
        }                                                                                                                | 
        v99 = -1073741637;                                                                                               | 
LABEL_329:                                                                                                               | 
        ExReleaseRundownProtection_0(v94 + 61);                                                                          | 
        goto LABEL_330;                                                                                                  | 
      case 0x29u:                                                                                                        | 
        v297 = 0LL;                                                                                                      | 
        v298 = 0LL;                                                                                                      | 
        v299 = 0LL;                                                                                                      | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        v23 = 0LL;                                                                                                       | 
        if ( a4 == 40 )                                                                                                  | 
        {                                                                                                                | 
          if ( PreviousMode )                                                                                            | 
          {                                                                                                              | 
            v297 = *(_OWORD *)a3;                                                                                        | 
            v298 = *((_OWORD *)a3 + 1);                                                                                  | 
            v299 = *((_QWORD *)a3 + 4);                                                                                  | 
            v23 = a3 + 8;                                                                                                | 
            v5 = &v297;                                                                                                  | 
          }                                                                                                              | 
          v24 = *(_DWORD *)v5;                                                                                           | 
          if ( *(_DWORD *)v5 > 0x40u || *((_DWORD *)v5 + 1) | *((_DWORD *)v5 + 2) | *((_DWORD *)v5 + 3) )                | 
          {                                                                                                              | 
            return -1073741811;                                                                                          | 
          }                                                                                                              | 
          ++v5;                                                                                                          | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          if ( a4 != 24 )                                                                                                | 
          {                                                                                                              | 
            return -1073741820;                                                                                          | 
          }                                                                                                              | 
          v24 = 0;                                                                                                       | 
          if ( PreviousMode )                                                                                            | 
          {                                                                                                              | 
            v298 = *(_OWORD *)a3;                                                                                        | 
            v23 = a3 + 4;                                                                                                | 
            v5 = &v298;                                                                                                  | 
          }                                                                                                              | 
        }                                                                                                                | 
        if ( !*(_QWORD *)v5 )                                                                                            | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        v305 = *(PVOID *)v5;                                                                                             | 
        *((_QWORD *)v5 + 2) = 0LL;                                                                                       | 
        result = MmAllocateUserStack((int)v5 + 16, *((_QWORD *)v5 + 1), (unsigned int)&v305, v24, 0);                    | 
        v25 = result;                                                                                                    | 
        if ( result >= 0 )                                                                                               | 
        {                                                                                                                | 
          if ( PreviousMode )                                                                                            | 
          {                                                                                                              | 
            *v23 = *((_QWORD *)v5 + 2);                                                                                  | 
          }                                                                                                              | 
          return v25;                                                                                                    | 
        }                                                                                                                | 
        return result;                                                                                                   | 
      case 0x2Du:                                                                                                        | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        LODWORD(Handle) = *a3;                                                                                           | 
        if ( ((unsigned int)Handle & 0xFFFFFFFC) != 0 )                                                                  | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        return PspSetProcessAffinityUpdateMode(CurrentThread, &Handle);                                                  | 
      case 0x2Eu:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v284 = *a3;                                                                                                      | 
        v173 = v284;                                                                                                     | 
        if ( (v284 & 0xFFFFFFFE) != 0 )                                                                                  | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( (v173 & 1) != 0 )                                                                                           | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)Object + 125, 0x200000u);                                            | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          _InterlockedAnd((volatile signed __int32 *)Object + 125, 0xFFDFFFFF);                                          | 
        }                                                                                                                | 
        goto LABEL_24;                                                                                                   | 
      case 0x30u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v174 = *a3;                                                                                                      | 
        v283 = *a3;                                                                                                      | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v175 = (void *)PsReferencePrimaryTokenWithTag(Object, 1953261124LL);                                             | 
        SeSetVirtualizationToken(v175, v174);                                                                            | 
        ObfDereferenceObject(v175);                                                                                      | 
        goto LABEL_24;                                                                                                   | 
      case 0x31u:                                                                                                        | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        v300 = *(struct _LIST_ENTRY **)a3;                                                                               | 
        if ( ((unsigned __int8)v300 & 3) != 1 )                                                                          | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        KeGetCurrentThread()->ApcState.Process[1].ProcessListEntry.Flink = v300;                                         | 
        return 0;                                                                                                        | 
      case 0x34u:                                                                                                        | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v244 = *(_QWORD *)a3;                                                                                            | 
        if ( v7 != (HANDLE)-1LL && (_DWORD)v244 != 2 )                                                                   | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        p_Lock = &KeGetCurrentThread()->ApcState.Process->Header.Lock;                                                   | 
        Object = (PVOID)p_Lock;                                                                                          | 
        if ( (_DWORD)v244 != 8 )                                                                                         | 
        {                                                                                                                | 
          if ( (_DWORD)v244 != 16 )                                                                                      | 
          {                                                                                                              | 
            switch ( (int)v244 )                                                                                         | 
            {                                                                                                            | 
              case 1:                                                                                                    | 
                if ( HIDWORD(v244) >= 0x10 )                                                                             | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x200000000LL) == 0 && (p_Lock[468] & 0x10) != 0                                            | 
                  || (v244 & 0x100000000LL) == 0 && (p_Lock[468] & 0x40) == 0 )                                          | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x800000000LL) != 0 )                                                                       | 
                {                                                                                                        | 
                  goto LABEL_382;                                                                                        | 
                }                                                                                                        | 
                if ( (p_Lock[468] & 8) != 0 )                                                                            | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x800000000LL) != 0 )                                                                       | 
                {                                                                                                        | 
LABEL_382:                                                                                                               | 
                  if ( (v244 & 0x200000000LL) == 0 )                                                                     | 
                  {                                                                                                      | 
                    v57 = -1073741776;                                                                                   | 
                    goto LABEL_259;                                                                                      | 
                  }                                                                                                      | 
                }                                                                                                        | 
                else if ( (v244 & 0x200000000LL) == 0 )                                                                  | 
                {                                                                                                        | 
LABEL_476:                                                                                                               | 
                  if ( (v244 & 0x100000000LL) != 0 )                                                                     | 
                  {                                                                                                      | 
                    _InterlockedAnd(p_Lock + 468, 0xFFFFFFBF);                                                           | 
                    p_Lock = (volatile signed __int32 *)Object;                                                          | 
                  }                                                                                                      | 
                  if ( (v244 & 0x800000000LL) != 0 )                                                                     | 
                  {                                                                                                      | 
                    _InterlockedOr(p_Lock + 468, 8u);                                                                    | 
                    p_Lock = (volatile signed __int32 *)Object;                                                          | 
                  }                                                                                                      | 
                  goto LABEL_480;                                                                                        | 
                }                                                                                                        | 
                _InterlockedOr(p_Lock + 468, 0x10u);                                                                     | 
                p_Lock = (volatile signed __int32 *)Object;                                                              | 
                goto LABEL_476;                                                                                          | 
              case 2:                                                                                                    | 
                v113 = HIDWORD(v244);                                                                                    | 
                if ( (v244 & 0xFFFFFFF000000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) != 0 && (v244 & 0x800000000LL) != 0 )                                        | 
                {                                                                                                        | 
                  v113 = HIDWORD(v244) & 0xFFFFFFF7;                                                                     | 
                  HIDWORD(v244) &= ~8u;                                                                                  | 
                }                                                                                                        | 
                v114 = v113 & 1;                                                                                         | 
                if ( (v113 & 1) == 0 && ((v113 & 2) != 0 || (v113 & 4) != 0) )                                           | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (unsigned int)PsIsSystemWideMitigationOptionSet(9LL) )                                              | 
                {                                                                                                        | 
                  v57 = -1073741637;                                                                                     | 
                  goto LABEL_258;                                                                                        | 
                }                                                                                                        | 
                if ( v242 == (HANDLE)-1LL )                                                                              | 
                {                                                                                                        | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                  v79 = 0;                                                                                               | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  v57 = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);     | 
                  if ( v57 < 0 )                                                                                         | 
                  {                                                                                                      | 
                    goto LABEL_258;                                                                                      | 
                  }                                                                                                      | 
                  v79 = 1;                                                                                               | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                  if ( Object != KeGetCurrentThread()->ApcState.Process )                                                | 
                  {                                                                                                      | 
                    v182 = *((_DWORD *)Object + 468);                                                                    | 
                    if ( (v182 & 0x100) != 0 )                                                                           | 
                    {                                                                                                    | 
                      memset(SubjectContext, 0, 32);                                                                     | 
                      SeCaptureSubjectContextEx(0LL, (PEPROCESS)Object, (PSECURITY_SUBJECT_CONTEXT)SubjectContext);      | 
                      IsSandboxedToken = RtlIsSandboxedToken((PSECURITY_SUBJECT_CONTEXT)SubjectContext);                 | 
                      SeReleaseSubjectContext((PSECURITY_SUBJECT_CONTEXT)SubjectContext);                                | 
                      if ( (unsigned __int8)RtlIsSandboxedToken(0LL)                                                     | 
                        || !IsSandboxedToken                                                                             | 
                        || (p_Lock = (volatile signed __int32 *)Object, (*((_DWORD *)Object + 468) & 0x400) == 0) )      | 
                      {                                                                                                  | 
                        v188 = SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode);                                   | 
                        p_Lock = (volatile signed __int32 *)Object;                                                      | 
                        if ( !v188 )                                                                                     | 
                        {                                                                                                | 
                          v57 = -1073741790;                                                                             | 
                          goto LABEL_260;                                                                                | 
                        }                                                                                                | 
                      }                                                                                                  | 
                      v184 = HIDWORD(v244);                                                                              | 
                    }                                                                                                    | 
                    else                                                                                                 | 
                    {                                                                                                    | 
                      v184 = HIDWORD(v244);                                                                              | 
                      if ( (v244 & 0x800000000LL) == 0 && (v113 & 1) == 0 && (v182 & 0x800) != 0 )                       | 
                      {                                                                                                  | 
                        v57 = -1073741790;                                                                               | 
                        goto LABEL_260;                                                                                  | 
                      }                                                                                                  | 
                    }                                                                                                    | 
                    v185 = 0LL;                                                                                          | 
                    if ( !v114 )                                                                                         | 
                    {                                                                                                    | 
                      v185 = 2304LL;                                                                                     | 
                    }                                                                                                    | 
                    v186 = -v114;                                                                                        | 
                    v187 = (v186 != 0 ? 0x100 : 0) | 0x800;                                                              | 
                    if ( (v184 & 8) == 0 )                                                                               | 
                    {                                                                                                    | 
                      v187 = v186 != 0 ? 0x900 : 0;                                                                      | 
                    }                                                                                                    | 
                    if ( (v184 & 2) != 0 )                                                                               | 
                    {                                                                                                    | 
                      v187 |= 0x200u;                                                                                    | 
                    }                                                                                                    | 
                    else                                                                                                 | 
                    {                                                                                                    | 
                      LODWORD(v185) = v185 | 0x200;                                                                      | 
                    }                                                                                                    | 
                    v189 = v184 >> 2;                                                                                    | 
                    v190 = v187;                                                                                         | 
                    LODWORD(v190) = v187 | 0x400;                                                                        | 
                    if ( (v189 & 1) == 0 )                                                                               | 
                    {                                                                                                    | 
                      v190 = v187;                                                                                       | 
                    }                                                                                                    | 
                    v191 = v185 | 0x400;                                                                                 | 
                    v192 = (v189 & 1) == 0;                                                                              | 
                    goto LABEL_852;                                                                                      | 
                  }                                                                                                      | 
                  v113 = HIDWORD(v244);                                                                                  | 
                }                                                                                                        | 
                if ( !v114 && (p_Lock[468] & 0x100) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_260;                                                                                        | 
                }                                                                                                        | 
                v151 = (v113 >> 3) & 1;                                                                                  | 
                if ( !v151 && !v114 && (p_Lock[468] & 0x800) != 0 )                                                      | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_260;                                                                                        | 
                }                                                                                                        | 
                v193 = (v113 >> 1) & 1;                                                                                  | 
                if ( v193 && (p_Lock[468] & 0x300) == 0x100 )                                                            | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_260;                                                                                        | 
                }                                                                                                        | 
                v194 = (v113 >> 2) & 1;                                                                                  | 
                if ( v194 && (p_Lock[468] & 0x500) == 0x100 )                                                            | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_260;                                                                                        | 
                }                                                                                                        | 
                if ( v114 )                                                                                              | 
                {                                                                                                        | 
                  v195 = 2304;                                                                                           | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  v195 = 0;                                                                                              | 
                  if ( v151 )                                                                                            | 
                  {                                                                                                      | 
                    v195 = 2048;                                                                                         | 
                  }                                                                                                      | 
                }                                                                                                        | 
                v185 = (v193 ^ 1u) << 9;                                                                                 | 
                v190 = v195;                                                                                             | 
                LODWORD(v190) = v195 | 0x200;                                                                            | 
                if ( !v193 )                                                                                             | 
                {                                                                                                        | 
                  v190 = v195;                                                                                           | 
                }                                                                                                        | 
                if ( v194 )                                                                                              | 
                {                                                                                                        | 
                  LODWORD(v190) = v190 | 0x400;                                                                          | 
                }                                                                                                        | 
                v191 = v185 | 0x400;                                                                                     | 
                v192 = v194 == 0;                                                                                        | 
LABEL_852:                                                                                                               | 
                if ( v192 )                                                                                              | 
                {                                                                                                        | 
                  v185 = v191;                                                                                           | 
                }                                                                                                        | 
                RtlInterlockedSetClearBits(p_Lock + 468, v190, v185);                                                    | 
                v57 = 0;                                                                                                 | 
                p_Lock = (volatile signed __int32 *)Object;                                                              | 
                goto LABEL_260;                                                                                          | 
              case 3:                                                                                                    | 
                if ( HIDWORD(v244) >= 4 )                                                                                | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x200000000LL) != 0 )                                                                       | 
                {                                                                                                        | 
                  goto LABEL_298;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) != 0 )                                                                       | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x200000000LL) != 0 )                                                                       | 
                {                                                                                                        | 
LABEL_298:                                                                                                               | 
                  if ( (v244 & 0x100000000LL) == 0 )                                                                     | 
                  {                                                                                                      | 
                    v57 = -1073741811;                                                                                   | 
                    goto LABEL_259;                                                                                      | 
                  }                                                                                                      | 
                }                                                                                                        | 
                v149 = ObReferenceProcessHandleTable(p_Lock);                                                            | 
                if ( v149 )                                                                                              | 
                {                                                                                                        | 
                  v176 = ExEnableHandleExceptions(v149, BYTE4(v244) & 1);                                                | 
                  ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)Object + 61);                                            | 
                  if ( !v176 )                                                                                           | 
                  {                                                                                                      | 
                    LODWORD(v8) = -1073741790;                                                                           | 
                  }                                                                                                      | 
                  v57 = (int)v8;                                                                                         | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  v57 = -1073741558;                                                                                     | 
                }                                                                                                        | 
                goto LABEL_258;                                                                                          | 
              case 4:                                                                                                    | 
                v150 = HIDWORD(v244);                                                                                    | 
                if ( (v244 & 0xFFFFFFF000000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) != 0 && (v244 & 0x200000000LL) != 0 )                                        | 
                {                                                                                                        | 
                  v150 = HIDWORD(v244) & 0xFFFFFFFD;                                                                     | 
                  HIDWORD(v244) &= ~2u;                                                                                  | 
                }                                                                                                        | 
                if ( (v150 & 4) != 0 && (v150 & 8) != 0 )                                                                | 
                {                                                                                                        | 
                  v150 &= ~8u;                                                                                           | 
                  HIDWORD(v244) = v150;                                                                                  | 
                }                                                                                                        | 
                v177 = v150 & 1;                                                                                         | 
                if ( (v150 & 1) == 0 && (p_Lock[468] & 0x1000) != 0 )                                                    | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v178 = (v150 >> 2) & 1;                                                                                  | 
                if ( !v178 && (p_Lock[502] & 2) != 0 )                                                                   | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v179 = (v150 >> 1) & 1;                                                                                  | 
                if ( !v179 && (v150 & 1) == 0 && (p_Lock[468] & 0x2000) != 0 )                                           | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v180 = (v150 >> 3) & 1;                                                                                  | 
                if ( !v180 && !v178 && (p_Lock[502] & 4) != 0 )                                                          | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( v177 || v179 )                                                                                      | 
                {                                                                                                        | 
                  IsGuiThread = PsIsGuiThread(CurrentThread);                                                            | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                  if ( IsGuiThread )                                                                                     | 
                  {                                                                                                      | 
                    v57 = -1073741431;                                                                                   | 
                    goto LABEL_259;                                                                                      | 
                  }                                                                                                      | 
                }                                                                                                        | 
                v57 = 0;                                                                                                 | 
                if ( v177 )                                                                                              | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 468, 0x3000u);                                                                 | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  if ( !v179 )                                                                                           | 
                  {                                                                                                      | 
                    goto LABEL_808;                                                                                      | 
                  }                                                                                                      | 
                  _InterlockedOr(p_Lock + 468, 0x2000u);                                                                 | 
                }                                                                                                        | 
                p_Lock = (volatile signed __int32 *)Object;                                                              | 
LABEL_808:                                                                                                               | 
                if ( v178 )                                                                                              | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 502, 6u);                                                                      | 
                  goto LABEL_258;                                                                                        | 
                }                                                                                                        | 
                if ( !v180 )                                                                                             | 
                {                                                                                                        | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                _InterlockedOr(p_Lock + 502, 4u);                                                                        | 
                goto LABEL_258;                                                                                          | 
              case 6:                                                                                                    | 
                if ( (v244 & 0xFFFFFFFE00000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) == 0 && (*((_DWORD *)p_Lock + 468) & 0x80u) != 0 )                           | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v57 = 0;                                                                                                 | 
                if ( (v244 & 0x100000000LL) == 0 )                                                                       | 
                {                                                                                                        | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                _InterlockedOr(p_Lock + 468, 0x80u);                                                                     | 
                goto LABEL_258;                                                                                          | 
              case 7:                                                                                                    | 
                if ( (v244 & 0x800000000LL) != 0 || (v244 & 0x1000000000LL) != 0 )                                       | 
                {                                                                                                        | 
                  goto LABEL_617;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0xFFFFFFE000000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (p_Lock[468] & 1) == 0 )                                                                            | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x400000000LL) == 0 )                                                                       | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                _InterlockedOr(p_Lock + 468, 4u);                                                                        | 
                v57 = 0;                                                                                                 | 
                goto LABEL_258;                                                                                          | 
              case 9:                                                                                                    | 
                if ( (v244 & 0xFFFFFFFC00000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) == 0 && (p_Lock[468] & 0x10000) != 0 )                                       | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) == 0 && (v244 & 0x200000000LL) == 0 && (p_Lock[468] & 0x20000) != 0 )        | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v57 = 0;                                                                                                 | 
                if ( (v244 & 0x100000000LL) != 0 )                                                                       | 
                {                                                                                                        | 
                  v78 = 0x10000LL;                                                                                       | 
                  v77 = 0x20000LL;                                                                                       | 
                  goto LABEL_257;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x200000000LL) == 0 )                                                                       | 
                {                                                                                                        | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v78 = 0x20000LL;                                                                                         | 
                v77 = 0x10000LL;                                                                                         | 
                goto LABEL_257;                                                                                          | 
              case 10:                                                                                                   | 
                v112 = HIDWORD(v244);                                                                                    | 
                if ( (v244 & 0xFFFFFFE000000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) != 0 && (v244 & 0x800000000LL) != 0 )                                        | 
                {                                                                                                        | 
                  v112 = HIDWORD(v244) & 0xFFFFFFF7;                                                                     | 
                }                                                                                                        | 
                if ( (v112 & 2) != 0 && (v112 & 0x10) != 0 )                                                             | 
                {                                                                                                        | 
                  v112 &= ~0x10u;                                                                                        | 
                }                                                                                                        | 
                v129 = v112 & 1;                                                                                         | 
                v130 = 0x80000;                                                                                          | 
                if ( (v112 & 1) == 0 && (p_Lock[468] & 0x80000) != 0 )                                                   | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                v131 = (v112 >> 1) & 1;                                                                                  | 
                if ( !v131 && (p_Lock[468] & 0x200000) != 0 )                                                            | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                v132 = (v112 >> 2) & 1;                                                                                  | 
                if ( !v132 && (p_Lock[468] & 0x40000) != 0 )                                                             | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v133 = (v112 >> 3) & 1;                                                                                  | 
                if ( !v133 && (v112 & 1) == 0 && (p_Lock[468] & 0x100000) != 0 )                                         | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                v134 = (v112 >> 4) & 1;                                                                                  | 
                if ( !v134 && !v131 && (p_Lock[468] & 0x400000) != 0 )                                                   | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v135 = 0;                                                                                                | 
                if ( v129 )                                                                                              | 
                {                                                                                                        | 
                  v135 = 0x100000;                                                                                       | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  v130 = 0;                                                                                              | 
                  if ( v133 )                                                                                            | 
                  {                                                                                                      | 
                    v130 = 0x100000;                                                                                     | 
                  }                                                                                                      | 
                }                                                                                                        | 
                if ( v131 )                                                                                              | 
                {                                                                                                        | 
                  v130 |= 0x200000u;                                                                                     | 
                  v135 |= 0x400000u;                                                                                     | 
                }                                                                                                        | 
                else if ( v134 )                                                                                         | 
                {                                                                                                        | 
                  v130 |= 0x400000u;                                                                                     | 
                }                                                                                                        | 
                v136 = v130;                                                                                             | 
                LODWORD(v136) = v130 | 0x40000;                                                                          | 
                if ( !v132 )                                                                                             | 
                {                                                                                                        | 
                  v136 = v130;                                                                                           | 
                }                                                                                                        | 
                RtlInterlockedSetClearBits(p_Lock + 468, v136, v135);                                                    | 
                v57 = 0;                                                                                                 | 
                goto LABEL_258;                                                                                          | 
              case 13:                                                                                                   | 
                v115 = HIDWORD(v244);                                                                                    | 
                if ( (v244 & 0xFFFFFFF800000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) == 0 && (v244 & 0x400000000LL) != 0 )                                        | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) != 0 && (v244 & 0x200000000LL) != 0 )                                        | 
                {                                                                                                        | 
                  v115 = HIDWORD(v244) & 0xFFFFFFFD;                                                                     | 
                }                                                                                                        | 
                NoChildProcessRestrictedPolicy = PspGetNoChildProcessRestrictedPolicy(p_Lock);                           | 
                v117 = NoChildProcessRestrictedPolicy;                                                                   | 
                v118 = v115 & 1;                                                                                         | 
                if ( (v115 & 1) == 0 && NoChildProcessRestrictedPolicy - 1 <= 1 )                                        | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_258;                                                                                        | 
                }                                                                                                        | 
                v119 = (v115 >> 2) & 1;                                                                                  | 
                if ( v119 && (_DWORD)v117 == 1 )                                                                         | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_258;                                                                                        | 
                }                                                                                                        | 
                v120 = (v115 >> 1) & 1;                                                                                  | 
                if ( v120 )                                                                                              | 
                {                                                                                                        | 
                  goto LABEL_399;                                                                                        | 
                }                                                                                                        | 
                if ( v118 )                                                                                              | 
                {                                                                                                        | 
                  goto LABEL_400;                                                                                        | 
                }                                                                                                        | 
                if ( (_DWORD)v117 == 3 )                                                                                 | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_258;                                                                                        | 
                }                                                                                                        | 
LABEL_399:                                                                                                               | 
                if ( v118 )                                                                                              | 
                {                                                                                                        | 
LABEL_400:                                                                                                               | 
                  v121 = Object;                                                                                         | 
                  if ( !v119 )                                                                                           | 
                  {                                                                                                      | 
                    PspSetNoChildProcessRestrictedPolicy(Object, 1LL, v117);                                             | 
                    v57 = 0;                                                                                             | 
                    goto LABEL_258;                                                                                      | 
                  }                                                                                                      | 
                  v139 = 2LL;                                                                                            | 
                  goto LABEL_445;                                                                                        | 
                }                                                                                                        | 
                if ( v120 )                                                                                              | 
                {                                                                                                        | 
                  v139 = 3LL;                                                                                            | 
                  v121 = Object;                                                                                         | 
LABEL_445:                                                                                                               | 
                  PspSetNoChildProcessRestrictedPolicy(v121, v139, v117);                                                | 
                  goto LABEL_446;                                                                                        | 
                }                                                                                                        | 
                break;                                                                                                   | 
              case 14:                                                                                                   | 
                if ( (v244 & 0xFFFFFFE000000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) == 0 && (p_Lock[468] & 0x40000000) != 0                                      | 
                  || ((HIDWORD(v244) >> 1) & 1) == 0 && *((int *)p_Lock + 468) < 0                                       | 
                  || ((HIDWORD(v244) >> 3) & 1) == 0 && (p_Lock[469] & 0x2000) != 0                                      | 
                  || ((HIDWORD(v244) >> 2) & 1) == 0 && (p_Lock[469] & 0x1000) != 0 )                                    | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                v196 = (HIDWORD(v244) >> 4) & 1;                                                                         | 
                if ( !v196 && (p_Lock[502] & 1) != 0 )                                                                   | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( v196 )                                                                                              | 
                {                                                                                                        | 
                  IsCoreIsolationMitigationPolicyEnforceable = KeIsCoreIsolationMitigationPolicyEnforceable(8LL);        | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                  if ( !IsCoreIsolationMitigationPolicyEnforceable )                                                     | 
                  {                                                                                                      | 
LABEL_617:                                                                                                               | 
                    v57 = -1073741637;                                                                                   | 
                    goto LABEL_259;                                                                                      | 
                  }                                                                                                      | 
                }                                                                                                        | 
                if ( (v244 & 0x100000000LL) != 0 )                                                                       | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 468, 0x40000000u);                                                             | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                if ( ((HIDWORD(v244) >> 1) & 1) != 0 )                                                                   | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 468, 0x80000000);                                                              | 
                  _InterlockedOr((volatile signed __int32 *)Object + 383, 0x200000u);                                    | 
                  PspWriteProcessSecurityDomain(Object, _InterlockedIncrement64(&PsNextSecurityDomain), 1LL);            | 
                  KeSynchronizeSecurityDomain(Object);                                                                   | 
                  PspSynchronizeThreadIsolationDomains(Object);                                                          | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                if ( ((HIDWORD(v244) >> 2) & 1) != 0 )                                                                   | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 469, 0x1000u);                                                                 | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                if ( ((HIDWORD(v244) >> 3) & 1) != 0 )                                                                   | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 469, 0x2000u);                                                                 | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                if ( !v196 )                                                                                             | 
                {                                                                                                        | 
LABEL_480:                                                                                                               | 
                  v57 = 0;                                                                                               | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                PspApplyCoreIsolationPolicy(p_Lock);                                                                     | 
                v57 = 0;                                                                                                 | 
                goto LABEL_258;                                                                                          | 
              case 15:                                                                                                   | 
                v152 = HIDWORD(v244);                                                                                    | 
                if ( (v244 & 0xFFFFFC0000000000uLL) != 0 )                                                               | 
                {                                                                                                        | 
                  v57 = -1073741811;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( (v244 & 0x1000000000LL) != 0 )                                                                      | 
                {                                                                                                        | 
                  v152 = HIDWORD(v244) | 1;                                                                              | 
                  HIDWORD(v244) |= 1u;                                                                                   | 
                }                                                                                                        | 
                if ( (v152 & 0x200) != 0 )                                                                               | 
                {                                                                                                        | 
                  v152 |= 4u;                                                                                            | 
                  HIDWORD(v244) = v152;                                                                                  | 
                }                                                                                                        | 
                if ( (v152 & 0x40) != 0 )                                                                                | 
                {                                                                                                        | 
                  v152 |= 0x20u;                                                                                         | 
                  HIDWORD(v244) = v152;                                                                                  | 
                }                                                                                                        | 
                v198 = (v152 >> 4) & 1;                                                                                  | 
                if ( !v198 && (p_Lock[469] & 0x100000) != 0 || (v152 & 1) == 0 && (p_Lock[469] & 0x4000) != 0 )          | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                if ( (v152 & 1) != 0 && (p_Lock[469] & 0x4000) == 0 )                                                    | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v199 = (v152 >> 9) & 1;                                                                                  | 
                if ( v199 && *((int *)p_Lock + 469) >= 0 )                                                               | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                v200 = (v152 >> 2) & 1;                                                                                  | 
                if ( !v200 && (p_Lock[469] & 0x20000) != 0 )                                                             | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                if ( v200 && (p_Lock[469] & 0x20000) == 0 )                                                              | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v201 = (v152 >> 6) & 1;                                                                                  | 
                if ( !v201 && (p_Lock[469] & 0x400000) != 0 )                                                            | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                v202 = (v152 >> 5) & 1;                                                                                  | 
                if ( !v202 && (p_Lock[469] & 0x200000) != 0 )                                                            | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v203 = (v152 >> 8) & 1;                                                                                  | 
                if ( !v203 && (p_Lock[469] & 0x40000000) != 0 )                                                          | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                v204 = (v152 >> 1) & 1;                                                                                  | 
                if ( v204 && (p_Lock[469] & 0x8000) == 0 )                                                               | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                if ( ((v152 >> 3) & 1) == 0 && (p_Lock[469] & 0x40000) != 0 )                                            | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                if ( ((v152 >> 3) & 1) != 0 && (p_Lock[469] & 0x40000) == 0 )                                            | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                v205 = (v152 >> 7) & 1;                                                                                  | 
                if ( !v205 && (p_Lock[469] & 0x800000) != 0 )                                                            | 
                {                                                                                                        | 
                  goto LABEL_773;                                                                                        | 
                }                                                                                                        | 
                if ( v205 && (p_Lock[469] & 0x800000) == 0 )                                                             | 
                {                                                                                                        | 
                  v57 = -1073741790;                                                                                     | 
                  goto LABEL_259;                                                                                        | 
                }                                                                                                        | 
                if ( v198 )                                                                                              | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 469, 0x100000u);                                                               | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                if ( !v199 && v200 )                                                                                     | 
                {                                                                                                        | 
                  _InterlockedAnd(p_Lock + 469, 0x7FFFFFFFu);                                                            | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                if ( v201 )                                                                                              | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 469, 0x200000u);                                                               | 
                  _InterlockedOr((volatile signed __int32 *)Object + 469, 0x400000u);                                    | 
                }                                                                                                        | 
                else                                                                                                     | 
                {                                                                                                        | 
                  if ( !v202 )                                                                                           | 
                  {                                                                                                      | 
                    goto LABEL_940;                                                                                      | 
                  }                                                                                                      | 
                  _InterlockedOr(p_Lock + 469, 0x200000u);                                                               | 
                }                                                                                                        | 
                p_Lock = (volatile signed __int32 *)Object;                                                              | 
LABEL_940:                                                                                                               | 
                if ( v203 )                                                                                              | 
                {                                                                                                        | 
                  _InterlockedOr(p_Lock + 469, 0x40000000u);                                                             | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                if ( !v204 )                                                                                             | 
                {                                                                                                        | 
                  _InterlockedAnd(p_Lock + 469, 0xFFFF7FFF);                                                             | 
                  p_Lock = (volatile signed __int32 *)Object;                                                            | 
                }                                                                                                        | 
                v57 = 0;                                                                                                 | 
                goto LABEL_259;                                                                                          | 
              default:                                                                                                   | 
                goto LABEL_617;                                                                                          | 
            }                                                                                                            | 
            goto LABEL_446;                                                                                              | 
          }                                                                                                              | 
          v80 = HIDWORD(v244);                                                                                           | 
          if ( (v244 & 0xFFFFFFFC00000000uLL) == 0 )                                                                     | 
          {                                                                                                              | 
            if ( (v244 & 0x100000000LL) != 0 && (v244 & 0x200000000LL) != 0 )                                            | 
            {                                                                                                            | 
              v80 = HIDWORD(v244) & 0xFFFFFFFD;                                                                          | 
            }                                                                                                            | 
            RedirectionTrustPolicy = PspGetRedirectionTrustPolicy(p_Lock);                                               | 
            v82 = v80 & 1;                                                                                               | 
            if ( (v80 & 1) == 0 && RedirectionTrustPolicy == 1 )                                                         | 
            {                                                                                                            | 
              v57 = -1073741790;                                                                                         | 
LABEL_258:                                                                                                               | 
              p_Lock = (volatile signed __int32 *)Object;                                                                | 
              goto LABEL_259;                                                                                            | 
            }                                                                                                            | 
            v103 = (v80 >> 1) & 1;                                                                                       | 
            if ( !v103 )                                                                                                 | 
            {                                                                                                            | 
              if ( v82 )                                                                                                 | 
              {                                                                                                          | 
                goto LABEL_350;                                                                                          | 
              }                                                                                                          | 
              if ( RedirectionTrustPolicy == 2 )                                                                         | 
              {                                                                                                          | 
                v57 = -1073741790;                                                                                       | 
                goto LABEL_258;                                                                                          | 
              }                                                                                                          | 
            }                                                                                                            | 
            if ( !v82 )                                                                                                  | 
            {                                                                                                            | 
              if ( !v103 )                                                                                               | 
              {                                                                                                          | 
LABEL_446:                                                                                                               | 
                v57 = 0;                                                                                                 | 
                goto LABEL_258;                                                                                          | 
              }                                                                                                          | 
              v104 = 2LL;                                                                                                | 
              goto LABEL_351;                                                                                            | 
            }                                                                                                            | 
LABEL_350:                                                                                                               | 
            v104 = 1LL;                                                                                                  | 
LABEL_351:                                                                                                               | 
            PspSetRedirectionTrustPolicy(Object, v104);                                                                  | 
            v57 = 0;                                                                                                     | 
            goto LABEL_258;                                                                                              | 
          }                                                                                                              | 
          v57 = -1073741811;                                                                                             | 
LABEL_259:                                                                                                               | 
          v79 = 0;                                                                                                       | 
LABEL_260:                                                                                                               | 
          if ( v79 == 1 )                                                                                                | 
          {                                                                                                              | 
            ObfDereferenceObjectWithTag((PVOID)p_Lock, 0x79517350u);                                                     | 
          }                                                                                                              | 
          return v57;                                                                                                    | 
        }                                                                                                                | 
        v73 = HIDWORD(v244);                                                                                             | 
        if ( (v244 & 0xFFFFFFE000000000uLL) != 0 )                                                                       | 
        {                                                                                                                | 
          v57 = -1073741811;                                                                                             | 
          goto LABEL_259;                                                                                                | 
        }                                                                                                                | 
        if ( (v244 & 0x100000000LL) != 0 && ((v244 & 0x800000000LL) != 0 || (v244 & 0x1000000000LL) != 0) )              | 
        {                                                                                                                | 
          v73 = HIDWORD(v244) & 0xFFFFFFE7;                                                                              | 
        }                                                                                                                | 
        if ( (v73 & 2) != 0 && (v73 & 0x10) != 0 )                                                                       | 
        {                                                                                                                | 
          v73 &= ~0x10u;                                                                                                 | 
        }                                                                                                                | 
        v74 = (v73 >> 3) & 1;                                                                                            | 
        if ( v74 && (v73 & 0x10) != 0 )                                                                                  | 
        {                                                                                                                | 
          v57 = -1073741811;                                                                                             | 
          goto LABEL_259;                                                                                                | 
        }                                                                                                                | 
        v75 = (v73 >> 1) & 1;                                                                                            | 
        if ( v75 + (v73 & 1) > 1 )                                                                                       | 
        {                                                                                                                | 
          v57 = -1073741811;                                                                                             | 
          goto LABEL_259;                                                                                                | 
        }                                                                                                                | 
        if ( (v73 & 1) != 0 )                                                                                            | 
        {                                                                                                                | 
          goto LABEL_243;                                                                                                | 
        }                                                                                                                | 
        if ( *((_BYTE *)p_Lock + 1528) == 8 )                                                                            | 
        {                                                                                                                | 
          goto LABEL_946;                                                                                                | 
        }                                                                                                                | 
        LOBYTE(v11) = 8;                                                                                                 | 
        if ( (unsigned int)SeCompareSigningLevels(v11) )                                                                 | 
        {                                                                                                                | 
          goto LABEL_948;                                                                                                | 
        }                                                                                                                | 
        p_Lock = (volatile signed __int32 *)Object;                                                                      | 
LABEL_946:                                                                                                               | 
        if ( *((_BYTE *)p_Lock + 1529) == 8 )                                                                            | 
        {                                                                                                                | 
          goto LABEL_773;                                                                                                | 
        }                                                                                                                | 
        LOBYTE(v11) = 8;                                                                                                 | 
        if ( !(unsigned int)SeCompareSigningLevels(v11) )                                                                | 
        {                                                                                                                | 
          p_Lock = (volatile signed __int32 *)Object;                                                                    | 
LABEL_773:                                                                                                               | 
          v57 = -1073741790;                                                                                             | 
          goto LABEL_259;                                                                                                | 
        }                                                                                                                | 
LABEL_948:                                                                                                               | 
        p_Lock = (volatile signed __int32 *)Object;                                                                      | 
        if ( !v75 )                                                                                                      | 
        {                                                                                                                | 
          if ( (unsigned int)SeCompareSigningLevels(*((unsigned __int8 *)Object + 1528))                                 | 
            && (unsigned int)SeCompareSigningLevels(*((unsigned __int8 *)Object + 1529)) )                               | 
          {                                                                                                              | 
            v57 = -1073741790;                                                                                           | 
            goto LABEL_258;                                                                                              | 
          }                                                                                                              | 
          p_Lock = (volatile signed __int32 *)Object;                                                                    | 
        }                                                                                                                | 
LABEL_243:                                                                                                               | 
        if ( (p_Lock[468] & 0x3000000) != 0 && (v73 & 0x10) == 0 && !v75 && !v74 && (v73 & 1) == 0 )                     | 
        {                                                                                                                | 
          v57 = -1073741790;                                                                                             | 
          goto LABEL_259;                                                                                                | 
        }                                                                                                                | 
        v57 = 0;                                                                                                         | 
        if ( (v73 & 1) != 0 )                                                                                            | 
        {                                                                                                                | 
          LOBYTE(v11) = 8;                                                                                               | 
          if ( (unsigned int)SeCompareSigningLevels(v11) )                                                               | 
          {                                                                                                              | 
            *((_BYTE *)Object + 1528) = 8;                                                                               | 
          }                                                                                                              | 
          LOBYTE(v76) = 8;                                                                                               | 
          if ( (unsigned int)SeCompareSigningLevels(v76) )                                                               | 
          {                                                                                                              | 
            *((_BYTE *)Object + 1529) = 8;                                                                               | 
          }                                                                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          if ( !v75 )                                                                                                    | 
          {                                                                                                              | 
            goto LABEL_250;                                                                                              | 
          }                                                                                                              | 
          if ( !(unsigned int)SeCompareSigningLevels(*((unsigned __int8 *)p_Lock + 1528)) )                              | 
          {                                                                                                              | 
            v57 = -1073741790;                                                                                           | 
            goto LABEL_258;                                                                                              | 
          }                                                                                                              | 
          LOBYTE(v143) = 6;                                                                                              | 
          if ( (unsigned int)SeCompareSigningLevels(v143) )                                                              | 
          {                                                                                                              | 
            *((_BYTE *)Object + 1529) = 6;                                                                               | 
          }                                                                                                              | 
        }                                                                                                                | 
        p_Lock = (volatile signed __int32 *)Object;                                                                      | 
LABEL_250:                                                                                                               | 
        if ( v73 )                                                                                                       | 
        {                                                                                                                | 
          LODWORD(v8) = 0x800000;                                                                                        | 
        }                                                                                                                | 
        v77 = (unsigned __int8)((v73 & 8) == 0) << 24;                                                                   | 
        v78 = (unsigned int)v8;                                                                                          | 
        LODWORD(v78) = (unsigned int)v8 | 0x1000000;                                                                     | 
        if ( !v74 )                                                                                                      | 
        {                                                                                                                | 
          v78 = (unsigned int)v8;                                                                                        | 
        }                                                                                                                | 
        if ( ((v73 >> 4) & 1) != 0 )                                                                                     | 
        {                                                                                                                | 
          LODWORD(v78) = v78 | 0x2000000;                                                                                | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v77 = (unsigned int)v77 | 0x2000000;                                                                           | 
        }                                                                                                                | 
LABEL_257:                                                                                                               | 
        RtlInterlockedSetClearBits(p_Lock + 468, v78, v77);                                                              | 
        goto LABEL_258;                                                                                                  | 
      case 0x35u:                                                                                                        | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( a4 != 16 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        *(_OWORD *)SubjectContext = *(_OWORD *)a3;                                                                       | 
        if ( SubjectContext[8] )                                                                                         | 
        {                                                                                                                | 
          return RtlRemoveDynamicUserFunctionTable(*(_QWORD *)SubjectContext);                                           | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          return RtlInsertDynamicUserFunctionTable(*(_QWORD *)SubjectContext);                                           | 
        }                                                                                                                | 
      case 0x36u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v86 = *a3;                                                                                                       | 
        v285 = *a3;                                                                                                      | 
        if ( (v86 & 0xFFFFFFFE) != 0 )                                                                                   | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v87 = ObReferenceProcessHandleTable(Object);                                                                     | 
        if ( v87 )                                                                                                       | 
        {                                                                                                                | 
          ExEnableHandleExceptions(v87, v86 & 1);                                                                        | 
LABEL_206:                                                                                                               | 
          ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)Object + 61);                                                    | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
LABEL_405:                                                                                                               | 
          LODWORD(v8) = -1073741558;                                                                                     | 
        }                                                                                                                | 
LABEL_207:                                                                                                               | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                | 
        return (int)v8;                                                                                                  | 
      case 0x38u:                                                                                                        | 
        *(_OWORD *)SubjectContext = 0LL;                                                                                 | 
        v206 = 0LL;                                                                                                      | 
        v275 = 0LL;                                                                                                      | 
        if ( PreviousMode != 1 )                                                                                         | 
        {                                                                                                                | 
          goto LABEL_963;                                                                                                | 
        }                                                                                                                | 
        v207 = 0x7FFFFFFF0000LL;                                                                                         | 
        if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )                                                                   | 
        {                                                                                                                | 
          v207 = (__int64)a3;                                                                                            | 
        }                                                                                                                | 
        v208 = *(_DWORD *)v207;                                                                                          | 
        *(_DWORD *)SubjectContext = v208;                                                                                | 
        v209 = *(_QWORD *)(v207 + 8);                                                                                    | 
        *(_QWORD *)&SubjectContext[8] = v209;                                                                            | 
        if ( !(_WORD)v208 )                                                                                              | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( (v209 & 1) != 0 )                                                                                           | 
        {                                                                                                                | 
          ExRaiseDatatypeMisalignment();                                                                                 | 
        }                                                                                                                | 
        v210 = (void *)ExAllocatePool2(0x40uLL, (unsigned __int16)v208, 0x6E497350u);                                    | 
        v206 = v210;                                                                                                     | 
        v275 = v210;                                                                                                     | 
        if ( !v210 )                                                                                                     | 
        {                                                                                                                | 
          return -1073741670;                                                                                            | 
        }                                                                                                                | 
        memmove(v210, *(const void **)&SubjectContext[8], *(unsigned __int16 *)SubjectContext);                          | 
        *(_QWORD *)&SubjectContext[8] = v206;                                                                            | 
        v5 = (__int128 *)SubjectContext;                                                                                 | 
        v301 = SubjectContext;                                                                                           | 
        v7 = v242;                                                                                                       | 
LABEL_963:                                                                                                               | 
        v71 = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);                 | 
        if ( v71 < 0 )                                                                                                   | 
        {                                                                                                                | 
          if ( v206 )                                                                                                    | 
          {                                                                                                              | 
            ExFreePoolWithTag(v206, 0);                                                                                  | 
          }                                                                                                              | 
          return v71;                                                                                                    | 
        }                                                                                                                | 
        v99 = IoRevokeHandlesForProcess(v5, Object);                                                                     | 
        if ( v206 )                                                                                                      | 
        {                                                                                                                | 
          ExFreePoolWithTag(v206, 0);                                                                                    | 
        }                                                                                                                | 
LABEL_330:                                                                                                               | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                | 
        return v99;                                                                                                      | 
      case 0x39u:                                                                                                        | 
        return MmProcessWorkingSetControl((ULONG_PTR)v7);                                                                | 
      case 0x3Bu:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result >= 0 )                                                                                               | 
        {                                                                                                                | 
          v273 = *(_DWORD *)v5 != 0;                                                                                     | 
          if ( KeGetCurrentThread()->ApcState.Process == Object )                                                        | 
          {                                                                                                              | 
            v240 = -1073741790;                                                                                          | 
          }                                                                                                              | 
          else if ( SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode) )                                             | 
          {                                                                                                              | 
            v240 = 0;                                                                                                    | 
            v211 = v273;                                                                                                 | 
            KeSetCheckStackExtentsProcess(Object, v273);                                                                 | 
            if ( !v211 && (*((_DWORD *)Object + 124) & 0x20000) != 0 )                                                   | 
            {                                                                                                            | 
              _InterlockedAnd((volatile signed __int32 *)Object + 124, 0xFFFDFFFF);                                      | 
            }                                                                                                            | 
          }                                                                                                              | 
          else                                                                                                           | 
          {                                                                                                              | 
            v240 = -1073741727;                                                                                          | 
          }                                                                                                              | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
          return v240;                                                                                                   | 
        }                                                                                                                | 
        return result;                                                                                                   | 
      case 0x3Eu:                                                                                                        | 
        if ( a4 != 16 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v308 = *(_OWORD *)a3;                                                                                            | 
        if ( (_WORD)v308 != 1 || DWORD1(v308) )                                                                          | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( *((_QWORD *)&v308 + 1) )                                                                                    | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)Object + 125, 0x100u);                                               | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          _InterlockedAnd((volatile signed __int32 *)Object + 125, 0xFFFFFEFF);                                          | 
        }                                                                                                                | 
        goto LABEL_24;                                                                                                   | 
      case 0x3Fu:                                                                                                        | 
        v305 = 0LL;                                                                                                      | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v305 = *(PVOID *)v5;                                                                                             | 
        v47 = PsSetProcessFaultInformation((PRKPROCESS)Object);                                                          | 
        goto LABEL_157;                                                                                                  | 
      case 0x41u:                                                                                                        | 
        if ( a4 != 32 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        *(_OWORD *)SubjectContext = *v5;                                                                                 | 
        *(_OWORD *)&SubjectContext[16] = v5[1];                                                                          | 
        if ( *(_DWORD *)SubjectContext != 3 )                                                                            | 
        {                                                                                                                | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
          return -1073741735;                                                                                            | 
        }                                                                                                                | 
        if ( (*(_DWORD *)&SubjectContext[4] & 0xFFFFFFF8) != 0                                                           | 
          || *(_QWORD *)&SubjectContext[8]                                                                               | 
          || *(_QWORD *)&SubjectContext[16]                                                                              | 
          || (((*(_DWORD *)&SubjectContext[4] >> 1) & 1) != 0 || (SubjectContext[4] & 4) != 0)                           | 
          && (SubjectContext[4] & 1) == 0 )                                                                              | 
        {                                                                                                                | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( ((*(_DWORD *)&SubjectContext[4] >> 1) & 1) != 0 || (SubjectContext[4] & 4) != 0 )                           | 
        {                                                                                                                | 
          v212 = MmReleaseCommitForMemResetPages((ULONG_PTR)Object);                                                     | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v212 = MmSetCommitReleaseEligibility(Object, SubjectContext[4] & 1);                                           | 
        }                                                                                                                | 
        v47 = v212;                                                                                                      | 
LABEL_157:                                                                                                               | 
        v20 = Object;                                                                                                    | 
LABEL_158:                                                                                                               | 
        ObfDereferenceObjectWithTag(v20, 0x79517350u);                                                                   | 
        return v47;                                                                                                      | 
      case 0x42u:                                                                                                        | 
      case 0x43u:                                                                                                        | 
        if ( (a4 & 7) != 0 || a4 > 0x100 )                                                                               | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        memmove(v318, a3, a4);                                                                                           | 
        v213 = (unsigned int)v4 >> 3;                                                                                    | 
        if ( a2 == 67 )                                                                                                  | 
        {                                                                                                                | 
          result = ExCpuSetResourceManagerAccessCheck(PreviousMode);                                                     | 
          if ( result < 0 )                                                                                              | 
          {                                                                                                              | 
            return result;                                                                                               | 
          }                                                                                                              | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v47 = KeSetCpuSetsProcess((_DWORD)Object, v213, (unsigned int)v318, a2 == 67, a2 != 67);                         | 
        goto LABEL_157;                                                                                                  | 
      case 0x44u:                                                                                                        | 
        if ( !(unsigned int)PsIsSessionInitializationProcess(KeGetCurrentThread()->ApcState.Process) )                   | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        v277 = 0LL;                                                                                                      | 
        v215 = ObReferenceObjectByHandle(v242, 0x200u, (POBJECT_TYPE)PsProcessType, PreviousMode, &v277, 0LL);           | 
        v47 = v215;                                                                                                      | 
        Object = v277;                                                                                                   | 
        if ( v215 >= 0 )                                                                                                 | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)v277 + 383, 0x40u);                                                  | 
          ObfDereferenceObject(Object);                                                                                  | 
        }                                                                                                                | 
        return v47;                                                                                                      | 
      case 0x46u:                                                                                                        | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        EtwTraceProcessSetInPrivateMode(Object);                                                                         | 
        _InterlockedOr((volatile signed __int32 *)Object + 124, 0x80000000);                                             | 
        goto LABEL_24;                                                                                                   | 
      case 0x47u:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v58 = *a3;                                                                                                       | 
        v288 = *a3;                                                                                                      | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v59 = ObReferenceProcessHandleTable(Object);                                                                     | 
        if ( !v59 )                                                                                                      | 
        {                                                                                                                | 
          goto LABEL_405;                                                                                                | 
        }                                                                                                                | 
        ExEnableRaiseUMExceptionOnInvalidHandleClose(v59, v58);                                                          | 
        goto LABEL_206;                                                                                                  | 
      case 0x48u:                                                                                                        | 
        return PsIumEnableOnDemandDebugWithResponse(v7, a3, a4);                                                         | 
      case 0x4Au:                                                                                                        | 
        if ( a4 != 1 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v216 = *(_BYTE *)a3;                                                                                             | 
        v258 = *(_BYTE *)a3;                                                                                             | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( v216 )                                                                                                      | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)Object + 383, 0x200u);                                               | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          _InterlockedAnd((volatile signed __int32 *)Object + 383, 0xFFFFFDFF);                                          | 
        }                                                                                                                | 
        goto LABEL_24;                                                                                                   | 
      case 0x4Du:                                                                                                        | 
        v305 = 0LL;                                                                                                      | 
        v306 = 0;                                                                                                        | 
        if ( a4 != 12 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v305 = *(PVOID *)a3;                                                                                             | 
        v70 = a3[2];                                                                                                     | 
        v306 = v70;                                                                                                      | 
        if ( (_DWORD)v305 != 1 || (HIDWORD(v305) & 0xFFFFFFF8) != 0 || (~HIDWORD(v305) & v70) != 0 )                     | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( !ExGetExtensionTable(PspBamExtensionHost) )                                                                 | 
        {                                                                                                                | 
          return -1073741822;                                                                                            | 
        }                                                                                                                | 
        v71 = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);               | 
        if ( v71 < 0 )                                                                                                   | 
        {                                                                                                                | 
          ExReleaseExtensionTable(PspBamExtensionHost);                                                                  | 
          return v71;                                                                                                    | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v72 = guard_dispatch_icall_no_overrides(Object, &v305);                                                        | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
          ExReleaseExtensionTable(PspBamExtensionHost);                                                                  | 
          return v72;                                                                                                    | 
        }                                                                                                                | 
      case 0x50u:                                                                                                        | 
        result = ExCpuSetResourceManagerAccessCheck(PreviousMode);                                                       | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( (_DWORD)v4 != 1 )                                                                                           | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v214 = *(_BYTE *)v5;                                                                                             | 
        v257 = *(_BYTE *)v5;                                                                                             | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( v214 )                                                                                                      | 
        {                                                                                                                | 
          _InterlockedOr((volatile signed __int32 *)Object + 124, 0x8000000u);                                           | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          _InterlockedAnd((volatile signed __int32 *)Object + 124, 0xF7FFFFFF);                                          | 
        }                                                                                                                | 
        KeRecomputeCpuSetAffinityProcess(Object);                                                                        | 
        goto LABEL_24;                                                                                                   | 
      case 0x52u:                                                                                                        | 
        if ( a4 < 8 )                                                                                                    | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        memset_0(v312, 0, sizeof(v312));                                                                                 | 
        if ( (unsigned int)v4 >= 0x90 )                                                                                  | 
        {                                                                                                                | 
          LODWORD(v4) = 144;                                                                                             | 
        }                                                                                                                | 
        memmove(v312, v5, (unsigned int)v4);                                                                             | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v47 = PoSetProcessEnergyTrackingState(Object, v312);                                                             | 
        v20 = Object;                                                                                                    | 
        if ( v47 >= 0 )                                                                                                  | 
        {                                                                                                                | 
          goto LABEL_25;                                                                                                 | 
        }                                                                                                                | 
        goto LABEL_158;                                                                                                  | 
      case 0x53u:                                                                                                        | 
        return -1073741637;                                                                                              | 
      case 0x55u:                                                                                                        | 
        if ( a4 != 24 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        memset_0(pszDest, 0, 0x41uLL);                                                                                   | 
        v217 = *v5;                                                                                                      | 
        *(_OWORD *)SubjectContext = v217;                                                                                | 
        *(_QWORD *)&SubjectContext[16] = *((_QWORD *)v5 + 2);                                                            | 
        v218 = (const char *)v217;                                                                                       | 
        if ( (unsigned __int64)(v217 + 65) > 0x7FFFFFFF0000LL || (__int64)v217 + 65 < (unsigned __int64)v217 )           | 
        {                                                                                                                | 
          v218 = *(const char **)SubjectContext;                                                                         | 
        }                                                                                                                | 
        RtlStringCbCopyA(pszDest, 0x41uLL, v218);                                                                        | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        *(_QWORD *)SubjectContext = pszDest;                                                                             | 
        pszDest[64] = 0;                                                                                                 | 
        v47 = EtwSetProcessTelemetryCoverage(Object, SubjectContext);                                                    | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                | 
        *((_DWORD *)v5 + 3) = *(_DWORD *)&SubjectContext[12];                                                            | 
        *((_DWORD *)v5 + 4) = *(_DWORD *)&SubjectContext[16];                                                            | 
        return v47;                                                                                                      | 
      case 0x57u:                                                                                                        | 
        break;                                                                                                           | 
      case 0x5Au:                                                                                                        | 
        return SeCodeIntegritySetInformationProcess(v7, a2, a3, a4);                                                     | 
      case 0x5Bu:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v287 = *a3;                                                                                                      | 
        v83 = v287;                                                                                                      | 
        if ( (v287 & 0xFFFFFFFE) != 0 )                                                                                  | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                     | 
        {                                                                                                                | 
          return -1073741727;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        LOBYTE(v84) = 1;                                                                                                 | 
        PspSetProcessForegroundBackgroundRequest(Object, v83 & 1, v84);                                                  | 
        goto LABEL_24;                                                                                                   | 
      case 0x5Fu:                                                                                                        | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v53 = *(_QWORD *)a3;                                                                                             | 
        v304 = *(_QWORD *)a3;                                                                                            | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v47 = ObpReferenceObjectByHandleWithTag(v53, 0x79517350u, (__int64)&v274, 0LL, 0LL);                             | 
        v20 = Object;                                                                                                    | 
        if ( v47 < 0 )                                                                                                   | 
        {                                                                                                                | 
          goto LABEL_158;                                                                                                | 
        }                                                                                                                | 
        v47 = PspCombineSecurityDomains(Object, v274);                                                                   | 
        ObfDereferenceObjectWithTag(v274, 0x79517350u);                                                                  | 
        goto LABEL_157;                                                                                                  | 
      case 0x61u:                                                                                                        | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v254 = *(void **)a3;                                                                                             | 
        if ( ((unsigned int)v254 & 0xFFFFFFFE) != 0 || v7 != (HANDLE)-1LL )                                              | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        v219 = KeGetCurrentThread()->ApcState.Process;                                                                   | 
        Object = v219;                                                                                                   | 
        Blink = v219[1].ProcessListEntry.Blink;                                                                          | 
        if ( !Blink )                                                                                                    | 
        {                                                                                                                | 
          return -1073741790;                                                                                            | 
        }                                                                                                                | 
        v221 = 0LL;                                                                                                      | 
        v222 = (__int64 *)v219[1].ReadyTime;                                                                             | 
        if ( v222 )                                                                                                      | 
        {                                                                                                                | 
          v221 = *v222;                                                                                                  | 
        }                                                                                                                | 
        v223 = (unsigned __int8)v254 & 1;                                                                                | 
        Flink = (int)Blink[124].Flink;                                                                                   | 
        if ( ((unsigned __int8)v254 & 1) != 0 )                                                                          | 
        {                                                                                                                | 
          v225 = Flink | 1;                                                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v225 = Flink & 0xFFFFFFFE;                                                                                     | 
        }                                                                                                                | 
        LODWORD(Blink[124].Flink) = v225;                                                                                | 
        if ( v221 )                                                                                                      | 
        {                                                                                                                | 
          v226 = *(_DWORD *)(v221 + 1140);                                                                               | 
          if ( v223 )                                                                                                    | 
          {                                                                                                              | 
            v227 = v226 | 1;                                                                                             | 
          }                                                                                                              | 
          else                                                                                                           | 
          {                                                                                                              | 
            v227 = v226 & 0xFFFFFFFE;                                                                                    | 
          }                                                                                                              | 
          *(_DWORD *)(v221 + 1140) = v227;                                                                               | 
        }                                                                                                                | 
        return (int)v8;                                                                                                  | 
      case 0x62u:                                                                                                        | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( PreviousMode != 1 )                                                                                         | 
        {                                                                                                                | 
          return -1073741823;                                                                                            | 
        }                                                                                                                | 
        if ( a4 != 32 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( !(unsigned __int8)((__int64 (*)(void))KeIsUserCetAllowed)()                                                 | 
          || !_bittest((const signed __int32 *)&KeGetCurrentThread()->116, 0x14u) )                                      | 
        {                                                                                                                | 
          return -1073741637;                                                                                            | 
        }                                                                                                                | 
        return PspSetupUserFiberShadowStack(*(_QWORD *)v5, *((_QWORD *)v5 + 1), (unsigned int)v5[1], (char *)v5 + 24);   | 
      case 0x63u:                                                                                                        | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( PreviousMode != 1 )                                                                                         | 
        {                                                                                                                | 
          return -1073741823;                                                                                            | 
        }                                                                                                                | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( (unsigned __int8)KeIsUserCetAllowed(8LL)                                                                    | 
          && _bittest((const signed __int32 *)&KeGetCurrentThread()->116, 0x14u) )                                       | 
        {                                                                                                                | 
          return PspFreeUserFiberShadowStack(*(PVOID *)v5);                                                              | 
        }                                                                                                                | 
        return -1073741637;                                                                                              | 
      case 0x64u:                                                                                                        | 
        memset(SubjectContext, 0, 20);                                                                                   | 
        if ( a4 != 20 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        *(_OWORD *)SubjectContext = *(_OWORD *)a3;                                                                       | 
        *(_DWORD *)&SubjectContext[16] = a3[4];                                                                          | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        v47 = PspSyscallProviderOptIn(Object, SubjectContext);                                                           | 
        goto LABEL_157;                                                                                                  | 
      case 0x65u:                                                                                                        | 
        LODWORD(Handle) = 0;                                                                                             | 
        v228 = 0LL;                                                                                                      | 
        if ( a4 != 16 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        *(_OWORD *)SubjectContext = *(_OWORD *)a3;                                                                       | 
        v229 = 16 * *(unsigned __int16 *)SubjectContext;                                                                 | 
        if ( !v229 )                                                                                                     | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( !*(_QWORD *)&SubjectContext[8] )                                                                            | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        v230 = v229;                                                                                                     | 
        ProbeForWrite(*(volatile void **)&SubjectContext[8], v229, 8u);                                                  | 
        if ( *(_WORD *)&SubjectContext[2] || *(_DWORD *)&SubjectContext[4] )                                             | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( PreviousMode != 1 )                                                                                         | 
        {                                                                                                                | 
          return -1073741790;                                                                                            | 
        }                                                                                                                | 
        v305 = 0LL;                                                                                                      | 
        result = ObReferenceObjectByHandle(v242, 0x200u, (POBJECT_TYPE)PsProcessType, 1, &v305, 0LL);                    | 
        Object = v305;                                                                                                   | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( v305 == KeGetCurrentThread()->ApcState.Process && (*((_DWORD *)v305 + 469) & 0x40000000) != 0 )             | 
        {                                                                                                                | 
          v47 = -1073741790;                                                                                             | 
        }                                                                                                                | 
        else if ( (*((_DWORD *)v305 + 469) & 0x4000) != 0 )                                                              | 
        {                                                                                                                | 
          v232 = (_DWORD *)ExAllocatePool2(0x101uLL, v230, 0x4E484544u);                                                 | 
          v228 = v232;                                                                                                   | 
          v254 = v232;                                                                                                   | 
          if ( v232 )                                                                                                    | 
          {                                                                                                              | 
            memmove(v232, *(const void **)&SubjectContext[8], v230);                                                     | 
            v47 = PspProcessDynamicEHContinuationTargets((ULONG_PTR)Object);                                             | 
            v243 = v47;                                                                                                  | 
            v241 = 0;                                                                                                    | 
          }                                                                                                              | 
          else                                                                                                           | 
          {                                                                                                              | 
            v47 = -1073741801;                                                                                           | 
          }                                                                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v47 = -1073741637;                                                                                             | 
        }                                                                                                                | 
        v231 = Object;                                                                                                   | 
        if ( Object )                                                                                                    | 
        {                                                                                                                | 
          goto LABEL_1055;                                                                                               | 
        }                                                                                                                | 
        goto LABEL_1056;                                                                                                 | 
      case 0x66u:                                                                                                        | 
        LODWORD(Handle) = 0;                                                                                             | 
        v228 = 0LL;                                                                                                      | 
        if ( a4 != 16 )                                                                                                  | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        *(_OWORD *)SubjectContext = *(_OWORD *)a3;                                                                       | 
        v233 = 24 * (unsigned int)*(unsigned __int16 *)SubjectContext;                                                   | 
        if ( !(_DWORD)v233 )                                                                                             | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( !*(_QWORD *)&SubjectContext[8] )                                                                            | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        v234 = (unsigned int)v233;                                                                                       | 
        ProbeForWrite(*(volatile void **)&SubjectContext[8], v233, 8u);                                                  | 
        if ( *(_WORD *)&SubjectContext[2] || *(_DWORD *)&SubjectContext[4] )                                             | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( PreviousMode != 1 )                                                                                         | 
        {                                                                                                                | 
          return -1073741790;                                                                                            | 
        }                                                                                                                | 
        v305 = 0LL;                                                                                                      | 
        result = ObReferenceObjectByHandle(v242, 0x200u, (POBJECT_TYPE)PsProcessType, 1, &v305, 0LL);                    | 
        Object = v305;                                                                                                   | 
        if ( result < 0 )                                                                                                | 
        {                                                                                                                | 
          return result;                                                                                                 | 
        }                                                                                                                | 
        if ( v305 == KeGetCurrentThread()->ApcState.Process && (*((_DWORD *)v305 + 469) & 0x40000000) != 0 )             | 
        {                                                                                                                | 
          v47 = -1073741790;                                                                                             | 
        }                                                                                                                | 
        else if ( (*((_DWORD *)v305 + 469) & 0x4000) != 0 )                                                              | 
        {                                                                                                                | 
          v235 = (_DWORD *)ExAllocatePool2(0x101uLL, v234, 0x52414544u);                                                 | 
          v228 = v235;                                                                                                   | 
          v254 = v235;                                                                                                   | 
          if ( v235 )                                                                                                    | 
          {                                                                                                              | 
            v236 = *(_QWORD *)&SubjectContext[8];                                                                        | 
            memmove(v235, *(const void **)&SubjectContext[8], v234);                                                     | 
            v47 = PspProcessDynamicEnforcedAddressRanges((ULONG_PTR)Object, (__int64)&Handle);                           | 
            v243 = v47;                                                                                                  | 
            while ( 1 )                                                                                                  | 
            {                                                                                                            | 
              v241 = (unsigned int)v8;                                                                                   | 
              if ( (unsigned int)v8 >= (unsigned int)Handle )                                                            | 
              {                                                                                                          | 
                break;                                                                                                   | 
              }                                                                                                          | 
              *(_DWORD *)(v236 + 24LL * (unsigned int)v8 + 16) = v228[6 * (unsigned int)v8 + 4];                         | 
              LODWORD(v8) = (_DWORD)v8 + 1;                                                                              | 
            }                                                                                                            | 
          }                                                                                                              | 
          else                                                                                                           | 
          {                                                                                                              | 
            v47 = -1073741801;                                                                                           | 
          }                                                                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v47 = -1073741637;                                                                                             | 
        }                                                                                                                | 
        v231 = Object;                                                                                                   | 
        if ( Object )                                                                                                    | 
        {                                                                                                                | 
LABEL_1055:                                                                                                              | 
          ObfDereferenceObject(v231);                                                                                    | 
        }                                                                                                                | 
LABEL_1056:                                                                                                              | 
        if ( v228 )                                                                                                      | 
        {                                                                                                                | 
          ExFreePoolWithTag(v228, 0);                                                                                    | 
        }                                                                                                                | 
        return v47;                                                                                                      | 
      case 0x69u:                                                                                                        | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v296 = *(_QWORD *)a3;                                                                                            | 
        if ( (~(MEMORY[0xFFFFF780000003D8] | MEMORY[0xFFFFF78000000708]) & v296) != 0 )                                  | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( PreviousMode != 1 )                                                                                         | 
        {                                                                                                                | 
          return -1073741790;                                                                                            | 
        }                                                                                                                | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);            | 
          if ( result < 0 )                                                                                              | 
          {                                                                                                              | 
            return result;                                                                                               | 
          }                                                                                                              | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
          if ( Object != KeGetCurrentThread()->ApcState.Process )                                                        | 
          {                                                                                                              | 
            return -1073741811;                                                                                          | 
          }                                                                                                              | 
        }                                                                                                                | 
        Object = KeGetCurrentThread()->ApcState.Process;                                                                 | 
        if ( *((_QWORD *)Object + 98) )                                                                                  | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        if ( (unsigned __int8)PsIsSystemProcess() )                                                                      | 
        {                                                                                                                | 
          return -1073741637;                                                                                            | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          return PspEnableProcessOptionalXStateFeatures(Object, v296);                                                   | 
        }                                                                                                                | 
      case 0x6Au:                                                                                                        | 
        return PfSnSetAltPrefetchParam((ULONG_PTR)v7);                                                                   | 
      case 0x6Bu:                                                                                                        | 
        if ( a4 < 0x10 || (a4 & 7) != 0 )                                                                                | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v153 = (struct _EX_RUNDOWN_REF *)ExAllocatePool2(0x101uLL, a4, 0x50707350u);                                     | 
        v154 = v153;                                                                                                     | 
        RunRef = v153;                                                                                                   | 
        if ( v153 )                                                                                                      | 
        {                                                                                                                | 
          memmove(v153, v5, v4);                                                                                         | 
          if ( (unsigned int)(v4 - 8) >> 3 < HIDWORD(v154->Ptr) || (v154->Count & 0xFFFFFFFE) != 0 )                     | 
          {                                                                                                              | 
            v155 = -1073741811;                                                                                          | 
          }                                                                                                              | 
          else                                                                                                           | 
          {                                                                                                              | 
            v305 = 0LL;                                                                                                  | 
            v155 = ObReferenceObjectByHandle(v242, 0x200u, (POBJECT_TYPE)PsProcessType, PreviousMode, &v305, 0LL);       | 
            Object = v305;                                                                                               | 
            if ( v155 >= 0 )                                                                                             | 
            {                                                                                                            | 
              v155 = PspAssignCpuPartitionsToProcess(                                                                    | 
                       (_DWORD)v305,                                                                                     | 
                       PreviousMode,                                                                                     | 
                       (int)v154 + 8,                                                                                    | 
                       HIDWORD(v154->Ptr),                                                                               | 
                       v154->Count & 1);                                                                                 | 
            }                                                                                                            | 
          }                                                                                                              | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v155 = -1073741670;                                                                                            | 
        }                                                                                                                | 
        if ( v154 )                                                                                                      | 
        {                                                                                                                | 
          ExFreePoolWithTag(v154, 0);                                                                                    | 
        }                                                                                                                | 
        if ( Object )                                                                                                    | 
        {                                                                                                                | 
          ObfDereferenceObject(Object);                                                                                  | 
        }                                                                                                                | 
        return v155;                                                                                                     | 
      case 0x6Cu:                                                                                                        | 
        if ( a4 != 4 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        v259 = *a3;                                                                                                      | 
        v54 = v259;                                                                                                      | 
        if ( !(_WORD)v259 || (v259 & 0xFFFC) != 0 )                                                                      | 
        {                                                                                                                | 
          return -1073741811;                                                                                            | 
        }                                                                                                                | 
        result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
        v55 = result;                                                                                                    | 
        if ( result >= 0 )                                                                                               | 
        {                                                                                                                | 
          if ( (v54 & 1) != 0                                                                                            | 
            && (unsigned int)dword_140E085C0 > 5                                                                         | 
            && (unsigned __int8)tlgKeywordOn(&dword_140E085C0, 0x400000000000LL) )                                       | 
          {                                                                                                              | 
            v239[0] = HIBYTE(v259);                                                                                      | 
            *(_QWORD *)&SubjectContext[32] = v239;                                                                       | 
            v314 = 1LL;                                                                                                  | 
            v305 = (PVOID)0x1000000;                                                                                     | 
            v315 = &v305;                                                                                                | 
            v316 = 8;                                                                                                    | 
            v317 = 0;                                                                                                    | 
            tlgWriteTransfer_EtwWriteTransfer(&dword_140E085C0, word_1400519CA, 0LL, 0LL, 4, SubjectContext);            | 
          }                                                                                                              | 
          if ( (v54 & 2) == 0 || (v55 = PspSetProcessPriorityClass(Object, BYTE2(v259), v242, PreviousMode), v55 >= 0) ) | 
          {                                                                                                              | 
            MmGetMinWsPagePriority();                                                                                    | 
            if ( (unsigned int)Feature_ForegroundBoost_ApiRemoval__private_IsEnabledDeviceUsageNoInline()                | 
              || (v54 & 1) == 0 )                                                                                        | 
            {                                                                                                            | 
              KeSetPriorityAndQuantumProcess(                                                                            | 
                (_DWORD)Object,                                                                                          | 
                *(_DWORD *)&PspPriorityTable[2 * *((unsigned __int8 *)Object + 839)],                                    | 
                0,                                                                                                       | 
                0,                                                                                                       | 
                0);                                                                                                      | 
            }                                                                                                            | 
            else                                                                                                         | 
            {                                                                                                            | 
              PsSetProcessPriorityByClass(Object, 1LL);                                                                  | 
            }                                                                                                            | 
          }                                                                                                              | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                              | 
          return v55;                                                                                                    | 
        }                                                                                                                | 
        return result;                                                                                                   | 
      case 0x70u:                                                                                                        | 
        if ( a4 != 8 )                                                                                                   | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        Handle = 0LL;                                                                                                    | 
        v47 = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v7, 0x79517350u, (__int64)&Object, 0LL, 0LL);                 | 
        if ( v47 >= 0 && (v47 = PspSchedulerSharedDataRegionCreate(Object), v47 >= 0) )                                  | 
        {                                                                                                                | 
          *(_QWORD *)v5 = Handle;                                                                                        | 
          Handle = 0LL;                                                                                                  | 
          v47 = 0;                                                                                                       | 
        }                                                                                                                | 
        else                                                                                                             | 
        {                                                                                                                | 
          v8 = Handle;                                                                                                   | 
        }                                                                                                                | 
        if ( v8 )                                                                                                        | 
        {                                                                                                                | 
          ObCloseHandle(v8, PreviousMode);                                                                               | 
        }                                                                                                                | 
        v20 = Object;                                                                                                    | 
        if ( Object )                                                                                                    | 
        {                                                                                                                | 
          goto LABEL_158;                                                                                                | 
        }                                                                                                                | 
        return v47;                                                                                                      | 
      case 0x71u:                                                                                                        | 
        if ( a4 )                                                                                                        | 
        {                                                                                                                | 
          return -1073741820;                                                                                            | 
        }                                                                                                                | 
        if ( v7 != (HANDLE)-1LL )                                                                                        | 
        {                                                                                                                | 
          return -1073741637;                                                                                            | 
        }                                                                                                                | 
        KeChangeMemoryAttributes(PreviousMode);                                                                          | 
        return 0;                                                                                                        | 
      default:                                                                                                           | 
        return -1073741821;                                                                                              | 
    }                                                                                                                    | 
  }                                                                                                                      | 
  if ( a2 == 87 && !a4 || a4 < 4 && a2 == 96 )                                                                           | 
  {                                                                                                                      | 
    return -1073741820;                                                                                                  | 
  }                                                                                                                      | 
  result = EtwCheckSecurityLoggerAccess(KeGetCurrentThread()->ApcState.Process, PreviousMode);                           | 
  if ( result >= 0 )                                                                                                     | 
  {                                                                                                                      | 
    if ( !SeSinglePrivilegeCheck(SeDebugPrivilege, PreviousMode)                                                         | 
      && !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )                                                         | 
    {                                                                                                                    | 
      return -1073741727;                                                                                                | 
    }                                                                                                                    | 
    v251 = 0;                                                                                                            | 
    if ( a2 == 87 )                                                                                                      | 
    {                                                                                                                    | 
      v14 = *(_BYTE *)v5;                                                                                                | 
      v251 = *(_BYTE *)v5 & 1;                                                                                           | 
      v13 = v14 & 3;                                                                                                     | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      v13 = *(_DWORD *)v5;                                                                                               | 
    }                                                                                                                    | 
    v251 = v13;                                                                                                          | 
    if ( (v13 & 0xFFFFFF80) != 0 )                                                                                       | 
    {                                                                                                                    | 
      return -1073741811;                                                                                                | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      result = ObpReferenceObjectByHandleWithTag((ULONG_PTR)v242, 0x79517350u, (__int64)&Object, 0LL, 0LL);              | 
      if ( result >= 0 )                                                                                                 | 
      {                                                                                                                  | 
        _InterlockedAnd((volatile signed __int32 *)Object + 383, 0x4FE7FFFFu);                                           | 
        v15 = (((v251 >> 2) & 1) << 19) | 0x100000;                                                                      | 
        if ( (v251 & 8) == 0 )                                                                                           | 
        {                                                                                                                | 
          v15 = ((v251 >> 2) & 1) << 19;                                                                                 | 
        }                                                                                                                | 
        v16 = v15 | 0x20000000;                                                                                          | 
        if ( (v251 & 0x10) == 0 )                                                                                        | 
        {                                                                                                                | 
          v16 = v15;                                                                                                     | 
        }                                                                                                                | 
        v17 = v16 | 0x10000000;                                                                                          | 
        if ( (v251 & 0x20) == 0 )                                                                                        | 
        {                                                                                                                | 
          v17 = v16;                                                                                                     | 
        }                                                                                                                | 
        v18 = v17 | 0x80000000;                                                                                          | 
        if ( (v251 & 0x40) == 0 )                                                                                        | 
        {                                                                                                                | 
          v18 = v17;                                                                                                     | 
        }                                                                                                                | 
        _InterlockedOr((volatile signed __int32 *)Object + 383, v18);                                                    | 
        _InterlockedAnd((volatile signed __int32 *)Object + 124, 0xFCFFFFFF);                                            | 
        v19 = ((v251 & 1) << 24) | 0x2000000;                                                                            | 
        if ( (v251 & 2) == 0 )                                                                                           | 
        {                                                                                                                | 
          v19 = (v251 & 1) << 24;                                                                                        | 
        }                                                                                                                | 
        _InterlockedOr((volatile signed __int32 *)Object + 124, v19);                                                    | 
LABEL_24:                                                                                                                | 
        v20 = Object;                                                                                                    | 
LABEL_25:                                                                                                                | 
        ObfDereferenceObjectWithTag(v20, 0x79517350u);                                                                   | 
        return 0;                                                                                                        | 
      }                                                                                                                  | 
    }                                                                                                                    | 
  }                                                                                                                      | 
  return result;                                                                                                         | 
}                                                                                                                        | 
