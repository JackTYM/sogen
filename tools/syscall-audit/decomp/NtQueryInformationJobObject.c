__int64 __fastcall NtQueryInformationJobObject(ULONG_PTR a1, int a2, _QWORD *a3, unsigned int a4, unsigned int *a5)        | 
{                                                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                            | 
                                                                                                                           | 
  v121 = a4;                                                                                                               | 
  v6 = (int)a3;                                                                                                            | 
  v7 = a2;                                                                                                                 | 
  BugCheckParameter1 = a1;                                                                                                 | 
  v128 = a3;                                                                                                               | 
  v123 = (__int64)a5;                                                                                                      | 
  memset_0(v205, 0, 0x1D0uLL);                                                                                             | 
  v168 = 0LL;                                                                                                              | 
  memset_0(&v207, 0, 0x108uLL);                                                                                            | 
  v117 = 0;                                                                                                                | 
  v134 = 0LL;                                                                                                              | 
  v133 = 0LL;                                                                                                              | 
  v129 = 0;                                                                                                                | 
  v137 = 0LL;                                                                                                              | 
  v131 = 0;                                                                                                                | 
  v177 = 0LL;                                                                                                              | 
  v178 = 0LL;                                                                                                              | 
  v179 = 0;                                                                                                                | 
  v180 = 0LL;                                                                                                              | 
  v136 = 0LL;                                                                                                              | 
  v130 = 0;                                                                                                                | 
  v139 = 0LL;                                                                                                              | 
  memset_0(v189, 0, 0x98uLL);                                                                                              | 
  v163 = 0LL;                                                                                                              | 
  v120[0] = 0;                                                                                                             | 
  v124 = 0;                                                                                                                | 
  v203 = 0;                                                                                                                | 
  memset_0(v202, 0, 0x44uLL);                                                                                              | 
  Object = 0LL;                                                                                                            | 
  v135 = 0;                                                                                                                | 
  memset_0(&v147, 0, 0x68uLL);                                                                                             | 
  memset_0(v181, 0, 0x50uLL);                                                                                              | 
  memset_0(v184, 0, 0x58uLL);                                                                                              | 
  memset(v176, 0, sizeof(v176));                                                                                           | 
  v161 = 0LL;                                                                                                              | 
  memset_0(&v164, 0, 0x48uLL);                                                                                             | 
  v169 = 0LL;                                                                                                              | 
  v170 = 0LL;                                                                                                              | 
  v171 = 0LL;                                                                                                              | 
  v172 = 0LL;                                                                                                              | 
  v173 = 0LL;                                                                                                              | 
  v174 = 0LL;                                                                                                              | 
  v175 = 0LL;                                                                                                              | 
  v132 = 0;                                                                                                                | 
  *(_OWORD *)P = 0LL;                                                                                                      | 
  v144 = 0LL;                                                                                                              | 
  v145 = 0LL;                                                                                                              | 
  LODWORD(v146) = 0;                                                                                                       | 
  v138 = 0LL;                                                                                                              | 
  memset_0(v204, 0, sizeof(v204));                                                                                         | 
  v162 = 0LL;                                                                                                              | 
  v141 = 0LL;                                                                                                              | 
  v142 = 0LL;                                                                                                              | 
  v143 = 0LL;                                                                                                              | 
  memset(v187, 0, sizeof(v187));                                                                                           | 
  v188 = 0;                                                                                                                | 
  *(_QWORD *)&Size[1] = 0LL;                                                                                               | 
  v116 = 0;                                                                                                                | 
  v118 = 0;                                                                                                                | 
  if ( (unsigned int)(v7 - 1) > 0x32 )                                                                                     | 
  {                                                                                                                        | 
    return 3221225475LL;                                                                                                   | 
  }                                                                                                                        | 
  switch ( (_DWORD)v7 )                                                                                                    | 
  {                                                                                                                        | 
    case 9:                                                                                                                | 
      if ( a4 != 144 && a4 != 152 )                                                                                        | 
      {                                                                                                                    | 
        return 3221225476LL;                                                                                               | 
      }                                                                                                                    | 
      goto LABEL_27;                                                                                                       | 
    case 0xC:                                                                                                              | 
      v12 = a4 - 48;                                                                                                       | 
      goto LABEL_41;                                                                                                       | 
    case 0xD:                                                                                                              | 
      v12 = a4 - 80;                                                                                                       | 
LABEL_41:                                                                                                                  | 
      v10 = (v12 & 0xFFFFFFF7) == 0;                                                                                       | 
LABEL_26:                                                                                                                  | 
      if ( !v10 )                                                                                                          | 
      {                                                                                                                    | 
        return 3221225476LL;                                                                                               | 
      }                                                                                                                    | 
      goto LABEL_27;                                                                                                       | 
    case 0x13:                                                                                                             | 
      if ( a4 == 432 || a4 == 448 )                                                                                        | 
      {                                                                                                                    | 
        goto LABEL_27;                                                                                                     | 
      }                                                                                                                    | 
      v10 = a4 == 464;                                                                                                     | 
      goto LABEL_26;                                                                                                       | 
    case 0x14:                                                                                                             | 
      if ( a4 == 40 )                                                                                                      | 
      {                                                                                                                    | 
        goto LABEL_27;                                                                                                     | 
      }                                                                                                                    | 
      v10 = a4 == 64;                                                                                                      | 
      goto LABEL_26;                                                                                                       | 
    case 0x1C:                                                                                                             | 
      if ( a4 == 16 )                                                                                                      | 
      {                                                                                                                    | 
        goto LABEL_27;                                                                                                     | 
      }                                                                                                                    | 
      v10 = a4 == 40;                                                                                                      | 
      goto LABEL_26;                                                                                                       | 
    case 0x1E:                                                                                                             | 
      if ( a4 == 16 )                                                                                                      | 
      {                                                                                                                    | 
        goto LABEL_27;                                                                                                     | 
      }                                                                                                                    | 
      v10 = a4 == 36;                                                                                                      | 
      goto LABEL_26;                                                                                                       | 
    case 0x2B:                                                                                                             | 
      if ( a4 == 1 )                                                                                                       | 
      {                                                                                                                    | 
        v9 = 1;                                                                                                            | 
        v8 = 1;                                                                                                            | 
        v114 = 1;                                                                                                          | 
        goto LABEL_29;                                                                                                     | 
      }                                                                                                                    | 
      return 3221225476LL;                                                                                                 | 
  }                                                                                                                        | 
  v8 = dword_140B3FF9C[v7];                                                                                                | 
  v114 = v8;                                                                                                               | 
  if ( a4 != v8 )                                                                                                          | 
  {                                                                                                                        | 
    if ( (_DWORD)v7 != 3                                                                                                   | 
      && (_DWORD)v7 != 5                                                                                                   | 
      && (_DWORD)v7 != 11                                                                                                  | 
      && (_DWORD)v7 != 14                                                                                                  | 
      && (_DWORD)v7 != 31                                                                                                  | 
      && (_DWORD)v7 != 37 )                                                                                                | 
    {                                                                                                                      | 
      if ( (_DWORD)v7 == 38 && a4 == 32 )                                                                                  | 
      {                                                                                                                    | 
        v8 = 32;                                                                                                           | 
        v114 = 32;                                                                                                         | 
        goto LABEL_28;                                                                                                     | 
      }                                                                                                                    | 
      return 3221225476LL;                                                                                                 | 
    }                                                                                                                      | 
    if ( a4 < v8 )                                                                                                         | 
    {                                                                                                                      | 
      return 3221225476LL;                                                                                                 | 
    }                                                                                                                      | 
LABEL_27:                                                                                                                  | 
    v114 = a4;                                                                                                             | 
    v8 = a4;                                                                                                               | 
  }                                                                                                                        | 
LABEL_28:                                                                                                                  | 
  v9 = 1;                                                                                                                  | 
LABEL_29:                                                                                                                  | 
  Size[0] = v8;                                                                                                            | 
  CurrentThread = KeGetCurrentThread();                                                                                    | 
  v119 = CurrentThread;                                                                                                    | 
  PreviousMode = CurrentThread->PreviousMode;                                                                              | 
  if ( PreviousMode )                                                                                                      | 
  {                                                                                                                        | 
    if ( a4 )                                                                                                              | 
    {                                                                                                                      | 
      if ( ((dword_140B4006C[v7] - 1) & v6) != 0 )                                                                         | 
      {                                                                                                                    | 
        ExRaiseDatatypeMisalignment();                                                                                     | 
      }                                                                                                                    | 
      v14 = 0x7FFFFFFF0000LL;                                                                                              | 
      CurrentThread = v119;                                                                                                | 
    }                                                                                                                      | 
    else                                                                                                                   | 
    {                                                                                                                      | 
      v14 = 0x7FFFFFFF0000LL;                                                                                              | 
    }                                                                                                                      | 
    if ( a5 )                                                                                                              | 
    {                                                                                                                      | 
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                       | 
      {                                                                                                                    | 
        v14 = (__int64)a5;                                                                                                 | 
      }                                                                                                                    | 
      *(_DWORD *)v14 = *(_DWORD *)v14;                                                                                     | 
    }                                                                                                                      | 
  }                                                                                                                        | 
  if ( BugCheckParameter1 )                                                                                                | 
  {                                                                                                                        | 
    result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);               | 
    if ( (int)result < 0 )                                                                                                 | 
    {                                                                                                                      | 
      return result;                                                                                                       | 
    }                                                                                                                      | 
    v15 = (char *)Object;                                                                                                  | 
    goto LABEL_56;                                                                                                         | 
  }                                                                                                                        | 
  v15 = (char *)CurrentThread->ApcState.Process[1].Padding[3];                                                             | 
  Object = v15;                                                                                                            | 
  if ( v15 )                                                                                                               | 
  {                                                                                                                        | 
    ObfReferenceObjectWithTag(v15, 0x79517350u);                                                                           | 
LABEL_56:                                                                                                                  | 
    CurrentThread = v119;                                                                                                  | 
    goto LABEL_59;                                                                                                         | 
  }                                                                                                                        | 
  if ( (((_DWORD)v7 - 31) & 0xFFFFFFF7) != 0 )                                                                             | 
  {                                                                                                                        | 
    return 3221225506LL;                                                                                                   | 
  }                                                                                                                        | 
