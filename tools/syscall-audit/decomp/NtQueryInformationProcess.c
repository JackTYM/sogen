NTSTATUS __fastcall NtQueryInformationProcess(                                                                                                                          | 
        ULONG_PTR BugCheckParameter1,                                                                                                                                   | 
        int a2,                                                                                                                                                         | 
        void *a3,                                                                                                                                                       | 
        unsigned int a4,                                                                                                                                                | 
        unsigned int *a5)                                                                                                                                               | 
{                                                                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                                         | 
                                                                                                                                                                        | 
  v5 = a4;                                                                                                                                                              | 
  v6 = (unsigned __int64)a3;                                                                                                                                            | 
  v169 = BugCheckParameter1;                                                                                                                                            | 
  Handle[1] = a3;                                                                                                                                                       | 
  v204 = a4;                                                                                                                                                            | 
  Handle[2] = a5;                                                                                                                                                       | 
  memset(v227, 0, sizeof(v227));                                                                                                                                        | 
  Ptr = 0LL;                                                                                                                                                            | 
  v166 = 0;                                                                                                                                                             | 
  Handle[0] = 0LL;                                                                                                                                                      | 
  v181 = 0;                                                                                                                                                             | 
  memset_0(&v231, 0, 0x108uLL);                                                                                                                                         | 
  FileObject = 0LL;                                                                                                                                                     | 
  v223 = 0LL;                                                                                                                                                           | 
  v199 = 0;                                                                                                                                                             | 
  v198 = 0;                                                                                                                                                             | 
  memset_0(v233, 0, 0x98uLL);                                                                                                                                           | 
  v215 = 0LL;                                                                                                                                                           | 
  v201 = 0LL;                                                                                                                                                           | 
  v228 = 0LL;                                                                                                                                                           | 
  v229 = 0LL;                                                                                                                                                           | 
  v230 = 0LL;                                                                                                                                                           | 
  v202 = 0LL;                                                                                                                                                           | 
  ObjectNameInformation = 0LL;                                                                                                                                          | 
  v168 = 0;                                                                                                                                                             | 
  v171 = 0;                                                                                                                                                             | 
  Object = 0LL;                                                                                                                                                         | 
  v206 = 0LL;                                                                                                                                                           | 
  v183 = 0LL;                                                                                                                                                           | 
  memset_0(v217, 0, 0x68uLL);                                                                                                                                           | 
  memset_0(&Src, 0, 0x70uLL);                                                                                                                                           | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                                                                    | 
  v162 = PreviousMode;                                                                                                                                                  | 
  CurrentThread = KeGetCurrentThread();                                                                                                                                 | 
  v176 = CurrentThread;                                                                                                                                                 | 
  if ( !PreviousMode )                                                                                                                                                  | 
  {                                                                                                                                                                     | 
    v13 = 1LL;                                                                                                                                                          | 
    v170 = 3LL;                                                                                                                                                         | 
    v14 = 0x7FFFFFFF0000LL;                                                                                                                                             | 
    goto LABEL_3;                                                                                                                                                       | 
  }                                                                                                                                                                     | 
  if ( a2 == 18 || a2 == 61 )                                                                                                                                           | 
  {                                                                                                                                                                     | 
    goto LABEL_84;                                                                                                                                                      | 
  }                                                                                                                                                                     | 
  if ( a2 == 65 )                                                                                                                                                       | 
  {                                                                                                                                                                     | 
    v29 = 7LL;                                                                                                                                                          | 
    v13 = 1LL;                                                                                                                                                          | 
    v170 = 3LL;                                                                                                                                                         | 
    goto LABEL_74;                                                                                                                                                      | 
  }                                                                                                                                                                     | 
  if ( ((a2 - 70) & 0xFFFFFFFB) == 0 )                                                                                                                                  | 
  {                                                                                                                                                                     | 
    goto LABEL_84;                                                                                                                                                      | 
  }                                                                                                                                                                     | 
  if ( a2 == 92 )                                                                                                                                                       | 
  {                                                                                                                                                                     | 
    v29 = 7LL;                                                                                                                                                          | 
    v13 = 1LL;                                                                                                                                                          | 
    v170 = 3LL;                                                                                                                                                         | 
    goto LABEL_74;                                                                                                                                                      | 
  }                                                                                                                                                                     | 
  if ( a2 != 94 )                                                                                                                                                       | 
  {                                                                                                                                                                     | 
    if ( a2 != 87 )                                                                                                                                                     | 
    {                                                                                                                                                                   | 
      if ( a2 == 47 )                                                                                                                                                   | 
      {                                                                                                                                                                 | 
        v13 = 1LL;                                                                                                                                                      | 
        v29 = 1LL;                                                                                                                                                      | 
        v170 = 3LL;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v170 = 3LL;                                                                                                                                                     | 
        v28 = 3;                                                                                                                                                        | 
        if ( a2 == 115 )                                                                                                                                                | 
        {                                                                                                                                                               | 
          v28 = 7;                                                                                                                                                      | 
        }                                                                                                                                                               | 
        v29 = v28;                                                                                                                                                      | 
        v13 = 1LL;                                                                                                                                                      | 
      }                                                                                                                                                                 | 
      goto LABEL_74;                                                                                                                                                    | 
    }                                                                                                                                                                   | 
LABEL_84:                                                                                                                                                               | 
    v29 = 0LL;                                                                                                                                                          | 
    v13 = 1LL;                                                                                                                                                          | 
    v170 = 3LL;                                                                                                                                                         | 
    goto LABEL_74;                                                                                                                                                      | 
  }                                                                                                                                                                     | 
  v29 = 7LL;                                                                                                                                                            | 
  v13 = 1LL;                                                                                                                                                            | 
  v170 = 3LL;                                                                                                                                                           | 
LABEL_74:                                                                                                                                                               | 
  if ( (_DWORD)v5 )                                                                                                                                                     | 
  {                                                                                                                                                                     | 
    if ( (v6 & v29) != 0 )                                                                                                                                              | 
    {                                                                                                                                                                   | 
      ExRaiseDatatypeMisalignment();                                                                                                                                    | 
    }                                                                                                                                                                   | 
    v14 = 0x7FFFFFFF0000LL;                                                                                                                                             | 
  }                                                                                                                                                                     | 
  else                                                                                                                                                                  | 
  {                                                                                                                                                                     | 
    v14 = 0x7FFFFFFF0000LL;                                                                                                                                             | 
  }                                                                                                                                                                     | 
  if ( a5 )                                                                                                                                                             | 
  {                                                                                                                                                                     | 
    v30 = 0x7FFFFFFF0000LL;                                                                                                                                             | 
    if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )                                                                                                                      | 
    {                                                                                                                                                                   | 
      v30 = (__int64)a5;                                                                                                                                                | 
    }                                                                                                                                                                   | 
    *(_DWORD *)v30 = *(_DWORD *)v30;                                                                                                                                    | 
  }                                                                                                                                                                     | 
LABEL_3:                                                                                                                                                                | 
  if ( a2 == 52 )                                                                                                                                                       | 
  {                                                                                                                                                                     | 
    v182 = 0;                                                                                                                                                           | 
    v164 = 0;                                                                                                                                                           | 
    v165 = 0;                                                                                                                                                           | 
    if ( (_DWORD)v5 != 8 )                                                                                                                                              | 
    {                                                                                                                                                                   | 
      return -1073741820;                                                                                                                                               | 
    }                                                                                                                                                                   | 
    v15 = *(_DWORD *)v6;                                                                                                                                                | 
    v197 = *(_DWORD *)v6;                                                                                                                                               | 
    if ( BugCheckParameter1 == -1LL )                                                                                                                                   | 
    {                                                                                                                                                                   | 
      Object = KeGetCurrentThread()->ApcState.Process;                                                                                                                  | 
      v15 = v197;                                                                                                                                                       | 
    }                                                                                                                                                                   | 
    else                                                                                                                                                                | 
    {                                                                                                                                                                   | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
    }                                                                                                                                                                   | 
    switch ( v15 )                                                                                                                                                      | 
    {                                                                                                                                                                   | 
      case 1:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        if ( (*((_DWORD *)Object + 468) & 0x40) == 0 )                                                                                                                  | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 468) & 0x10) != 0 )                                                                                                                  | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 468) & 0x20) != 0 )                                                                                                                  | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 468) & 8) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 8u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 2:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        v17 = *((_DWORD *)Object + 468);                                                                                                                                | 
        if ( (v17 & 0x100) != 0 )                                                                                                                                       | 
        {                                                                                                                                                               | 
          v18 = *(_DWORD *)(v6 + 4) | 1;                                                                                                                                | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          if ( (v17 & 0x800) == 0 )                                                                                                                                     | 
          {                                                                                                                                                             | 
LABEL_10:                                                                                                                                                               | 
            if ( (*((_DWORD *)Object + 468) & 0x200) != 0 )                                                                                                             | 
            {                                                                                                                                                           | 
              *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                | 
            }                                                                                                                                                           | 
            if ( (*((_DWORD *)Object + 468) & 0x400) != 0 )                                                                                                             | 
            {                                                                                                                                                           | 
              *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                | 
            }                                                                                                                                                           | 
            goto LABEL_17;                                                                                                                                              | 
          }                                                                                                                                                             | 
          v18 = *(_DWORD *)(v6 + 4) | 8;                                                                                                                                | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 4) = v18;                                                                                                                                      | 
        goto LABEL_10;                                                                                                                                                  | 
      case 3:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        v31 = ObReferenceProcessHandleTable(Object);                                                                                                                    | 
        if ( v31 )                                                                                                                                                      | 
        {                                                                                                                                                               | 
          ExQueryHandleExceptionsPermanency(v31, &v164, &v165);                                                                                                         | 
          *(_DWORD *)(v6 + 4) = 0;                                                                                                                                      | 
          if ( v164 )                                                                                                                                                   | 
          {                                                                                                                                                             | 
            *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                  | 
          }                                                                                                                                                             | 
          if ( v165 )                                                                                                                                                   | 
          {                                                                                                                                                             | 
            *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                  | 
          }                                                                                                                                                             | 
          ExReleaseRundownProtection_0((PEX_RUNDOWN_REF)Object + 61);                                                                                                   | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          WorkingSetInformation = -1073741558;                                                                                                                          | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 4:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        v32 = *((_DWORD *)Object + 468);                                                                                                                                | 
        if ( (v32 & 0x1000) != 0 )                                                                                                                                      | 
        {                                                                                                                                                               | 
          v35 = *(_DWORD *)(v6 + 4) | 1;                                                                                                                                | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          if ( (v32 & 0x2000) == 0 )                                                                                                                                    | 
          {                                                                                                                                                             | 
            goto LABEL_139;                                                                                                                                             | 
          }                                                                                                                                                             | 
          v35 = *(_DWORD *)(v6 + 4) | 2;                                                                                                                                | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 4) = v35;                                                                                                                                      | 
LABEL_139:                                                                                                                                                              | 
        v33 = *((_DWORD *)Object + 502);                                                                                                                                | 
        if ( (v33 & 2) != 0 )                                                                                                                                           | 
        {                                                                                                                                                               | 
          v34 = *(_DWORD *)(v6 + 4) | 4;                                                                                                                                | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          if ( (v33 & 4) == 0 )                                                                                                                                         | 
          {                                                                                                                                                             | 
            goto LABEL_17;                                                                                                                                              | 
          }                                                                                                                                                             | 
          v34 = *(_DWORD *)(v6 + 4) | 8;                                                                                                                                | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 4) = v34;                                                                                                                                      | 
LABEL_17:                                                                                                                                                               | 
        if ( BugCheckParameter1 != -1LL )                                                                                                                               | 
        {                                                                                                                                                               | 
          ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                             | 
        }                                                                                                                                                               | 
        return WorkingSetInformation;                                                                                                                                   | 
      case 6:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        if ( (*((_DWORD *)Object + 468) & 0x80u) != 0 )                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 7:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        if ( (*((_DWORD *)Object + 468) & 1) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 468) & 2) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 468) & 4) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 8:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        PsQueryProcessSignatureMitigationPolicy(Object, &v182);                                                                                                         | 
        *(_DWORD *)(v6 + 4) = v182;                                                                                                                                     | 
        goto LABEL_17;                                                                                                                                                  | 
      case 9:                                                                                                                                                           | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        v40 = *((_DWORD *)Object + 468);                                                                                                                                | 
        if ( (v40 & 0x10000) != 0 )                                                                                                                                     | 
        {                                                                                                                                                               | 
          v41 = *(_DWORD *)(v6 + 4) | 1;                                                                                                                                | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          if ( (v40 & 0x20000) == 0 )                                                                                                                                   | 
          {                                                                                                                                                             | 
            goto LABEL_17;                                                                                                                                              | 
          }                                                                                                                                                             | 
          v41 = *(_DWORD *)(v6 + 4) | 2;                                                                                                                                | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 4) = v41;                                                                                                                                      | 
        goto LABEL_17;                                                                                                                                                  | 
      case 10:                                                                                                                                                          | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        v36 = *((_DWORD *)Object + 468);                                                                                                                                | 
        if ( (v36 & 0x80000) != 0 )                                                                                                                                     | 
        {                                                                                                                                                               | 
          v39 = *(_DWORD *)(v6 + 4) | 1;                                                                                                                                | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          if ( (v36 & 0x100000) == 0 )                                                                                                                                  | 
          {                                                                                                                                                             | 
            goto LABEL_150;                                                                                                                                             | 
          }                                                                                                                                                             | 
          v39 = *(_DWORD *)(v6 + 4) | 8;                                                                                                                                | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 4) = v39;                                                                                                                                      | 
