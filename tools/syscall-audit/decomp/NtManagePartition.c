__int64 __fastcall NtManagePartition(__int64 a1, __int64 a2, unsigned int a3, unsigned __int64 a4, unsigned int Size) | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  v6 = a3;                                                                                                            | 
  v36 = a2;                                                                                                           | 
  memset_0(Src, 0, 0xF0uLL);                                                                                          | 
  v33 = 0;                                                                                                            | 
  v32 = 0;                                                                                                            | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                  | 
  v35 = 0LL;                                                                                                          | 
  v10 = 0LL;                                                                                                          | 
  v34 = 0LL;                                                                                                          | 
  if ( (unsigned int)v6 >= 0xF )                                                                                      | 
  {                                                                                                                   | 
    v14 = -1073741821;                                                                                                | 
    goto LABEL_25;                                                                                                    | 
  }                                                                                                                   | 
  v11 = (unsigned int)v6;                                                                                             | 
  v12 = HIBYTE(PspPartitionInfoDetails[v6]);                                                                          | 
  if ( (v12 & 0x10) == 0 && Size != WORD2(PspPartitionInfoDetails[v6]) )                                              | 
  {                                                                                                                   | 
    v14 = -1073741820;                                                                                                | 
    goto LABEL_25;                                                                                                    | 
  }                                                                                                                   | 
  if ( (v12 & 1) != 0 )                                                                                               | 
  {                                                                                                                   | 
    if ( PreviousMode && Size && ((BYTE6(PspPartitionInfoDetails[v6]) - 1LL) & a4) != 0 )                             | 
    {                                                                                                                 | 
      ExRaiseDatatypeMisalignment();                                                                                  | 
    }                                                                                                                 | 
    memmove(Src, (const void *)a4, Size);                                                                             | 
  }                                                                                                                   | 
  v13 = PspPartitionInfoDetails[v6];                                                                                  | 
  if ( (_DWORD)v6 == 11 && Src[0] == 2 )                                                                              | 
  {                                                                                                                   | 
    v13 = 2;                                                                                                          | 
  }                                                                                                                   | 
  LOBYTE(v8) = PreviousMode;                                                                                          | 
  v14 = PsReferencePartitionByHandle(a1, v13, v8, 1884123984LL, &v34);                                                | 
  if ( v14 < 0 )                                                                                                      | 
  {                                                                                                                   | 
    v10 = v34;                                                                                                        | 
    goto LABEL_25;                                                                                                    | 
  }                                                                                                                   | 
  v32 = 1;                                                                                                            | 
  if ( (PspPartitionInfoDetails[v6] & 0x400000000000000LL) != 0 )                                                     | 
  {                                                                                                                   | 
    LOBYTE(v15) = PreviousMode;                                                                                       | 
    v14 = PsReferencePartitionByHandle(v36, v13, v15, 1884123984LL, &v35);                                            | 
    v10 = v34;                                                                                                        | 
    if ( v14 < 0 )                                                                                                    | 
    {                                                                                                                 | 
      goto LABEL_25;                                                                                                  | 
    }                                                                                                                 | 
    v33 = 1;                                                                                                          | 
    if ( v35 == v34 && ((_DWORD)v6 != 1 || (v38 & 0x400) == 0) )                                                      | 
    {                                                                                                                 | 
      v14 = -1073741811;                                                                                              | 
      goto LABEL_25;                                                                                                  | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v10 = v34;                                                                                                        | 
    if ( v36 )                                                                                                        | 
    {                                                                                                                 | 
      v14 = -1073741584;                                                                                              | 
      goto LABEL_25;                                                                                                  | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  v14 = MmCheckPartitionManagementSupported(v10, (unsigned int)v6);                                                   | 
  if ( v14 >= 0 )                                                                                                     | 
  {                                                                                                                   | 
    if ( !v35 || (v14 = MmCheckPartitionManagementSupported(v35, (unsigned int)v6), v14 >= 0) )                       | 
    {                                                                                                                 | 
      if ( (int)v6 > 7 )                                                                                              | 
      {                                                                                                               | 
        v26 = v6 - 8;                                                                                                 | 
        if ( !v26 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v17) = PreviousMode;                                                                                 | 
          LargePages = MmManagePartitionCreateLargePages(v10, Src, v17);                                              | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v27 = v26 - 1;                                                                                                | 
        if ( !v27 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v18) = PreviousMode;                                                                                 | 
          LargePages = MiQuerySpecialPurposeMemoryInformation(*v10, a4, Size, v18);                                   | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v28 = v27 - 1;                                                                                                | 
        if ( !v28 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v17) = PreviousMode;                                                                                 | 
          LargePages = MiOpenSpecialPurposeMemory(*v10, Src, v17);                                                    | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v29 = v28 - 1;                                                                                                | 
        if ( !v29 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v18) = PreviousMode;                                                                                 | 
          LargePages = MmManagePartitionCharges(v10, Src, a4, v18);                                                   | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v30 = v29 - 1;                                                                                                | 
        if ( v30 )                                                                                                    | 
        {                                                                                                             | 
          if ( v30 == 1 )                                                                                             | 
          {                                                                                                           | 
            LargePages = MmManagePartitionSetMemoryThresholds(v10, Src);                                              | 
          }                                                                                                           | 
          else                                                                                                        | 
          {                                                                                                           | 
            LOBYTE(v16) = PreviousMode;                                                                               | 
            LargePages = MmIssueMemoryListCommand(Src[0], v16, v10);                                                  | 
          }                                                                                                           | 
LABEL_20:                                                                                                             | 
          v14 = LargePages;                                                                                           | 
          if ( LargePages >= 0 && (PspPartitionInfoDetails[v11] & 0x200000000000000LL) != 0 )                         | 
          {                                                                                                           | 
            if ( PreviousMode )                                                                                       | 
            {                                                                                                         | 
              ProbeForWrite((volatile void *)a4, Size, BYTE6(PspPartitionInfoDetails[v11]));                          | 
            }                                                                                                         | 
            memmove((void *)a4, Src, Size);                                                                           | 
          }                                                                                                           | 
          goto LABEL_25;                                                                                              | 
        }                                                                                                             | 
        v31 = 0LL;                                                                                                    | 
        LOBYTE(v17) = PreviousMode;                                                                                   | 
        v24 = v10;                                                                                                    | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        if ( (_DWORD)v6 == 7 )                                                                                        | 
        {                                                                                                             | 
          LOBYTE(v17) = PreviousMode;                                                                                 | 
          LargePages = MmManagePartitionNodeInformation(v10, Src, v17);                                               | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        if ( !(_DWORD)v6 )                                                                                            | 
        {                                                                                                             | 
          LargePages = MmManagePartitionMemoryInformation(v10, Src);                                                  | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v21 = v6 - 1;                                                                                                 | 
        if ( !v21 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v18) = PreviousMode;                                                                                 | 
          LargePages = MmManagePartitionMoveMemory(v10, v35, Src, v18);                                               | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v22 = v21 - 1;                                                                                                | 
        if ( !v22 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v18) = PreviousMode;                                                                                 | 
          LargePages = MiCreatePagingFile(a4, a4 + 16, a4 + 24, v18, v39, *v10);                                      | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v23 = v22 - 1;                                                                                                | 
        v24 = v10;                                                                                                    | 
        if ( !v23 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v18) = PreviousMode;                                                                                 | 
          LargePages = MmManagePartitionCombineMemory(v10, Src, a4, v18);                                             | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v25 = v23 - 1;                                                                                                | 
        if ( !v25 )                                                                                                   | 
        {                                                                                                             | 
          LOBYTE(v18) = PreviousMode;                                                                                 | 
          LargePages = MmManagePartitionInitialAddMemory(v10, Src, a4, v18);                                          | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        LOBYTE(v17) = PreviousMode;                                                                                   | 
        if ( v25 == 1 )                                                                                               | 
        {                                                                                                             | 
          LargePages = MmManagePartitionGetMemoryEvents(v10, Src, v17);                                               | 
          goto LABEL_20;                                                                                              | 
        }                                                                                                             | 
        v31 = 1LL;                                                                                                    | 
      }                                                                                                               | 
      LargePages = MmManagePartitionUpdateAttributes(v24, Src, v17, v31);                                             | 
      goto LABEL_20;                                                                                                  | 
    }                                                                                                                 | 
  }                                                                                                                   | 
LABEL_25:                                                                                                             | 
  if ( v32 )                                                                                                          | 
  {                                                                                                                   | 
    PsDereferencePartition(v10);                                                                                      | 
  }                                                                                                                   | 
  if ( v33 )                                                                                                          | 
  {                                                                                                                   | 
    PsDereferencePartition(v35);                                                                                      | 
  }                                                                                                                   | 
  return (unsigned int)v14;                                                                                           | 
}                                                                                                                     | 
199 row(s)