LABEL_59:                                                                                                                  | 
  HIDWORD(v121) = v8;                                                                                                      | 
  v112 = 0;                                                                                                                | 
  v16 = 0LL;                                                                                                               | 
  JobIoAttribution = 0;                                                                                                    | 
  if ( (int)v7 > 26 )                                                                                                      | 
  {                                                                                                                        | 
    if ( (int)v7 > 39 )                                                                                                    | 
    {                                                                                                                      | 
      v94 = v7 - 42;                                                                                                       | 
      if ( !v94 )                                                                                                          | 
      {                                                                                                                    | 
        memset_0(v202, 0, 0x48uLL);                                                                                        | 
        PspLockJobShared(v15, v119);                                                                                       | 
        JobIoAttribution = PspQueryJobIoAttribution(v15, v202);                                                            | 
        PspUnlockJob(v15, v119);                                                                                           | 
        v16 = v202;                                                                                                        | 
        goto LABEL_281;                                                                                                    | 
      }                                                                                                                    | 
      v95 = v94 - 1;                                                                                                       | 
      if ( !v95 )                                                                                                          | 
      {                                                                                                                    | 
        JobIoAttribution = 0;                                                                                              | 
        v110 = *((_QWORD *)v15 + 224);                                                                                     | 
        if ( v110 )                                                                                                        | 
        {                                                                                                                  | 
          v111 = v116;                                                                                                     | 
          if ( v110 != -1 )                                                                                                | 
          {                                                                                                                | 
            v111 = 1;                                                                                                      | 
          }                                                                                                                | 
          v116 = v111;                                                                                                     | 
        }                                                                                                                  | 
        v16 = &v116;                                                                                                       | 
        goto LABEL_281;                                                                                                    | 
      }                                                                                                                    | 
      v96 = v95 - 1;                                                                                                       | 
      if ( !v96 )                                                                                                          | 
      {                                                                                                                    | 
        v180 = *((_OWORD *)v15 + 93);                                                                                      | 
        v16 = (char *)&v180;                                                                                               | 
        goto LABEL_72;                                                                                                     | 
      }                                                                                                                    | 
      v97 = v96 - 2;                                                                                                       | 
      if ( !v97 )                                                                                                          | 
      {                                                                                                                    | 
        PspLockJobShared(v15, CurrentThread);                                                                              | 
        v139 = *((_QWORD *)v15 + 226);                                                                                     | 
        PspUnlockJob(v15, v119);                                                                                           | 
        v16 = (char *)&v139;                                                                                               | 
        goto LABEL_72;                                                                                                     | 
      }                                                                                                                    | 
      v98 = v97 - 1;                                                                                                       | 
      if ( !v98 )                                                                                                          | 
      {                                                                                                                    | 
        v118 = (*((_DWORD *)v15 + 389) & 2) != 0;                                                                          | 
        v16 = (char *)&v118;                                                                                               | 
        goto LABEL_72;                                                                                                     | 
      }                                                                                                                    | 
      v99 = v98 - 1;                                                                                                       | 
      if ( !v99 )                                                                                                          | 
      {                                                                                                                    | 
        PspLockJobShared(v15, CurrentThread);                                                                              | 
        v108 = *((_DWORD *)v15 + 272);                                                                                     | 
        v109 = v134;                                                                                                       | 
        if ( v108 < 5 )                                                                                                    | 
        {                                                                                                                  | 
          v109 = 1;                                                                                                        | 
        }                                                                                                                  | 
        v134 = __PAIR64__(v108, v109);                                                                                     | 
        PspUnlockJob(v15, v119);                                                                                           | 
        v16 = (char *)&v134;                                                                                               | 
        goto LABEL_72;                                                                                                     | 
      }                                                                                                                    | 
      v100 = v99 - 1;                                                                                                      | 
      if ( !v100 )                                                                                                         | 
      {                                                                                                                    | 
        PspLockJobShared(v15, CurrentThread);                                                                              | 
        v106 = *((_DWORD *)v15 + 274);                                                                                     | 
        v107 = v133;                                                                                                       | 
        if ( v106 < 8 )                                                                                                    | 
        {                                                                                                                  | 
          v107 = 1;                                                                                                        | 
        }                                                                                                                  | 
        v133 = __PAIR64__(v106, v107);                                                                                     | 
        PspUnlockJob(v15, v119);                                                                                           | 
        v16 = (char *)&v133;                                                                                               | 
        goto LABEL_72;                                                                                                     | 
      }                                                                                                                    | 
      v101 = v100 - 1;                                                                                                     | 
      if ( v101 )                                                                                                          | 
      {                                                                                                                    | 
        if ( v101 == 1 )                                                                                                   | 
        {                                                                                                                  | 
          PspQueryJobHierarchyAccountingInformation(v15);                                                                  | 
          v168 = v206;                                                                                                     | 
          v16 = (char *)&v168;                                                                                             | 
          goto LABEL_72;                                                                                                   | 
        }                                                                                                                  | 
        goto LABEL_258;                                                                                                    | 
      }                                                                                                                    | 
      PsGetJobServerSilo(v15, &Size[1]);                                                                                   | 
      if ( !(unsigned __int8)PsIsHostSilo(*(_QWORD *)&Size[1]) )                                                           | 
      {                                                                                                                    | 
        ServerSiloGlobals = PsGetServerSiloGlobals(*(_QWORD *)&Size[1]);                                                   | 
        if ( *(_DWORD *)(ServerSiloGlobals + 1340) )                                                                       | 
        {                                                                                                                  | 
          v187[0] = *(_OWORD *)(ServerSiloGlobals + 1368);                                                                 | 
          LODWORD(v187[1]) = *(_DWORD *)(ServerSiloGlobals + 1344);                                                        | 
          v103 = 0;                                                                                                        | 
          v104 = (_WORD *)&v187[1] + 2;                                                                                    | 
          v105 = (unsigned __int8 *)(ServerSiloGlobals + 1348);                                                            | 
          do                                                                                                               | 
          {                                                                                                                | 
            *v104 = *v105;                                                                                                 | 
            ++v103;                                                                                                        | 
            ++v105;                                                                                                        | 
            ++v104;                                                                                                        | 
          }                                                                                                                | 
          while ( v103 < 0xF );                                                                                            | 
          JobIoAttribution = 0;                                                                                            | 
          v16 = (char *)v187;                                                                                              | 
        }                                                                                                                  | 
        else                                                                                                               | 
        {                                                                                                                  | 
          JobIoAttribution = -2147483614;                                                                                  | 
        }                                                                                                                  | 
        goto LABEL_281;                                                                                                    | 
      }                                                                                                                    | 
    }                                                                                                                      | 
    else                                                                                                                   | 
    {                                                                                                                      | 
      if ( (_DWORD)v7 == 39 )                                                                                              | 
      {                                                                                                                    | 
        if ( v15 )                                                                                                         | 
        {                                                                                                                  | 
          PsGetJobServerSilo(v15, &Size[1]);                                                                               | 
          v91 = *(_QWORD *)&Size[1];                                                                                       | 
        }                                                                                                                  | 
        else                                                                                                               | 
        {                                                                                                                  | 
          v91 = PdcCreateWatchdogAroundClientCall(CurrentThread);                                                          | 
        }                                                                                                                  | 
        v92 = PsGetServerSiloGlobals(v91);                                                                                 | 
        v19 = a3;                                                                                                          | 
        memmove(a3, *(const void **)(v92 + 1288), v93);                                                                    | 
        v112 = 1;                                                                                                          | 
        v18 = a5;                                                                                                          | 
        goto LABEL_283;                                                                                                    | 
      }                                                                                                                    | 
      if ( (int)v7 <= 33 )                                                                                                 | 
      {                                                                                                                    | 
        if ( (_DWORD)v7 != 33 )                                                                                            | 
        {                                                                                                                  | 
          v46 = v7 - 28;                                                                                                   | 
          if ( !v46 )                                                                                                      | 
          {                                                                                                                | 
            *(_OWORD *)&v176[24] = 0LL;                                                                                    | 
            PspLockJobMemoryLimitsShared(v15, CurrentThread);                                                              | 
            *(_QWORD *)&v176[8] = *((_QWORD *)v15 + 90) << 12;                                                             | 
            *(_QWORD *)v176 = *((_QWORD *)v15 + 151) << 12;                                                                | 
            *(_QWORD *)&v176[16] = *((_QWORD *)v15 + 196) << 12;                                                           | 
            v15 = (char *)Object;                                                                                          | 
            PspUnlockJobMemoryLimitsShared(Object, v119);                                                                  | 
            v16 = v176;                                                                                                    | 
            goto LABEL_72;                                                                                                 | 
          }                                                                                                                | 
          v47 = v46 - 1;                                                                                                   | 
          if ( !v47 )                                                                                                      | 
          {                                                                                                                | 
            v138 = *((_QWORD *)v15 + 196);                                                                                 | 
            v16 = (char *)&v138;                                                                                           | 
            JobIoAttribution = 0;                                                                                          | 
            v15 = (char *)Object;                                                                                          | 
            goto LABEL_281;                                                                                                | 
          }                                                                                                                | 
          v48 = v47 - 1;                                                                                                   | 
          if ( !v48 )                                                                                                      | 
          {                                                                                                                | 
            v177 = *((_OWORD *)v15 + 92);                                                                                  | 
            if ( (_DWORD)v121 == 36 )                                                                                      | 
            {                                                                                                              | 
              v178 = *((_OWORD *)v15 + 93);                                                                                | 
              v179 = *((_DWORD *)v15 + 367);                                                                               | 
            }                                                                                                              | 
            v16 = (char *)&v177;                                                                                           | 
            goto LABEL_72;                                                                                                 | 
          }                                                                                                                | 
          v49 = v48 - 1;                                                                                                   | 
          if ( !v49 )                                                                                                      | 
          {                                                                                                                | 
            JobIoAttribution = -1073741822;                                                                                | 
            goto LABEL_281;                                                                                                | 
          }                                                                                                                | 
          if ( v49 == 1 )                                                                                                  | 
          {                                                                                                                | 
            PspLockJobShared(v15, CurrentThread);                                                                          | 
            v50 = *((_QWORD *)v15 + 193);                                                                                  | 
            if ( v50 )                                                                                                     | 
            {                                                                                                              | 
              DWORD2(v161) = 1;                                                                                            | 
              v51 = *(_DWORD *)(v50 + 48);                                                                                 | 
              if ( (v51 & 1) != 0 )                                                                                        | 
              {                                                                                                            | 
                v9 = 3;                                                                                                    | 
                DWORD2(v161) = 3;                                                                                          | 
                *(_QWORD *)&v161 = *(_QWORD *)(v50 + 40);                                                                  | 
              }                                                                                                            | 
              if ( (v51 & 2) != 0 )                                                                                        | 
              {                                                                                                            | 
                DWORD2(v161) = v9 | 4;                                                                                     | 
                BYTE12(v161) = *(_BYTE *)(v50 + 64);                                                                       | 
              }                                                                                                            | 
            }                                                                                                              | 
            PspUnlockJob(v15, v119);                                                                                       | 
            v16 = (char *)&v161;                                                                                           | 
            goto LABEL_72;                                                                                                 | 
          }                                                                                                                | 
          goto LABEL_258;                                                                                                  | 
        }                                                                                                                  | 
LABEL_167:                                                                                                                 | 
        PspLockJobShared(v15, CurrentThread);                                                                              | 
        v52 = (int *)*((_QWORD *)v15 + 152);                                                                               | 
        if ( v52 )                                                                                                         | 
        {                                                                                                                  | 
          v166 = *v52;                                                                                                     | 
          v164 = *(_OWORD *)(v52 + 2);                                                                                     | 
          *(_QWORD *)v165 = *((_QWORD *)v52 + 3);                                                                          | 
          v167 = *((_QWORD *)v52 + 4) << 12;                                                                               | 
          *(_QWORD *)&v165[8] = *((_QWORD *)v52 + 5) << 12;                                                                | 
          v53 = 0LL;                                                                                                       | 
          v54 = v52 + 15;                                                                                                  | 
          do                                                                                                               | 
          {                                                                                                                | 
            v55 = (_DWORD *)PspNotificationLimitRateControlToleranceField(&v164, v53, v54);                                | 
            *v55 = *(_DWORD *)(v56 - 12);                                                                                  | 
            v57 = (_DWORD *)PspNotificationLimitRateControlToleranceIntervalField(&v164);                                  | 
            *v57 = *v58;                                                                                                   | 
            v53 = (unsigned int)(v59 + 1);                                                                                 | 
            v54 = v58 + 1;                                                                                                 | 
          }                                                                                                                | 
          while ( (int)v53 < 3 );                                                                                          | 
        }                                                                                                                  | 
        PspUnlockJob(v15, v119);                                                                                           | 
        if ( (_DWORD)v121 == 48 )                                                                                          | 
        {                                                                                                                  | 
          v169 = v164;                                                                                                     | 
          v170 = *(_OWORD *)v165;                                                                                          | 
          *(_QWORD *)&v171 = *(_QWORD *)&v165[16];                                                                         | 
          *((_QWORD *)&v171 + 1) = v166 & 0x70204LL;                                                                       | 
          v16 = (char *)&v169;                                                                                             | 
        }                                                                                                                  | 
        else if ( (_DWORD)v121 == 56 )                                                                                     | 
        {                                                                                                                  | 
          v172 = v164;                                                                                                     | 
          *(_QWORD *)&v173 = *(_QWORD *)v165;                                                                              | 
          *((_QWORD *)&v173 + 1) = v167;                                                                                   | 
          v174 = *(_OWORD *)&v165[8];                                                                                      | 
          v175 = v166 & 0x278204LL;                                                                                        | 
          v16 = (char *)&v172;                                                                                             | 
        }                                                                                                                  | 
        else                                                                                                               | 
        {                                                                                                                  | 
          v16 = (char *)&v164;                                                                                             | 
        }                                                                                                                  | 
        goto LABEL_72;                                                                                                     | 
      }                                                                                                                    | 
      switch ( (_DWORD)v7 )                                                                                                | 
      {                                                                                                                    | 
        case '"':                                                                                                          | 
LABEL_204:                                                                                                                 | 
          LOWORD(v114) = 0;                                                                                                | 
          BYTE2(v114) = 0;                                                                                                 | 
          PspQueryJobHierarchyAccountingInformation(v15);                                                                  | 
          v148 = v205[9];                                                                                                  | 
          v150 = v205[10];                                                                                                 | 
          v152 = v205[0];                                                                                                  | 
          PspLockJobExclusive(v15, v119);                                                                                  | 
          if ( (*((_DWORD *)v15 + 388) & 0x4008) != 0x4008 )                                                               | 
          {                                                                                                                | 
            _InterlockedAnd((volatile signed __int32 *)v15 + 388, 0xFFFFFFFB);                                             | 
            v15 = (char *)Object;                                                                                          | 
          }                                                                                                                | 
          PspLockJobMemoryLimitsShared(v15, 0LL);                                                                          | 
          v154 = *((_QWORD *)v15 + 151) << 12;                                                                             | 
          PspUnlockJobMemoryLimitsShared(v15, 0LL);                                                                        | 
          v65 = (_DWORD *)*((_QWORD *)v15 + 152);                                                                          | 
          if ( v65 )                                                                                                       | 
          {                                                                                                                | 
            LODWORD(v147) = *v65;                                                                                          | 
            HIDWORD(v147) = *(_DWORD *)(*((_QWORD *)v15 + 152) + 4LL);                                                     | 
            v66 = (_QWORD *)*((_QWORD *)v15 + 152);                                                                        | 
            v67 = v66[9];                                                                                                  | 
            if ( !v67 )                                                                                                    | 
            {                                                                                                              | 
              v67 = v66[1];                                                                                                | 
            }                                                                                                              | 
            v149 = v67;                                                                                                    | 
            v68 = v66[10];                                                                                                 | 
            if ( !v68 )                                                                                                    | 
            {                                                                                                              | 
              v68 = v66[2];                                                                                                | 
            }                                                                                                              | 
            v151 = v68;                                                                                                    | 
            v69 = v66[11];                                                                                                 | 
            if ( !v69 )                                                                                                    | 
            {                                                                                                              | 
              v69 = v66[3];                                                                                                | 
            }                                                                                                              | 
            v153 = v69;                                                                                                    | 
            v70 = v66[12];                                                                                                 | 
            if ( !v70 )                                                                                                    | 
            {                                                                                                              | 
              v70 = v66[4];                                                                                                | 
            }                                                                                                              | 
            v158 = v70 << 12;                                                                                              | 
            v71 = v66[13];                                                                                                 | 
            if ( !v71 )                                                                                                    | 
            {                                                                                                              | 
              v71 = v66[5];                                                                                                | 
            }                                                                                                              | 
            v155 = v71 << 12;                                                                                              | 
            v72 = 0LL;                                                                                                     | 
            do                                                                                                             | 
            {                                                                                                              | 
              v73 = (_DWORD *)PspLimitViolationRateControlToleranceLimitField(&v147, v72);                                 | 
              v77 = v75[16];                                                                                               | 
              if ( v77 )                                                                                                   | 
              {                                                                                                            | 
                *v76 = 1;                                                                                                  | 
              }                                                                                                            | 
              else                                                                                                         | 
              {                                                                                                            | 
                v77 = *v75;                                                                                                | 
              }                                                                                                            | 
              *v73 = v77;                                                                                                  | 
              v72 = (unsigned int)(v74 + 1);                                                                               | 
              v78 = v76 + 1;                                                                                               | 
            }                                                                                                              | 
            while ( (int)v72 < 3 );                                                                                        | 
          }                                                                                                                | 
          else                                                                                                             | 
          {                                                                                                                | 
            v147 = 0LL;                                                                                                    | 
            v149 = 0LL;                                                                                                    | 
            v151 = 0LL;                                                                                                    | 
            v153 = 0LL;                                                                                                    | 
            v158 = 0LL;                                                                                                    | 
            v155 = 0LL;                                                                                                    | 
            v79 = 0LL;                                                                                                     | 
            do                                                                                                             | 
            {                                                                                                              | 
              *(_DWORD *)PspLimitViolationRateControlToleranceLimitField(&v147, v79) = 0;                                  | 
              v79 = (unsigned int)(v80 + 1);                                                                               | 
            }                                                                                                              | 
            while ( (int)v79 < 3 );                                                                                        | 
          }                                                                                                                | 
          v81 = 0;                                                                                                         | 
          v82 = &v114;                                                                                                     | 
          v83 = 60LL;                                                                                                      | 
          v123 = 60LL;                                                                                                     | 
          do                                                                                                               | 
          {                                                                                                                | 
            v84 = *((_QWORD *)v15 + 152);                                                                                  | 
            if ( v84 && (v85 = PspRateControlLimitFlag((unsigned int)v81, v84, v83, v78), (v85 & *v86) != 0) )             | 
            {                                                                                                              | 
              v87 = *(_DWORD *)((char *)v86 + v83);                                                                        | 
            }                                                                                                              | 
            else                                                                                                           | 
            {                                                                                                              | 
              v87 = 1;                                                                                                     | 
            }                                                                                                              | 
            v124 = v87;                                                                                                    | 
            LOBYTE(v83) = *(_BYTE *)v82;                                                                                   | 
            PspQueryRateControlHistory((_DWORD)v15, v87, v83, (unsigned int)&v132, v81);                                   | 
            if ( v81 )                                                                                                     | 
            {                                                                                                              | 
              if ( v81 == 1 )                                                                                              | 
              {                                                                                                            | 
                v88 = &v159;                                                                                               | 
              }                                                                                                            | 
              else                                                                                                         | 
              {                                                                                                            | 
                v88 = (char *)&v156;                                                                                       | 
              }                                                                                                            | 
            }                                                                                                              | 
            else                                                                                                           | 
            {                                                                                                              | 
              v88 = &v160;                                                                                                 | 
            }                                                                                                              | 
            *(_DWORD *)v88 = v132;                                                                                         | 
            ++v81;                                                                                                         | 
            v83 = v123 + 4;                                                                                                | 
            v123 += 4LL;                                                                                                   | 
            v82 = (int *)((char *)v82 + 1);                                                                                | 
          }                                                                                                                | 
          while ( v81 < 3 );                                                                                               | 
          v89 = *((_QWORD *)v15 + 152);                                                                                    | 
          v90 = v119;                                                                                                      | 
          if ( v89 )                                                                                                       | 
          {                                                                                                                | 
            *(_DWORD *)(v89 + 4) = 0;                                                                                      | 
            memset_0((void *)(*((_QWORD *)v15 + 152) + 72LL), 0, 0x40uLL);                                                 | 
          }                                                                                                                | 
          PspUnlockJob(v15, v90);                                                                                          | 
          if ( (_DWORD)v121 == 88 )                                                                                        | 
          {                                                                                                                | 
            v184[1] = v148;                                                                                                | 
            v184[2] = v149;                                                                                                | 
            v184[3] = v150;                                                                                                | 
            v184[4] = v151;                                                                                                | 
            v184[5] = v152;                                                                                                | 
            v184[6] = v153;                                                                                                | 
            v184[7] = v154;                                                                                                | 
            v184[8] = v158;                                                                                                | 
            v184[9] = v155;                                                                                                | 
            v185 = v156;                                                                                                   | 
            v186 = v157;                                                                                                   | 
            v184[0] = v147 & 0x27820400278204LL;                                                                           | 
            v16 = (char *)v184;                                                                                            | 
          }                                                                                                                | 
          else if ( (_DWORD)v121 == 80 )                                                                                   | 
          {                                                                                                                | 
            v181[1] = v148;                                                                                                | 
            v181[2] = v149;                                                                                                | 
            v181[3] = v150;                                                                                                | 
            v181[4] = v151;                                                                                                | 
            v181[5] = v152;                                                                                                | 
            v181[6] = v153;                                                                                                | 
            v181[7] = v154;                                                                                                | 
            v181[8] = v155;                                                                                                | 
            v182 = v156;                                                                                                   | 
            v183 = v157;                                                                                                   | 
            v181[0] = v147 & 0x7020400070204LL;                                                                            | 
            v16 = (char *)v181;                                                                                            | 
          }                                                                                                                | 
          else                                                                                                             | 
          {                                                                                                                | 
            v16 = (char *)&v147;                                                                                           | 
          }                                                                                                                | 
          JobIoAttribution = 0;                                                                                            | 
          if ( (PerfGlobalGroupMask & 0x80000) != 0 )                                                                      | 
          {                                                                                                                | 
            EtwTraceJobSetQuery((_DWORD)v15, v7, (unsigned int)&v147, (unsigned int)&v124, 0, 1830);                       | 
          }                                                                                                                | 
          goto LABEL_281;                                                                                                  | 
        case '$':                                                                                                          | 
          if ( (*((_DWORD *)v15 + 388) & 0x40000000) != 0 )                                                                | 
          {                                                                                                                | 
            LODWORD(v162) = *((_DWORD *)v15 + 367);                                                                        | 
            ParentSilo = PsGetParentSilo(v15);                                                                             | 
            if ( (unsigned __int8)PsIsHostSilo(ParentSilo) )                                                               | 
            {                                                                                                              | 
              DWORD1(v162) = 0;                                                                                            | 
            }                                                                                                              | 
            else                                                                                                           | 
            {                                                                                                              | 
              DWORD1(v162) = *(_DWORD *)(ParentSilo + 1468);                                                               | 
            }                                                                                                              | 
            DWORD2(v162) = *((_DWORD *)v15 + 364);                                                                         | 
            BYTE12(v162) = PspIsSiloInServerSilo(v15);                                                                     | 
            v16 = (char *)&v162;                                                                                           | 
            goto LABEL_72;                                                                                                 | 
          }                                                                                                                | 
          break;                                                                                                           | 
        case '%':                                                                                                          | 
          JobSilo = PspGetJobSilo(v15);                                                                                    | 
          if ( JobSilo )                                                                                                   | 
          {                                                                                                                | 
            JobIoAttribution = ObGetSiloRootDirectoryPath(JobSilo, P);                                                     | 
            if ( JobIoAttribution >= 0 )                                                                                   | 
            {                                                                                                              | 
              v112 = 1;                                                                                                    | 
              v63 = LOWORD(P[0]);                                                                                          | 
              v8 = LOWORD(P[0]) + 16;                                                                                      | 
              v114 = v8;                                                                                                   | 
              if ( v8 > (unsigned int)v121 )                                                                               | 
              {                                                                                                            | 
                JobIoAttribution = -1073741789;                                                                            | 
                v19 = a3;                                                                                                  | 
                v18 = a5;                                                                                                  | 
              }                                                                                                            | 
              else                                                                                                         | 
              {                                                                                                            | 
                Size[2] = 0;                                                                                               | 
                LOWORD(Size[1]) = P[0];                                                                                    | 
                HIWORD(Size[1]) = P[0];                                                                                    | 
                v19 = a3;                                                                                                  | 
                v126 = a3 + 2;                                                                                             | 
                if ( PreviousMode )                                                                                        | 
                {                                                                                                          | 
                  RtlCopyToUser(a3, &Size[1], 0x10uLL);                                                                    | 
                }                                                                                                          | 
                else                                                                                                       | 
                {                                                                                                          | 
                  RtlCopyVolatileMemory(a3, &Size[1], 0x10uLL);                                                            | 
                }                                                                                                          | 
                if ( PreviousMode )                                                                                        | 
                {                                                                                                          | 
                  RtlCopyToUser(v126, P[1], v63);                                                                          | 
                }                                                                                                          | 
                else                                                                                                       | 
                {                                                                                                          | 
                  RtlCopyVolatileMemory(v126, P[1], v63);                                                                  | 
                }                                                                                                          | 
                v18 = a5;                                                                                                  | 
                if ( a5 )                                                                                                  | 
                {                                                                                                          | 
                  *a5 = v8;                                                                                                | 
                }                                                                                                          | 
              }                                                                                                            | 
              ExFreePoolWithTag(P[1], 0);                                                                                  | 
              goto LABEL_283;                                                                                              | 
            }                                                                                                              | 
            goto LABEL_281;                                                                                                | 
          }                                                                                                                | 
          break;                                                                                                           | 
        case '&':                                                                                                          | 
          PsGetJobServerSilo(v15, &Size[1]);                                                                               | 
          v60 = *(_QWORD *)&Size[1];                                                                                       | 
          if ( !(unsigned __int8)PsIsHostSilo(*(_QWORD *)&Size[1]) )                                                       | 
          {                                                                                                                | 
            v61 = *(_QWORD *)(v60 + 1504);                                                                                 | 
            *(_QWORD *)((char *)&v141 + 4) = *(_QWORD *)(v61 + 1272);                                                      | 
            BYTE12(v141) = 0;                                                                                              | 
            LODWORD(v143) = *(_DWORD *)(v61 + 1336);                                                                       | 
            HIDWORD(v143) = (unsigned __int16)NtBuildNumber;                                                               | 
            if ( PreviousMode )                                                                                            | 
            {                                                                                                              | 
              v142 = 0LL;                                                                                                  | 
            }                                                                                                              | 
            else                                                                                                           | 
            {                                                                                                              | 
              v123 = 0LL;                                                                                                  | 
              MmQueryApiSetSchema(&v123, &v128);                                                                           | 
              *(_QWORD *)&v142 = *(_QWORD *)(PsGetServerSiloGlobals(v60) + 1008);                                          | 
              *((_QWORD *)&v142 + 1) = *(_QWORD *)v123;                                                                    | 
            }                                                                                                              | 
            LODWORD(v141) = PsGetServerSiloServiceSessionId(v60);                                                          | 
            JobIoAttribution = 0;                                                                                          | 
            v16 = (char *)&v141;                                                                                           | 
            goto LABEL_281;                                                                                                | 
          }                                                                                                                | 
          break;                                                                                                           | 
        default:                                                                                                           | 
          goto LABEL_258;                                                                                                  | 
      }                                                                                                                    | 
    }                                                                                                                      | 
    JobIoAttribution = -1073740535;                                                                                        | 
    goto LABEL_281;                                                                                                        | 
  }                                                                                                                        | 
  if ( (_DWORD)v7 == 26 )                                                                                                  | 
  {                                                                                                                        | 
    v19 = a3;                                                                                                              | 
    JobIoAttribution = PspQueryJobHierarchyInterferenceCount(v15);                                                         | 
    v112 = JobIoAttribution >= 0;                                                                                          | 
    HIDWORD(v121) = ((JobIoAttribution >> 31) & 0xFFFFFFF8) + 8;                                                           | 
    goto LABEL_74;                                                                                                         | 
  }                                                                                                                        | 
  if ( (int)v7 > 12 )                                                                                                      | 
  {                                                                                                                        | 
    if ( (_DWORD)v7 != 13 )                                                                                                | 
    {                                                                                                                      | 
      if ( (_DWORD)v7 != 14 )                                                                                              | 
      {                                                                                                                    | 
        switch ( (_DWORD)v7 )                                                                                              | 
        {                                                                                                                  | 
          case 0xF:                                                                                                        | 
            PspLockJobShared(v15, CurrentThread);                                                                          | 
            v29 = *((_QWORD *)v15 + 155);                                                                                  | 
            if ( v29 )                                                                                                     | 
            {                                                                                                              | 
              v30 = *(_DWORD *)(v29 + 40);                                                                                 | 
              if ( (v30 & 0x40) == 0 )                                                                                     | 
              {                                                                                                            | 
                HIDWORD(v136) = *(_DWORD *)(v29 + 44);                                                                     | 
                v31 = ((v30 & 4 | 2u) >> 1) | 4;                                                                           | 
                if ( (v30 & 1) == 0 )                                                                                      | 
                {                                                                                                          | 
                  v31 = (v30 & 4 | 2u) >> 1;                                                                               | 
                }                                                                                                          | 
                v32 = v31 | 8;                                                                                             | 
                if ( (v30 & 2) == 0 )                                                                                      | 
                {                                                                                                          | 
                  v32 = v31;                                                                                               | 
                }                                                                                                          | 
                v33 = v32 | 0x10;                                                                                          | 
                if ( (v30 & 0x20) == 0 )                                                                                   | 
                {                                                                                                          | 
                  v33 = v32;                                                                                               | 
                }                                                                                                          | 
                v34 = v33 | 0x20;                                                                                          | 
                if ( (v30 & 0x80) == 0 )                                                                                   | 
                {                                                                                                          | 
                  v34 = v33;                                                                                               | 
                }                                                                                                          | 
                LODWORD(v136) = v34;                                                                                       | 
              }                                                                                                            | 
            }                                                                                                              | 
            PspUnlockJob(v15, v119);                                                                                       | 
            v16 = (char *)&v136;                                                                                           | 
            goto LABEL_72;                                                                                                 | 
          case 0x10:                                                                                                       | 
            v131 = *((_DWORD *)v15 + 276);                                                                                 | 
            v16 = (char *)&v131;                                                                                           | 
            goto LABEL_72;                                                                                                 | 
          case 0x11:                                                                                                       | 
            v137 = *((_QWORD *)v15 + 71);                                                                                  | 
            v16 = (char *)&v137;                                                                                           | 
            goto LABEL_72;                                                                                                 | 
          case 0x12:                                                                                                       | 
            WORD3(v163) = 0;                                                                                               | 
            PspLockJobShared(v15, CurrentThread);                                                                          | 
            v28 = *((_DWORD *)v15 + 388);                                                                                  | 
            BYTE4(v163) = (v28 & 0x200) != 0;                                                                              | 
            BYTE5(v163) = (v28 & 0x80000) != 0;                                                                            | 
            LODWORD(v163) = ~(unsigned __int8)*((_DWORD *)v15 + 389) & 8 | 7;                                              | 
            *((_QWORD *)&v163 + 1) = *((_QWORD *)v15 + 148);                                                               | 
            PspUnlockJob(v15, v119);                                                                                       | 
            v16 = (char *)&v163;                                                                                           | 
            goto LABEL_72;                                                                                                 | 
        }                                                                                                                  | 
        if ( (_DWORD)v7 != 19 )                                                                                            | 
        {                                                                                                                  | 
          if ( (_DWORD)v7 == 20 )                                                                                          | 
          {                                                                                                                | 
            NotificationChannel = PspAllocateAndQueryNotificationChannel(CurrentThread, v15, v204);                        | 
            v16 = v204;                                                                                                    | 
            if ( NotificationChannel < 0 )                                                                                 | 
            {                                                                                                              | 
              v16 = 0LL;                                                                                                   | 
            }                                                                                                              | 
            JobIoAttribution = 0;                                                                                          | 
            if ( NotificationChannel < 0 )                                                                                 | 
            {                                                                                                              | 
              JobIoAttribution = NotificationChannel;                                                                      | 
            }                                                                                                              | 
            goto LABEL_281;                                                                                                | 
          }                                                                                                                | 
          if ( (_DWORD)v7 != 21 )                                                                                          | 
          {                                                                                                                | 
            goto LABEL_258;                                                                                                | 
          }                                                                                                                | 
          v117 = (*((_DWORD *)v15 + 388) & 0x400) != 0;                                                                    | 
          v16 = (char *)&v117;                                                                                             | 
          goto LABEL_72;                                                                                                   | 
        }                                                                                                                  | 
        goto LABEL_114;                                                                                                    | 
      }                                                                                                                    | 
      if ( a5 )                                                                                                            | 
      {                                                                                                                    | 
        v35 = v121;                                                                                                        | 
        if ( (v121 & 0xF) == 0 )                                                                                           | 
        {                                                                                                                  | 
          PspLockJobShared(v15, CurrentThread);                                                                            | 
          v36 = 2LL;                                                                                                       | 
          if ( (*((_DWORD *)v15 + 64) & 0x10) != 0 )                                                                       | 
          {                                                                                                                | 
            v39 = v15 + 264;                                                                                               | 
            v40 = &v207;                                                                                                   | 
            do                                                                                                             | 
            {                                                                                                              | 
              *(_OWORD *)v40 = *v39;                                                                                       | 
              *((_OWORD *)v40 + 1) = v39[1];                                                                               | 
              *((_OWORD *)v40 + 2) = v39[2];                                                                               | 
              *((_OWORD *)v40 + 3) = v39[3];                                                                               | 
              *((_OWORD *)v40 + 4) = v39[4];                                                                               | 
              *((_OWORD *)v40 + 5) = v39[5];                                                                               | 
              *((_OWORD *)v40 + 6) = v39[6];                                                                               | 
              v40 += 64;                                                                                                   | 
              *((_OWORD *)v40 - 1) = v39[7];                                                                               | 
              v39 += 8;                                                                                                    | 
              --v36;                                                                                                       | 
            }                                                                                                              | 
            while ( v36 );                                                                                                 | 
            *(_QWORD *)v40 = *(_QWORD *)v39;                                                                               | 
          }                                                                                                                | 
          else                                                                                                             | 
          {                                                                                                                | 
            v37 = &v207;                                                                                                   | 
            v38 = &KeActiveProcessors;                                                                                     | 
            do                                                                                                             | 
            {                                                                                                              | 
              *(_OWORD *)v37 = *(_OWORD *)&v38->Count;                                                                     | 
              *((_OWORD *)v37 + 1) = *(_OWORD *)&v38->StaticBitmap[1];                                                     | 
              *((_OWORD *)v37 + 2) = *(_OWORD *)&v38->StaticBitmap[3];                                                     | 
              *((_OWORD *)v37 + 3) = *(_OWORD *)&v38->StaticBitmap[5];                                                     | 
              *((_OWORD *)v37 + 4) = *(_OWORD *)&v38->StaticBitmap[7];                                                     | 
              *((_OWORD *)v37 + 5) = *(_OWORD *)&v38->StaticBitmap[9];                                                     | 
              *((_OWORD *)v37 + 6) = *(_OWORD *)&v38->StaticBitmap[11];                                                    | 
              v37 += 64;                                                                                                   | 
              *((_OWORD *)v37 - 1) = *(_OWORD *)&v38->StaticBitmap[13];                                                    | 
              v38 = (struct _KAFFINITY_EX *)((char *)v38 + 128);                                                           | 
              --v36;                                                                                                       | 
            }                                                                                                              | 
            while ( v36 );                                                                                                 | 
            *(_QWORD *)v37 = *(_QWORD *)&v38->Count;                                                                       | 
          }                                                                                                                | 
          PspUnlockJob(v15, v119);                                                                                         | 
          v41 = v207;                                                                                                      | 
          memset_0(v209, 0, 16LL * v207);                                                                                  | 
          if ( (_WORD)v41 )                                                                                                | 
          {                                                                                                                | 
            v42 = v209;                                                                                                    | 
            v43 = &v208;                                                                                                   | 
            v44 = 0;                                                                                                       | 
            v45 = v120[0];                                                                                                 | 
            do                                                                                                             | 
            {                                                                                                              | 
              *((_WORD *)v42 + 4) = v45;                                                                                   | 
              *(_QWORD *)v42 = *(_QWORD *)v43;                                                                             | 
              v45 = ++v44;                                                                                                 | 
              v43 += 8;                                                                                                    | 
              v42 += 16;                                                                                                   | 
            }                                                                                                              | 
            while ( v44 < (unsigned __int16)v41 );                                                                         | 
          }                                                                                                                | 
          HIDWORD(v121) = 16 * v41;                                                                                        | 
          v8 = 16 * v41;                                                                                                   | 
          if ( v35 < 16 * v41 )                                                                                            | 
          {                                                                                                                | 
            v8 = v35;                                                                                                      | 
          }                                                                                                                | 
          v114 = v8;                                                                                                       | 
          JobIoAttribution = v35 < 16 * v41 ? 0xC0000023 : 0;                                                              | 
          v112 = 1;                                                                                                        | 
          v19 = a3;                                                                                                        | 
          memmove(a3, v209, v8);                                                                                           | 
          v18 = a5;                                                                                                        | 
          goto LABEL_283;                                                                                                  | 
        }                                                                                                                  | 
      }                                                                                                                    | 
LABEL_90:                                                                                                                  | 
      JobIoAttribution = -1073741811;                                                                                      | 
      goto LABEL_281;                                                                                                      | 
    }                                                                                                                      | 
    goto LABEL_204;                                                                                                        | 
  }                                                                                                                        | 
  if ( (_DWORD)v7 == 12 )                                                                                                  | 
  {                                                                                                                        | 
    goto LABEL_167;                                                                                                        | 
  }                                                                                                                        | 
  if ( (int)v7 > 6 )                                                                                                       | 
  {                                                                                                                        | 
    switch ( (_DWORD)v7 )                                                                                                  | 
    {                                                                                                                      | 
      case 8:                                                                                                              | 
        goto LABEL_114;                                                                                                    | 
      case 9:                                                                                                              | 
        goto LABEL_92;                                                                                                     | 
      case 0xA:                                                                                                            | 
        PspLockJobListShared(CurrentThread);                                                                               | 
        PspUnlockJobListShared(v119);                                                                                      | 
        v16 = (char *)&v135;                                                                                               | 
        goto LABEL_72;                                                                                                     | 
      case 0xB:                                                                                                            | 
        if ( a5 )                                                                                                          | 
        {                                                                                                                  | 
          v20 = v121;                                                                                                      | 
          if ( (v121 & 1) == 0 )                                                                                           | 
          {                                                                                                                | 
            PspLockJobShared(v15, CurrentThread);                                                                          | 
            v21 = 0;                                                                                                       | 
            v22 = *((_WORD *)v15 + 132);                                                                                   | 
            if ( v22 )                                                                                                     | 
            {                                                                                                              | 
              v23 = v15 + 272;                                                                                             | 
              v24 = 0;                                                                                                     | 
              v25 = 0;                                                                                                     | 
              v26 = v120[0];                                                                                               | 
              do                                                                                                           | 
              {                                                                                                            | 
                if ( *v23 )                                                                                                | 
                {                                                                                                          | 
                  Src[v21++] = v26;                                                                                        | 
                  v24 = v25;                                                                                               | 
                }                                                                                                          | 
                v26 = ++v24;                                                                                               | 
                ++v23;                                                                                                     | 
                v25 = v24;                                                                                                 | 
              }                                                                                                            | 
              while ( v24 < v22 );                                                                                         | 
            }                                                                                                              | 
            PspUnlockJob(v15, v119);                                                                                       | 
            v8 = 2 * v21;                                                                                                  | 
            HIDWORD(v121) = v8;                                                                                            | 
            v112 = 1;                                                                                                      | 
            JobIoAttribution = v20 < v8 ? 0xC0000023 : 0;                                                                  | 
            if ( v8 > v20 )                                                                                                | 
            {                                                                                                              | 
              v8 = Size[0];                                                                                                | 
            }                                                                                                              | 
            Size[0] = v8;                                                                                                  | 
            v19 = a3;                                                                                                      | 
            memmove(a3, Src, v8);                                                                                          | 
            v18 = a5;                                                                                                      | 
            goto LABEL_283;                                                                                                | 
          }                                                                                                                | 
        }                                                                                                                  | 
        goto LABEL_90;                                                                                                     | 
    }                                                                                                                      | 
LABEL_258:                                                                                                                 | 
    JobIoAttribution = -1073741821;                                                                                        | 
    goto LABEL_281;                                                                                                        | 
  }                                                                                                                        | 
  switch ( (_DWORD)v7 )                                                                                                    | 
  {                                                                                                                        | 
    case 6:                                                                                                                | 
      v130 = *((_DWORD *)v15 + 137);                                                                                       | 
      v16 = (char *)&v130;                                                                                                 | 
LABEL_72:                                                                                                                  | 
      JobIoAttribution = 0;                                                                                                | 
LABEL_281:                                                                                                                 | 
      v18 = a5;                                                                                                            | 
      goto LABEL_282;                                                                                                      | 
    case 1:                                                                                                                | 
LABEL_114:                                                                                                                 | 
      PspQueryJobHierarchyAccountingInformation(v15);                                                                      | 
      v16 = (char *)v205;                                                                                                  | 
      goto LABEL_72;                                                                                                       | 
    case 2:                                                                                                                | 
LABEL_92:                                                                                                                  | 
      PspLockJobShared(v15, CurrentThread);                                                                                | 
      v191 = *((_QWORD *)v15 + 30);                                                                                        | 
      v192 = *((_QWORD *)v15 + 31);                                                                                        | 
      v193 = *((_DWORD *)v15 + 65);                                                                                        | 
      v195 = (unsigned __int8)v15[1101];                                                                                   | 
      v196 = *((_DWORD *)v15 + 145);                                                                                       | 
      if ( (unsigned int)KeIsSingleGroupAffinityEx(v15 + 264, v120) )                                                      | 
      {                                                                                                                    | 
        v194 = *(_QWORD *)&v15[8 * v120[0] + 272];                                                                         | 
      }                                                                                                                    | 
      else                                                                                                                 | 
      {                                                                                                                    | 
        v194 = 0LL;                                                                                                        | 
      }                                                                                                                    | 
      v189[0] = *((_QWORD *)v15 + 28);                                                                                     | 
      v189[1] = *((_QWORD *)v15 + 29);                                                                                     | 
      v190 = (PspGetJobLimitInformationValidFlags((unsigned int)v7, (unsigned int)v121) | 0x7FFF) & *((_DWORD *)v15 + 64); | 
      if ( (_DWORD)v7 == 9 )                                                                                               | 
      {                                                                                                                    | 
        PspLockJobMemoryLimitsShared(v15, 0LL);                                                                            | 
        v197 = *((_QWORD *)v15 + 86) << 12;                                                                                | 
        v198 = *((_QWORD *)v15 + 87) << 12;                                                                                | 
        v200 = *((_QWORD *)v15 + 90) << 12;                                                                                | 
        v201 = *((_QWORD *)v15 + 88) << 12;                                                                                | 
        PspUnlockJobMemoryLimitsShared(v15, 0LL);                                                                          | 
        PspUnlockJob(v15, v119);                                                                                           | 
        v199 = *((_QWORD *)v15 + 89) << 12;                                                                                | 
      }                                                                                                                    | 
      else                                                                                                                 | 
      {                                                                                                                    | 
        PspUnlockJob(v15, v119);                                                                                           | 
      }                                                                                                                    | 
      v16 = (char *)v189;                                                                                                  | 
      goto LABEL_72;                                                                                                       | 
  }                                                                                                                        | 
  if ( (_DWORD)v7 != 3 )                                                                                                   | 
  {                                                                                                                        | 
    if ( (_DWORD)v7 != 4 )                                                                                                 | 
    {                                                                                                                      | 
      if ( (_DWORD)v7 == 5 )                                                                                               | 
      {                                                                                                                    | 
        v146 = 0LL;                                                                                                        | 
        v112 = 1;                                                                                                          | 
        *(_OWORD *)a3 = 0LL;                                                                                               | 
        *((_OWORD *)a3 + 1) = 0LL;                                                                                         | 
        a3[4] = v146;                                                                                                      | 
        v18 = a5;                                                                                                          | 
LABEL_282:                                                                                                                 | 
        v19 = a3;                                                                                                          | 
        goto LABEL_283;                                                                                                    | 
      }                                                                                                                    | 
      goto LABEL_258;                                                                                                      | 
    }                                                                                                                      | 
    PspLockJobShared(v15, CurrentThread);                                                                                  | 
    v129 = *((_DWORD *)v15 + 136);                                                                                         | 
    PspUnlockJob(v15, v119);                                                                                               | 
    v16 = (char *)&v129;                                                                                                   | 
    goto LABEL_72;                                                                                                         | 
  }                                                                                                                        | 
  HIDWORD(v121) = 0;                                                                                                       | 
  v19 = a3;                                                                                                                | 
  JobIoAttribution = PspQueryJobHierarchyProcessIdList(v15);                                                               | 
  v112 = 1;                                                                                                                | 
LABEL_74:                                                                                                                  | 
  v18 = a5;                                                                                                                | 
LABEL_283:                                                                                                                 | 
  if ( v15 )                                                                                                               | 
  {                                                                                                                        | 
    ObfDereferenceObjectWithTag(v15, 0x79517350u);                                                                         | 
  }                                                                                                                        | 
  if ( JobIoAttribution >= 0 && !v112 )                                                                                    | 
  {                                                                                                                        | 
    memmove(v19, v16, v8);                                                                                                 | 
  }                                                                                                                        | 
  if ( v18 )                                                                                                               | 
  {                                                                                                                        | 
    *v18 = HIDWORD(v121);                                                                                                  | 
  }                                                                                                                        | 
  return (unsigned int)JobIoAttribution;                                                                                   | 
}                                                                                                                          | 
1109 row(s)