LABEL_150:                                                                                                                                                              | 
        v37 = *((_DWORD *)Object + 468);                                                                                                                                | 
        if ( (v37 & 0x200000) != 0 )                                                                                                                                    | 
        {                                                                                                                                                               | 
          v38 = *(_DWORD *)(v6 + 4) | 2;                                                                                                                                | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          if ( (v37 & 0x400000) == 0 )                                                                                                                                  | 
          {                                                                                                                                                             | 
            goto LABEL_152;                                                                                                                                             | 
          }                                                                                                                                                             | 
          v38 = *(_DWORD *)(v6 + 4) | 0x10;                                                                                                                             | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 4) = v38;                                                                                                                                      | 
LABEL_152:                                                                                                                                                              | 
        if ( (*((_DWORD *)Object + 468) & 0x40000) != 0 )                                                                                                               | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 11:                                                                                                                                                          | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        *(_DWORD *)(v6 + 4) ^= ((unsigned __int8)PsGetWin32KFilterSet() ^ (unsigned __int8)*(_DWORD *)(v6 + 4)) & 0xF;                                                  | 
        goto LABEL_17;                                                                                                                                                  | 
      case 12:                                                                                                                                                          | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        if ( (*((_DWORD *)Object + 469) & 1) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 4) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x400) != 0 )                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x10u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x10) != 0 )                                                                                                                  | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x40u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x40) != 0 )                                                                                                                  | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x100u;                                                                                                                                | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x100) != 0 )                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x400u;                                                                                                                                | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 2) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 8) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 8u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x800) != 0 )                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x20u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x20) != 0 )                                                                                                                  | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x80u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x80u) != 0 )                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x200u;                                                                                                                                | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x200) != 0 )                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x800u;                                                                                                                                | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 13:                                                                                                                                                          | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        NoChildProcessRestrictedPolicy = PspGetNoChildProcessRestrictedPolicy(Object, v10, v14, v13);                                                                   | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        v43 = NoChildProcessRestrictedPolicy - 1;                                                                                                                       | 
        if ( !v43 )                                                                                                                                                     | 
        {                                                                                                                                                               | 
          goto LABEL_176;                                                                                                                                               | 
        }                                                                                                                                                               | 
        v44 = v43 - 1;                                                                                                                                                  | 
        if ( v44 )                                                                                                                                                      | 
        {                                                                                                                                                               | 
          if ( v44 == 1 )                                                                                                                                               | 
          {                                                                                                                                                             | 
            *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                  | 
          }                                                                                                                                                             | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                    | 
LABEL_176:                                                                                                                                                              | 
          *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 14:                                                                                                                                                          | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        if ( (*((_DWORD *)Object + 468) & 0x40000000) != 0 )                                                                                                            | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( *((int *)Object + 468) < 0 )                                                                                                                               | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x1000) != 0 )                                                                                                                | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x2000) != 0 )                                                                                                                | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 8u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 502) & 1) != 0 )                                                                                                                     | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x10u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 15:                                                                                                                                                          | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        if ( (*((_DWORD *)Object + 469) & 0x4000) != 0 )                                                                                                                | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 1u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x8000) != 0 )                                                                                                                | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 2u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x20000) != 0 )                                                                                                               | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 4u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x40000) != 0 )                                                                                                               | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 8u;                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x100000) != 0 )                                                                                                              | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x10u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x200000) != 0 )                                                                                                              | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x20u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x400000) != 0 )                                                                                                              | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x40u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x800000) != 0 )                                                                                                              | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x80u;                                                                                                                                 | 
        }                                                                                                                                                               | 
        if ( (*((_DWORD *)Object + 469) & 0x40000000) != 0 )                                                                                                            | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x100u;                                                                                                                                | 
        }                                                                                                                                                               | 
        if ( *((int *)Object + 469) < 0 )                                                                                                                               | 
        {                                                                                                                                                               | 
          *(_DWORD *)(v6 + 4) |= 0x200u;                                                                                                                                | 
        }                                                                                                                                                               | 
        goto LABEL_17;                                                                                                                                                  | 
      case 16:                                                                                                                                                          | 
        WorkingSetInformation = 0;                                                                                                                                      | 
        RedirectionTrustPolicy = PspGetRedirectionTrustPolicy(Object);                                                                                                  | 
        *(_DWORD *)(v6 + 4) = 0;                                                                                                                                        | 
        v102 = RedirectionTrustPolicy - 1;                                                                                                                              | 
        if ( v102 )                                                                                                                                                     | 
        {                                                                                                                                                               | 
          if ( v102 != 1 )                                                                                                                                              | 
          {                                                                                                                                                             | 
            goto LABEL_17;                                                                                                                                              | 
          }                                                                                                                                                             | 
          v103 = *(_DWORD *)(v6 + 4) | 2;                                                                                                                               | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          v103 = *(_DWORD *)(v6 + 4) | 1;                                                                                                                               | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 4) = v103;                                                                                                                                     | 
        goto LABEL_17;                                                                                                                                                  | 
      default:                                                                                                                                                          | 
        WorkingSetInformation = -1073741637;                                                                                                                            | 
        goto LABEL_17;                                                                                                                                                  | 
    }                                                                                                                                                                   | 
  }                                                                                                                                                                     | 
  switch ( a2 )                                                                                                                                                         | 
  {                                                                                                                                                                     | 
    case 0:                                                                                                                                                             | 
      v163 = 0;                                                                                                                                                         | 
      if ( (_DWORD)v5 == 64 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        ObjectNameInformation = (POBJECT_NAME_INFORMATION)v6;                                                                                                           | 
        v19 = 64;                                                                                                                                                       | 
        FileObject = (PFILE_OBJECT)64;                                                                                                                                  | 
        *(_QWORD *)v6 = 64LL;                                                                                                                                           | 
        v6 += 8LL;                                                                                                                                                      | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        if ( (_DWORD)v5 != 48 )                                                                                                                                         | 
        {                                                                                                                                                               | 
          return -1073741820;                                                                                                                                           | 
        }                                                                                                                                                               | 
        ObjectNameInformation = 0LL;                                                                                                                                    | 
        v19 = 48;                                                                                                                                                       | 
        FileObject = (PFILE_OBJECT)48;                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v166 = v19;                                                                                                                                                       | 
      v179 = v6;                                                                                                                                                        | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      v20 = result;                                                                                                                                                     | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v21 = (_KPROCESS *)Object;                                                                                                                                        | 
      *(_DWORD *)v6 = *((_DWORD *)Object + 341);                                                                                                                        | 
      *(_QWORD *)(v6 + 8) = v21[1].ProcessListEntry.Blink;                                                                                                              | 
      if ( (v21->SecureState.SecureHandle & 1) != 0 )                                                                                                                   | 
      {                                                                                                                                                                 | 
        VslGetSecurePebAddress(v21);                                                                                                                                    | 
      }                                                                                                                                                                 | 
      if ( v20 < 0 )                                                                                                                                                    | 
      {                                                                                                                                                                 | 
        goto LABEL_62;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v22 = 0LL;                                                                                                                                                        | 
      if ( (*(_DWORD *)&v21->0 & 0x1000) != 0 && v21 != v176->Process )                                                                                                 | 
      {                                                                                                                                                                 | 
        goto LABEL_33;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v231 = 2097153LL;                                                                                                                                                 | 
      memset_0(v232, 0, 0x100uLL);                                                                                                                                      | 
      KeQueryAffinityProcess(v21, &v231, &v223, 0LL, &v168);                                                                                                            | 
      if ( v21 == v176->Process )                                                                                                                                       | 
      {                                                                                                                                                                 | 
        if ( !(unsigned int)KeTestBitGroupMask(&v223, v176->UserAffinityPrimaryGroup) )                                                                                 | 
        {                                                                                                                                                               | 
          goto LABEL_33;                                                                                                                                                | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v23 = v168;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      if ( v23 != 32 )                                                                                                                                                  | 
      {                                                                                                                                                                 | 
        v22 = v232[v23];                                                                                                                                                | 
      }                                                                                                                                                                 | 
LABEL_33:                                                                                                                                                               | 
      v24 = ObjectNameInformation;                                                                                                                                      | 
      if ( ObjectNameInformation )                                                                                                                                      | 
      {                                                                                                                                                                 | 
        PsQueryProcessAttributes(v21, &v163, 0LL);                                                                                                                      | 
        v24 = ObjectNameInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      *(_QWORD *)(v6 + 16) = v22;                                                                                                                                       | 
      *(_DWORD *)(v6 + 24) = v21->BasePriority;                                                                                                                         | 
      *(_QWORD *)(v6 + 32) = v21[1].Header.WaitListHead.Flink;                                                                                                          | 
      *(_QWORD *)(v6 + 40) = *(_QWORD *)&v21[1].StackCount.Value;                                                                                                       | 
      if ( v24 )                                                                                                                                                        | 
      {                                                                                                                                                                 | 
        LODWORD(v24[3].Name.Buffer) = 0;                                                                                                                                | 
        if ( (v21[3].ActiveGroupsMask.Masks[1] & 0x70000) != 0 )                                                                                                        | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) = 1;                                                                                                                              | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          LODWORD(v170) = 2;                                                                                                                                            | 
        }                                                                                                                                                               | 
        if ( v21[1].ReadyTime )                                                                                                                                         | 
        {                                                                                                                                                               | 
          v27 = WORD2(v21[3].PerProcessorCycleTimes);                                                                                                                   | 
          if ( v27 == 332 || v27 == 452 )                                                                                                                               | 
          {                                                                                                                                                             | 
            LODWORD(v24[3].Name.Buffer) = v170;                                                                                                                         | 
          }                                                                                                                                                             | 
        }                                                                                                                                                               | 
        if ( (v21[1].DirectoryTableBase & 0x4000000800000000LL) != 0 )                                                                                                  | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) |= 4u;                                                                                                                            | 
        }                                                                                                                                                               | 
        v25 = Object;                                                                                                                                                   | 
        v26 = (__int64)v24[3].Name.Buffer & 0xFFFFFFF7 | (*((_DWORD *)Object + 124) >> 4) & 8;                                                                          | 
        LODWORD(v24[3].Name.Buffer) = v26;                                                                                                                              | 
        if ( v25[84] + ((v25[34] >> 3) & 1) )                                                                                                                           | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) = v26 | 0x10;                                                                                                                     | 
        }                                                                                                                                                               | 
        if ( *((_QWORD *)v25 + 84) && *(_DWORD *)(*((_QWORD *)Object + 84) + 1072LL) )                                                                                  | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) |= 0x20u;                                                                                                                         | 
        }                                                                                                                                                               | 
        if ( v163 )                                                                                                                                                     | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) |= 0x40u;                                                                                                                         | 
        }                                                                                                                                                               | 
        v21 = (_KPROCESS *)Object;                                                                                                                                      | 
        if ( (*((_BYTE *)Object + 368) & 1) != 0 )                                                                                                                      | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) |= 0x80u;                                                                                                                         | 
        }                                                                                                                                                               | 
        if ( (v21->SecureState.SecureHandle & 2) != 0 )                                                                                                                 | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) |= 0x200u;                                                                                                                        | 
        }                                                                                                                                                               | 
        if ( v21[3].Padding[5] )                                                                                                                                        | 
        {                                                                                                                                                               | 
          LODWORD(v24[3].Name.Buffer) |= 0x100u;                                                                                                                        | 
        }                                                                                                                                                               | 
        v20 = WorkingSetInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = (unsigned int)FileObject;                                                                                                                                 | 
      }                                                                                                                                                                 | 
