__int64 __fastcall NtSetInformationVirtualMemory(                                                                         | 
        ULONG_PTR a1,                                                                                                     | 
        unsigned int a2,                                                                                                  | 
        unsigned __int64 a3,                                                                                              | 
        void *a4,                                                                                                         | 
        __int64 a5,                                                                                                       | 
        int a6)                                                                                                           | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  v6 = (char)a4;                                                                                                          | 
  Src = a4;                                                                                                               | 
  v52 = a3;                                                                                                               | 
  v58 = a1;                                                                                                               | 
  Object = 0LL;                                                                                                           | 
  memset(v60, 0, sizeof(v60));                                                                                            | 
  v51 = 0;                                                                                                                | 
  v56 = 0LL;                                                                                                              | 
  v8 = 0;                                                                                                                 | 
  v47 = 0;                                                                                                                | 
  v9 = 0;                                                                                                                 | 
  P = v61;                                                                                                                | 
  valid = 0;                                                                                                              | 
  v45 = 0;                                                                                                                | 
  if ( a2 >= 2 )                                                                                                          | 
  {                                                                                                                       | 
    if ( a2 == 2 )                                                                                                        | 
    {                                                                                                                     | 
      v11 = a6;                                                                                                           | 
      if ( a6 == 40 )                                                                                                     | 
      {                                                                                                                   | 
        goto LABEL_10;                                                                                                    | 
      }                                                                                                                   | 
      return 3221225716LL;                                                                                                | 
    }                                                                                                                     | 
    if ( a2 != 3 && a2 != 4 && a2 != 5 && a2 - 6 > 1 )                                                                    | 
    {                                                                                                                     | 
      return 3221225712LL;                                                                                                | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  if ( !a5 )                                                                                                              | 
  {                                                                                                                       | 
    return 3221225715LL;                                                                                                  | 
  }                                                                                                                       | 
  v11 = a6;                                                                                                               | 
  if ( a6 != 4 )                                                                                                          | 
  {                                                                                                                       | 
    return 3221225716LL;                                                                                                  | 
  }                                                                                                                       | 
  if ( a2 == 4 )                                                                                                          | 
  {                                                                                                                       | 
    if ( !dword_140E374EC )                                                                                               | 
    {                                                                                                                     | 
      return 3221225659LL;                                                                                                | 
    }                                                                                                                     | 
    if ( a3 != 1 )                                                                                                        | 
    {                                                                                                                     | 
      return 3221225713LL;                                                                                                | 
    }                                                                                                                     | 
  }                                                                                                                       | 
LABEL_10:                                                                                                                 | 
  if ( a3 - 1 > 0xFFFFFFFFFFFFFFELL )                                                                                     | 
  {                                                                                                                       | 
    return 3221225713LL;                                                                                                  | 
  }                                                                                                                       | 
  CurrentThread = KeGetCurrentThread();                                                                                   | 
  v57 = CurrentThread;                                                                                                    | 
  p_LockNV = &CurrentThread->ApcState.Process->Header.LockNV;                                                             | 
  v50 = p_LockNV;                                                                                                         | 
  if ( a2 == 2 && (p_LockNV[468] & 0x100) != 0 )                                                                          | 
  {                                                                                                                       | 
    if ( (*(_DWORD *)(&CurrentThread[1].SwapListEntry + 1) & 0x40000) == 0 )                                              | 
    {                                                                                                                     | 
      v8 = 1;                                                                                                             | 
    }                                                                                                                     | 
    v47 = v8;                                                                                                             | 
  }                                                                                                                       | 
  *(_OWORD *)Address = 0LL;                                                                                               | 
  *(_OWORD *)v54 = 0LL;                                                                                                   | 
  v55 = 0LL;                                                                                                              | 
  PreviousMode = CurrentThread->PreviousMode;                                                                             | 
  if ( PreviousMode )                                                                                                     | 
  {                                                                                                                       | 
    if ( 16 * a3 && (v6 & 3) != 0 )                                                                                       | 
    {                                                                                                                     | 
      ExRaiseDatatypeMisalignment();                                                                                      | 
    }                                                                                                                     | 
    if ( a2 == 2 )                                                                                                        | 
    {                                                                                                                     | 
      if ( v11 && (a5 & 3) != 0 )                                                                                         | 
      {                                                                                                                   | 
        ExRaiseDatatypeMisalignment();                                                                                    | 
      }                                                                                                                   | 
      *(_OWORD *)Address = *(_OWORD *)a5;                                                                                 | 
      *(_OWORD *)v54 = *(_OWORD *)(a5 + 16);                                                                              | 
      v55 = *(_QWORD *)(a5 + 32);                                                                                         | 
      v14 = _mm_cvtsi128_si32(*(__m128i *)Address);                                                                       | 
      v48 = v14;                                                                                                          | 
      if ( !v14 )                                                                                                         | 
      {                                                                                                                   | 
        return 3221225715LL;                                                                                              | 
      }                                                                                                                   | 
      v15 = v14;                                                                                                          | 
      if ( HIDWORD(Address[0]) )                                                                                          | 
      {                                                                                                                   | 
        return 3221225715LL;                                                                                              | 
      }                                                                                                                   | 
      ProbeForWrite(Address[1], 4uLL, 4u);                                                                                | 
      v16 = 16 * v15;                                                                                                     | 
      if ( v16 )                                                                                                          | 
      {                                                                                                                   | 
        if ( (v54[0] & 3) != 0 )                                                                                          | 
        {                                                                                                                 | 
          ExRaiseDatatypeMisalignment();                                                                                  | 
        }                                                                                                                 | 
        if ( v54[0] + v16 > 0x7FFFFFFF0000LL || v54[0] + v16 < v54[0] )                                                   | 
        {                                                                                                                 | 
          v48 = (unsigned int)Address[0];                                                                                 | 
        }                                                                                                                 | 
      }                                                                                                                   | 
      p_LockNV = v50;                                                                                                     | 
    }                                                                                                                     | 
    else                                                                                                                  | 
    {                                                                                                                     | 
      if ( (a5 & 3) != 0 )                                                                                                | 
      {                                                                                                                   | 
        ExRaiseDatatypeMisalignment();                                                                                    | 
      }                                                                                                                   | 
      v45 = *(_DWORD *)a5;                                                                                                | 
      v48 = (unsigned int)Address[0];                                                                                     | 
    }                                                                                                                     | 
    v18 = v48;                                                                                                            | 
    goto LABEL_35;                                                                                                        | 
  }                                                                                                                       | 
  if ( a2 != 2 )                                                                                                          | 
  {                                                                                                                       | 
    v45 = *(_DWORD *)a5;                                                                                                  | 
    v18 = (unsigned int)Address[0];                                                                                       | 
    goto LABEL_35;                                                                                                        | 
  }                                                                                                                       | 
  *(_OWORD *)Address = *(_OWORD *)a5;                                                                                     | 
  *(_OWORD *)v54 = *(_OWORD *)(a5 + 16);                                                                                  | 
  v55 = *(_QWORD *)(a5 + 32);                                                                                             | 
  v18 = _mm_cvtsi128_si32(*(__m128i *)Address);                                                                           | 
  if ( !v18 || HIDWORD(Address[0]) )                                                                                      | 
  {                                                                                                                       | 
    return 3221225715LL;                                                                                                  | 
  }                                                                                                                       | 
LABEL_35:                                                                                                                 | 
  if ( a1 == -1LL )                                                                                                       | 
  {                                                                                                                       | 
    Object = p_LockNV;                                                                                                    | 
  }                                                                                                                       | 
  else                                                                                                                    | 
  {                                                                                                                       | 
    result = ObpReferenceObjectByHandleWithTag(a1, 0x66506D4Du, (__int64)&Object, 0LL, 0LL);                              | 
    valid = result;                                                                                                       | 
    if ( (int)result < 0 )                                                                                                | 
    {                                                                                                                     | 
      return result;                                                                                                      | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  Pool = v62;                                                                                                             | 
  v50 = v62;                                                                                                              | 
  if ( v52 > 0x10 )                                                                                                       | 
  {                                                                                                                       | 
    Pool = (_QWORD *)MiAllocatePool(0x40uLL, 16 * v52);                                                                   | 
    v50 = Pool;                                                                                                           | 
    if ( !Pool )                                                                                                          | 
    {                                                                                                                     | 
      Pool = v62;                                                                                                         | 
LABEL_123:                                                                                                                | 
      v30 = v61;                                                                                                          | 
      valid = -1073741670;                                                                                                | 
      goto LABEL_58;                                                                                                      | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  if ( a2 == 2 )                                                                                                          | 
  {                                                                                                                       | 
    if ( v18 > 0x10 )                                                                                                     | 
    {                                                                                                                     | 
      P = (_BYTE *)MiAllocatePool(0x40uLL, 16LL * v18);                                                                   | 
      if ( !P )                                                                                                           | 
      {                                                                                                                   | 
        goto LABEL_123;                                                                                                   | 
      }                                                                                                                   | 
    }                                                                                                                     | 
    if ( v54[1] )                                                                                                         | 
    {                                                                                                                     | 
      valid = ObpReferenceObjectByHandleWithTag(v54[1], 0x66506D4Du, (__int64)&v56, 0LL, 0LL);                            | 
      if ( valid < 0 )                                                                                                    | 
      {                                                                                                                   | 
        goto LABEL_57;                                                                                                    | 
      }                                                                                                                   | 
    }                                                                                                                     | 
  }                                                                                                                       | 
  memmove(Pool, Src, 16 * v52);                                                                                           | 
  if ( a2 == 2 )                                                                                                          | 
  {                                                                                                                       | 
    memmove(P, (const void *)v54[0], 16LL * v18);                                                                         | 
  }                                                                                                                       | 
  if ( v57->ApcState.Process != Object )                                                                                  | 
  {                                                                                                                       | 
    if ( a2 - 3 <= 1 )                                                                                                    | 
    {                                                                                                                     | 
      valid = -1073741585;                                                                                                | 
LABEL_57:                                                                                                                 | 
      v30 = P;                                                                                                            | 
      goto LABEL_58;                                                                                                      | 
    }                                                                                                                     | 
    KiStackAttachProcess((ULONG_PTR)Object);                                                                              | 
    v9 = 1;                                                                                                               | 
  }                                                                                                                       | 
  v20 = 0LL;                                                                                                              | 
  for ( i = Pool; i < &Pool[2 * v52]; i += 2 )                                                                            | 
  {                                                                                                                       | 
    v22 = i[1];                                                                                                           | 
    if ( !v22                                                                                                             | 
      || (unsigned __int64)(v22 + *i - 1LL) < *i                                                                          | 
      || (unsigned __int64)(v22 + *i - 1LL) > 0x7FFFFFFEFFFFLL                                                            | 
      || v20 + ((v22 + (unsigned __int64)(*(_DWORD *)i & 0xFFF) + 4095) >> 12) < v20 )                                    | 
    {                                                                                                                     | 
      valid = -1073741582;                                                                                                | 
      goto LABEL_57;                                                                                                      | 
    }                                                                                                                     | 
    v20 += (v22 + (unsigned __int64)(*(_DWORD *)i & 0xFFF) + 4095) >> 12;                                                 | 
  }                                                                                                                       | 
  if ( !a2 )                                                                                                              | 
  {                                                                                                                       | 
    if ( (v45 & 0xFFFFFFFE) == 0 )                                                                                        | 
    {                                                                                                                     | 
      v23 = MiGetEffectivePagePriorityThread(v57) & 7;                                                                    | 
      v25 = v23 | (v23 < 5 ? 131136 : 0x20000) | (8 * v23);                                                               | 
      if ( v23 < 5 )                                                                                                      | 
      {                                                                                                                   | 
        v25 = v25 & 0xFFFFFFC7 | 0x28;                                                                                    | 
      }                                                                                                                   | 
      v26 = v45;                                                                                                          | 
      LOBYTE(v26) = -(v45 & 1);                                                                                           | 
      if ( (int)PsGetIoPriorityThread(v24, v26, v24, v25 | ((v45 & 1) != 0 ? 0x40000 : 0x4000)) <= 1 )                    | 
      {                                                                                                                   | 
        LODWORD(v27) = v27 | 0x400;                                                                                       | 
      }                                                                                                                   | 
      v29 = MiPrefetchVirtualMemory(v28, Pool, (char *)Object + 1024, v27);                                               | 
      goto LABEL_56;                                                                                                      | 
    }                                                                                                                     | 
    goto LABEL_83;                                                                                                        | 
  }                                                                                                                       | 
  v32 = a2 - 1;                                                                                                           | 
  if ( !v32 )                                                                                                             | 
  {                                                                                                                       | 
    if ( v45 <= 5 )                                                                                                       | 
    {                                                                                                                     | 
      v33 = v45;                                                                                                          | 
      v34 = 1LL;                                                                                                          | 
LABEL_82:                                                                                                                 | 
      v29 = MiProcessVaRangesInfoClass(v52, Pool, v34, v33);                                                              | 
      goto LABEL_56;                                                                                                      | 
    }                                                                                                                     | 
LABEL_83:                                                                                                                 | 
    valid = -1073741581;                                                                                                  | 
    goto LABEL_57;                                                                                                        | 
  }                                                                                                                       | 
  v35 = v32 - 1;                                                                                                          | 
  if ( v35 )                                                                                                              | 
  {                                                                                                                       | 
    v36 = v35 - 1;                                                                                                        | 
    if ( v36 )                                                                                                            | 
    {                                                                                                                     | 
      v37 = v36 - 1;                                                                                                      | 
      if ( v37 )                                                                                                          | 
      {                                                                                                                   | 
        v38 = v37 - 1;                                                                                                    | 
        if ( v38 )                                                                                                        | 
        {                                                                                                                 | 
          v39 = v38 - 1;                                                                                                  | 
          if ( v39 )                                                                                                      | 
          {                                                                                                               | 
            if ( v39 != 1 )                                                                                               | 
            {                                                                                                             | 
              goto LABEL_57;                                                                                              | 
            }                                                                                                             | 
            if ( (v45 & 0xFFFFFFFE) != 0 )                                                                                | 
            {                                                                                                             | 
              goto LABEL_83;                                                                                              | 
            }                                                                                                             | 
            v29 = MiProcessRemoveFromWorkingSet(v52, Pool, v45);                                                          | 
          }                                                                                                               | 
          else                                                                                                            | 
          {                                                                                                               | 
            if ( v45 )                                                                                                    | 
            {                                                                                                             | 
              valid = -1073741811;                                                                                        | 
              goto LABEL_57;                                                                                              | 
            }                                                                                                             | 
            v29 = VmPrefetchVirtualAddresses(Pool);                                                                       | 
          }                                                                                                               | 
        }                                                                                                                 | 
        else                                                                                                              | 
        {                                                                                                                 | 
          LargestPageIndex = MiGetLargestPageIndex();                                                                     | 
          v41 = LargestPageIndex;                                                                                         | 
          if ( LargestPageIndex < 3 )                                                                                     | 
          {                                                                                                               | 
            v42 = &MiPageSizes[LargestPageIndex];                                                                         | 
            do                                                                                                            | 
            {                                                                                                             | 
              if ( *v42 == v45 )                                                                                          | 
              {                                                                                                           | 
                break;                                                                                                    | 
              }                                                                                                           | 
              ++v41;                                                                                                      | 
              ++v42;                                                                                                      | 
            }                                                                                                             | 
            while ( v41 < 3 );                                                                                            | 
          }                                                                                                               | 
          if ( v41 - 1 > 1 )                                                                                              | 
          {                                                                                                               | 
            goto LABEL_99;                                                                                                | 
          }                                                                                                               | 
          if ( !SeSinglePrivilegeCheck(SeLockMemoryPrivilege, PreviousMode) )                                             | 
          {                                                                                                               | 
            valid = -1073741727;                                                                                          | 
            goto LABEL_57;                                                                                                | 
          }                                                                                                               | 
          v29 = MiProcessVaContiguityInformation(Pool, v52, v41);                                                         | 
        }                                                                                                                 | 
LABEL_56:                                                                                                                 | 
        valid = v29;                                                                                                      | 
        goto LABEL_57;                                                                                                    | 
      }                                                                                                                   | 
    }                                                                                                                     | 
    else                                                                                                                  | 
    {                                                                                                                     | 
      if ( v45 )                                                                                                          | 
      {                                                                                                                   | 
        goto LABEL_83;                                                                                                    | 
      }                                                                                                                   | 
      if ( (*((_DWORD *)Object + 125) & 0x10) != 0 )                                                                      | 
      {                                                                                                                   | 
        v33 = 0LL;                                                                                                        | 
        v34 = 3LL;                                                                                                        | 
        goto LABEL_82;                                                                                                    | 
      }                                                                                                                   | 
    }                                                                                                                     | 
LABEL_99:                                                                                                                 | 
    valid = -1073741637;                                                                                                  | 
    goto LABEL_57;                                                                                                        | 
  }                                                                                                                       | 
  v30 = P;                                                                                                                | 
  if ( v52 == 1 )                                                                                                         | 
  {                                                                                                                       | 
    valid = MiCfgMarkValidEntries((_DWORD)Object, *Pool, Pool[1], (_DWORD)P, v18, (__int64)&v51, v47, (__int64)v56, v55); | 
    if ( v9 )                                                                                                             | 
    {                                                                                                                     | 
      KiUnstackDetachProcess(v60, 0LL);                                                                                   | 
      LOBYTE(v9) = 0;                                                                                                     | 
    }                                                                                                                     | 
    *(_DWORD *)Address[1] = v51;                                                                                          | 
    v31 = a1;                                                                                                             | 
    goto LABEL_59;                                                                                                        | 
  }                                                                                                                       | 
  valid = -1073741582;                                                                                                    | 
LABEL_58:                                                                                                                 | 
  v31 = a1;                                                                                                               | 
LABEL_59:                                                                                                                 | 
  if ( (v9 & 1) != 0 )                                                                                                    | 
  {                                                                                                                       | 
    KiUnstackDetachProcess(v60, 0LL);                                                                                     | 
  }                                                                                                                       | 
  if ( v56 )                                                                                                              | 
  {                                                                                                                       | 
    ObfDereferenceObjectWithTag(v56, 0x66506D4Du);                                                                        | 
  }                                                                                                                       | 
  if ( v31 != -1LL )                                                                                                      | 
  {                                                                                                                       | 
    ObfDereferenceObjectWithTag(Object, 0x66506D4Du);                                                                     | 
  }                                                                                                                       | 
  if ( Pool != (_QWORD *)v62 )                                                                                            | 
  {                                                                                                                       | 
    ExFreePoolWithTag(Pool, 0);                                                                                           | 
  }                                                                                                                       | 
  if ( v30 != v61 )                                                                                                       | 
  {                                                                                                                       | 
    ExFreePoolWithTag(v30, 0);                                                                                            | 
  }                                                                                                                       | 
  return (unsigned int)valid;                                                                                             | 
}                                                                                                                         | 
