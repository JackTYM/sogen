__int64 __fastcall NtSetInformationJobObject(ULONG_PTR BugCheckParameter1, int a2, void *a3, unsigned int a4)                                        | 
{                                                                                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                      | 
                                                                                                                                                     | 
  v4 = a4;                                                                                                                                           | 
  v5 = a2;                                                                                                                                           | 
  v152 = BugCheckParameter1;                                                                                                                         | 
  v142 = a2;                                                                                                                                         | 
  v144 = a2;                                                                                                                                         | 
  Src = a3;                                                                                                                                          | 
  memset_0(v214, 0, 0x100uLL);                                                                                                                       | 
  v162 = 0LL;                                                                                                                                        | 
  v161 = 0LL;                                                                                                                                        | 
  v140 = 0;                                                                                                                                          | 
  v163 = 0LL;                                                                                                                                        | 
  memset_0(&v201, 0, 0x98uLL);                                                                                                                       | 
  v193 = 0LL;                                                                                                                                        | 
  v165 = 0LL;                                                                                                                                        | 
  Object[0] = 0LL;                                                                                                                                   | 
  memset_0(v182, 0, 0x90uLL);                                                                                                                        | 
  Event = 0LL;                                                                                                                                       | 
  v172 = 0LL;                                                                                                                                        | 
  v173 = 0LL;                                                                                                                                        | 
  memset(&SubjectContext, 0, sizeof(SubjectContext));                                                                                                | 
  LODWORD(v159) = 0;                                                                                                                                 | 
  memset_0(&v190, 0, 0x48uLL);                                                                                                                       | 
  v198 = 0LL;                                                                                                                                        | 
  v199 = 0LL;                                                                                                                                        | 
  v200 = 0LL;                                                                                                                                        | 
  v194 = 0LL;                                                                                                                                        | 
  v195 = 0LL;                                                                                                                                        | 
  v196 = 0LL;                                                                                                                                        | 
  v197 = 0LL;                                                                                                                                        | 
  v145 = 0LL;                                                                                                                                        | 
  memset(&RequiredPrivileges, 0, sizeof(RequiredPrivileges));                                                                                        | 
  BugCheckParameter1a[0] = 0LL;                                                                                                                      | 
  *(_OWORD *)v160 = 0LL;                                                                                                                             | 
  if ( (unsigned int)(v5 - 1) > 0x32 )                                                                                                               | 
  {                                                                                                                                                  | 
    return 3221225475LL;                                                                                                                             | 
  }                                                                                                                                                  | 
  switch ( (_DWORD)v5 )                                                                                                                              | 
  {                                                                                                                                                  | 
    case 9:                                                                                                                                          | 
      if ( (_DWORD)v4 == 144 || (_DWORD)v4 == 152 )                                                                                                  | 
      {                                                                                                                                              | 
        goto LABEL_14;                                                                                                                               | 
      }                                                                                                                                              | 
      return 3221225476LL;                                                                                                                           | 
    case 0xC:                                                                                                                                        | 
      v8 = (((_DWORD)v4 - 48) & 0xFFFFFFF7) == 0;                                                                                                    | 
LABEL_13:                                                                                                                                            | 
      if ( v8 )                                                                                                                                      | 
      {                                                                                                                                              | 
        goto LABEL_14;                                                                                                                               | 
      }                                                                                                                                              | 
      return 3221225476LL;                                                                                                                           | 
    case 0x1F:                                                                                                                                       | 
      if ( (_DWORD)v4 == 48 || (_DWORD)v4 == 96 )                                                                                                    | 
      {                                                                                                                                              | 
        goto LABEL_14;                                                                                                                               | 
      }                                                                                                                                              | 
      v8 = (_DWORD)v4 == 144;                                                                                                                        | 
      goto LABEL_13;                                                                                                                                 | 
  }                                                                                                                                                  | 
  v7 = dword_140B3FF9C[v5];                                                                                                                          | 
  if ( (_DWORD)v4 != v7 && ((_DWORD)v5 != 11 && (_DWORD)v5 != 14 || (unsigned int)v4 < v7) )                                                         | 
  {                                                                                                                                                  | 
    return 3221225476LL;                                                                                                                             | 
  }                                                                                                                                                  | 
LABEL_14:                                                                                                                                            | 
  Thread = KeGetCurrentThread();                                                                                                                     | 
  PreviousMode = Thread->PreviousMode;                                                                                                               | 
  v141 = PreviousMode;                                                                                                                               | 
  if ( PreviousMode )                                                                                                                                | 
  {                                                                                                                                                  | 
    v9 = (__m128i *)Src;                                                                                                                             | 
    if ( (_DWORD)v4 )                                                                                                                                | 
    {                                                                                                                                                | 
      if ( ((dword_140B4006C[v5] - 1) & (unsigned int)Src) != 0 )                                                                                    | 
      {                                                                                                                                              | 
        ExRaiseDatatypeMisalignment();                                                                                                               | 
      }                                                                                                                                              | 
      if ( (unsigned __int64)Src + v4 > 0x7FFFFFFF0000LL || (char *)Src + v4 < Src )                                                                 | 
      {                                                                                                                                              | 
        v9 = (__m128i *)Src;                                                                                                                         | 
      }                                                                                                                                              | 
    }                                                                                                                                                | 
  }                                                                                                                                                  | 
  else                                                                                                                                               | 
  {                                                                                                                                                  | 
    v9 = (__m128i *)Src;                                                                                                                             | 
  }                                                                                                                                                  | 
  if ( !BugCheckParameter1 )                                                                                                                         | 
  {                                                                                                                                                  | 
    return 3221225480LL;                                                                                                                             | 
  }                                                                                                                                                  | 
  result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Event, 0LL, 0LL);                                            | 
  if ( (int)result >= 0 )                                                                                                                            | 
  {                                                                                                                                                  | 
    v12 = 0;                                                                                                                                         | 
    v139 = 0;                                                                                                                                        | 
    v13 = 0;                                                                                                                                         | 
    --Thread->SpecialApcDisable;                                                                                                                     | 
    if ( v142 <= 25 )                                                                                                                                | 
    {                                                                                                                                                | 
      if ( v142 != 25 )                                                                                                                              | 
      {                                                                                                                                              | 
        if ( v142 <= 14 )                                                                                                                            | 
        {                                                                                                                                            | 
          if ( v142 == 14 )                                                                                                                          | 
          {                                                                                                                                          | 
LABEL_190:                                                                                                                                           | 
            P = 0LL;                                                                                                                                 | 
            v51 = 16;                                                                                                                                | 
            if ( v144 != 14 )                                                                                                                        | 
            {                                                                                                                                        | 
              v51 = 2;                                                                                                                               | 
            }                                                                                                                                        | 
            v144 = v51;                                                                                                                              | 
            v52 = (unsigned int)v4 / v51;                                                                                                            | 
            if ( (unsigned int)v4 % v51 )                                                                                                            | 
            {                                                                                                                                        | 
              v12 = -1073741820;                                                                                                                     | 
              goto LABEL_446;                                                                                                                        | 
            }                                                                                                                                        | 
            v157 = (unsigned int)v4 / v51;                                                                                                           | 
            v213 = 2097153LL;                                                                                                                        | 
            memset_0(v214, 0, 0x100uLL);                                                                                                             | 
            v53 = KeQueryActiveGroupCount() - 1;                                                                                                     | 
            v54 = v144;                                                                                                                              | 
            while ( v52 )                                                                                                                            | 
            {                                                                                                                                        | 
              if ( v142 == 14 )                                                                                                                      | 
              {                                                                                                                                      | 
                v165 = *v9;                                                                                                                          | 
                epi16 = _mm_extract_epi16(v165, 4);                                                                                                  | 
                v143 = epi16;                                                                                                                        | 
                v152 = v165.m128i_i64[0];                                                                                                            | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                v56 = v9->m128i_u16[0];                                                                                                              | 
                v143 = v56;                                                                                                                          | 
                if ( (unsigned __int16)v56 > v53 )                                                                                                   | 
                {                                                                                                                                    | 
                  goto LABEL_204;                                                                                                                    | 
                }                                                                                                                                    | 
                v152 = KeActiveProcessors.Bitmap[v56];                                                                                               | 
                epi16 = v143;                                                                                                                        | 
              }                                                                                                                                      | 
              if ( epi16 > v53 || v214[epi16] || v152 != (v152 & KeActiveProcessors.Bitmap[epi16]) )                                                 | 
              {                                                                                                                                      | 
LABEL_204:                                                                                                                                           | 
                v12 = -1073741811;                                                                                                                   | 
                v139 = -1073741811;                                                                                                                  | 
                break;                                                                                                                               | 
              }                                                                                                                                      | 
              KeAddGroupAffinityEx(&v213, v143);                                                                                                     | 
              v52 = --v157;                                                                                                                          | 
              v9 = (__m128i *)((char *)Src + v54);                                                                                                   | 
              Src = (char *)Src + v54;                                                                                                               | 
              v12 = v139;                                                                                                                            | 
            }                                                                                                                                        | 
            Pool2 = 0LL;                                                                                                                             | 
            v21 = Event;                                                                                                                             | 
            if ( v12 < 0 )                                                                                                                           | 
            {                                                                                                                                        | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
            if ( !*(_QWORD *)&Event[22].Header.Lock )                                                                                                | 
            {                                                                                                                                        | 
              Pool2 = (struct _LIST_ENTRY *)ExAllocatePool2(0x100uLL, 0x28uLL, 0x614A7350u);                                                         | 
              if ( !Pool2 )                                                                                                                          | 
              {                                                                                                                                      | 
                goto LABEL_123;                                                                                                                      | 
              }                                                                                                                                      | 
              P = (PVOID)PsChargeSharedPoolQuota(KeGetCurrentThread()->ApcState.Process, 40LL);                                                      | 
              if ( !P )                                                                                                                              | 
              {                                                                                                                                      | 
                v35 = Pool2;                                                                                                                         | 
                goto LABEL_126;                                                                                                                      | 
              }                                                                                                                                      | 
              v21 = Event;                                                                                                                           | 
            }                                                                                                                                        | 
            SeCaptureSubjectContextEx(                                                                                                               | 
              Thread,                                                                                                                                | 
              Thread->ApcState.Process,                                                                                                              | 
              (PSECURITY_SUBJECT_CONTEXT)&SubjectContext.ImpersonationLevel);                                                                        | 
            p_ImpersonationLevel = &SubjectContext.ImpersonationLevel;                                                                               | 
            if ( PreviousMode )                                                                                                                      | 
            {                                                                                                                                        | 
              RequiredPrivileges.PrivilegeCount = 1;                                                                                                 | 
              RequiredPrivileges.Control = 1;                                                                                                        | 
              RequiredPrivileges.Privilege[0].Luid = SeDebugPrivilege;                                                                               | 
              RequiredPrivileges.Privilege[0].Attributes = 0;                                                                                        | 
              v59 = SePrivilegeCheck(                                                                                                                | 
                      &RequiredPrivileges,                                                                                                           | 
                      (PSECURITY_SUBJECT_CONTEXT)&SubjectContext.ImpersonationLevel,                                                                 | 
                      1);                                                                                                                            | 
              LODWORD(SubjectContext.ClientToken) = (__int64)SubjectContext.ClientToken & 0xFFFFFFFE | v59 & 1;                                      | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              LODWORD(SubjectContext.ClientToken) |= 1u;                                                                                             | 
            }                                                                                                                                        | 
            ExAcquireResourceExclusiveLite((PERESOURCE)&v21[2].Header.WaitListHead, 1u);                                                             | 
            v12 = PspEnumJobsAndProcessesInJobHierarchy(v21, (__int64)&SubjectContext, 1);                                                           | 
            v139 = v12;                                                                                                                              | 
            if ( v12 >= 0 )                                                                                                                          | 
            {                                                                                                                                        | 
              v60 = *(_QWORD *)&v21[22].Header.Lock;                                                                                                 | 
              if ( v60 )                                                                                                                             | 
              {                                                                                                                                      | 
                v172 = *(_OWORD *)(v60 + 8);                                                                                                         | 
                v173 = *(_OWORD *)(v60 + 24);                                                                                                        | 
                p_ImpersonationLevel = (SECURITY_IMPERSONATION_LEVEL *)&v172;                                                                        | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                *(_QWORD *)&v21[22].Header.Lock = Pool2;                                                                                             | 
                v21[22].Header.WaitListHead.Flink = (struct _LIST_ENTRY *)P;                                                                         | 
                Pool2 = 0LL;                                                                                                                         | 
                p_ImpersonationLevel = 0LL;                                                                                                          | 
              }                                                                                                                                      | 
              v61 = *(struct _SECURITY_SUBJECT_CONTEXT **)&v21[22].Header.Lock;                                                                      | 
              *v61 = SubjectContext;                                                                                                                 | 
              v61[1].ClientToken = v159;                                                                                                             | 
              v160[0] = (__int64)v21;                                                                                                                | 
              HIDWORD(v160[1]) = v21[10].Header.WaitListHead.Blink;                                                                                  | 
              LODWORD(v160[1]) = -17;                                                                                                                | 
              v62 = v21 + 11;                                                                                                                        | 
              v63 = &v213;                                                                                                                           | 
              v64 = 2LL;                                                                                                                             | 
              do                                                                                                                                     | 
              {                                                                                                                                      | 
                *(_OWORD *)&v62->Header.Lock = *(_OWORD *)v63;                                                                                       | 
                *(_OWORD *)&v62->Header.WaitListHead.Blink = *((_OWORD *)v63 + 1);                                                                   | 
                v62[1].Header.WaitListHead = (LIST_ENTRY)*((_OWORD *)v63 + 2);                                                                       | 
                *(_OWORD *)&v62[2].Header.Lock = *((_OWORD *)v63 + 3);                                                                               | 
                *(_OWORD *)&v62[2].Header.WaitListHead.Blink = *((_OWORD *)v63 + 4);                                                                 | 
                v62[3].Header.WaitListHead = (LIST_ENTRY)*((_OWORD *)v63 + 5);                                                                       | 
                *(_OWORD *)&v62[4].Header.Lock = *((_OWORD *)v63 + 6);                                                                               | 
                v62 = (struct _KEVENT *)((char *)v62 + 128);                                                                                         | 
                v62[-1].Header.WaitListHead = (LIST_ENTRY)*((_OWORD *)v63 + 7);                                                                      | 
                v63 += 16;                                                                                                                           | 
                --v64;                                                                                                                               | 
              }                                                                                                                                      | 
              while ( v64 );                                                                                                                         | 
              *(_QWORD *)&v62->Header.Lock = *v63;                                                                                                   | 
              if ( v142 == 14 && (unsigned int)KeIsEmptyAffinityEx(&v21[11], v62, 0LL, 128LL) )                                                      | 
              {                                                                                                                                      | 
                LODWORD(v21[10].Header.WaitListHead.Blink) &= ~0x10u;                                                                                | 
                _InterlockedAnd((volatile signed __int32 *)&v21[64].Header.WaitListHead.Blink, 0xFFFFFFFD);                                          | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                LODWORD(v21[10].Header.WaitListHead.Blink) |= 0x10u;                                                                                 | 
                _InterlockedOr((volatile signed __int32 *)&v21[64].Header.WaitListHead.Blink, 2u);                                                   | 
              }                                                                                                                                      | 
              v21 = Event;                                                                                                                           | 
              PspEnumJobsAndProcessesInJobHierarchy(Event, (__int64)v160, 5);                                                                        | 
              v12 = v139;                                                                                                                            | 
            }                                                                                                                                        | 
            ExReleaseResourceLite((PERESOURCE)&v21[2].Header.WaitListHead);                                                                          | 
            if ( p_ImpersonationLevel )                                                                                                              | 
            {                                                                                                                                        | 
              SeReleaseSubjectContext((PSECURITY_SUBJECT_CONTEXT)p_ImpersonationLevel);                                                              | 
            }                                                                                                                                        | 
            if ( !Pool2 )                                                                                                                            | 
            {                                                                                                                                        | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
            ExFreePoolWithTag(Pool2, 0x614A7350u);                                                                                                   | 
            v50 = P;                                                                                                                                 | 
LABEL_229:                                                                                                                                           | 
            PsReturnSharedPoolQuota(v50);                                                                                                            | 
            goto LABEL_485;                                                                                                                          | 
          }                                                                                                                                          | 
          if ( v142 == 2 )                                                                                                                           | 
          {                                                                                                                                          | 
LABEL_70:                                                                                                                                            | 
            memmove(&v201, v9, v4);                                                                                                                  | 
            if ( (unsigned int)v4 < 0x98 )                                                                                                           | 
            {                                                                                                                                        | 
              memset_0((char *)&v201 + v4, 0, 152 - v4);                                                                                             | 
            }                                                                                                                                        | 
            JobLimitInformationValidFlags = PspGetJobLimitInformationValidFlags((unsigned int)v142, (unsigned int)v4);                               | 
            v26 = v203;                                                                                                                              | 
            if ( (~JobLimitInformationValidFlags & (unsigned int)v203) != 0 )                                                                        | 
            {                                                                                                                                        | 
              goto LABEL_73;                                                                                                                         | 
            }                                                                                                                                        | 
            memset_0(v215, 0, 0x728uLL);                                                                                                             | 
            memset(&SubjectContext, 0, sizeof(SubjectContext));                                                                                      | 
            v159 = 0LL;                                                                                                                              | 
            v220 = v26;                                                                                                                              | 
            P = 0LL;                                                                                                                                 | 
            v147 = 0LL;                                                                                                                              | 
            v156 = 0LL;                                                                                                                              | 
            v221 = (v26 & 8) != 0 ? v206 : 0;                                                                                                        | 
            if ( (v26 & 0x20) != 0 )                                                                                                                 | 
            {                                                                                                                                        | 
              if ( v208 > 6 )                                                                                                                        | 
              {                                                                                                                                      | 
                goto LABEL_456;                                                                                                                      | 
              }                                                                                                                                      | 
              if ( v208 - 3 <= 1 )                                                                                                                   | 
              {                                                                                                                                      | 
                LOBYTE(v27) = PreviousMode;                                                                                                          | 
                v28 = ((__int64 (__fastcall *)(_QWORD, _QWORD, _QWORD, _QWORD))SeCheckPrivilegedObject)(                                             | 
                        SeIncreaseBasePriorityPrivilege,                                                                                             | 
                        v152,                                                                                                                        | 
                        2LL,                                                                                                                         | 
                        v27) & 1;                                                                                                                    | 
                v13 = 4 * v28;                                                                                                                       | 
                if ( !v28 )                                                                                                                          | 
                {                                                                                                                                    | 
                  goto LABEL_78;                                                                                                                     | 
                }                                                                                                                                    | 
              }                                                                                                                                      | 
              v228 = v208;                                                                                                                           | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              v228 = 0;                                                                                                                              | 
            }                                                                                                                                        | 
            if ( (v26 & 0x80u) == 0 )                                                                                                                | 
            {                                                                                                                                        | 
              v224 = 5;                                                                                                                              | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              v29 = v209;                                                                                                                            | 
              if ( v209 >= 0xA )                                                                                                                     | 
              {                                                                                                                                      | 
                goto LABEL_456;                                                                                                                      | 
              }                                                                                                                                      | 
              if ( v209 > 5 )                                                                                                                        | 
              {                                                                                                                                      | 
                LOBYTE(v27) = PreviousMode;                                                                                                          | 
                v30 = ((__int64 (__fastcall *)(_QWORD, _QWORD, _QWORD, _QWORD))SeCheckPrivilegedObject)(                                             | 
                        SeIncreaseBasePriorityPrivilege,                                                                                             | 
                        v152,                                                                                                                        | 
                        2LL,                                                                                                                         | 
                        v27) & 1;                                                                                                                    | 
                v13 = 4 * v30;                                                                                                                       | 
                if ( !v30 )                                                                                                                          | 
                {                                                                                                                                    | 
                  goto LABEL_78;                                                                                                                     | 
                }                                                                                                                                    | 
                v29 = v209;                                                                                                                          | 
              }                                                                                                                                      | 
              v224 = v29;                                                                                                                            | 
            }                                                                                                                                        | 
            if ( (v26 & 2) != 0 )                                                                                                                    | 
            {                                                                                                                                        | 
              if ( !v201 )                                                                                                                           | 
              {                                                                                                                                      | 
                goto LABEL_456;                                                                                                                      | 
              }                                                                                                                                      | 
              v216 = v201;                                                                                                                           | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              v216 = 0LL;                                                                                                                            | 
            }                                                                                                                                        | 
            if ( (v26 & 4) != 0 )                                                                                                                    | 
            {                                                                                                                                        | 
              if ( !v202 )                                                                                                                           | 
              {                                                                                                                                      | 
                goto LABEL_456;                                                                                                                      | 
              }                                                                                                                                      | 
              Blink = v202;                                                                                                                          | 
            }                                                                                                                                        | 
            if ( (v26 & 1) == 0 )                                                                                                                    | 
            {                                                                                                                                        | 
              v218 = 0LL;                                                                                                                            | 
              v219 = 0LL;                                                                                                                            | 
              goto LABEL_106;                                                                                                                        | 
            }                                                                                                                                        | 
            v31 = v204;                                                                                                                              | 
            if ( !v204 && !v205 || v204 == -1LL && v205 == -1LL || v204 > v205 || v204 < 0x14000 )                                                   | 
            {                                                                                                                                        | 
              goto LABEL_456;                                                                                                                        | 
            }                                                                                                                                        | 
            if ( v204 <= PspMinimumWorkingSet || SeSinglePrivilegeCheck(SeIncreaseBasePriorityPrivilege, PreviousMode) )                             | 
            {                                                                                                                                        | 
              v218 = v31;                                                                                                                            | 
              v219 = (struct _LIST_ENTRY *)v205;                                                                                                     | 
LABEL_106:                                                                                                                                           | 
              if ( (v26 & 0x100) != 0 )                                                                                                              | 
              {                                                                                                                                      | 
                if ( v210 < 0x1000 )                                                                                                                 | 
                {                                                                                                                                    | 
                  goto LABEL_456;                                                                                                                    | 
                }                                                                                                                                    | 
                v225 = (struct _LIST_ENTRY *)(v210 >> 12);                                                                                           | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                v225 = 0LL;                                                                                                                          | 
              }                                                                                                                                      | 
              if ( (v26 & 0x200) != 0 )                                                                                                              | 
              {                                                                                                                                      | 
                if ( v211 < 0x1000 )                                                                                                                 | 
                {                                                                                                                                    | 
                  goto LABEL_456;                                                                                                                    | 
                }                                                                                                                                    | 
                v226 = v211 >> 12;                                                                                                                   | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                v226 = 0LL;                                                                                                                          | 
              }                                                                                                                                      | 
              if ( (v26 & 0x200000) == 0 )                                                                                                           | 
              {                                                                                                                                      | 
                v227 = 0LL;                                                                                                                          | 
                goto LABEL_118;                                                                                                                      | 
              }                                                                                                                                      | 
              if ( v212 >= 0x1000 )                                                                                                                  | 
              {                                                                                                                                      | 
                v227 = (struct _LIST_ENTRY *)(v212 >> 12);                                                                                           | 
LABEL_118:                                                                                                                                           | 
                v222 = 2097153LL;                                                                                                                    | 
                memset_0(v223, 0, 0x100uLL);                                                                                                         | 
                v32 = v220;                                                                                                                          | 
                v144 = v220;                                                                                                                         | 
                v21 = Event;                                                                                                                         | 
                v33 = 2LL;                                                                                                                           | 
                if ( (v220 & 0x10) == 0 )                                                                                                            | 
                {                                                                                                                                    | 
                  goto LABEL_133;                                                                                                                    | 
                }                                                                                                                                    | 
                if ( ((__int64)Event[64].Header.WaitListHead.Blink & 2) == 0 && v207 )                                                               | 
                {                                                                                                                                    | 
                  if ( !*(_QWORD *)&Event[22].Header.Lock )                                                                                          | 
                  {                                                                                                                                  | 
                    v34 = (struct _LIST_ENTRY *)ExAllocatePool2(0x100uLL, 0x28uLL, 0x614A7350u);                                                     | 
                    P = v34;                                                                                                                         | 
                    if ( !v34 )                                                                                                                      | 
                    {                                                                                                                                | 
                      goto LABEL_123;                                                                                                                | 
                    }                                                                                                                                | 
                    v147 = PsChargeSharedPoolQuota(KeGetCurrentThread()->ApcState.Process, 40LL);                                                    | 
                    if ( !v147 )                                                                                                                     | 
                    {                                                                                                                                | 
                      v35 = v34;                                                                                                                     | 
LABEL_126:                                                                                                                                           | 
                      v36 = 1632269136;                                                                                                              | 
LABEL_127:                                                                                                                                           | 
                      ExFreePoolWithTag(v35, v36);                                                                                                   | 
                      v12 = -1073741670;                                                                                                             | 
                      goto LABEL_446;                                                                                                                | 
                    }                                                                                                                                | 
                    v21 = Event;                                                                                                                     | 
                    v144 = v220;                                                                                                                     | 
                    v12 = v139;                                                                                                                      | 
                  }                                                                                                                                  | 
                  SeCaptureSubjectContextEx(                                                                                                         | 
                    Thread,                                                                                                                          | 
                    Thread->ApcState.Process,                                                                                                        | 
                    (PSECURITY_SUBJECT_CONTEXT)&SubjectContext.ImpersonationLevel);                                                                  | 
                  v156 = (PSECURITY_SUBJECT_CONTEXT)&SubjectContext.ImpersonationLevel;                                                              | 
                  if ( PreviousMode )                                                                                                                | 
                  {                                                                                                                                  | 
                    RequiredPrivileges.PrivilegeCount = 1;                                                                                           | 
                    RequiredPrivileges.Control = 1;                                                                                                  | 
                    RequiredPrivileges.Privilege[0].Luid = SeDebugPrivilege;                                                                         | 
                    RequiredPrivileges.Privilege[0].Attributes = 0;                                                                                  | 
                    v37 = SePrivilegeCheck(                                                                                                          | 
                            &RequiredPrivileges,                                                                                                     | 
                            (PSECURITY_SUBJECT_CONTEXT)&SubjectContext.ImpersonationLevel,                                                           | 
                            1);                                                                                                                      | 
                    LODWORD(SubjectContext.ClientToken) = (__int64)SubjectContext.ClientToken & 0xFFFFFFFE | v37 & 1;                                | 
                  }                                                                                                                                  | 
                  else                                                                                                                               | 
                  {                                                                                                                                  | 
                    LODWORD(SubjectContext.ClientToken) |= 1u;                                                                                       | 
                  }                                                                                                                                  | 
                  v13 |= 2u;                                                                                                                         | 
                  v32 = v144;                                                                                                                        | 
LABEL_133:                                                                                                                                           | 
                  ExAcquireResourceExclusiveLite((PERESOURCE)&v21[2].Header.WaitListHead, 1u);                                                       | 
                  if ( (v32 & 4) == 0 )                                                                                                              | 
                  {                                                                                                                                  | 
                    if ( (v32 & 0x40) != 0 )                                                                                                         | 
                    {                                                                                                                                | 
                      v32 |= (__int64)v21[10].Header.WaitListHead.Blink & 4;                                                                         | 
                      Blink = v21[9].Header.WaitListHead.Blink;                                                                                      | 
                    }                                                                                                                                | 
                    else                                                                                                                             | 
                    {                                                                                                                                | 
                      Blink = 0LL;                                                                                                                   | 
                    }                                                                                                                                | 
                  }                                                                                                                                  | 
                  v38 = v32 & 0xFFFFFFBF;                                                                                                            | 
                  v220 = v38;                                                                                                                        | 
                  if ( (v13 & 2) != 0 )                                                                                                              | 
                  {                                                                                                                                  | 
                    if ( ((__int64)v21[64].Header.WaitListHead.Blink & 2) != 0 )                                                                     | 
                    {                                                                                                                                | 
LABEL_139:                                                                                                                                           | 
                      v12 = -1073741811;                                                                                                             | 
                      v139 = -1073741811;                                                                                                            | 
LABEL_140:                                                                                                                                           | 
                      v39 = (struct _LIST_ENTRY *)v147;                                                                                              | 
LABEL_173:                                                                                                                                           | 
                      if ( (v13 & 1) != 0 )                                                                                                          | 
                      {                                                                                                                              | 
                        v46 = KeAbPreAcquire(&qword_140FC60E8, 0LL, 0LL);                                                                            | 
                        v47 = v46;                                                                                                                   | 
                        if ( _interlockedbittestandset64((volatile signed __int32 *)&qword_140FC60E8, 0LL) )                                         | 
                        {                                                                                                                            | 
                          ExfAcquirePushLockExclusiveEx(&qword_140FC60E8, v46, &qword_140FC60E8);                                                    | 
                        }                                                                                                                            | 
                        if ( v47 )                                                                                                                   | 
                        {                                                                                                                            | 
                          *(_BYTE *)(v47 + 10) = 1;                                                                                                  | 
                        }                                                                                                                            | 
                        _InterlockedOr((volatile signed __int32 *)&Event[64].Header.WaitListHead.Blink, 0x100u);                                     | 
                        v21 = Event;                                                                                                                 | 
                        p_Blink = (struct _LIST_ENTRY *)&Event[1].Header.WaitListHead.Blink;                                                         | 
                        for ( i = Event[1].Header.WaitListHead.Blink; i != p_Blink; i = i->Flink )                                                   | 
                        {                                                                                                                            | 
                          if ( ((__int64)i[-23].Blink & 1) == 0 )                                                                                    | 
                          {                                                                                                                          | 
                            PspAddProcessToWorkingSetChangeList(&i[-54].Blink);                                                                      | 
                          }                                                                                                                          | 
                        }                                                                                                                            | 
                        v12 = v139;                                                                                                                  | 
                      }                                                                                                                              | 
                      ExReleaseResourceLite((PERESOURCE)&v21[2].Header.WaitListHead);                                                                | 
                      if ( (v13 & 1) != 0 )                                                                                                          | 
                      {                                                                                                                              | 
                        PspApplyWorkingSetLimits(v21);                                                                                               | 
                      }                                                                                                                              | 
                      if ( v156 )                                                                                                                    | 
                      {                                                                                                                              | 
                        SeReleaseSubjectContext(v156);                                                                                               | 
                      }                                                                                                                              | 
                      if ( !P )                                                                                                                      | 
                      {                                                                                                                              | 
                        goto LABEL_485;                                                                                                              | 
                      }                                                                                                                              | 
                      ExFreePoolWithTag(P, 0x614A7350u);                                                                                             | 
                      v50 = v39;                                                                                                                     | 
                      goto LABEL_229;                                                                                                                | 
                    }                                                                                                                                | 
                    if ( ((__int64)v21[10].Header.WaitListHead.Blink & 0x10) != 0 )                                                                  | 
                    {                                                                                                                                | 
                      KeFirstGroupAffinityEx(&v165, &v21[11]);                                                                                       | 
                      PrimaryGroupThread = v165.m128i_u16[4];                                                                                        | 
                    }                                                                                                                                | 
                    else                                                                                                                             | 
                    {                                                                                                                                | 
                      PrimaryGroupThread = KeQueryPrimaryGroupThread(KeGetCurrentThread());                                                          | 
                      v165.m128i_i16[4] = PrimaryGroupThread;                                                                                        | 
                    }                                                                                                                                | 
                    if ( v207 != (KeActiveProcessors.Bitmap[PrimaryGroupThread] & v207) )                                                            | 
                    {                                                                                                                                | 
                      v21 = Event;                                                                                                                   | 
                      goto LABEL_139;                                                                                                                | 
                    }                                                                                                                                | 
                    KeAddGroupAffinityEx(&v222, v165.m128i_u16[4]);                                                                                  | 
                    v21 = Event;                                                                                                                     | 
                    v12 = PspEnumJobsAndProcessesInJobHierarchy(Event, (__int64)&SubjectContext, 1);                                                 | 
                    v139 = v12;                                                                                                                      | 
                    if ( v12 < 0 )                                                                                                                   | 
                    {                                                                                                                                | 
                      goto LABEL_140;                                                                                                                | 
                    }                                                                                                                                | 
                    v41 = *(_QWORD *)&v21[22].Header.Lock;                                                                                           | 
                    v39 = (struct _LIST_ENTRY *)v147;                                                                                                | 
                    if ( v41 )                                                                                                                       | 
                    {                                                                                                                                | 
                      v172 = *(_OWORD *)(v41 + 8);                                                                                                   | 
                      v173 = *(_OWORD *)(v41 + 24);                                                                                                  | 
                      v156 = (PSECURITY_SUBJECT_CONTEXT)&v172;                                                                                       | 
                    }                                                                                                                                | 
                    else                                                                                                                             | 
                    {                                                                                                                                | 
                      *(_QWORD *)&v21[22].Header.Lock = P;                                                                                           | 
                      v21[22].Header.WaitListHead.Flink = v39;                                                                                       | 
                      P = 0LL;                                                                                                                       | 
                      v156 = 0LL;                                                                                                                    | 
                    }                                                                                                                                | 
                    v42 = *(struct _SECURITY_SUBJECT_CONTEXT **)&v21[22].Header.Lock;                                                                | 
                    *v42 = SubjectContext;                                                                                                           | 
                    v42[1].ClientToken = v159;                                                                                                       | 
                  }                                                                                                                                  | 
                  else                                                                                                                               | 
                  {                                                                                                                                  | 
                    v45 = (v38 & 0x4000) != 0;                                                                                                       | 
                    v39 = (struct _LIST_ENTRY *)v147;                                                                                                | 
                    if ( v45 && ((__int64)v21[64].Header.WaitListHead.Blink & 2) == 0 )                                                              | 
                    {                                                                                                                                | 
                      v12 = -1073741811;                                                                                                             | 
                      v139 = -1073741811;                                                                                                            | 
                      goto LABEL_173;                                                                                                                | 
                    }                                                                                                                                | 
                  }                                                                                                                                  | 
                  if ( ((__int64)v21[10].Header.WaitListHead.Blink & 1) != 0 && (v220 & 1) == 0 )                                                    | 
                  {                                                                                                                                  | 
                    _InterlockedOr(v136, 0);                                                                                                         | 
                    if ( (qword_140FC60E8 & 1) != 0 )                                                                                                | 
                    {                                                                                                                                | 
                      ExfAcquireReleasePushLockExclusive((ULONG_PTR)&qword_140FC60E8);                                                               | 
                    }                                                                                                                                | 
                    v12 = v139;                                                                                                                      | 
                    v21 = Event;                                                                                                                     | 
                  }                                                                                                                                  | 
                  v160[0] = (__int64)v21;                                                                                                            | 
                  HIDWORD(v160[1]) = v21[10].Header.WaitListHead.Blink;                                                                              | 
                  *(_QWORD *)&v21[10].Header.Lock = v218;                                                                                            | 
                  v21[10].Header.WaitListHead.Flink = v219;                                                                                          | 
                  HIDWORD(v21[10].Header.WaitListHead.Blink) = v221;                                                                                 | 
                  if ( (v13 & 2) != 0 )                                                                                                              | 
                  {                                                                                                                                  | 
                    v43 = v21 + 11;                                                                                                                  | 
                    v44 = &v222;                                                                                                                     | 
                    do                                                                                                                               | 
                    {                                                                                                                                | 
                      *(_OWORD *)&v43->Header.Lock = *(_OWORD *)v44;                                                                                 | 
                      *(_OWORD *)&v43->Header.WaitListHead.Blink = *((_OWORD *)v44 + 1);                                                             | 
                      v43[1].Header.WaitListHead = (LIST_ENTRY)*((_OWORD *)v44 + 2);                                                                 | 
                      *(_OWORD *)&v43[2].Header.Lock = *((_OWORD *)v44 + 3);                                                                         | 
                      *(_OWORD *)&v43[2].Header.WaitListHead.Blink = *((_OWORD *)v44 + 4);                                                           | 
                      v43[3].Header.WaitListHead = (LIST_ENTRY)*((_OWORD *)v44 + 5);                                                                 | 
                      *(_OWORD *)&v43[4].Header.Lock = *((_OWORD *)v44 + 6);                                                                         | 
                      v43 = (struct _KEVENT *)((char *)v43 + 128);                                                                                   | 
                      v43[-1].Header.WaitListHead = (LIST_ENTRY)*((_OWORD *)v44 + 7);                                                                | 
                      v44 += 16;                                                                                                                     | 
                      --v33;                                                                                                                         | 
                    }                                                                                                                                | 
                    while ( v33 );                                                                                                                   | 
                    *(_QWORD *)&v43->Header.Lock = *v44;                                                                                             | 
                  }                                                                                                                                  | 
                  BYTE5(v21[45].Header.WaitListHead.Blink) = v228;                                                                                   | 
                  v21[24].Header.SignalState = v224;                                                                                                 | 
                  v21[9].Header.WaitListHead.Flink = v216;                                                                                           | 
                  v21[9].Header.WaitListHead.Blink = Blink;                                                                                          | 
                  if ( v142 == 9 )                                                                                                                   | 
                  {                                                                                                                                  | 
                    PspLockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                  | 
                    LODWORD(v21[10].Header.WaitListHead.Blink) = v220 | (__int64)v21[10].Header.WaitListHead.Blink & ~JobLimitInformationValidFlags; | 
                    v21[28].Header.WaitListHead.Blink = v225;                                                                                        | 
                    *(_QWORD *)&v21[29].Header.Lock = v226;                                                                                          | 
                    v21[29].Header.WaitListHead.Flink = v227;                                                                                        | 
                    PspUnlockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                | 
                  }                                                                                                                                  | 
                  else                                                                                                                               | 
                  {                                                                                                                                  | 
                    LODWORD(v21[10].Header.WaitListHead.Blink) = v220 | (__int64)v21[10].Header.WaitListHead.Blink & ~JobLimitInformationValidFlags; | 
                  }                                                                                                                                  | 
                  LODWORD(v160[1]) = ~(LODWORD(v21[10].Header.WaitListHead.Blink) | HIDWORD(v160[1]));                                               | 
                  if ( (v203 & 4) != 0 )                                                                                                             | 
                  {                                                                                                                                  | 
                    PspEnumJobsAndProcessesInJobHierarchy(v21, (__int64)&v21[9].Header.WaitListHead.Blink, 1);                                       | 
                    v21[7].Header.WaitListHead.Blink = 0LL;                                                                                          | 
                    *(_QWORD *)&v21[8].Header.Lock = 0LL;                                                                                            | 
                    KeResetEvent(v21);                                                                                                               | 
                  }                                                                                                                                  | 
                  if ( ((__int64)v21[10].Header.WaitListHead.Blink & 6) != 0 )                                                                       | 
                  {                                                                                                                                  | 
                    _InterlockedAdd64(&PspJobTimeLimitsRequest, 1uLL);                                                                               | 
                    v21 = Event;                                                                                                                     | 
                    v12 = v139;                                                                                                                      | 
                  }                                                                                                                                  | 
                  if ( (v160[1] & 1) == 0 )                                                                                                          | 
                  {                                                                                                                                  | 
                    v13 |= 1u;                                                                                                                       | 
                  }                                                                                                                                  | 
                  PspEnumJobsAndProcessesInJobHierarchy(v21, (__int64)v160, 5);                                                                      | 
                  goto LABEL_173;                                                                                                                    | 
                }                                                                                                                                    | 
                goto LABEL_460;                                                                                                                      | 
              }                                                                                                                                      | 
LABEL_456:                                                                                                                                           | 
              v12 = -1073741811;                                                                                                                     | 
              goto LABEL_446;                                                                                                                        | 
            }                                                                                                                                        | 
LABEL_78:                                                                                                                                            | 
            v12 = -1073741727;                                                                                                                       | 
            goto LABEL_446;                                                                                                                          | 
          }                                                                                                                                          | 
          if ( v142 != 4 )                                                                                                                           | 
          {                                                                                                                                          | 
            if ( v142 == 5 )                                                                                                                         | 
            {                                                                                                                                        | 
              v174 = *v9;                                                                                                                            | 
              v175 = v9[1];                                                                                                                          | 
              v176.m128i_i64[0] = v9[2].m128i_i64[0];                                                                                                | 
              v12 = (v174.m128i_i32[0] & 0xFFFFFFF0) != 0 ? -1073741811 : -1073741637;                                                               | 
LABEL_446:                                                                                                                                           | 
              v21 = Event;                                                                                                                           | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
            if ( v142 != 6 )                                                                                                                         | 
            {                                                                                                                                        | 
              if ( v142 == 7 )                                                                                                                       | 
              {                                                                                                                                      | 
                MiniCompletionPacket = 0LL;                                                                                                          | 
                *(__m128i *)BugCheckParameter1a = *v9;                                                                                               | 
                if ( BugCheckParameter1a[1] )                                                                                                        | 
                {                                                                                                                                    | 
                  v12 = ObpReferenceObjectByHandleWithTag(                                                                                           | 
                          BugCheckParameter1a[1],                                                                                                    | 
                          0x624A7350u,                                                                                                               | 
                          (__int64)Object,                                                                                                           | 
                          0LL,                                                                                                                       | 
                          0LL);                                                                                                                      | 
                  v21 = Event;                                                                                                                       | 
                  if ( v12 >= 0 )                                                                                                                    | 
                  {                                                                                                                                  | 
                    if ( Event[51].Header.WaitListHead.Flink                                                                                         | 
                      || (MiniCompletionPacket = IoAllocateMiniCompletionPacket(&PspNotificationPacketCallback, Event)) != 0 )                       | 
                    {                                                                                                                                | 
                      ExAcquireResourceExclusiveLite((PERESOURCE)&v21[2].Header.WaitListHead, 1u);                                                   | 
                      if ( *(_QWORD *)&v21[23].Header.Lock                                                                                           | 
                        || ((__int64)v21[10].Header.WaitListHead.Blink & 0x2000) != 0                                                                | 
                        && ((__int64)v21[64].Header.WaitListHead.Blink & 1) != 0 )                                                                   | 
                      {                                                                                                                              | 
                        ExReleaseResourceLite((PERESOURCE)&v21[2].Header.WaitListHead);                                                              | 
                        ObfDereferenceObjectWithTag(Object[0], 0x624A7350u);                                                                         | 
                        v12 = -1073741811;                                                                                                           | 
                      }                                                                                                                              | 
                      else                                                                                                                           | 
                      {                                                                                                                              | 
                        if ( !v21[51].Header.WaitListHead.Flink )                                                                                    | 
                        {                                                                                                                            | 
                          v21[51].Header.WaitListHead.Flink = (struct _LIST_ENTRY *)MiniCompletionPacket;                                            | 
                          MiniCompletionPacket = 0LL;                                                                                                | 
                        }                                                                                                                            | 
                        PspLockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                              | 
                        v21[23].Header.WaitListHead.Flink = (struct _LIST_ENTRY *)BugCheckParameter1a[0];                                            | 
                        *(PVOID *)&v21[23].Header.Lock = Object[0];                                                                                  | 
                        v21[23].Header.WaitListHead.Blink = 0LL;                                                                                     | 
                        PspUnlockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                            | 
                        if ( (v21[46].Header.LockNV & 0x40) != 0 )                                                                                   | 
                        {                                                                                                                            | 
                          PspEnumJobsAndProcessesInJobHierarchy(v21, (__int64)v21, 1);                                                               | 
                        }                                                                                                                            | 
                        ExReleaseResourceLite((PERESOURCE)&v21[2].Header.WaitListHead);                                                              | 
                      }                                                                                                                              | 
                    }                                                                                                                                | 
                    else                                                                                                                             | 
                    {                                                                                                                                | 
                      v12 = -1073741670;                                                                                                             | 
                    }                                                                                                                                | 
                  }                                                                                                                                  | 
                  if ( MiniCompletionPacket )                                                                                                        | 
                  {                                                                                                                                  | 
                    *(_QWORD *)(MiniCompletionPacket + 56) = 0LL;                                                                                    | 
                    IopFreeMiniCompletionPacket(MiniCompletionPacket);                                                                               | 
                  }                                                                                                                                  | 
                }                                                                                                                                    | 
                else                                                                                                                                 | 
                {                                                                                                                                    | 
                  v21 = Event;                                                                                                                       | 
                  ExAcquireResourceExclusiveLite((PERESOURCE)&Event[2].Header.WaitListHead, 1u);                                                     | 
                  PspLockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                    | 
                  v22 = *(void **)&v21[23].Header.Lock;                                                                                              | 
                  *(_QWORD *)&v21[23].Header.Lock = 0LL;                                                                                             | 
                  PspUnlockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                  | 
                  ExReleaseResourceLite((PERESOURCE)&v21[2].Header.WaitListHead);                                                                    | 
                  if ( v22 )                                                                                                                         | 
                  {                                                                                                                                  | 
                    ObfDereferenceObjectWithTag(v22, 0x624A7350u);                                                                                   | 
                  }                                                                                                                                  | 
                }                                                                                                                                    | 
                goto LABEL_485;                                                                                                                      | 
              }                                                                                                                                      | 
              if ( v142 != 9 )                                                                                                                       | 
              {                                                                                                                                      | 
                if ( v142 != 11 )                                                                                                                    | 
                {                                                                                                                                    | 
                  if ( v142 == 12 )                                                                                                                  | 
                  {                                                                                                                                  | 
                    goto LABEL_42;                                                                                                                   | 
                  }                                                                                                                                  | 
LABEL_445:                                                                                                                                           | 
                  v12 = -1073741821;                                                                                                                 | 
                  goto LABEL_446;                                                                                                                    | 
                }                                                                                                                                    | 
                goto LABEL_190;                                                                                                                      | 
              }                                                                                                                                      | 
              goto LABEL_70;                                                                                                                         | 
            }                                                                                                                                        | 
            v166 = v9->m128i_i32[0];                                                                                                                 | 
            v23 = v166;                                                                                                                              | 
            v21 = Event;                                                                                                                             | 
            if ( v166 <= 1 )                                                                                                                         | 
            {                                                                                                                                        | 
              ExAcquireResourceExclusiveLite((PERESOURCE)&Event[2].Header.WaitListHead, 1u);                                                         | 
              HIDWORD(v21[22].Header.WaitListHead.Blink) = v23;                                                                                      | 
LABEL_66:                                                                                                                                            | 
              p_WaitListHead = (struct _ERESOURCE *)&v21[2].Header.WaitListHead;                                                                     | 
LABEL_67:                                                                                                                                            | 
              ExReleaseResourceLite(p_WaitListHead);                                                                                                 | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
LABEL_460:                                                                                                                                           | 
            v12 = -1073741811;                                                                                                                       | 
            goto LABEL_485;                                                                                                                          | 
          }                                                                                                                                          | 
          v167 = v9->m128i_i32[0];                                                                                                                   | 
          v21 = Event;                                                                                                                               | 
          Silo = PspSetUILimitJobObject((__int64)Event);                                                                                             | 
          goto LABEL_484;                                                                                                                            | 
        }                                                                                                                                            | 
        if ( v142 != 15 )                                                                                                                            | 
        {                                                                                                                                            | 
          if ( v142 != 16 )                                                                                                                          | 
          {                                                                                                                                          | 
            if ( v142 == 18 )                                                                                                                        | 
            {                                                                                                                                        | 
              v193 = *v9;                                                                                                                            | 
              if ( !v193.m128i_i32[0] || (v193.m128i_i32[0] & 0xFFFFFFF0) != 0 )                                                                     | 
              {                                                                                                                                      | 
                goto LABEL_73;                                                                                                                       | 
              }                                                                                                                                      | 
              if ( (v193.m128i_i8[0] & 1) != 0 || (v193.m128i_i8[0] & 8) == 0 )                                                                      | 
              {                                                                                                                                      | 
                v21 = Event;                                                                                                                         | 
                v12 = PspFreezeJobTree(Event, &v193);                                                                                                | 
                if ( v12 >= 0 )                                                                                                                      | 
                {                                                                                                                                    | 
                  v9->m128i_i32[0] = v193.m128i_i32[0];                                                                                              | 
                }                                                                                                                                    | 
                goto LABEL_485;                                                                                                                      | 
              }                                                                                                                                      | 
              goto LABEL_456;                                                                                                                        | 
            }                                                                                                                                        | 
            if ( v142 != 21 )                                                                                                                        | 
            {                                                                                                                                        | 
              switch ( v142 )                                                                                                                        | 
              {                                                                                                                                      | 
                case 22:                                                                                                                             | 
                  LOBYTE(v140) = v9->m128i_i8[0];                                                                                                    | 
                  v21 = Event;                                                                                                                       | 
                  ExAcquireResourceExclusiveLite((PERESOURCE)&Event[2].Header.WaitListHead, 1u);                                                     | 
                  if ( ((__int64)v21[64].Header.WaitListHead.Blink & 0x20) == 0 )                                                                    | 
                  {                                                                                                                                  | 
                    goto LABEL_238;                                                                                                                  | 
                  }                                                                                                                                  | 
                  v69 = v21[51].Header.WaitListHead.Blink;                                                                                           | 
                  v70 = (unsigned int)v69[2].Blink;                                                                                                  | 
                  if ( (v70 & 0x40) != 0 )                                                                                                           | 
                  {                                                                                                                                  | 
                    goto LABEL_238;                                                                                                                  | 
                  }                                                                                                                                  | 
                  if ( (v70 & 0x21) != 0 )                                                                                                           | 
                  {                                                                                                                                  | 
                    v12 = -1073741637;                                                                                                               | 
                    goto LABEL_66;                                                                                                                   | 
                  }                                                                                                                                  | 
                  if ( ((v70 >> 3) & 1) == ((_BYTE)v140 != 0) )                                                                                      | 
                  {                                                                                                                                  | 
                    goto LABEL_238;                                                                                                                  | 
                  }                                                                                                                                  | 
                  LOBYTE(v68) = -(char)v140;                                                                                                         | 
                  v71 = ((_BYTE)v140 != 0 ? 8 : 0) | v70 & 0xFFFFFFF7;                                                                               | 
                  LODWORD(v69[2].Blink) = v71;                                                                                                       | 
                  LOBYTE(v71) = v140;                                                                                                                | 
                  KeSetSchedulingGroupRankBias(&v21[51].Header.WaitListHead.Blink[8], v71, v68);                                                     | 
                  PspEnumJobsAndProcessesInJobHierarchy(v21, (__int64)&v140, 1);                                                                     | 
                  break;                                                                                                                             | 
                case 23:                                                                                                                             | 
                  if ( v9->m128i_i8[0] != 1 )                                                                                                        | 
                  {                                                                                                                                  | 
                    v12 = -1073741811;                                                                                                               | 
                    v139 = -1073741811;                                                                                                              | 
                    v21 = Event;                                                                                                                     | 
                    goto LABEL_485;                                                                                                                  | 
                  }                                                                                                                                  | 
                  v21 = Event;                                                                                                                       | 
                  PspEnumJobsAndProcessesInJobHierarchy(Event, 0LL, 0);                                                                              | 
                  goto LABEL_315;                                                                                                                    | 
                case 24:                                                                                                                             | 
                  v65 = v9->m128i_i64[0];                                                                                                            | 
                  v179 = v9->m128i_i64[0];                                                                                                           | 
                  v21 = Event;                                                                                                                       | 
                  ExAcquireResourceExclusiveLite((PERESOURCE)&Event[2].Header.WaitListHead, 1u);                                                     | 
                  if ( ((__int64)v21[64].Header.WaitListHead.Blink & 0x20) == 0 )                                                                    | 
                  {                                                                                                                                  | 
LABEL_238:                                                                                                                                           | 
                    v12 = -1073741811;                                                                                                               | 
                    goto LABEL_66;                                                                                                                   | 
                  }                                                                                                                                  | 
                  v66 = v21[51].Header.WaitListHead.Blink;                                                                                           | 
                  v67 = (int)v66[2].Blink;                                                                                                           | 
                  if ( (v67 & 0x10) == 0 )                                                                                                           | 
                  {                                                                                                                                  | 
                    LODWORD(v66[2].Blink) = v67 | 0x10;                                                                                              | 
                    KeInitializeDpc((PRKDPC)&v66[3], PspJobCycleTimeNotificationDpcRoutine, v21);                                                    | 
                  }                                                                                                                                  | 
                  KeSetSchedulingGroupCycleNotification(&v66[8], &v66[3], v65);                                                                      | 
                  break;                                                                                                                             | 
                default:                                                                                                                             | 
                  goto LABEL_445;                                                                                                                    | 
              }                                                                                                                                      | 
              v12 = 0;                                                                                                                               | 
              goto LABEL_66;                                                                                                                         | 
            }                                                                                                                                        | 
            v150 = v9->m128i_i8[0];                                                                                                                  | 
            v21 = Event;                                                                                                                             | 
            v72 = PspSetBackgroundJobTree(Event);                                                                                                    | 
LABEL_253:                                                                                                                                           | 
            v12 = v72;                                                                                                                               | 
            if ( v72 < 0 )                                                                                                                           | 
            {                                                                                                                                        | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
            goto LABEL_315;                                                                                                                          | 
          }                                                                                                                                          | 
          v168 = v9->m128i_i32[0];                                                                                                                   | 
          v73 = v168;                                                                                                                                | 
          v21 = Event;                                                                                                                               | 
          if ( (v168 & 0xFFFFC001) != 0 )                                                                                                            | 
          {                                                                                                                                          | 
            goto LABEL_460;                                                                                                                          | 
          }                                                                                                                                          | 
          ExAcquireResourceExclusiveLite((PERESOURCE)&Event[2].Header.WaitListHead, 1u);                                                             | 
          PspLockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                            | 
          v21[46].Header.LockNV = v73;                                                                                                               | 
          PspUnlockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                          | 
          goto LABEL_314;                                                                                                                            | 
        }                                                                                                                                            | 
        memmove(&v161, v9, v4);                                                                                                                      | 
        v74 = v161;                                                                                                                                  | 
        if ( (v161 & 0xFFFFFFC0) != 0 )                                                                                                              | 
        {                                                                                                                                            | 
          goto LABEL_456;                                                                                                                            | 
        }                                                                                                                                            | 
        v75 = 0;                                                                                                                                     | 
        LODWORD(v145) = HIDWORD(v161);                                                                                                               | 
        if ( (v161 & 1) == 0 )                                                                                                                       | 
        {                                                                                                                                            | 
LABEL_278:                                                                                                                                           | 
          v21 = Event;                                                                                                                               | 
          PspLockJobChain(Event, Thread, 0LL);                                                                                                       | 
          v77 = (int)v21[64].Header.WaitListHead.Blink;                                                                                              | 
          if ( (v74 & 1) != 0 )                                                                                                                      | 
          {                                                                                                                                          | 
            if ( (v77 & 0x20) == 0 )                                                                                                                 | 
            {                                                                                                                                        | 
              RateControl = PspAllocateRateControl(2LL);                                                                                             | 
              v79 = RateControl;                                                                                                                     | 
              if ( !RateControl )                                                                                                                    | 
              {                                                                                                                                      | 
                v12 = -1073741670;                                                                                                                   | 
                goto LABEL_312;                                                                                                                      | 
              }                                                                                                                                      | 
              v21[51].Header.WaitListHead.Blink = (struct _LIST_ENTRY *)RateControl;                                                                 | 
LABEL_286:                                                                                                                                           | 
              LODWORD(v21[51].Header.WaitListHead.Blink[2].Blink) = 0;                                                                               | 
              v80 = WORD2(v161);                                                                                                                     | 
              HIDWORD(v21[51].Header.WaitListHead.Blink[2].Blink) = HIDWORD(v161);                                                                   | 
              if ( (v74 & 1) != 0 )                                                                                                                  | 
              {                                                                                                                                      | 
                if ( (v74 & 4) != 0 )                                                                                                                | 
                {                                                                                                                                    | 
                  LODWORD(v21[51].Header.WaitListHead.Blink[2].Blink) |= 1u;                                                                         | 
                  WORD1(v145) = v80;                                                                                                                 | 
                }                                                                                                                                    | 
                if ( (v74 & 2) != 0 )                                                                                                                | 
                {                                                                                                                                    | 
                  LODWORD(v21[51].Header.WaitListHead.Blink[2].Blink) |= 4u;                                                                         | 
                  HIDWORD(v145) = v75 & 0xFFFFFFFE;                                                                                                  | 
                }                                                                                                                                    | 
                else                                                                                                                                 | 
                {                                                                                                                                    | 
                  HIDWORD(v145) = v75 | 1;                                                                                                           | 
                  if ( (v74 & 0x14) == 0 )                                                                                                           | 
                  {                                                                                                                                  | 
                    WORD1(v145) = 10000;                                                                                                             | 
                  }                                                                                                                                  | 
                }                                                                                                                                    | 
                if ( (v74 & 8) != 0 )                                                                                                                | 
                {                                                                                                                                    | 
                  LODWORD(v21[51].Header.WaitListHead.Blink[2].Blink) |= 2u;                                                                         | 
                }                                                                                                                                    | 
                if ( (v74 & 0x10) != 0 )                                                                                                             | 
                {                                                                                                                                    | 
                  LODWORD(v21[51].Header.WaitListHead.Blink[2].Blink) |= 0x20u;                                                                      | 
                }                                                                                                                                    | 
                if ( (v74 & 0x20) != 0 )                                                                                                             | 
                {                                                                                                                                    | 
                  LODWORD(v21[51].Header.WaitListHead.Blink[2].Blink) |= 0x80u;                                                                      | 
                }                                                                                                                                    | 
                v81 = v21[51].Header.WaitListHead.Blink;                                                                                             | 
                v82 = v81 + 8;                                                                                                                       | 
                if ( v81 == (struct _LIST_ENTRY *)v79 )                                                                                              | 
                {                                                                                                                                    | 
                  v82->Flink = v145;                                                                                                                 | 
                  v12 = PspAddSchedulingGroupToJobChain(v21[54].Header.WaitListHead.Flink, v21);                                                     | 
                  v139 = v12;                                                                                                                        | 
                  if ( v12 < 0 )                                                                                                                     | 
                  {                                                                                                                                  | 
                    v83 = v21[51].Header.WaitListHead.Blink;                                                                                         | 
                    if ( v83 )                                                                                                                       | 
                    {                                                                                                                                | 
                      PspFreeRateControl(v83);                                                                                                       | 
                      v21[51].Header.WaitListHead.Blink = 0LL;                                                                                       | 
                    }                                                                                                                                | 
                    goto LABEL_312;                                                                                                                  | 
                  }                                                                                                                                  | 
                  _InterlockedOr((volatile signed __int32 *)&v21[64].Header.WaitListHead.Blink, 0x20u);                                              | 
                  v21 = Event;                                                                                                                       | 
                }                                                                                                                                    | 
                else                                                                                                                                 | 
                {                                                                                                                                    | 
                  BugCheckParameter1a[0] = (ULONG_PTR)&v81[8];                                                                                       | 
                  if ( ((__int64)v81[2].Blink & 4) != 0 )                                                                                            | 
                  {                                                                                                                                  | 
                    KeSetSchedulingGroupWeights(1LL, BugCheckParameter1a, &v145);                                                                    | 
                  }                                                                                                                                  | 
                  else                                                                                                                               | 
                  {                                                                                                                                  | 
                    KeSetSchedulingGroupCpuRates(v82, BugCheckParameter1a, &v145);                                                                   | 
                  }                                                                                                                                  | 
                }                                                                                                                                    | 
                v162 = v21[51].Header.WaitListHead.Blink[2].Blink;                                                                                   | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                HIDWORD(v145) = v75 | 3;                                                                                                             | 
                LODWORD(v145) = 655370000;                                                                                                           | 
                LODWORD(v21[51].Header.WaitListHead.Blink[2].Blink) |= 0x40u;                                                                        | 
                v84 = (int)v145;                                                                                                                     | 
                HIDWORD(v21[51].Header.WaitListHead.Blink[2].Blink) = (_DWORD)v145;                                                                  | 
                BugCheckParameter1a[0] = (ULONG_PTR)&v21[51].Header.WaitListHead.Blink[8];                                                           | 
                HIBYTE(v140) = (*(_DWORD *)(BugCheckParameter1a[0] + 4) & 4) != 0;                                                                   | 
                v21[52].Header.WaitListHead.Flink = (struct _LIST_ENTRY *)((char *)v21[52].Header.WaitListHead.Flink                                 | 
                                                                         + KeQuerySchedulingGroupReadyTime(                                          | 
                                                                             BugCheckParameter1a[0],                                                 | 
                                                                             v79));                                                                  | 
                KeSetSchedulingGroupCpuRates(v85, BugCheckParameter1a, &v145);                                                                       | 
                HIDWORD(v162) = v84;                                                                                                                 | 
                if ( HIBYTE(v140) )                                                                                                                  | 
                {                                                                                                                                    | 
                  HIBYTE(v140) = 0;                                                                                                                  | 
                  PspEnumJobsAndProcessesInJobHierarchy(v21, (__int64)&v140 + 1, 1);                                                                 | 
                }                                                                                                                                    | 
              }                                                                                                                                      | 
              v12 = 0;                                                                                                                               | 
              if ( (PerfGlobalGroupMask & 0x80000) != 0 )                                                                                            | 
              {                                                                                                                                      | 
                EtwTraceJobSetQuery((_DWORD)v21, 15, (unsigned int)&v162, 0, 0, 1829);                                                               | 
              }                                                                                                                                      | 
LABEL_312:                                                                                                                                           | 
              PspUnlockJobChain(v21, Thread);                                                                                                        | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
          }                                                                                                                                          | 
          else if ( (v77 & 0x20) == 0 )                                                                                                              | 
          {                                                                                                                                          | 
            v12 = -1073741811;                                                                                                                       | 
            goto LABEL_312;                                                                                                                          | 
          }                                                                                                                                          | 
          v79 = 0LL;                                                                                                                                 | 
          goto LABEL_286;                                                                                                                            | 
        }                                                                                                                                            | 
        if ( (v161 & 2) != 0 )                                                                                                                       | 
        {                                                                                                                                            | 
          if ( (v161 & 0x10) != 0 )                                                                                                                  | 
          {                                                                                                                                          | 
            goto LABEL_456;                                                                                                                          | 
          }                                                                                                                                          | 
          v76 = (unsigned int)(HIDWORD(v161) - 1) <= 8;                                                                                              | 
        }                                                                                                                                            | 
        else                                                                                                                                         | 
        {                                                                                                                                            | 
          if ( (v161 & 0x10) == 0 )                                                                                                                  | 
          {                                                                                                                                          | 
            if ( (unsigned int)(HIDWORD(v161) - 1) > 0x270F )                                                                                        | 
            {                                                                                                                                        | 
              goto LABEL_456;                                                                                                                        | 
            }                                                                                                                                        | 
            goto LABEL_276;                                                                                                                          | 
          }                                                                                                                                          | 
          if ( (v161 & 4) != 0 || !WORD2(v161) || WORD2(v161) > HIWORD(v161) )                                                                       | 
          {                                                                                                                                          | 
            goto LABEL_456;                                                                                                                          | 
          }                                                                                                                                          | 
          v76 = HIWORD(v161) <= 0x2710u;                                                                                                             | 
        }                                                                                                                                            | 
        if ( !v76 )                                                                                                                                  | 
        {                                                                                                                                            | 
          goto LABEL_456;                                                                                                                            | 
        }                                                                                                                                            | 
LABEL_276:                                                                                                                                           | 
        if ( (v161 & 0x20) != 0 )                                                                                                                    | 
        {                                                                                                                                            | 
          v75 = 8;                                                                                                                                   | 
        }                                                                                                                                            | 
        goto LABEL_278;                                                                                                                              | 
      }                                                                                                                                              | 
      v21 = Event;                                                                                                                                   | 
      ExAcquireResourceExclusiveLite((PERESOURCE)&Event[2].Header.WaitListHead, 1u);                                                                 | 
      KeResetEvent(v21);                                                                                                                             | 
LABEL_314:                                                                                                                                           | 
      ExReleaseResourceLite((PERESOURCE)&v21[2].Header.WaitListHead);                                                                                | 
      goto LABEL_315;                                                                                                                                | 
    }                                                                                                                                                | 
    if ( v142 > 42 )                                                                                                                                 | 
    {                                                                                                                                                | 
      if ( v142 == 43 )                                                                                                                              | 
      {                                                                                                                                              | 
        v181 = v9->m128i_i64[0];                                                                                                                     | 
        LOBYTE(v11) = PreviousMode;                                                                                                                  | 
        v21 = Event;                                                                                                                                 | 
        Silo = PspSetJobMemoryPartition(Event, v11);                                                                                                 | 
        goto LABEL_484;                                                                                                                              | 
      }                                                                                                                                              | 
      if ( v142 == 44 )                                                                                                                              | 
      {                                                                                                                                              | 
        v21 = Event;                                                                                                                                 | 
        ExAcquireResourceExclusiveLite((PERESOURCE)&Event[2].Header.WaitListHead, 1u);                                                               | 
        if ( SLODWORD(v21[64].Header.WaitListHead.Blink) < 0 )                                                                                       | 
        {                                                                                                                                            | 
          v12 = -1073741791;                                                                                                                         | 
          goto LABEL_66;                                                                                                                             | 
        }                                                                                                                                            | 
        *(__m128i *)&v21[62].Header.Lock = *v9;                                                                                                      | 
        _InterlockedOr((volatile signed __int32 *)&v21[64].Header.WaitListHead.Blink, 0x80000000);                                                   | 
        v21 = Event;                                                                                                                                 | 
        goto LABEL_314;                                                                                                                              | 
      }                                                                                                                                              | 
      if ( v142 != 45 )                                                                                                                              | 
      {                                                                                                                                              | 
        switch ( v142 )                                                                                                                              | 
        {                                                                                                                                            | 
          case '.':                                                                                                                                  | 
            v163 = v9->m128i_i64[0];                                                                                                                 | 
            v21 = Event;                                                                                                                             | 
            v72 = PspSetEnergyTrackingStateJobTree(Event, (__int64)&v163);                                                                           | 
            goto LABEL_253;                                                                                                                          | 
          case '/':                                                                                                                                  | 
            v151 = v9->m128i_i8[0];                                                                                                                  | 
            if ( v151 != 1 )                                                                                                                         | 
            {                                                                                                                                        | 
              goto LABEL_456;                                                                                                                        | 
            }                                                                                                                                        | 
            v129 = SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode);                                                                             | 
            v21 = Event;                                                                                                                             | 
            if ( !v129 )                                                                                                                             | 
            {                                                                                                                                        | 
              v12 = -1073741727;                                                                                                                     | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
            if ( ((__int64)Event[64].Header.WaitListHead.Blink & 0x40000000) == 0 )                                                                  | 
            {                                                                                                                                        | 
              goto LABEL_460;                                                                                                                        | 
            }                                                                                                                                        | 
            v12 = (unsigned __int8)PspSetJobSiloThreadImpersonationPolicy(Event, 2LL) == 0 ? 0xC0000022 : 0;                                         | 
LABEL_485:                                                                                                                                           | 
            v135 = Thread;                                                                                                                           | 
            v8 = Thread->SpecialApcDisable++ == -1;                                                                                                  | 
            if ( v8 && ($727077A9B6E167EAE1398C74674DC5A5 *)v135->ApcState.ApcListHead[0].Flink != &v135->152 )                                      | 
            {                                                                                                                                        | 
              KiCheckForKernelApcDelivery();                                                                                                         | 
            }                                                                                                                                        | 
            if ( v12 )                                                                                                                               | 
            {                                                                                                                                        | 
              if ( (PerfGlobalGroupMask & 0x80000) != 0 )                                                                                            | 
              {                                                                                                                                      | 
                EtwTraceJobSetQuery((_DWORD)v21, v142, 0, 0, v12, 1831);                                                                             | 
              }                                                                                                                                      | 
            }                                                                                                                                        | 
            ObfDereferenceObjectWithTag(v21, 0x79517350u);                                                                                           | 
            return (unsigned int)v12;                                                                                                                | 
          case '0':                                                                                                                                  | 
            Object[0] = (PVOID)v9->m128i_i64[0];                                                                                                     | 
            if ( ((__int64)Object[0] & 0xFFFFFFFE) != 0 )                                                                                            | 
            {                                                                                                                                        | 
              goto LABEL_73;                                                                                                                         | 
            }                                                                                                                                        | 
            if ( HIDWORD(Object[0]) >= 5 )                                                                                                           | 
            {                                                                                                                                        | 
              goto LABEL_456;                                                                                                                        | 
            }                                                                                                                                        | 
            v21 = Event;                                                                                                                             | 
            PspSetIoPriorityLimitJobTree(Event);                                                                                                     | 
            break;                                                                                                                                   | 
          case '1':                                                                                                                                  | 
            Object[0] = (PVOID)v9->m128i_i64[0];                                                                                                     | 
            if ( ((__int64)Object[0] & 0xFFFFFFFE) != 0 )                                                                                            | 
            {                                                                                                                                        | 
              goto LABEL_73;                                                                                                                         | 
            }                                                                                                                                        | 
            if ( HIDWORD(Object[0]) >= 8 )                                                                                                           | 
            {                                                                                                                                        | 
              goto LABEL_456;                                                                                                                        | 
            }                                                                                                                                        | 
            v21 = Event;                                                                                                                             | 
            if ( ((__int64)Object[0] & 1) != 0 && !HIDWORD(Object[0]) )                                                                              | 
            {                                                                                                                                        | 
              goto LABEL_460;                                                                                                                        | 
            }                                                                                                                                        | 
            PspSetPagePriorityLimitJobTree(Event);                                                                                                   | 
            break;                                                                                                                                   | 
          default:                                                                                                                                   | 
            goto LABEL_445;                                                                                                                          | 
        }                                                                                                                                            | 
LABEL_315:                                                                                                                                           | 
        v12 = 0;                                                                                                                                     | 
        goto LABEL_485;                                                                                                                              | 
      }                                                                                                                                              | 
      v21 = Event;                                                                                                                                   | 
      if ( ((__int64)Event[64].Header.WaitListHead.Blink & 0x40000000) == 0 )                                                                        | 
      {                                                                                                                                              | 
        goto LABEL_341;                                                                                                                              | 
      }                                                                                                                                              | 
      v164 = 0LL;                                                                                                                                    | 
      *(__m128i *)Object = *v9;                                                                                                                      | 
      v130 = _mm_srli_si128(*(__m128i *)Object, 8).m128i_u64[0];                                                                                     | 
      v131 = (PVOID)v130;                                                                                                                            | 
      if ( !v130                                                                                                                                     | 
        || (v132 = (unsigned __int16)Object[0], (unsigned __int16)(LOWORD(Object[0]) - 1) > 0x206u)                                                  | 
        || ((__int64)Object[0] & 1) != 0 )                                                                                                           | 
      {                                                                                                                                              | 
        v12 = -1073741811;                                                                                                                           | 
        v139 = -1073741811;                                                                                                                          | 
        goto LABEL_485;                                                                                                                              | 
      }                                                                                                                                              | 
      if ( PreviousMode == 1 )                                                                                                                       | 
      {                                                                                                                                              | 
        if ( (v130 & 1) != 0 )                                                                                                                       | 
        {                                                                                                                                            | 
          ExRaiseDatatypeMisalignment();                                                                                                             | 
        }                                                                                                                                            | 
        if ( v130 + LOWORD(Object[0]) > 0x7FFFFFFF0000LL || v130 + LOWORD(Object[0]) < v130 )                                                        | 
        {                                                                                                                                            | 
          v21 = Event;                                                                                                                               | 
          v131 = Object[1];                                                                                                                          | 
          v132 = (unsigned __int16)Object[0];                                                                                                        | 
        }                                                                                                                                            | 
      }                                                                                                                                              | 
      v133 = (void *)ExAllocatePool2(0x100uLL, v132, 0x72537350u);                                                                                   | 
      v134 = v133;                                                                                                                                   | 
      v164 = v133;                                                                                                                                   | 
      if ( !v133 )                                                                                                                                   | 
      {                                                                                                                                              | 
        v12 = -1073741670;                                                                                                                           | 
        v139 = -1073741670;                                                                                                                          | 
        goto LABEL_485;                                                                                                                              | 
      }                                                                                                                                              | 
      memmove(v133, v131, v132);                                                                                                                     | 
      if ( wcsnlen((const wchar_t *)v134, (unsigned __int64)v132 >> 1) == (unsigned __int64)v132 >> 1 )                                              | 
      {                                                                                                                                              | 
        Object[1] = v134;                                                                                                                            | 
        v12 = PspAssignSiloSystemRootPath(v21, Object);                                                                                              | 
      }                                                                                                                                              | 
      else                                                                                                                                           | 
      {                                                                                                                                              | 
        v12 = -1073741811;                                                                                                                           | 
      }                                                                                                                                              | 
      if ( !v134 )                                                                                                                                   | 
      {                                                                                                                                              | 
        goto LABEL_485;                                                                                                                              | 
      }                                                                                                                                              | 
      v125 = 1918071632;                                                                                                                             | 
      v126 = v134;                                                                                                                                   | 
LABEL_429:                                                                                                                                           | 
      ExFreePoolWithTag(v126, v125);                                                                                                                 | 
      goto LABEL_485;                                                                                                                                | 
    }                                                                                                                                                | 
    if ( v142 == 42 )                                                                                                                                | 
    {                                                                                                                                                | 
      v127 = 0;                                                                                                                                      | 
      v174 = *v9;                                                                                                                                    | 
      v175 = v9[1];                                                                                                                                  | 
      v176 = v9[2];                                                                                                                                  | 
      v177 = v9[3];                                                                                                                                  | 
      v178 = v9[4].m128i_i64[0];                                                                                                                     | 
      v21 = Event;                                                                                                                                   | 
      if ( (v174.m128i_i32[0] & 0xFFFFFFFC) != 0 || (v174.m128i_i8[0] & 3) == 0 )                                                                    | 
      {                                                                                                                                              | 
        v12 = -1073741811;                                                                                                                           | 
      }                                                                                                                                              | 
      else                                                                                                                                           | 
      {                                                                                                                                              | 
        v127 = 8;                                                                                                                                    | 
        v128 = Thread;                                                                                                                               | 
        PspLockRootJobExclusive(Event, Thread, BugCheckParameter1a);                                                                                 | 
        PspLockJobConditionally(v21, BugCheckParameter1a);                                                                                           | 
        v12 = PspSetJobIoAttribution(v21);                                                                                                           | 
        if ( v12 >= 0 )                                                                                                                              | 
        {                                                                                                                                            | 
          PspUnlockJobConditionally(v21, BugCheckParameter1a);                                                                                       | 
          PspUnlockJob(BugCheckParameter1a[0], v128);                                                                                                | 
          v127 = 0;                                                                                                                                  | 
        }                                                                                                                                            | 
      }                                                                                                                                              | 
      if ( v127 == 8 )                                                                                                                               | 
      {                                                                                                                                              | 
        PspUnlockJobConditionally(v21, BugCheckParameter1a);                                                                                         | 
        PspUnlockJob(BugCheckParameter1a[0], Thread);                                                                                                | 
      }                                                                                                                                              | 
      goto LABEL_485;                                                                                                                                | 
    }                                                                                                                                                | 
    if ( v142 == 27 )                                                                                                                                | 
    {                                                                                                                                                | 
      v21 = Event;                                                                                                                                   | 
      PspLockJobMemoryLimitsExclusive(Event, 0LL, 0LL);                                                                                              | 
      *(_QWORD *)&v21[30].Header.Lock = 0LL;                                                                                                         | 
      v21[29].Header.WaitListHead.Blink = 0LL;                                                                                                       | 
      PspUnlockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                              | 
      v12 = 0;                                                                                                                                       | 
      goto LABEL_485;                                                                                                                                | 
    }                                                                                                                                                | 
    if ( v142 != 31 )                                                                                                                                | 
    {                                                                                                                                                | 
      switch ( v142 )                                                                                                                                | 
      {                                                                                                                                              | 
        case ' ':                                                                                                                                    | 
          v21 = Event;                                                                                                                               | 
          Silo = PspSetNetRateControl(v9, (unsigned int)v4);                                                                                         | 
          goto LABEL_484;                                                                                                                            | 
        case '!':                                                                                                                                    | 
LABEL_42:                                                                                                                                            | 
          if ( (_DWORD)v4 == 48 )                                                                                                                    | 
          {                                                                                                                                          | 
            memmove(&v198, v9, v4);                                                                                                                  | 
            v15 = DWORD2(v200);                                                                                                                      | 
            v18 = *((_QWORD *)&v198 + 1);                                                                                                            | 
            v19 = v198;                                                                                                                              | 
            v190 = v198;                                                                                                                             | 
            v16 = *((_QWORD *)&v199 + 1);                                                                                                            | 
            v17 = v199;                                                                                                                              | 
            *(_OWORD *)v191 = v199;                                                                                                                  | 
            *(_DWORD *)&v191[16] = v200;                                                                                                             | 
            *(_QWORD *)&v191[20] = *(_QWORD *)((char *)&v200 + 4);                                                                                   | 
            v14 = 459268;                                                                                                                            | 
            JobLimitInformationValidFlags = 459268;                                                                                                  | 
          }                                                                                                                                          | 
          else                                                                                                                                       | 
          {                                                                                                                                          | 
            if ( (_DWORD)v4 == 56 )                                                                                                                  | 
            {                                                                                                                                        | 
              memmove(&v194, v9, v4);                                                                                                                | 
              v15 = v197;                                                                                                                            | 
              *(_DWORD *)&v191[24] = v197;                                                                                                           | 
              v18 = *((_QWORD *)&v194 + 1);                                                                                                          | 
              v19 = v194;                                                                                                                            | 
              v190 = v194;                                                                                                                           | 
              v92 = *((_QWORD *)&v195 + 1);                                                                                                          | 
              v93 = v195;                                                                                                                            | 
              v192 = v93 >> 64;                                                                                                                      | 
              v17 = v93;                                                                                                                             | 
              *(_QWORD *)v191 = v195;                                                                                                                | 
              v16 = v196;                                                                                                                            | 
              *(_OWORD *)&v191[8] = v196;                                                                                                            | 
              v14 = 2589188;                                                                                                                         | 
              JobLimitInformationValidFlags = 2589188;                                                                                               | 
              goto LABEL_349;                                                                                                                        | 
            }                                                                                                                                        | 
            memmove(&v190, v9, v4);                                                                                                                  | 
            v14 = 2064900;                                                                                                                           | 
            JobLimitInformationValidFlags = 2064900;                                                                                                 | 
            v15 = *(_DWORD *)&v191[24];                                                                                                              | 
            v16 = *(_QWORD *)&v191[8];                                                                                                               | 
            v17 = *(_QWORD *)v191;                                                                                                                   | 
            v18 = *((_QWORD *)&v190 + 1);                                                                                                            | 
            v19 = v190;                                                                                                                              | 
          }                                                                                                                                          | 
          v92 = v192;                                                                                                                                | 
LABEL_349:                                                                                                                                           | 
          if ( (~v14 & v15) == 0 )                                                                                                                   | 
          {                                                                                                                                          | 
            if ( (v15 & 0x8000) != 0 )                                                                                                               | 
            {                                                                                                                                        | 
              if ( v92 < 0x1000 )                                                                                                                    | 
              {                                                                                                                                      | 
                goto LABEL_73;                                                                                                                       | 
              }                                                                                                                                      | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              v92 = 0LL;                                                                                                                             | 
              v192 = 0LL;                                                                                                                            | 
            }                                                                                                                                        | 
            if ( (v15 & 0x200) != 0 )                                                                                                                | 
            {                                                                                                                                        | 
              if ( v16 < 0x1000 || v16 < v92 )                                                                                                       | 
              {                                                                                                                                      | 
                goto LABEL_73;                                                                                                                       | 
              }                                                                                                                                      | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              *(_QWORD *)&v191[8] = 0LL;                                                                                                             | 
            }                                                                                                                                        | 
            if ( (v15 & 4) != 0 )                                                                                                                    | 
            {                                                                                                                                        | 
              if ( !v17 )                                                                                                                            | 
              {                                                                                                                                      | 
                goto LABEL_73;                                                                                                                       | 
              }                                                                                                                                      | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              *(_QWORD *)v191 = 0LL;                                                                                                                 | 
            }                                                                                                                                        | 
            if ( (v15 & 0x10000) != 0 )                                                                                                              | 
            {                                                                                                                                        | 
              if ( !v19 )                                                                                                                            | 
              {                                                                                                                                      | 
                goto LABEL_73;                                                                                                                       | 
              }                                                                                                                                      | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              *(_QWORD *)&v190 = 0LL;                                                                                                                | 
            }                                                                                                                                        | 
            if ( (v15 & 0x20000) != 0 )                                                                                                              | 
            {                                                                                                                                        | 
              if ( !v18 )                                                                                                                            | 
              {                                                                                                                                      | 
                goto LABEL_73;                                                                                                                       | 
              }                                                                                                                                      | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              *((_QWORD *)&v190 + 1) = 0LL;                                                                                                          | 
            }                                                                                                                                        | 
            v94 = 0LL;                                                                                                                               | 
            do                                                                                                                                       | 
            {                                                                                                                                        | 
              v95 = PspNotificationLimitRateControlToleranceField(&v190, v94);                                                                       | 
              PspNotificationLimitRateControlToleranceIntervalField(&v190, v96, v97, v95);                                                           | 
              v99 = PspRateControlLimitFlag(v98);                                                                                                    | 
              if ( (v99 & v103) != 0 )                                                                                                               | 
              {                                                                                                                                      | 
                if ( !*v101 || *v101 > 3 || !*v102 || *v102 > 3 )                                                                                    | 
                {                                                                                                                                    | 
                  goto LABEL_456;                                                                                                                    | 
                }                                                                                                                                    | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                *v101 = 0;                                                                                                                           | 
                *v102 = 0;                                                                                                                           | 
              }                                                                                                                                      | 
              v94 = (unsigned int)(v100 + 1);                                                                                                        | 
            }                                                                                                                                        | 
            while ( (int)v94 < 3 );                                                                                                                  | 
            v21 = Event;                                                                                                                             | 
            v104 = &Event[50].Header.WaitListHead.Blink;                                                                                             | 
            if ( Event[50].Header.WaitListHead.Blink )                                                                                               | 
            {                                                                                                                                        | 
              v105 = 0LL;                                                                                                                            | 
              v106 = 0LL;                                                                                                                            | 
              goto LABEL_386;                                                                                                                        | 
            }                                                                                                                                        | 
            v105 = (struct _LIST_ENTRY *)ExAllocatePool2(0x100uLL, 0x88uLL, 0x624A7350u);                                                            | 
            if ( v105 )                                                                                                                              | 
            {                                                                                                                                        | 
              v106 = (void *)PsChargeSharedPoolQuota(KeGetCurrentThread()->ApcState.Process, 136LL);                                                 | 
              v35 = v105;                                                                                                                            | 
              if ( !v106 )                                                                                                                           | 
              {                                                                                                                                      | 
                v36 = 1649046352;                                                                                                                    | 
                goto LABEL_127;                                                                                                                      | 
              }                                                                                                                                      | 
              memset_0(v105, 0, 0x88uLL);                                                                                                            | 
              v21 = Event;                                                                                                                           | 
LABEL_386:                                                                                                                                           | 
              ExAcquireResourceExclusiveLite((PERESOURCE)&v21[2].Header.WaitListHead, 1u);                                                           | 
              if ( *v104 )                                                                                                                           | 
              {                                                                                                                                      | 
                if ( v105 )                                                                                                                          | 
                {                                                                                                                                    | 
                  ExFreePoolWithTag(v105, 0x624A7350u);                                                                                              | 
                  PsReturnSharedPoolQuota(v106);                                                                                                     | 
                }                                                                                                                                    | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                *v104 = v105;                                                                                                                        | 
                *(_QWORD *)&v21[51].Header.Lock = v106;                                                                                              | 
              }                                                                                                                                      | 
              v107 = v21[50].Header.WaitListHead.Blink;                                                                                              | 
              Flink = (int)v107->Flink;                                                                                                              | 
              *(_OWORD *)&v107->Blink = v190;                                                                                                        | 
              v107[1].Blink = *(struct _LIST_ENTRY **)v191;                                                                                          | 
              v109 = 0LL;                                                                                                                            | 
              do                                                                                                                                     | 
              {                                                                                                                                      | 
                v110 = (_DWORD *)PspNotificationLimitRateControlToleranceField(&v190, v109);                                                         | 
                *(_DWORD *)(v111 - 12) = *v110;                                                                                                      | 
                v114 = (_DWORD *)PspNotificationLimitRateControlToleranceIntervalField(&v190, v112, v111, v113);                                     | 
                *v115 = *v114;                                                                                                                       | 
                v109 = (unsigned int)(v116 + 1);                                                                                                     | 
              }                                                                                                                                      | 
              while ( (int)v109 < 3 );                                                                                                               | 
              PspLockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                        | 
              v107[2].Flink = (struct _LIST_ENTRY *)(v192 >> 12);                                                                                    | 
              v107[2].Blink = (struct _LIST_ENTRY *)(*(_QWORD *)&v191[8] >> 12);                                                                     | 
              LODWORD(v21[50].Header.WaitListHead.Blink->Flink) = *(_DWORD *)&v191[24];                                                              | 
              PspUnlockJobMemoryLimitsExclusive(v21, 0LL, 0LL);                                                                                      | 
              if ( ((__int64)v21[50].Header.WaitListHead.Blink->Flink & 0xFFFF7DFF) != 0 )                                                           | 
              {                                                                                                                                      | 
                _InterlockedAdd64(&PspJobTimeLimitsRequest, 1uLL);                                                                                   | 
                v21 = Event;                                                                                                                         | 
              }                                                                                                                                      | 
              if ( Flink )                                                                                                                           | 
              {                                                                                                                                      | 
                v117 = v21[50].Header.WaitListHead.Blink;                                                                                            | 
                if ( LODWORD(v117->Flink) )                                                                                                          | 
                {                                                                                                                                    | 
LABEL_399:                                                                                                                                           | 
                  if ( (PerfGlobalGroupMask & 0x80000) != 0 )                                                                                        | 
                  {                                                                                                                                  | 
                    EtwTraceJobSetQuery((_DWORD)v21, v142, (unsigned int)&v190, 0, 0, 1829);                                                         | 
                  }                                                                                                                                  | 
                  ExReleaseResourceLite((PERESOURCE)&v21[2].Header.WaitListHead);                                                                    | 
                  PspLockJobMemoryLimitsShared(v21, 0LL);                                                                                            | 
                  v118 = v21[65].Header.WaitListHead.Flink;                                                                                          | 
                  v21 = Event;                                                                                                                       | 
                  JobMemoryUsageNotificationViolations = PspGetJobMemoryUsageNotificationViolations(                                                 | 
                                                           Event,                                                                                    | 
                                                           Event[50].Header.WaitListHead.Flink,                                                      | 
                                                           (char *)Event[50].Header.WaitListHead.Flink                                               | 
                                                         + (unsigned __int64)v118,                                                                   | 
                                                           33280LL);                                                                                 | 
                  PspUnlockJobMemoryLimitsShared(v21, 0LL);                                                                                          | 
                  if ( JobMemoryUsageNotificationViolations )                                                                                        | 
                  {                                                                                                                                  | 
                    PspScheduleEnforcementWorker(v21[54].Header.WaitListHead.Blink);                                                                 | 
                    v21 = Event;                                                                                                                     | 
                  }                                                                                                                                  | 
                  goto LABEL_315;                                                                                                                    | 
                }                                                                                                                                    | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                v117 = v21[50].Header.WaitListHead.Blink;                                                                                            | 
                if ( !LODWORD(v117->Flink) )                                                                                                         | 
                {                                                                                                                                    | 
                  goto LABEL_399;                                                                                                                    | 
                }                                                                                                                                    | 
              }                                                                                                                                      | 
              PspEnumJobsAndProcessesInJobHierarchy(v21, (__int64)v117, 5);                                                                          | 
              goto LABEL_399;                                                                                                                        | 
            }                                                                                                                                        | 
LABEL_123:                                                                                                                                           | 
            v12 = -1073741670;                                                                                                                       | 
            goto LABEL_485;                                                                                                                          | 
          }                                                                                                                                          | 
LABEL_73:                                                                                                                                            | 
          v12 = -1073741811;                                                                                                                         | 
          goto LABEL_446;                                                                                                                            | 
        case '#':                                                                                                                                    | 
          v21 = Event;                                                                                                                               | 
          Silo = PspCreateSilo(Event);                                                                                                               | 
          goto LABEL_484;                                                                                                                            | 
        case '%':                                                                                                                                    | 
          v91 = SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode);                                                                                | 
          v21 = Event;                                                                                                                               | 
          if ( v91 )                                                                                                                                 | 
          {                                                                                                                                          | 
            if ( ((__int64)Event[64].Header.WaitListHead.Blink & 0x40000000) != 0 )                                                                  | 
            {                                                                                                                                        | 
              if ( !(unsigned __int8)PspSetJobSiloThreadImpersonationPolicy(Event, 4LL) )                                                            | 
              {                                                                                                                                      | 
                goto LABEL_343;                                                                                                                      | 
              }                                                                                                                                      | 
              v169 = v9->m128i_i32[0];                                                                                                               | 
              Silo = ObCreateSiloRootDirectory(v21);                                                                                                 | 
              goto LABEL_484;                                                                                                                        | 
            }                                                                                                                                        | 
LABEL_341:                                                                                                                                           | 
            v12 = -1073740535;                                                                                                                       | 
            goto LABEL_485;                                                                                                                          | 
          }                                                                                                                                          | 
          break;                                                                                                                                     | 
        case '(':                                                                                                                                    | 
          v88 = PreviousMode;                                                                                                                        | 
          v89 = SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode);                                                                                | 
          v21 = Event;                                                                                                                               | 
          if ( v89 )                                                                                                                                 | 
          {                                                                                                                                          | 
            if ( ((__int64)Event[64].Header.WaitListHead.Blink & 0x40000000) != 0 )                                                                  | 
            {                                                                                                                                        | 
              if ( !(unsigned __int8)PspSetJobSiloThreadImpersonationPolicy(Event, 4LL) )                                                            | 
              {                                                                                                                                      | 
                goto LABEL_343;                                                                                                                      | 
              }                                                                                                                                      | 
              v180 = v9->m128i_i64[0];                                                                                                               | 
              v170 = v9->m128i_i32[2];                                                                                                               | 
              LOBYTE(v90) = v88;                                                                                                                     | 
              Silo = PspConvertSiloToServerSilo(v21, v90);                                                                                           | 
LABEL_484:                                                                                                                                           | 
              v12 = Silo;                                                                                                                            | 
              goto LABEL_485;                                                                                                                        | 
            }                                                                                                                                        | 
            goto LABEL_341;                                                                                                                          | 
          }                                                                                                                                          | 
          break;                                                                                                                                     | 
        case ')':                                                                                                                                    | 
          v171 = v9->m128i_i32[0];                                                                                                                   | 
          v21 = Event;                                                                                                                               | 
          if ( (unsigned __int8)PspJobIsAppSilo(Event) && v86 == 2 )                                                                                 | 
          {                                                                                                                                          | 
            v87 = (struct _ERESOURCE *)&v21[2].Header.WaitListHead;                                                                                  | 
            ExAcquireResourceExclusiveLite((PERESOURCE)&v21[2].Header.WaitListHead, 1u);                                                             | 
            if ( LODWORD(v21[60].Header.WaitListHead.Blink) )                                                                                        | 
            {                                                                                                                                        | 
              v12 = -1073740529;                                                                                                                     | 
            }                                                                                                                                        | 
            else                                                                                                                                     | 
            {                                                                                                                                        | 
              v45 = _interlockedbittestandset((volatile signed __int32 *)&v21[64].Header.WaitListHead.Blink, 0x1Du);                                 | 
              v21 = Event;                                                                                                                           | 
              if ( v45 )                                                                                                                             | 
              {                                                                                                                                      | 
                v12 = 255;                                                                                                                           | 
              }                                                                                                                                      | 
              else                                                                                                                                   | 
              {                                                                                                                                      | 
                PspHardDereferenceSiloWorker(Event);                                                                                                 | 
                v12 = 0;                                                                                                                             | 
              }                                                                                                                                      | 
            }                                                                                                                                        | 
            p_WaitListHead = v87;                                                                                                                    | 
            goto LABEL_67;                                                                                                                           | 
          }                                                                                                                                          | 
LABEL_343:                                                                                                                                           | 
          v12 = -1073741811;                                                                                                                         | 
          goto LABEL_485;                                                                                                                            | 
        default:                                                                                                                                     | 
          goto LABEL_445;                                                                                                                            | 
      }                                                                                                                                              | 
      v12 = -1073741727;                                                                                                                             | 
      goto LABEL_485;                                                                                                                                | 
    }                                                                                                                                                | 
    v120 = 0LL;                                                                                                                                      | 
    Object[1] = 0LL;                                                                                                                                 | 
    memmove(v182, v9, v4);                                                                                                                           | 
    if ( v184 )                                                                                                                                      | 
    {                                                                                                                                                | 
      v121 = v186;                                                                                                                                   | 
      if ( !v186 )                                                                                                                                   | 
      {                                                                                                                                              | 
        goto LABEL_417;                                                                                                                              | 
      }                                                                                                                                              | 
      if ( ((unsigned __int8)v184 & 1) != 0 )                                                                                                        | 
      {                                                                                                                                              | 
        ExRaiseDatatypeMisalignment();                                                                                                               | 
      }                                                                                                                                              | 
      if ( (unsigned __int64)v184 + v186 > 0x7FFFFFFF0000LL || (char *)v184 + v186 < v184 )                                                          | 
      {                                                                                                                                              | 
        v121 = v186;                                                                                                                                 | 
        v120 = Object[1];                                                                                                                            | 
      }                                                                                                                                              | 
      if ( !v121 || (v121 & 1) != 0 )                                                                                                                | 
      {                                                                                                                                              | 
LABEL_417:                                                                                                                                           | 
        v12 = -1073741811;                                                                                                                           | 
        v139 = -1073741811;                                                                                                                          | 
        v21 = Event;                                                                                                                                 | 
        goto LABEL_427;                                                                                                                              | 
      }                                                                                                                                              | 
      v122 = v121 + 2LL;                                                                                                                             | 
      v123 = 32LL;                                                                                                                                   | 
      if ( PreviousMode == 1 )                                                                                                                       | 
      {                                                                                                                                              | 
        v123 = 257LL;                                                                                                                                | 
      }                                                                                                                                              | 
      v124 = (_WORD *)ExAllocatePool2(v123, v122, 0x624A7350u);                                                                                      | 
      v120 = v124;                                                                                                                                   | 
      Object[1] = v124;                                                                                                                              | 
      if ( !v124 )                                                                                                                                   | 
      {                                                                                                                                              | 
        v12 = -1073741801;                                                                                                                           | 
        v139 = -1073741801;                                                                                                                          | 
        v21 = Event;                                                                                                                                 | 
        goto LABEL_427;                                                                                                                              | 
      }                                                                                                                                              | 
      memmove(v124, v184, v186);                                                                                                                     | 
      v120[(unsigned __int64)v186 >> 1] = 0;                                                                                                         | 
      v184 = v120;                                                                                                                                   | 
    }                                                                                                                                                | 
    if ( (v185 & 0xFFFFFFF0) != 0 )                                                                                                                  | 
    {                                                                                                                                                | 
      v12 = -1073741811;                                                                                                                             | 
    }                                                                                                                                                | 
    else                                                                                                                                             | 
    {                                                                                                                                                | 
      if ( !v187 && !v183 && !v188 || (unsigned __int8)PspIsContextAdmin() )                                                                         | 
      {                                                                                                                                              | 
        v21 = Event;                                                                                                                                 | 
        v12 = PspSetJobIoRateControl(Event, v182);                                                                                                   | 
        goto LABEL_427;                                                                                                                              | 
      }                                                                                                                                              | 
      v12 = -1073741790;                                                                                                                             | 
    }                                                                                                                                                | 
    v21 = Event;                                                                                                                                     | 
LABEL_427:                                                                                                                                           | 
    if ( !v120 )                                                                                                                                     | 
    {                                                                                                                                                | 
      goto LABEL_485;                                                                                                                                | 
    }                                                                                                                                                | 
    v125 = 0;                                                                                                                                        | 
    v126 = v120;                                                                                                                                     | 
    goto LABEL_429;                                                                                                                                  | 
  }                                                                                                                                                  | 
  return result;                                                                                                                                     | 
}                                                                                                                                                    | 
1712 row(s)