LABEL_62:                                                                                                                                                               | 
      ObfDereferenceObjectWithTag(v21, 0x79517350u);                                                                                                                    | 
      return v20;                                                                                                                                                       | 
    case 1:                                                                                                                                                             | 
      return PspQueryQuotaLimits(BugCheckParameter1, (__int64)a5, PreviousMode);                                                                                        | 
    case 2:                                                                                                                                                             | 
      if ( (_DWORD)v5 != 48 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      DeviceMapInformation = result;                                                                                                                                    | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        PsQueryStatisticsProcess(Object, v217);                                                                                                                         | 
        Src = v218;                                                                                                                                                     | 
        v188 = v219;                                                                                                                                                    | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        *(__m256i *)v6 = Src;                                                                                                                                           | 
        *(_OWORD *)(v6 + 32) = v188;                                                                                                                                    | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 48;                                                                                                                                                     | 
        }                                                                                                                                                               | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 3:                                                                                                                                                             | 
      if ( (_DWORD)v5 != 112 && (_DWORD)v5 != 88 && (_DWORD)v5 != 96 )                                                                                                  | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        *(_OWORD *)Src.m256i_i8 = *((_OWORD *)Object + 34);                                                                                                             | 
        Src.m256i_i32[4] = *((_DWORD *)Object + 257);                                                                                                                   | 
        v52 = Object;                                                                                                                                                   | 
        KiStackAttachProcess((ULONG_PTR)Object);                                                                                                                        | 
        WorkingSetInformation = MmQueryWorkingSetInformation(                                                                                                           | 
                                  (unsigned int)&Src.m256i_u32[6],                                                                                                      | 
                                  (unsigned int)&v188,                                                                                                                  | 
                                  (unsigned int)&v195,                                                                                                                  | 
                                  (unsigned int)&v202,                                                                                                                  | 
                                  (__int64)&v201,                                                                                                                       | 
                                  (__int64)&v199);                                                                                                                      | 
        KiUnstackDetachProcess(v227, 0LL);                                                                                                                              | 
        v189 = v52[65];                                                                                                                                                 | 
        *((_QWORD *)&v188 + 1) = *((_QWORD *)Object + 67);                                                                                                              | 
        v191 = *((_QWORD *)Object + 64);                                                                                                                                | 
        v190 = *((_QWORD *)Object + 66);                                                                                                                                | 
        v192 = *((_QWORD *)Object + 123) << 12;                                                                                                                         | 
        v193 = *((_QWORD *)Object + 124) << 12;                                                                                                                         | 
        v196 = *((_QWORD *)Object + 213) << 12;                                                                                                                         | 
        v194 = v192;                                                                                                                                                    | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        result = WorkingSetInformation;                                                                                                                                 | 
        if ( WorkingSetInformation >= 0 )                                                                                                                               | 
        {                                                                                                                                                               | 
          memmove((void *)v6, &Src, v5);                                                                                                                                | 
          if ( a5 )                                                                                                                                                     | 
          {                                                                                                                                                             | 
            *a5 = v5;                                                                                                                                                   | 
          }                                                                                                                                                             | 
          return 0;                                                                                                                                                     | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 4:                                                                                                                                                             | 
      if ( (_DWORD)v5 != 32 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      DeviceMapInformation = result;                                                                                                                                    | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        v45 = Object;                                                                                                                                                   | 
        v47 = PsQueryRuntimeProcess(Object, &v183);                                                                                                                     | 
        v48 = (unsigned int)KeMaximumIncrement;                                                                                                                         | 
        *(_QWORD *)(v6 + 16) = (unsigned int)KeMaximumIncrement * v47;                                                                                                  | 
        *(_QWORD *)(v6 + 24) = v48 * v183;                                                                                                                              | 
        *(_QWORD *)v6 = v45[63];                                                                                                                                        | 
        *(_QWORD *)(v6 + 8) = v45[184];                                                                                                                                 | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 32;                                                                                                                                                     | 
        }                                                                                                                                                               | 
        goto LABEL_192;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 7:                                                                                                                                                             | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        v98 = -(__int64)(*((_QWORD *)Object + 97) != 0LL);                                                                                                              | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        *(_QWORD *)v6 = v98;                                                                                                                                            | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 8;                                                                                                                                                      | 
        }                                                                                                                                                               | 
        return 0;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 10:                                                                                                                                                            | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        ProcessTelemetryCoverage = xKdEnumerateDebuggingDevices();                                                                                                      | 
        goto LABEL_333;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 12:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        v71 = *((_DWORD *)Object + 226);                                                                                                                                | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        *(_DWORD *)v6 = v71;                                                                                                                                            | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 4;                                                                                                                                                      | 
        }                                                                                                                                                               | 
        return 0;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 14:                                                                                                                                                            | 
      return PspQueryPooledQuotaLimits(BugCheckParameter1, PreviousMode);                                                                                               | 
    case 15:                                                                                                                                                            | 
    case 42:                                                                                                                                                            | 
      return PspQueryWorkingSetWatch(BugCheckParameter1, (__int64)a5, PreviousMode);                                                                                    | 
    case 18:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 2 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        HIBYTE(v171) = *((_BYTE *)Object + 839);                                                                                                                        | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        *(_WORD *)v6 = v171;                                                                                                                                            | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 2;                                                                                                                                                      | 
        }                                                                                                                                                               | 
        return 0;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 19:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      DeviceMapInformation = ObpReferenceObjectByHandleWithTag(                                                                                                         | 
                               BugCheckParameter1,                                                                                                                      | 
                               0x79517350u,                                                                                                                             | 
                               (__int64)&Object,                                                                                                                        | 
                               0LL,                                                                                                                                     | 
                               0LL);                                                                                                                                    | 
      if ( DeviceMapInformation >= 0 )                                                                                                                                  | 
      {                                                                                                                                                                 | 
        v118 = *((_BYTE *)Object + 503) & 1;                                                                                                                            | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        *(_DWORD *)v6 = v118;                                                                                                                                           | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 4;                                                                                                                                                      | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return DeviceMapInformation;                                                                                                                                      | 
    case 20:                                                                                                                                                            | 
      LODWORD(P) = 0;                                                                                                                                                   | 
      if ( (((_DWORD)v5 - 4) & 0xFFFFFFFB) != 0 )                                                                                                                       | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      ProcessHandleCount = ObGetProcessHandleCount(Object, &P);                                                                                                         | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_DWORD *)v6 = ProcessHandleCount;                                                                                                                               | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        *(_DWORD *)(v6 + 4) = (_DWORD)P;                                                                                                                                | 
      }                                                                                                                                                                 | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 21:                                                                                                                                                            | 
      if ( (((_DWORD)v5 - 8) & 0xFFFFFFF7) != 0 )                                                                                                                       | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      SectionInformation = result;                                                                                                                                      | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                           | 
      if ( (*((_DWORD *)Object + 34) & 0x1000) != 0 )                                                                                                                   | 
      {                                                                                                                                                                 | 
        SectionInformation = -1073741811;                                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v231 = 2097153LL;                                                                                                                                               | 
        memset_0(v232, 0, 0x100uLL);                                                                                                                                    | 
        KeQueryAffinityProcess(v64, &v231, &v223, 0LL, &v168);                                                                                                          | 
        v144 = v232[v168];                                                                                                                                              | 
        if ( (_DWORD)v5 == 16 )                                                                                                                                         | 
        {                                                                                                                                                               | 
          *(_WORD *)(v6 + 8) = v168;                                                                                                                                    | 
        }                                                                                                                                                               | 
        *(_QWORD *)v6 = v144;                                                                                                                                           | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = v5;                                                                                                                                                     | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      goto LABEL_299;                                                                                                                                                   | 
    case 22:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      DeviceMapInformation = ObpReferenceObjectByHandleWithTag(                                                                                                         | 
                               BugCheckParameter1,                                                                                                                      | 
                               0x79517350u,                                                                                                                             | 
                               (__int64)&Object,                                                                                                                        | 
                               0LL,                                                                                                                                     | 
                               0LL);                                                                                                                                    | 
      if ( DeviceMapInformation >= 0 )                                                                                                                                  | 
      {                                                                                                                                                                 | 
        v129 = (*((_DWORD *)Object + 34) >> 1) & 1;                                                                                                                     | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        *(_DWORD *)v6 = v129;                                                                                                                                           | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 4;                                                                                                                                                      | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return DeviceMapInformation;                                                                                                                                      | 
    case 23:                                                                                                                                                            | 
      if ( (unsigned int)v5 < 0x24 )                                                                                                                                    | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( (_DWORD)v5 == 48 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        v175 = *(_DWORD *)(v6 + 40);                                                                                                                                    | 
        v56 = v175;                                                                                                                                                     | 
        if ( (v175 & 0xFFFFFFFE) != 0 )                                                                                                                                 | 
        {                                                                                                                                                               | 
          return -1073741811;                                                                                                                                           | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        if ( (_DWORD)v5 != 36 )                                                                                                                                         | 
        {                                                                                                                                                               | 
          return -1073741820;                                                                                                                                           | 
        }                                                                                                                                                               | 
        v56 = 0;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      DeviceMapInformation = ObQueryDeviceMapInformation(Object, v6, v56);                                                                                              | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      if ( DeviceMapInformation >= 0 && a5 )                                                                                                                            | 
      {                                                                                                                                                                 | 
        *a5 = v5;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      return DeviceMapInformation;                                                                                                                                      | 
    case 24:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      SessionId = PsGetSessionId(Object);                                                                                                                               | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_DWORD *)v6 = SessionId;                                                                                                                                        | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 26:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v73 = (void **)*((_QWORD *)Object + 98);                                                                                                                          | 
      if ( v73 )                                                                                                                                                        | 
      {                                                                                                                                                                 | 
        Ptr = *v73;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_QWORD *)v6 = Ptr;                                                                                                                                              | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 8;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 27:                                                                                                                                                            | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      if ( (unsigned int)v5 < 0x10 )                                                                                                                                    | 
      {                                                                                                                                                                 | 
        v6 = (unsigned __int64)&v215;                                                                                                                                   | 
        v50 = 0;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        Ptr = (void *)(v6 + 16);                                                                                                                                        | 
        v50 = v5 - 16;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v166 = v50;                                                                                                                                                       | 
      v51 = PsQueryFullProcessImageName(Object, v6, Ptr, &v166);                                                                                                        | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      if ( v51 >= 0 && !Ptr )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        v51 = -1073741820;                                                                                                                                              | 
      }                                                                                                                                                                 | 
      if ( a5 && ((int)(v51 + 0x80000000) < 0 || v51 == -1073741820) )                                                                                                  | 
      {                                                                                                                                                                 | 
        *a5 = v166 + 16;                                                                                                                                                | 
      }                                                                                                                                                                 | 
      return v51;                                                                                                                                                       | 
    case 28:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      *(_DWORD *)v6 = 1;                                                                                                                                                | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 29:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v116 = (*((_DWORD *)Object + 125) >> 13) & 1;                                                                                                                     | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_DWORD *)v6 = v116;                                                                                                                                             | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 30:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      SectionInformation = DbgkOpenProcessDebugPort(Object, PreviousMode, Handle);                                                                                      | 
      v128 = Handle[0];                                                                                                                                                 | 
      if ( SectionInformation < 0 )                                                                                                                                     | 
      {                                                                                                                                                                 | 
        v128 = 0LL;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      Handle[0] = v128;                                                                                                                                                 | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_QWORD *)v6 = v128;                                                                                                                                             | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 8;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return SectionInformation;                                                                                                                                        | 
    case 31:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v94 = Object;                                                                                                                                                     | 
      *(_DWORD *)v6 = (*((_DWORD *)Object + 125) & 2) == 0;                                                                                                             | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      goto LABEL_423;                                                                                                                                                   | 
    case 32:                                                                                                                                                            | 
      LODWORD(P) = 0;                                                                                                                                                   | 
      if ( (unsigned int)v5 < 0x10 )                                                                                                                                    | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( (unsigned int)ExIsRestrictedCaller(PreviousMode, &P, 0x7FFFFFFF0000LL, 1LL) )                                                                                | 
      {                                                                                                                                                                 | 
        return -1073741790;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      v183 = (unsigned int)(v5 - 16) / 0xA0uLL;                                                                                                                         | 
      ObjectNameInformation = (POBJECT_NAME_INFORMATION)(v6 + 16);                                                                                                      | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      SectionInformation = result;                                                                                                                                      | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                           | 
      v82 = ObReferenceProcessHandleTable(Object);                                                                                                                      | 
      v83 = v82;                                                                                                                                                        | 
      v202 = v82;                                                                                                                                                       | 
      if ( !v82 )                                                                                                                                                       | 
      {                                                                                                                                                                 | 
        goto LABEL_536;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      v84 = ExReferenceHandleDebugInfo(v82);                                                                                                                            | 
      v85 = v84;                                                                                                                                                        | 
      v201 = v84;                                                                                                                                                       | 
      if ( v84 )                                                                                                                                                        | 
      {                                                                                                                                                                 | 
        v130 = 0;                                                                                                                                                       | 
        v162 = 0;                                                                                                                                                       | 
        if ( (*(_DWORD *)(v84 + 8) & 3) != 0 )                                                                                                                          | 
        {                                                                                                                                                               | 
          ExAcquireFastMutex((PKGUARDED_MUTEX)(v84 + 16));                                                                                                              | 
          v130 = 1;                                                                                                                                                     | 
          v162 = 1;                                                                                                                                                     | 
        }                                                                                                                                                               | 
        *(_DWORD *)(v6 + 8) = 0;                                                                                                                                        | 
        v131 = (unsigned int)(*(_DWORD *)(v85 + 72) % *(_DWORD *)(v85 + 4));                                                                                            | 
        v180 = v131;                                                                                                                                                    | 
        v132 = 0;                                                                                                                                                       | 
        v203 = 0;                                                                                                                                                       | 
        while ( v132 < *(_DWORD *)(v85 + 4) )                                                                                                                           | 
        {                                                                                                                                                               | 
          v133 = (_OWORD *)(v85 + 160 * v131);                                                                                                                          | 
          v220 = v133[5];                                                                                                                                               | 
          v221 = v133[6];                                                                                                                                               | 
          v222[0] = v133[7];                                                                                                                                            | 
          v222[1] = v133[8];                                                                                                                                            | 
          v222[2] = v133[9];                                                                                                                                            | 
          v222[3] = v133[10];                                                                                                                                           | 
          v222[4] = v133[11];                                                                                                                                           | 
          v222[5] = v133[12];                                                                                                                                           | 
          v222[6] = v133[13];                                                                                                                                           | 
          v222[7] = v133[14];                                                                                                                                           | 
          v134 = v221;                                                                                                                                                  | 
          if ( *(_QWORD *)v6 == (_QWORD)v221 || !*(_QWORD *)v6 )                                                                                                        | 
          {                                                                                                                                                             | 
            v135 = DWORD2(v221);                                                                                                                                        | 
            if ( DWORD2(v221) )                                                                                                                                         | 
            {                                                                                                                                                           | 
              ++*(_DWORD *)(v6 + 8);                                                                                                                                    | 
              if ( (_DWORD)v183 )                                                                                                                                       | 
              {                                                                                                                                                         | 
                v183 = (unsigned int)(v183 - 1);                                                                                                                        | 
                v209 = v183;                                                                                                                                            | 
                v136 = ObjectNameInformation;                                                                                                                           | 
                *(_QWORD *)&ObjectNameInformation->Name.Length = v134;                                                                                                  | 
                *(_OWORD *)&v136->Name.Buffer = v220;                                                                                                                   | 
                LODWORD(v136[1].Name.Buffer) = v135;                                                                                                                    | 
                v137 = 0;                                                                                                                                               | 
                v200 = 0;                                                                                                                                               | 
                while ( v137 < 0x10 )                                                                                                                                   | 
                {                                                                                                                                                       | 
                  if ( (_DWORD)P )                                                                                                                                      | 
                  {                                                                                                                                                     | 
                    v138 = 0LL;                                                                                                                                         | 
                  }                                                                                                                                                     | 
                  else                                                                                                                                                  | 
                  {                                                                                                                                                     | 
                    v138 = *((_QWORD *)v222 + v137);                                                                                                                    | 
                  }                                                                                                                                                     | 
                  *((_QWORD *)&v136[2].Name.Length + v137++) = v138;                                                                                                    | 
                  v200 = v137;                                                                                                                                          | 
                }                                                                                                                                                       | 
                ObjectNameInformation = v136 + 10;                                                                                                                      | 
                v213 = v136 + 10;                                                                                                                                       | 
              }                                                                                                                                                         | 
              else                                                                                                                                                      | 
              {                                                                                                                                                         | 
                SectionInformation = -1073741820;                                                                                                                       | 
                WorkingSetInformation = -1073741820;                                                                                                                    | 
              }                                                                                                                                                         | 
            }                                                                                                                                                           | 
          }                                                                                                                                                             | 
          if ( !(_DWORD)v131 )                                                                                                                                          | 
          {                                                                                                                                                             | 
            LODWORD(v131) = *(_DWORD *)(v85 + 4);                                                                                                                       | 
          }                                                                                                                                                             | 
          v131 = (unsigned int)(v131 - 1);                                                                                                                              | 
          v180 = v131;                                                                                                                                                  | 
          v203 = ++v132;                                                                                                                                                | 
        }                                                                                                                                                               | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = (_DWORD)ObjectNameInformation - v6;                                                                                                                     | 
        }                                                                                                                                                               | 
        if ( v130 )                                                                                                                                                     | 
        {                                                                                                                                                               | 
          KeReleaseGuardedMutex((PKGUARDED_MUTEX)(v85 + 16));                                                                                                           | 
        }                                                                                                                                                               | 
        ExDereferenceHandleDebugInfo(v83, v85);                                                                                                                         | 
        v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                         | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        SectionInformation = -1073741811;                                                                                                                               | 
      }                                                                                                                                                                 | 
      goto LABEL_298;                                                                                                                                                   | 
    case 33:                                                                                                                                                            | 
    case 110:                                                                                                                                                           | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v107 = (*((_DWORD *)Object + 125) >> 27) & 7;                                                                                                                     | 
      if ( a2 == 33 )                                                                                                                                                   | 
      {                                                                                                                                                                 | 
        *(_DWORD *)v6 = v107;                                                                                                                                           | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v108 = *((_QWORD *)Object + 84);                                                                                                                                | 
        if ( v108 )                                                                                                                                                     | 
        {                                                                                                                                                               | 
          v109 = *(_DWORD *)(v108 + 1084);                                                                                                                              | 
          if ( v107 >= v109 )                                                                                                                                           | 
          {                                                                                                                                                             | 
            v107 = v109;                                                                                                                                                | 
          }                                                                                                                                                             | 
        }                                                                                                                                                               | 
        *(_DWORD *)v6 = v107;                                                                                                                                           | 
        LODWORD(Ptr) = WorkingSetInformation;                                                                                                                           | 
      }                                                                                                                                                                 | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      goto LABEL_334;                                                                                                                                                   | 
    case 34:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 == -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        Process = KeGetCurrentThread()->ApcState.Process;                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                        | 
        if ( result < 0 )                                                                                                                                               | 
        {                                                                                                                                                               | 
          return result;                                                                                                                                                | 
        }                                                                                                                                                               | 
        Process = Object;                                                                                                                                               | 
      }                                                                                                                                                                 | 
      LODWORD(Ptr) = KeGetExecuteOptions(Process, &v181, v14, v13);                                                                                                     | 
      if ( BugCheckParameter1 != -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        ObfDereferenceObjectWithTag(v75, 0x79517350u);                                                                                                                  | 
      }                                                                                                                                                                 | 
      if ( (int)Ptr >= 0 )                                                                                                                                              | 
      {                                                                                                                                                                 | 
        *(_DWORD *)v6 = v181;                                                                                                                                           | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 4;                                                                                                                                                      | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return (int)Ptr;                                                                                                                                                  | 
    case 36:                                                                                                                                                            | 
      v179 = 0LL;                                                                                                                                                       | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 == -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        p_LockNV = &KeGetCurrentThread()->ApcState.Process->Header.LockNV;                                                                                              | 
        Object = p_LockNV;                                                                                                                                              | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v208 = 0LL;                                                                                                                                                     | 
        result = ObReferenceObjectByHandle(                                                                                                                             | 
                   (HANDLE)BugCheckParameter1,                                                                                                                          | 
                   0x20u,                                                                                                                                               | 
                   (POBJECT_TYPE)PsProcessType,                                                                                                                         | 
                   PreviousMode,                                                                                                                                        | 
                   &v208,                                                                                                                                               | 
                   0LL);                                                                                                                                                | 
        p_LockNV = v208;                                                                                                                                                | 
        Object = v208;                                                                                                                                                  | 
        if ( result < 0 )                                                                                                                                               | 
        {                                                                                                                                                               | 
          return result;                                                                                                                                                | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      for ( WorkingSetInformation = 0; ; LODWORD(Ptr) = WorkingSetInformation )                                                                                         | 
      {                                                                                                                                                                 | 
        v78 = p_LockNV[174];                                                                                                                                            | 
        if ( v78 )                                                                                                                                                      | 
        {                                                                                                                                                               | 
          break;                                                                                                                                                        | 
        }                                                                                                                                                               | 
        KeQuerySystemTimePrecise(&v179, v10, v14, v13);                                                                                                                 | 
        CurrentPrcb = KeGetCurrentPrcb();                                                                                                                               | 
        LowPart = KeQueryPerformanceCounter(0LL).LowPart;                                                                                                               | 
        v121 = ExGenRandom(1LL);                                                                                                                                        | 
        v122 = __rdtsc();                                                                                                                                               | 
        v10 = (unsigned int)v122 ^ v121 ^ CurrentPrcb->MmPageFaultCount;                                                                                                | 
        v14 = (unsigned int)v179 ^ HIDWORD(v179) ^ (unsigned int)v10 ^ LowPart ^ CurrentPrcb->InterruptCount ^ CurrentPrcb->InterruptTime ^ CurrentPrcb->KeSystemCalls; | 
        _InterlockedCompareExchange((volatile signed __int32 *)Object + 174, v14, 0);                                                                                   | 
        p_LockNV = Object;                                                                                                                                              | 
      }                                                                                                                                                                 | 
      *(_DWORD *)v6 = v78;                                                                                                                                              | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 != -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        ObfDereferenceObject(p_LockNV);                                                                                                                                 | 
        return (int)Ptr;                                                                                                                                                | 
      }                                                                                                                                                                 | 
      return (int)Ptr;                                                                                                                                                  | 
    case 37:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 64 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      v166 = result;                                                                                                                                                    | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v57 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                           | 
      if ( Object == v176->Process )                                                                                                                                    | 
      {                                                                                                                                                                 | 
        Ptr = (void *)*((_QWORD *)Object + 85);                                                                                                                         | 
        if ( !Ptr )                                                                                                                                                     | 
        {                                                                                                                                                               | 
          result = -1073741558;                                                                                                                                         | 
        }                                                                                                                                                               | 
        v166 = result;                                                                                                                                                  | 
        v58 = 0;                                                                                                                                                        | 
        SectionInformation = result;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v58 = 1;                                                                                                                                                        | 
        if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                                                                               | 
        {                                                                                                                                                               | 
          Ptr = v57[85].Ptr;                                                                                                                                            | 
          if ( Ptr )                                                                                                                                                    | 
          {                                                                                                                                                             | 
            PsReferenceSiloContext(v57[85].Count);                                                                                                                      | 
            SectionInformation = v166;                                                                                                                                  | 
          }                                                                                                                                                             | 
          else                                                                                                                                                          | 
          {                                                                                                                                                             | 
            SectionInformation = -1073741811;                                                                                                                           | 
          }                                                                                                                                                             | 
          ExReleaseRundownProtection_0(v57 + 61);                                                                                                                       | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          SectionInformation = -1073741558;                                                                                                                             | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      ObfDereferenceObjectWithTag(v57, 0x79517350u);                                                                                                                    | 
      if ( SectionInformation >= 0 )                                                                                                                                    | 
      {                                                                                                                                                                 | 
        SectionInformation = MmGetSectionInformation(Ptr, 1LL, v6);                                                                                                     | 
        if ( v58 )                                                                                                                                                      | 
        {                                                                                                                                                               | 
          ObfDereferenceObject(Ptr);                                                                                                                                    | 
        }                                                                                                                                                               | 
        if ( SectionInformation >= 0 && a5 )                                                                                                                            | 
        {                                                                                                                                                               | 
          *a5 = 64;                                                                                                                                                     | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return SectionInformation;                                                                                                                                        | 
    case 38:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 16 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      DeviceMapInformation = result;                                                                                                                                    | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v45 = Object;                                                                                                                                                     | 
      *(_QWORD *)v6 = PsQueryTotalCycleTimeProcess(Object, &v206);                                                                                                      | 
      *(_QWORD *)(v6 + 8) = v206;                                                                                                                                       | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 16;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      goto LABEL_192;                                                                                                                                                   | 
    case 39:                                                                                                                                                            | 
    case 111:                                                                                                                                                           | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v104 = (*((_DWORD *)Object + 124) >> 12) & 7;                                                                                                                     | 
      if ( a2 == 39 )                                                                                                                                                   | 
      {                                                                                                                                                                 | 
        *(_DWORD *)v6 = v104;                                                                                                                                           | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v105 = *((_QWORD *)Object + 84);                                                                                                                                | 
        if ( v105 )                                                                                                                                                     | 
        {                                                                                                                                                               | 
          v106 = *(_DWORD *)(v105 + 1092);                                                                                                                              | 
          if ( v104 >= v106 )                                                                                                                                           | 
          {                                                                                                                                                             | 
            v104 = v106;                                                                                                                                                | 
          }                                                                                                                                                             | 
        }                                                                                                                                                               | 
        *(_DWORD *)v6 = v104;                                                                                                                                           | 
        LODWORD(Ptr) = WorkingSetInformation;                                                                                                                           | 
      }                                                                                                                                                                 | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      goto LABEL_334;                                                                                                                                                   | 
    case 43:                                                                                                                                                            | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      DeviceMapInformation = result;                                                                                                                                    | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v60 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                           | 
      v61 = (struct _FILE_OBJECT *)*((_QWORD *)Object + 102);                                                                                                           | 
      FileObject = v61;                                                                                                                                                 | 
      if ( v61 )                                                                                                                                                        | 
      {                                                                                                                                                                 | 
        if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                                                                               | 
        {                                                                                                                                                               | 
          PsReferenceSiloContext(v61);                                                                                                                                  | 
          ExReleaseRundownProtection_0(v60 + 61);                                                                                                                       | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          DeviceMapInformation = -1073741558;                                                                                                                           | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        DeviceMapInformation = PsReferenceProcessFilePointer(Object, &FileObject);                                                                                      | 
        v61 = FileObject;                                                                                                                                               | 
      }                                                                                                                                                                 | 
      ObfDereferenceObjectWithTag(v60, 0x79517350u);                                                                                                                    | 
      if ( DeviceMapInformation < 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      DeviceMapInformation = IoQueryFileDosDeviceName(v61, &ObjectNameInformation);                                                                                     | 
      ObfDereferenceObject(v61);                                                                                                                                        | 
      if ( DeviceMapInformation < 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      v62 = ObjectNameInformation;                                                                                                                                      | 
      v63 = ObjectNameInformation->Name.MaximumLength + 16;                                                                                                             | 
      if ( v63 > (unsigned int)v5 )                                                                                                                                     | 
      {                                                                                                                                                                 | 
        DeviceMapInformation = -1073741820;                                                                                                                             | 
        WorkingSetInformation = -1073741820;                                                                                                                            | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        *(_WORD *)v6 = ObjectNameInformation->Name.Length;                                                                                                              | 
        *(_WORD *)(v6 + 2) = v62->Name.MaximumLength;                                                                                                                   | 
        if ( v62->Name.MaximumLength )                                                                                                                                  | 
        {                                                                                                                                                               | 
          Ptr = (void *)(v6 + 16);                                                                                                                                      | 
          memmove((void *)(v6 + 16), v62->Name.Buffer, v62->Name.MaximumLength);                                                                                        | 
        }                                                                                                                                                               | 
        *(_QWORD *)(v6 + 8) = Ptr;                                                                                                                                      | 
      }                                                                                                                                                                 | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = v63;                                                                                                                                                      | 
      }                                                                                                                                                                 | 
      ExFreePoolWithTag(v62, 0);                                                                                                                                        | 
      return DeviceMapInformation;                                                                                                                                      | 
    case 44:                                                                                                                                                            | 
      P = 0LL;                                                                                                                                                          | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      v139 = *(_QWORD *)v6;                                                                                                                                             | 
      v214 = *(_QWORD *)v6;                                                                                                                                             | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      DeviceMapInformation = ObpReferenceObjectByHandleWithTag(v139, 0x79517350u, (__int64)&FileObject, 0LL, 0LL);                                                      | 
      v49 = Object;                                                                                                                                                     | 
      if ( DeviceMapInformation < 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        goto LABEL_193;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      v51 = PsReferenceProcessFilePointer(Object, &P);                                                                                                                  | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      if ( v51 < 0 )                                                                                                                                                    | 
      {                                                                                                                                                                 | 
        ObfDereferenceObject(FileObject);                                                                                                                               | 
        return v51;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        SectionObjectPointer = FileObject->SectionObjectPointer;                                                                                                        | 
        v141 = P;                                                                                                                                                       | 
        v142 = (SECTION_OBJECT_POINTERS *)*((_QWORD *)P + 5);                                                                                                           | 
        v143 = 0;                                                                                                                                                       | 
        if ( SectionObjectPointer != v142 )                                                                                                                             | 
        {                                                                                                                                                               | 
          v143 = -1073741823;                                                                                                                                           | 
        }                                                                                                                                                               | 
        ObfDereferenceObject(FileObject);                                                                                                                               | 
        ObfDereferenceObject(v141);                                                                                                                                     | 
        result = v143;                                                                                                                                                  | 
        if ( SectionObjectPointer == v142 && a5 )                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 0;                                                                                                                                                      | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 45:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v175 = *((_DWORD *)Object + 124);                                                                                                                                 | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      if ( (v175 & 0x80000) != 0 )                                                                                                                                      | 
      {                                                                                                                                                                 | 
        LODWORD(Ptr) = 1;                                                                                                                                               | 
      }                                                                                                                                                                 | 
      if ( (v175 & 0x40000) != 0 )                                                                                                                                      | 
      {                                                                                                                                                                 | 
        LODWORD(Ptr) = (unsigned int)Ptr | 2;                                                                                                                           | 
      }                                                                                                                                                                 | 
      *(_DWORD *)v6 = (_DWORD)Ptr;                                                                                                                                      | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 46:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v175 = *((_DWORD *)Object + 125);                                                                                                                                 | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      if ( (v175 & 0x200000) != 0 )                                                                                                                                     | 
      {                                                                                                                                                                 | 
        LODWORD(Ptr) = 1;                                                                                                                                               | 
      }                                                                                                                                                                 | 
      *(_DWORD *)v6 = (_DWORD)Ptr;                                                                                                                                      | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 47:                                                                                                                                                            | 
      if ( !a5 || (v5 & 1) != 0 )                                                                                                                                       | 
      {                                                                                                                                                                 | 
        return -1073741811;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        v145 = Object;                                                                                                                                                  | 
        v223 = *(_OWORD *)KeQueryGroupMaskProcess(&v184, Object);                                                                                                       | 
        do                                                                                                                                                              | 
        {                                                                                                                                                               | 
          FirstSetRightGroupMask = KeFindFirstSetRightGroupMask(&v223);                                                                                                 | 
          v198 = FirstSetRightGroupMask;                                                                                                                                | 
          v147 = (_DWORD)Ptr + 2;                                                                                                                                       | 
          v166 = (_DWORD)Ptr + 2;                                                                                                                                       | 
          if ( (int)Ptr + 2 <= (unsigned int)v5 )                                                                                                                       | 
          {                                                                                                                                                             | 
            *(_WORD *)v6 = FirstSetRightGroupMask;                                                                                                                      | 
            v6 += 2LL;                                                                                                                                                  | 
            v216 = v6;                                                                                                                                                  | 
          }                                                                                                                                                             | 
          KeClearBitGroupMask(&v223);                                                                                                                                   | 
          LODWORD(Ptr) = (_DWORD)Ptr + 2;                                                                                                                               | 
        }                                                                                                                                                               | 
        while ( !(unsigned int)KeIsEmptyGroupMask(&v223) );                                                                                                             | 
        *a5 = v147;                                                                                                                                                     | 
        WorkingSetInformation = (unsigned int)v5 < v147 ? 0xC0000023 : 0;                                                                                               | 
        ObfDereferenceObjectWithTag(v145, 0x79517350u);                                                                                                                 | 
        return (unsigned int)v5 < v147 ? 0xC0000023 : 0;                                                                                                                | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 49:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v117 = *((_QWORD *)Object + 91);                                                                                                                                  | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_QWORD *)v6 = v117;                                                                                                                                             | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 8;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 50:                                                                                                                                                            | 
      v177 = 0;                                                                                                                                                         | 
      v178 = 0LL;                                                                                                                                                       | 
      v210[0] = 0LL;                                                                                                                                                    | 
      P = 0LL;                                                                                                                                                          | 
      if ( PreviousMode != 1 )                                                                                                                                          | 
      {                                                                                                                                                                 | 
        return -1073741823;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                         | 
        if ( ExAcquireRundownProtection_0((PEX_RUNDOWN_REF)Object + 61) )                                                                                               | 
        {                                                                                                                                                               | 
          KiStackAttachProcess((ULONG_PTR)v64);                                                                                                                         | 
          v65 = 0LL;                                                                                                                                                    | 
          Count = (__int64 *)v64[98].Count;                                                                                                                             | 
          if ( Count )                                                                                                                                                  | 
          {                                                                                                                                                             | 
            v65 = *Count;                                                                                                                                               | 
          }                                                                                                                                                             | 
          if ( v65 )                                                                                                                                                    | 
          {                                                                                                                                                             | 
            v174 = (PVOID)*(unsigned int *)(v65 + 16);                                                                                                                  | 
            v177 = *((_DWORD *)v174 + 26);                                                                                                                              | 
            RtlCopyVolatileMemory(&P, (char *)v174 + 112, 8uLL);                                                                                                        | 
            v69 = (unsigned __int16)P;                                                                                                                                  | 
            v178.m128i_i32[0] = (int)P;                                                                                                                                 | 
            v178.m128i_i64[1] = HIDWORD(P);                                                                                                                             | 
          }                                                                                                                                                             | 
          else                                                                                                                                                          | 
          {                                                                                                                                                             | 
            v174 = *(PVOID *)(v64[92].Count + 32);                                                                                                                      | 
            v67 = (__int64)v174 + 164;                                                                                                                                  | 
            if ( (unsigned __int64)v174 + 164 >= 0x7FFFFFFF0000LL )                                                                                                     | 
            {                                                                                                                                                           | 
              v67 = 0x7FFFFFFF0000LL;                                                                                                                                   | 
            }                                                                                                                                                           | 
            v177 = *(_DWORD *)v67;                                                                                                                                      | 
            v184 = 0LL;                                                                                                                                                 | 
            v68 = (__int64)v174 + 176;                                                                                                                                  | 
            if ( (unsigned __int64)v174 + 176 >= 0x7FFFFFFF0000LL )                                                                                                     | 
            {                                                                                                                                                           | 
              v68 = 0x7FFFFFFF0000LL;                                                                                                                                   | 
            }                                                                                                                                                           | 
            v184.m128i_i32[0] = *(_DWORD *)v68;                                                                                                                         | 
            v184.m128i_i64[1] = *(_QWORD *)(v68 + 8);                                                                                                                   | 
            v178 = v184;                                                                                                                                                | 
            v69 = _mm_cvtsi128_si32(v184);                                                                                                                              | 
          }                                                                                                                                                             | 
          SectionInformation = WorkingSetInformation;                                                                                                                   | 
          KiUnstackDetachProcess(v227, 0LL);                                                                                                                            | 
          if ( SectionInformation < 0 )                                                                                                                                 | 
          {                                                                                                                                                             | 
            goto LABEL_297;                                                                                                                                             | 
          }                                                                                                                                                             | 
          v70 = v69 + 6;                                                                                                                                                | 
          if ( a5 )                                                                                                                                                     | 
          {                                                                                                                                                             | 
            *a5 = v70;                                                                                                                                                  | 
          }                                                                                                                                                             | 
          if ( (unsigned int)v5 < v70 )                                                                                                                                 | 
          {                                                                                                                                                             | 
            SectionInformation = -1073741820;                                                                                                                           | 
            WorkingSetInformation = -1073741820;                                                                                                                        | 
          }                                                                                                                                                             | 
          else                                                                                                                                                          | 
          {                                                                                                                                                             | 
            *(_DWORD *)v6 = v177;                                                                                                                                       | 
            *(_WORD *)(v6 + 4) = v69;                                                                                                                                   | 
          }                                                                                                                                                             | 
          if ( SectionInformation >= 0 && v69 )                                                                                                                         | 
          {                                                                                                                                                             | 
            v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                     | 
            SectionInformation = MiCopyVirtualMemory(                                                                                                                   | 
                                   (_DWORD)Object,                                                                                                                      | 
                                   v178.m128i_i32[2],                                                                                                                   | 
                                   KeGetCurrentThread()->ApcState.Process,                                                                                              | 
                                   (int)v6 + 6,                                                                                                                         | 
                                   v178.m128i_u16[0],                                                                                                                   | 
                                   1,                                                                                                                                   | 
                                   (__int64)v210,                                                                                                                       | 
                                   0);                                                                                                                                  | 
          }                                                                                                                                                             | 
          else                                                                                                                                                          | 
          {                                                                                                                                                             | 
LABEL_297:                                                                                                                                                              | 
            v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                     | 
          }                                                                                                                                                             | 
LABEL_298:                                                                                                                                                              | 
          ExReleaseRundownProtection_0(v64 + 61);                                                                                                                       | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
LABEL_536:                                                                                                                                                              | 
          SectionInformation = -1073741558;                                                                                                                             | 
        }                                                                                                                                                               | 
LABEL_299:                                                                                                                                                              | 
        ObfDereferenceObjectWithTag(v64, 0x79517350u);                                                                                                                  | 
        return SectionInformation;                                                                                                                                      | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 51:                                                                                                                                                            | 
      if ( (unsigned int)v5 >= 0x10 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        v174 = 0LL;                                                                                                                                                     | 
        result = ObReferenceObjectByHandle(                                                                                                                             | 
                   (HANDLE)BugCheckParameter1,                                                                                                                          | 
                   0x400u,                                                                                                                                              | 
                   (POBJECT_TYPE)PsProcessType,                                                                                                                         | 
                   PreviousMode,                                                                                                                                        | 
                   &v174,                                                                                                                                               | 
                   0LL);                                                                                                                                                | 
        if ( result >= 0 )                                                                                                                                              | 
        {                                                                                                                                                               | 
          v86 = v176;                                                                                                                                                   | 
          --v176->KernelApcDisable;                                                                                                                                     | 
          v87 = (struct _EX_RUNDOWN_REF *)v174;                                                                                                                         | 
          v88 = ObReferenceProcessHandleTable(v174);                                                                                                                    | 
          if ( v88 )                                                                                                                                                    | 
          {                                                                                                                                                             | 
            ProcessHandleInformation = ExQueryProcessHandleInformation(v88, v6, (unsigned int)v5, a5);                                                                  | 
            ExReleaseRundownProtection_0(v87 + 61);                                                                                                                     | 
          }                                                                                                                                                             | 
          else                                                                                                                                                          | 
          {                                                                                                                                                             | 
            ProcessHandleInformation = -1073741558;                                                                                                                     | 
          }                                                                                                                                                             | 
          KiLeaveCriticalRegionUnsafe(v86);                                                                                                                             | 
          ObfDereferenceObject(v87);                                                                                                                                    | 
          return ProcessHandleInformation;                                                                                                                              | 
        }                                                                                                                                                               | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 16;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      return -1073741820;                                                                                                                                               | 
    case 54:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      SectionInformation = result;                                                                                                                                      | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                           | 
      v151 = ObReferenceProcessHandleTable(Object);                                                                                                                     | 
      if ( !v151 )                                                                                                                                                      | 
      {                                                                                                                                                                 | 
        goto LABEL_536;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      LOBYTE(Ptr) = (*(_BYTE *)(v151 + 44) & 2) != 0;                                                                                                                   | 
      *(_DWORD *)v6 = (_DWORD)Ptr;                                                                                                                                      | 
      goto LABEL_298;                                                                                                                                                   | 
    case 55:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v94 = Object;                                                                                                                                                     | 
      *(_DWORD *)v6 = *((_DWORD *)Object + 459) & 0x7FFFFFFF;                                                                                                           | 
      *(_DWORD *)(v6 + 4) = v94[467] & 0x7FFFFFFF;                                                                                                                      | 
      goto LABEL_423;                                                                                                                                                   | 
    case 58:                                                                                                                                                            | 
      v174 = 0LL;                                                                                                                                                       | 
      P = 0LL;                                                                                                                                                          | 
      if ( (unsigned int)v5 < 4 )                                                                                                                                       | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ExLockUserBuffer(                                                                                                                                        | 
                 v6,                                                                                                                                                    | 
                 (unsigned int)v5,                                                                                                                                      | 
                 (unsigned __int8)KeGetCurrentThread()->PreviousMode,                                                                                                   | 
                 1LL,                                                                                                                                                   | 
                 &v174,                                                                                                                                                 | 
                 &P);                                                                                                                                                   | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        v91 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                           | 
        if ( v91 < 0 )                                                                                                                                                  | 
        {                                                                                                                                                               | 
          ExUnlockUserBuffer(P);                                                                                                                                        | 
          return v91;                                                                                                                                                   | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          v178 = 0LL;                                                                                                                                                   | 
          v92 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                       | 
          v93 = ObReferenceProcessHandleTable(Object);                                                                                                                  | 
          if ( v93 )                                                                                                                                                    | 
          {                                                                                                                                                             | 
            v178.m128i_i64[0] = (__int64)v174;                                                                                                                          | 
            v178.m128i_i64[1] = (unsigned int)v5 >> 2;                                                                                                                  | 
            ExEnumHandleTable(v93, PspHandleTableWalker, &v178, 0LL);                                                                                                   | 
            ExReleaseRundownProtection_0(v92 + 61);                                                                                                                     | 
          }                                                                                                                                                             | 
          else                                                                                                                                                          | 
          {                                                                                                                                                             | 
            LODWORD(Ptr) = -1073741558;                                                                                                                                 | 
          }                                                                                                                                                             | 
          ObfDereferenceObjectWithTag(v92, 0x79517350u);                                                                                                                | 
          if ( a5 )                                                                                                                                                     | 
          {                                                                                                                                                             | 
            *a5 = 4 * v178.m128i_i32[3];                                                                                                                                | 
          }                                                                                                                                                             | 
          ExUnlockUserBuffer(P);                                                                                                                                        | 
          return (int)Ptr;                                                                                                                                              | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 59:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v94 = Object;                                                                                                                                                     | 
      *(_DWORD *)v6 = (*((_DWORD *)Object + 34) >> 5) & 1;                                                                                                              | 
      goto LABEL_423;                                                                                                                                                   | 
    case 60:                                                                                                                                                            | 
      DeviceMapInformation = ObpReferenceObjectByHandleWithTag(                                                                                                         | 
                               BugCheckParameter1,                                                                                                                      | 
                               0x79517350u,                                                                                                                             | 
                               (__int64)&Object,                                                                                                                        | 
                               0LL,                                                                                                                                     | 
                               0LL);                                                                                                                                    | 
      if ( DeviceMapInformation < 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      v45 = Object;                                                                                                                                                     | 
      ProcessCommandLine = PsQueryProcessCommandLine((PRKPROCESS)Object, (__int64)a5);                                                                                  | 
      goto LABEL_361;                                                                                                                                                   | 
    case 61:                                                                                                                                                            | 
      DeviceMapInformation = ObpReferenceObjectByHandleWithTag(                                                                                                         | 
                               BugCheckParameter1,                                                                                                                      | 
                               0x79517350u,                                                                                                                             | 
                               (__int64)&Object,                                                                                                                        | 
                               0LL,                                                                                                                                     | 
                               0LL);                                                                                                                                    | 
      if ( DeviceMapInformation < 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      if ( !(_DWORD)v5 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
LABEL_516:                                                                                                                                                              | 
        DeviceMapInformation = -1073741820;                                                                                                                             | 
        goto LABEL_358;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      v49 = Object;                                                                                                                                                     | 
      *(_BYTE *)v6 = *((_BYTE *)Object + 1530);                                                                                                                         | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 1;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      DeviceMapInformation = 0;                                                                                                                                         | 
      goto LABEL_210;                                                                                                                                                   | 
    case 64:                                                                                                                                                            | 
      DeviceMapInformation = ObpReferenceObjectByHandleWithTag(                                                                                                         | 
                               BugCheckParameter1,                                                                                                                      | 
                               0x79517350u,                                                                                                                             | 
                               (__int64)&Object,                                                                                                                        | 
                               0LL,                                                                                                                                     | 
                               0LL);                                                                                                                                    | 
      if ( DeviceMapInformation < 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      v45 = Object;                                                                                                                                                     | 
      ProcessCommandLine = EtwQueryProcessTelemetryInfo((ULONG_PTR)Object, (void *)v6, (unsigned int)v5, (__int64)a5);                                                  | 
LABEL_361:                                                                                                                                                              | 
      DeviceMapInformation = ProcessCommandLine;                                                                                                                        | 
      if ( !v45 )                                                                                                                                                       | 
      {                                                                                                                                                                 | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      goto LABEL_192;                                                                                                                                                   | 
    case 65:                                                                                                                                                            | 
      LODWORD(P) = 0;                                                                                                                                                   | 
      if ( (_DWORD)v5 != 32 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        goto LABEL_516;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      v184 = *(__m128i *)v6;                                                                                                                                            | 
      v185 = *(_OWORD *)(v6 + 16);                                                                                                                                      | 
      if ( v184.m128i_i32[0] == 3 )                                                                                                                                     | 
      {                                                                                                                                                                 | 
        if ( v184.m128i_i32[1] >= 8u || v184.m128i_i64[1] )                                                                                                             | 
        {                                                                                                                                                               | 
          DeviceMapInformation = -1073741811;                                                                                                                           | 
          v49 = Object;                                                                                                                                                 | 
        }                                                                                                                                                               | 
        else                                                                                                                                                            | 
        {                                                                                                                                                               | 
          DeviceMapInformation = ObpReferenceObjectByHandleWithTag(                                                                                                     | 
                                   BugCheckParameter1,                                                                                                                  | 
                                   0x79517350u,                                                                                                                         | 
                                   (__int64)&Object,                                                                                                                    | 
                                   0LL,                                                                                                                                 | 
                                   0LL);                                                                                                                                | 
          if ( DeviceMapInformation >= 0 )                                                                                                                              | 
          {                                                                                                                                                             | 
            MmQueryCommitReleaseState(                                                                                                                                  | 
              (_DWORD)Object,                                                                                                                                           | 
              (unsigned int)&P,                                                                                                                                         | 
              (unsigned int)&v184.m128i_u32[2],                                                                                                                         | 
              (unsigned int)&v185,                                                                                                                                      | 
              (__int64)&v185 + 8);                                                                                                                                      | 
            v184.m128i_i32[1] = (unsigned __int8)P & 1 | v184.m128i_i32[1] & 0xFFFFFFFE;                                                                                | 
            v184.m128i_i64[1] <<= 12;                                                                                                                                   | 
            *(_QWORD *)&v185 = (_QWORD)v185 << 12;                                                                                                                      | 
            *((_QWORD *)&v185 + 1) <<= 12;                                                                                                                              | 
            *(__m128i *)v6 = v184;                                                                                                                                      | 
            *(_OWORD *)(v6 + 16) = v185;                                                                                                                                | 
            DeviceMapInformation = 0;                                                                                                                                   | 
          }                                                                                                                                                             | 
LABEL_358:                                                                                                                                                              | 
          v49 = Object;                                                                                                                                                 | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        DeviceMapInformation = -1073741735;                                                                                                                             | 
        v49 = Object;                                                                                                                                                   | 
      }                                                                                                                                                                 | 
LABEL_210:                                                                                                                                                              | 
      if ( v49 )                                                                                                                                                        | 
      {                                                                                                                                                                 | 
        goto LABEL_193;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      return DeviceMapInformation;                                                                                                                                      | 
    case 66:                                                                                                                                                            | 
    case 67:                                                                                                                                                            | 
      if ( (v5 & 7) != 0 || (unsigned int)v5 > 0x100 )                                                                                                                  | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      SectionInformation = ObpReferenceObjectByHandleWithTag(                                                                                                           | 
                             BugCheckParameter1,                                                                                                                        | 
                             0x79517350u,                                                                                                                               | 
                             (__int64)&Object,                                                                                                                          | 
                             0LL,                                                                                                                                       | 
                             0LL);                                                                                                                                      | 
      WorkingSetInformation = SectionInformation;                                                                                                                       | 
      if ( SectionInformation >= 0 )                                                                                                                                    | 
      {                                                                                                                                                                 | 
        LOBYTE(Ptr) = a2 == 67;                                                                                                                                         | 
        LODWORD(Ptr) = 8 * KeQueryCpuSetsProcess(Object, v239, 32LL, (unsigned int)Ptr);                                                                                | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        v205 = (int)Ptr;                                                                                                                                                | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = (unsigned int)Ptr;                                                                                                                                      | 
        }                                                                                                                                                               | 
        if ( (unsigned int)Ptr >= (unsigned int)v5 )                                                                                                                    | 
        {                                                                                                                                                               | 
          LODWORD(Ptr) = v5;                                                                                                                                            | 
        }                                                                                                                                                               | 
        v205 = (int)Ptr;                                                                                                                                                | 
        memmove((void *)v6, v239, (unsigned int)Ptr);                                                                                                                   | 
      }                                                                                                                                                                 | 
      return SectionInformation;                                                                                                                                        | 
    case 69:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 40 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      if ( *((_QWORD *)Object + 84) )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        KiStackAttachProcess((ULONG_PTR)Object);                                                                                                                        | 
        ZwQueryInformationJobObject(0LL, 28LL, &v228, 40LL, 0LL);                                                                                                       | 
        ZwQueryInformationJobObject(0LL, 9LL, v233, 152LL, 0LL);                                                                                                        | 
        KiUnstackDetachProcess(v227, 0LL);                                                                                                                              | 
        *(_OWORD *)&Src.m256i_u64[1] = v228;                                                                                                                            | 
        Src.m256i_i64[0] = v229;                                                                                                                                        | 
        v99 = v188;                                                                                                                                                     | 
        if ( (v234 & 0x200000) != 0 )                                                                                                                                   | 
        {                                                                                                                                                               | 
          v99 = v236;                                                                                                                                                   | 
        }                                                                                                                                                               | 
        *(_QWORD *)&v188 = v99;                                                                                                                                         | 
        v100 = Src.m256i_i64[3];                                                                                                                                        | 
        if ( (v234 & 0x200) != 0 )                                                                                                                                      | 
        {                                                                                                                                                               | 
          v100 = v235;                                                                                                                                                  | 
        }                                                                                                                                                               | 
        Src.m256i_i64[3] = v100;                                                                                                                                        | 
        *(__m256i *)v6 = Src;                                                                                                                                           | 
        *(_QWORD *)(v6 + 32) = v188;                                                                                                                                    | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 40;                                                                                                                                                     | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        LODWORD(Ptr) = -1073741394;                                                                                                                                     | 
      }                                                                                                                                                                 | 
      goto LABEL_334;                                                                                                                                                   | 
    case 70:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 1 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v94 = Object;                                                                                                                                                     | 
      *(_BYTE *)v6 = *((int *)Object + 124) < 0;                                                                                                                        | 
      goto LABEL_423;                                                                                                                                                   | 
    case 71:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      SectionInformation = result;                                                                                                                                      | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v64 = (struct _EX_RUNDOWN_REF *)Object;                                                                                                                           | 
      v152 = ObReferenceProcessHandleTable(Object);                                                                                                                     | 
      if ( !v152 )                                                                                                                                                      | 
      {                                                                                                                                                                 | 
        goto LABEL_536;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      LOBYTE(Ptr) = (*(_BYTE *)(v152 + 44) & 0x10) != 0;                                                                                                                | 
      *(_DWORD *)v6 = (_DWORD)Ptr;                                                                                                                                      | 
      goto LABEL_298;                                                                                                                                                   | 
    case 72:                                                                                                                                                            | 
      return PsIumGetOnDemandDebugChallenge(BugCheckParameter1, v6, (unsigned int)v5, a5);                                                                              | 
    case 73:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 3 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 == -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        v95 = KeGetCurrentThread()->ApcState.Process;                                                                                                                   | 
        Object = v95;                                                                                                                                                   | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                        | 
        if ( result < 0 )                                                                                                                                               | 
        {                                                                                                                                                               | 
          return result;                                                                                                                                                | 
        }                                                                                                                                                               | 
        v95 = Object;                                                                                                                                                   | 
      }                                                                                                                                                                 | 
      v96 = PspGetNoChildProcessRestrictedPolicy(v95, v10, v14, v13);                                                                                                   | 
      *(_WORD *)v6 = 0;                                                                                                                                                 | 
      *(_BYTE *)(v6 + 2) = 0;                                                                                                                                           | 
      if ( v96 == 2 )                                                                                                                                                   | 
      {                                                                                                                                                                 | 
        *(_BYTE *)(v6 + 1) = 1;                                                                                                                                         | 
        goto LABEL_434;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      v97 = v96 - 1;                                                                                                                                                    | 
      if ( !v97 )                                                                                                                                                       | 
      {                                                                                                                                                                 | 
LABEL_434:                                                                                                                                                              | 
        *(_BYTE *)v6 = 1;                                                                                                                                               | 
        goto LABEL_431;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      if ( v97 == 2 )                                                                                                                                                   | 
      {                                                                                                                                                                 | 
        *(_BYTE *)(v6 + 2) = 1;                                                                                                                                         | 
      }                                                                                                                                                                 | 
LABEL_431:                                                                                                                                                              | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 3;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 != -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
LABEL_436:                                                                                                                                                              | 
        ObfDereferenceObjectWithTag(v95, 0x79517350u);                                                                                                                  | 
        return (int)Ptr;                                                                                                                                                | 
      }                                                                                                                                                                 | 
      return (int)Ptr;                                                                                                                                                  | 
    case 74:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 1 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v94 = Object;                                                                                                                                                     | 
      *(_BYTE *)v6 = (*((_DWORD *)Object + 383) & 0x200) != 0;                                                                                                          | 
      goto LABEL_423;                                                                                                                                                   | 
    case 75:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      DeviceMapInformation = result;                                                                                                                                    | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v49 = Object;                                                                                                                                                     | 
      if ( *((_QWORD *)Object + 200) )                                                                                                                                  | 
      {                                                                                                                                                                 | 
        LODWORD(Ptr) = qword_140F05890;                                                                                                                                 | 
      }                                                                                                                                                                 | 
      *(_DWORD *)v6 = (_DWORD)Ptr;                                                                                                                                      | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      goto LABEL_193;                                                                                                                                                   | 
    case 76:                                                                                                                                                            | 
      memset_0(v238, 0, 0x1B8uLL);                                                                                                                                      | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v95 = Object;                                                                                                                                                     | 
      PsQueryProcessEnergyValues(Object, v238);                                                                                                                         | 
      v148 = 432LL;                                                                                                                                                     | 
      if ( (unsigned int)v5 <= 0x1B0 )                                                                                                                                  | 
      {                                                                                                                                                                 | 
        v148 = (unsigned int)v5;                                                                                                                                        | 
      }                                                                                                                                                                 | 
      memmove((void *)v6, v238, v148);                                                                                                                                  | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 432;                                                                                                                                                      | 
      }                                                                                                                                                                 | 
      goto LABEL_436;                                                                                                                                                   | 
    case 77:                                                                                                                                                            | 
      v224 = 0LL;                                                                                                                                                       | 
      v225 = 0;                                                                                                                                                         | 
      if ( (_DWORD)v5 != 12 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( !ExGetExtensionTable(PspBamExtensionHost, v10, 0x7FFFFFFF0000LL, 1LL) )                                                                                      | 
      {                                                                                                                                                                 | 
        return -1073741822;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      v53 = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                             | 
      if ( v53 < 0 )                                                                                                                                                    | 
      {                                                                                                                                                                 | 
        ExReleaseExtensionTable(PspBamExtensionHost);                                                                                                                   | 
        return v53;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v54 = Object;                                                                                                                                                   | 
        v55 = guard_dispatch_icall_no_overrides(Object, &v224);                                                                                                         | 
        WorkingSetInformation = v55;                                                                                                                                    | 
        if ( v55 >= 0 )                                                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_QWORD *)v6 = v224;                                                                                                                                         | 
          *(_DWORD *)(v6 + 8) = v225;                                                                                                                                   | 
          if ( a5 )                                                                                                                                                     | 
          {                                                                                                                                                             | 
            *a5 = 12;                                                                                                                                                   | 
          }                                                                                                                                                             | 
          v55 = 0;                                                                                                                                                      | 
          WorkingSetInformation = 0;                                                                                                                                    | 
        }                                                                                                                                                               | 
        ObfDereferenceObjectWithTag(v54, 0x79517350u);                                                                                                                  | 
        ExReleaseExtensionTable(PspBamExtensionHost);                                                                                                                   | 
        return v55;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
    case 79:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 == -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        v94 = &KeGetCurrentThread()->ApcState.Process->Header.LockNV;                                                                                                   | 
        Object = v94;                                                                                                                                                   | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                        | 
        if ( result < 0 )                                                                                                                                               | 
        {                                                                                                                                                               | 
          return result;                                                                                                                                                | 
        }                                                                                                                                                               | 
        v94 = Object;                                                                                                                                                   | 
        LODWORD(v13) = 1;                                                                                                                                               | 
      }                                                                                                                                                                 | 
      *(_DWORD *)v6 = 0;                                                                                                                                                | 
      v110 = 0;                                                                                                                                                         | 
      if ( (v94[468] & 0x4000) != 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        *(_DWORD *)v6 = v13;                                                                                                                                            | 
        v110 = v13;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      if ( (v94[468] & 0x8000) != 0 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        *(_DWORD *)v6 = v110 | 2;                                                                                                                                       | 
      }                                                                                                                                                                 | 
      *(_DWORD *)(v6 + 4) = v94[442];                                                                                                                                   | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 8;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 != -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
LABEL_423:                                                                                                                                                              | 
        ObfDereferenceObjectWithTag(v94, 0x79517350u);                                                                                                                  | 
        return (int)Ptr;                                                                                                                                                | 
      }                                                                                                                                                                 | 
      return (int)Ptr;                                                                                                                                                  | 
    case 81:                                                                                                                                                            | 
      if ( PreviousMode )                                                                                                                                               | 
      {                                                                                                                                                                 | 
        return -1073741790;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( (_DWORD)v5 != 48 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v45 = Object;                                                                                                                                                     | 
      DeviceMapInformation = PspAllocateAndQueryProcessNotificationChannel(v176, Object, v6);                                                                           | 
      if ( DeviceMapInformation >= 0 && a5 )                                                                                                                            | 
      {                                                                                                                                                                 | 
        *a5 = 48;                                                                                                                                                       | 
        ObfDereferenceObjectWithTag(v45, 0x79517350u);                                                                                                                  | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
LABEL_192:                                                                                                                                                              | 
        v49 = v45;                                                                                                                                                      | 
LABEL_193:                                                                                                                                                              | 
        ObfDereferenceObjectWithTag(v49, 0x79517350u);                                                                                                                  | 
        return DeviceMapInformation;                                                                                                                                    | 
      }                                                                                                                                                                 | 
    case 82:                                                                                                                                                            | 
      memset_0(v237, 0, sizeof(v237));                                                                                                                                  | 
      if ( (unsigned int)v5 < 8 )                                                                                                                                       | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v95 = Object;                                                                                                                                                     | 
      PoQueryProcessEnergyTrackingState(Object, v237);                                                                                                                  | 
      if ( (unsigned int)v5 >= 0x90 )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        LODWORD(v5) = 144;                                                                                                                                              | 
      }                                                                                                                                                                 | 
      memmove((void *)v6, v237, (unsigned int)v5);                                                                                                                      | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = v5;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      WorkingSetInformation = 0;                                                                                                                                        | 
      goto LABEL_436;                                                                                                                                                   | 
    case 84:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v90 = Object;                                                                                                                                                     | 
      if ( *((_QWORD *)Object + 46) )                                                                                                                                   | 
      {                                                                                                                                                                 | 
        v153 = VslLiveDumpCaptureProcess(Object);                                                                                                                       | 
        ObfDereferenceObjectWithTag(v90, 0x79517350u);                                                                                                                  | 
        return v153;                                                                                                                                                    | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        return -1073741790;                                                                                                                                             | 
      }                                                                                                                                                                 | 
    case 85:                                                                                                                                                            | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      ProcessTelemetryCoverage = EtwQueryProcessTelemetryCoverage(Object, v6, (unsigned int)v5, a5);                                                                    | 
LABEL_333:                                                                                                                                                              | 
      LODWORD(Ptr) = ProcessTelemetryCoverage;                                                                                                                          | 
      goto LABEL_334;                                                                                                                                                   | 
    case 87:                                                                                                                                                            | 
    case 96:                                                                                                                                                            | 
      if ( a2 == 87 && !(_DWORD)v5 || a2 == 96 && (unsigned int)v5 < 4 )                                                                                                | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      v80 = result;                                                                                                                                                     | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        v81 = Object;                                                                                                                                                   | 
        *(_BYTE *)v6 = HIBYTE(*((_DWORD *)Object + 124)) & 3;                                                                                                           | 
        if ( a2 == 96 )                                                                                                                                                 | 
        {                                                                                                                                                               | 
          *(_DWORD *)v6 |= (v81[383] & 0x80000 | v81[383] & 0x100000 | ((v81[383] & 0x10000000 | (v81[383] >> 2) & 0x8000000u) >> 6)) >> 17;                            | 
        }                                                                                                                                                               | 
        ObfDereferenceObjectWithTag(v81, 0x79517350u);                                                                                                                  | 
        return v80;                                                                                                                                                     | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 88:                                                                                                                                                            | 
      v211 = 0LL;                                                                                                                                                       | 
      v212 = 0LL;                                                                                                                                                       | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v123 = Object;                                                                                                                                                    | 
      PspLockProcessShared(Object, v176);                                                                                                                               | 
      v124 = v123[212] >> 61;                                                                                                                                           | 
      if ( v124 == 4 || v124 == 3 )                                                                                                                                     | 
      {                                                                                                                                                                 | 
        v125 = v123[212] & 0x1FFFFFFFFFFFFFFFLL;                                                                                                                        | 
        *(_QWORD *)&v188 = v125;                                                                                                                                        | 
        Src.m256i_i64[2] = v123[211];                                                                                                                                   | 
        v126 = Src.m256i_i64[2] - v123[208];                                                                                                                            | 
        Src.m256i_i64[3] = v126;                                                                                                                                        | 
        if ( v124 == 3 )                                                                                                                                                | 
        {                                                                                                                                                               | 
          v127 = v189 | 0x200;                                                                                                                                          | 
LABEL_550:                                                                                                                                                              | 
          LODWORD(v189) = v127 & 0xFFFFFE00 | *((_BYTE *)v123 + 1531) & 7 | (unsigned __int8)(2                                                                         | 
                                                                                            * (*((_BYTE *)v123 + 1531) & 0x38)) | ((*((_DWORD *)v123 + 383) & 4) << 6); | 
          PspUnlockProcessShared(v123, v176);                                                                                                                           | 
          PsGetProcessDeepFreezeStats(v123, &v211);                                                                                                                     | 
          *(_OWORD *)Src.m256i_i8 = v211;                                                                                                                               | 
          *((_QWORD *)&v188 + 1) = v212;                                                                                                                                | 
          if ( !v126 )                                                                                                                                                  | 
          {                                                                                                                                                             | 
            Src.m256i_i64[3] = v211 - v123[208];                                                                                                                        | 
          }                                                                                                                                                             | 
          if ( !v125 )                                                                                                                                                  | 
          {                                                                                                                                                             | 
            *(_QWORD *)&v188 = *((_QWORD *)&v211 + 1) - v123[209] - v212;                                                                                               | 
          }                                                                                                                                                             | 
          if ( (unsigned int)v5 >= 0x38 )                                                                                                                               | 
          {                                                                                                                                                             | 
            LODWORD(v5) = 56;                                                                                                                                           | 
          }                                                                                                                                                             | 
          memmove((void *)v6, &Src, (unsigned int)v5);                                                                                                                  | 
          if ( a5 )                                                                                                                                                     | 
          {                                                                                                                                                             | 
            *a5 = v5;                                                                                                                                                   | 
          }                                                                                                                                                             | 
          WorkingSetInformation = 0;                                                                                                                                    | 
          ObfDereferenceObjectWithTag(v123, 0x79517350u);                                                                                                               | 
          return 0;                                                                                                                                                     | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v125 = v188;                                                                                                                                                    | 
        v126 = Src.m256i_i64[3];                                                                                                                                        | 
      }                                                                                                                                                                 | 
      v127 = v189;                                                                                                                                                      | 
      goto LABEL_550;                                                                                                                                                   | 
    case 89:                                                                                                                                                            | 
      v207 = 0LL;                                                                                                                                                       | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( PreviousMode )                                                                                                                                               | 
      {                                                                                                                                                                 | 
        if ( v6 < 0x7FFFFFFF0000LL )                                                                                                                                    | 
        {                                                                                                                                                               | 
          v14 = v6;                                                                                                                                                     | 
        }                                                                                                                                                               | 
        *(_QWORD *)v14 = *(_QWORD *)v14;                                                                                                                                | 
      }                                                                                                                                                                 | 
      v154 = (PVOID *)KeGetCurrentThread()->ApcState.Process;                                                                                                           | 
      Object = v154;                                                                                                                                                    | 
      if ( BugCheckParameter1 != -1LL || v154 != (PVOID *)CurrentThread->Process )                                                                                      | 
      {                                                                                                                                                                 | 
        return -1073741790;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObOpenObjectByPointer(v154[85], 0, 0LL, 5u, MmSectionObjectType, PreviousMode, &v207);                                                                   | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result >= 0 )                                                                                                                                                | 
      {                                                                                                                                                                 | 
        *(_QWORD *)v6 = v207;                                                                                                                                           | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 8;                                                                                                                                                      | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      return result;                                                                                                                                                    | 
    case 92:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      v94 = Object;                                                                                                                                                     | 
      *(_QWORD *)v6 = *((_QWORD *)Object + 207);                                                                                                                        | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 8;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      goto LABEL_423;                                                                                                                                                   | 
    case 94:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      WorkingSetInformation = result;                                                                                                                                   | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      if ( (*((_DWORD *)Object + 124) & 0x8000) == 0 )                                                                                                                  | 
      {                                                                                                                                                                 | 
        ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                               | 
        return -1073741700;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      _InterlockedOr(v159, 0);                                                                                                                                          | 
      *(_QWORD *)v6 = *((_QWORD *)Object + 236);                                                                                                                        | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 8;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      WorkingSetInformation = 0;                                                                                                                                        | 
LABEL_334:                                                                                                                                                              | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      return (int)Ptr;                                                                                                                                                  | 
    case 97:                                                                                                                                                            | 
      if ( (_DWORD)v5 != 8 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 8;                                                                                                                                                      | 
        }                                                                                                                                                               | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 != -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return -1073741811;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      v155 = KeGetCurrentThread()->ApcState.Process;                                                                                                                    | 
      Object = v155;                                                                                                                                                    | 
      WorkingSetInformation = 0;                                                                                                                                        | 
      Blink = v155[1].ProcessListEntry.Blink;                                                                                                                           | 
      if ( !Blink )                                                                                                                                                     | 
      {                                                                                                                                                                 | 
        goto LABEL_787;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      v157 = 0LL;                                                                                                                                                       | 
      ReadyTime = (__int64 *)v155[1].ReadyTime;                                                                                                                         | 
      if ( ReadyTime )                                                                                                                                                  | 
      {                                                                                                                                                                 | 
        v157 = *ReadyTime;                                                                                                                                              | 
      }                                                                                                                                                                 | 
      if ( v157 )                                                                                                                                                       | 
      {                                                                                                                                                                 | 
        if ( (*(_DWORD *)(v157 + 1140) & 1) == 0 )                                                                                                                      | 
        {                                                                                                                                                               | 
          goto LABEL_787;                                                                                                                                               | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else if ( ((__int64)Blink[124].Flink & 1) == 0 )                                                                                                                  | 
      {                                                                                                                                                                 | 
        goto LABEL_787;                                                                                                                                                 | 
      }                                                                                                                                                                 | 
      Src.m256i_i32[0] |= 1u;                                                                                                                                           | 
LABEL_787:                                                                                                                                                              | 
      *(_QWORD *)v6 = Src.m256i_i64[0];                                                                                                                                 | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 8;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return (int)Ptr;                                                                                                                                                  | 
    case 106:                                                                                                                                                           | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      Src.m256i_i32[0] = 16;                                                                                                                                            | 
      *(_DWORD *)v6 = 16;                                                                                                                                               | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 109:                                                                                                                                                           | 
      if ( (_DWORD)v5 != 4 )                                                                                                                                            | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                                                                                           | 
      {                                                                                                                                                                 | 
        SiloIdentifier = Src.m256i_i32[0];                                                                                                                              | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        EffectiveServerSilo = PsGetEffectiveServerSilo(*((_QWORD *)Object + 84));                                                                                       | 
        SiloIdentifier = PsGetSiloIdentifier(EffectiveServerSilo);                                                                                                      | 
      }                                                                                                                                                                 | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_DWORD *)v6 = SiloIdentifier;                                                                                                                                   | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return 0;                                                                                                                                                         | 
    case 114:                                                                                                                                                           | 
      if ( (_DWORD)v5 != 16 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      result = ObpReferenceObjectByHandleWithTag(BugCheckParameter1, 0x79517350u, (__int64)&Object, 0LL, 0LL);                                                          | 
      LODWORD(Ptr) = result;                                                                                                                                            | 
      if ( result < 0 )                                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return result;                                                                                                                                                  | 
      }                                                                                                                                                                 | 
      *(_OWORD *)Src.m256i_i8 = *(_OWORD *)*((_QWORD *)Object + 254);                                                                                                   | 
      ObfDereferenceObjectWithTag(Object, 0x79517350u);                                                                                                                 | 
      *(_OWORD *)v6 = *(_OWORD *)Src.m256i_i8;                                                                                                                          | 
      if ( a5 )                                                                                                                                                         | 
      {                                                                                                                                                                 | 
        *a5 = 16;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      return (int)Ptr;                                                                                                                                                  | 
    case 115:                                                                                                                                                           | 
      v226 = 0LL;                                                                                                                                                       | 
      if ( (_DWORD)v5 != 16 )                                                                                                                                           | 
      {                                                                                                                                                                 | 
        return -1073741820;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( BugCheckParameter1 != -1LL )                                                                                                                                 | 
      {                                                                                                                                                                 | 
        return -1073741811;                                                                                                                                             | 
      }                                                                                                                                                                 | 
      if ( PreviousMode )                                                                                                                                               | 
      {                                                                                                                                                                 | 
        RtlCopyVolatileMemory(&v226, (const void *)v6, 0x10uLL);                                                                                                        | 
        if ( (v6 & 3) != 0 )                                                                                                                                            | 
        {                                                                                                                                                               | 
          ExRaiseDatatypeMisalignment();                                                                                                                                | 
        }                                                                                                                                                               | 
        v111 = 0x7FFFFFFF0000LL;                                                                                                                                        | 
        if ( v6 < 0x7FFFFFFF0000LL )                                                                                                                                    | 
        {                                                                                                                                                               | 
          v111 = v6;                                                                                                                                                    | 
        }                                                                                                                                                               | 
        *(_DWORD *)v111 = *(_DWORD *)v111;                                                                                                                              | 
        v112 = &v226;                                                                                                                                                   | 
      }                                                                                                                                                                 | 
      else                                                                                                                                                              | 
      {                                                                                                                                                                 | 
        v112 = (__int128 *)v6;                                                                                                                                          | 
      }                                                                                                                                                                 | 
      FirstThreadByTebValue = PspFindFirstThreadByTebValue((ULONG_PTR)KeGetCurrentThread()->ApcState.Process);                                                          | 
      v114 = FirstThreadByTebValue;                                                                                                                                     | 
      if ( FirstThreadByTebValue == -1073741275 )                                                                                                                       | 
      {                                                                                                                                                                 | 
        *(_DWORD *)v112 = 0;                                                                                                                                            | 
        v114 = 0;                                                                                                                                                       | 
      }                                                                                                                                                                 | 
      else if ( FirstThreadByTebValue < 0 )                                                                                                                             | 
      {                                                                                                                                                                 | 
        return v114;                                                                                                                                                    | 
      }                                                                                                                                                                 | 
      if ( PreviousMode )                                                                                                                                               | 
      {                                                                                                                                                                 | 
        *(_DWORD *)v6 = *(_DWORD *)v112;                                                                                                                                | 
        if ( a5 )                                                                                                                                                       | 
        {                                                                                                                                                               | 
          *a5 = 4;                                                                                                                                                      | 
        }                                                                                                                                                               | 
      }                                                                                                                                                                 | 
      else if ( a5 )                                                                                                                                                    | 
      {                                                                                                                                                                 | 
        *a5 = 4;                                                                                                                                                        | 
      }                                                                                                                                                                 | 
      return v114;                                                                                                                                                      | 
    default:                                                                                                                                                            | 
      return -1073741821;                                                                                                                                               | 
  }                                                                                                                                                                     | 
}                                                                                                                                                                       | 
