__int64 __fastcall NtManageHotPatch(int a1, volatile void *a2, unsigned int a3, unsigned __int64 a4)                     | 
{                                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                          | 
                                                                                                                         | 
  v4 = a3;                                                                                                               | 
  v32 = a4;                                                                                                              | 
  v27 = a4;                                                                                                              | 
  DestinationString = 0LL;                                                                                               | 
  Sid = 0;                                                                                                               | 
  memset_0(&Src, 0, 0x64uLL);                                                                                            | 
  P = 0LL;                                                                                                               | 
  v29 = 0LL;                                                                                                             | 
  v30 = 0LL;                                                                                                             | 
  RtlInitUnicodeString(&DestinationString, 0LL);                                                                         | 
  Size = 0;                                                                                                              | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                     | 
  v22 = PreviousMode;                                                                                                    | 
  if ( (dword_140E374F4 & 1) == 0 )                                                                                      | 
  {                                                                                                                      | 
    goto LABEL_2;                                                                                                        | 
  }                                                                                                                      | 
  if ( !byte_140E374E8 )                                                                                                 | 
  {                                                                                                                      | 
    ProcessActivePatches = -1073741661;                                                                                  | 
    goto LABEL_3;                                                                                                        | 
  }                                                                                                                      | 
  if ( a1 != 7 && a1 != 8 && (unsigned int)(a1 - 9) >= 2 )                                                               | 
  {                                                                                                                      | 
    if ( !SeSinglePrivilegeCheck(SeLoadDriverPrivilege, PreviousMode) )                                                  | 
    {                                                                                                                    | 
      ProcessActivePatches = -1073741727;                                                                                | 
      goto LABEL_3;                                                                                                      | 
    }                                                                                                                    | 
    CurrentServerSilo = PsGetCurrentServerSilo();                                                                        | 
    if ( CurrentServerSilo != PdcCreateWatchdogAroundClientCall() )                                                      | 
    {                                                                                                                    | 
      ProcessActivePatches = -1073741790;                                                                                | 
      goto LABEL_3;                                                                                                      | 
    }                                                                                                                    | 
    PreviousMode = v22;                                                                                                  | 
  }                                                                                                                      | 
  v11 = 0;                                                                                                               | 
  Size_4 = 0;                                                                                                            | 
  if ( a1 > 5 )                                                                                                          | 
  {                                                                                                                      | 
    if ( a1 != 6 )                                                                                                       | 
    {                                                                                                                    | 
      switch ( a1 )                                                                                                      | 
      {                                                                                                                  | 
        case 7:                                                                                                          | 
          v11 = 1;                                                                                                       | 
          v12 = 32;                                                                                                      | 
          Size_4 = 1;                                                                                                    | 
          goto LABEL_34;                                                                                                 | 
        case 8:                                                                                                          | 
          v12 = 48;                                                                                                      | 
          goto LABEL_34;                                                                                                 | 
        case 9:                                                                                                          | 
          v11 = 1;                                                                                                       | 
          v12 = 8;                                                                                                       | 
          v13 = 4;                                                                                                       | 
          Size_4 = 1;                                                                                                    | 
          goto LABEL_35;                                                                                                 | 
      }                                                                                                                  | 
      if ( a1 != 10 )                                                                                                    | 
      {                                                                                                                  | 
        goto LABEL_29;                                                                                                   | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    v12 = 40;                                                                                                            | 
    goto LABEL_34;                                                                                                       | 
  }                                                                                                                      | 
  if ( a1 == 5 )                                                                                                         | 
  {                                                                                                                      | 
    goto LABEL_23;                                                                                                       | 
  }                                                                                                                      | 
  if ( a1 )                                                                                                              | 
  {                                                                                                                      | 
    if ( a1 == 1 )                                                                                                       | 
    {                                                                                                                    | 
LABEL_2:                                                                                                                 | 
      ProcessActivePatches = -1073741637;                                                                                | 
LABEL_3:                                                                                                                 | 
      v23 = ProcessActivePatches;                                                                                        | 
      v9 = 0x7FFFFFFF0000LL;                                                                                             | 
LABEL_101:                                                                                                               | 
      v19 = v22;                                                                                                         | 
      goto LABEL_102;                                                                                                    | 
    }                                                                                                                    | 
    if ( a1 != 2 )                                                                                                       | 
    {                                                                                                                    | 
      if ( a1 != 3 )                                                                                                     | 
      {                                                                                                                  | 
        if ( a1 == 4 )                                                                                                   | 
        {                                                                                                                | 
          goto LABEL_2;                                                                                                  | 
        }                                                                                                                | 
LABEL_29:                                                                                                                | 
        ProcessActivePatches = -1073741821;                                                                              | 
        goto LABEL_3;                                                                                                    | 
      }                                                                                                                  | 
      goto LABEL_22;                                                                                                     | 
    }                                                                                                                    | 
LABEL_23:                                                                                                                | 
    v12 = 96;                                                                                                            | 
    goto LABEL_34;                                                                                                       | 
  }                                                                                                                      | 
LABEL_22:                                                                                                                | 
  v12 = 104;                                                                                                             | 
LABEL_34:                                                                                                                | 
  v13 = 8;                                                                                                               | 
LABEL_35:                                                                                                                | 
  if ( (unsigned int)v4 < v12 )                                                                                          | 
  {                                                                                                                      | 
    ProcessActivePatches = -1073741820;                                                                                  | 
    goto LABEL_3;                                                                                                        | 
  }                                                                                                                      | 
  if ( PreviousMode )                                                                                                    | 
  {                                                                                                                      | 
    v9 = 0x7FFFFFFF0000LL;                                                                                               | 
    v14 = 0x7FFFFFFF0000LL;                                                                                              | 
    if ( a4 < 0x7FFFFFFF0000LL )                                                                                         | 
    {                                                                                                                    | 
      v14 = a4;                                                                                                          | 
    }                                                                                                                    | 
    *(_DWORD *)v14 = *(_DWORD *)v14;                                                                                     | 
    if ( v11 )                                                                                                           | 
    {                                                                                                                    | 
      if ( (_DWORD)v4 && ((v13 - 1) & (unsigned int)a2) != 0 )                                                           | 
      {                                                                                                                  | 
        ExRaiseDatatypeMisalignment();                                                                                   | 
      }                                                                                                                  | 
    }                                                                                                                    | 
    else                                                                                                                 | 
    {                                                                                                                    | 
      ProbeForWrite(a2, v4, v13);                                                                                        | 
    }                                                                                                                    | 
    memmove(&Src, (const void *)a2, v12);                                                                                | 
  }                                                                                                                      | 
  else                                                                                                                   | 
  {                                                                                                                      | 
    memmove(&Src, (const void *)a2, v12);                                                                                | 
    v9 = 0x7FFFFFFF0000LL;                                                                                               | 
  }                                                                                                                      | 
  if ( a1 )                                                                                                              | 
  {                                                                                                                      | 
    if ( a1 == 2 )                                                                                                       | 
    {                                                                                                                    | 
      goto LABEL_78;                                                                                                     | 
    }                                                                                                                    | 
    if ( a1 != 3 )                                                                                                       | 
    {                                                                                                                    | 
      if ( a1 != 5 )                                                                                                     | 
      {                                                                                                                  | 
        switch ( a1 )                                                                                                    | 
        {                                                                                                                | 
          case 6:                                                                                                        | 
            if ( Src == 3 || Src >= 2 && Src <= 3 )                                                                      | 
            {                                                                                                            | 
              ProcessActivePatches = MiQueryProcessActivePatches(*(ULONG_PTR *)&StringIn.Length, v4, PreviousMode, Src); | 
              v23 = ProcessActivePatches;                                                                                | 
              if ( ProcessActivePatches < 0 )                                                                            | 
              {                                                                                                          | 
                goto LABEL_101;                                                                                          | 
              }                                                                                                          | 
              if ( Size <= (unsigned int)v4 )                                                                            | 
              {                                                                                                          | 
                p_Src = (unsigned int *)v29;                                                                             | 
                goto LABEL_97;                                                                                           | 
              }                                                                                                          | 
              goto LABEL_68;                                                                                             | 
            }                                                                                                            | 
            break;                                                                                                       | 
          case 7:                                                                                                        | 
            ProcessActivePatches = MiApplyImageHotPatchRequest(&Src);                                                    | 
            v23 = ProcessActivePatches;                                                                                  | 
            MiLogHotPatchManagement(7LL, &Src, 0LL, (unsigned int)ProcessActivePatches);                                 | 
            goto LABEL_71;                                                                                               | 
          case 8:                                                                                                        | 
            if ( Src == 1 && *(_QWORD *)&StringIn.Length == -1LL && !v36[0] && StringIn.Buffer )                         | 
            {                                                                                                            | 
              ProcessActivePatches = MiQuerySingleLoadedPatch(                                                           | 
                                       (unsigned int)&v30,                                                               | 
                                       (unsigned int)&Size,                                                              | 
                                       StringIn.Buffer,                                                                  | 
                                       0,                                                                                | 
                                       (__int64)a2,                                                                      | 
                                       v4);                                                                              | 
              v23 = ProcessActivePatches;                                                                                | 
              if ( ProcessActivePatches < 0 )                                                                            | 
              {                                                                                                          | 
                goto LABEL_101;                                                                                          | 
              }                                                                                                          | 
              if ( Size <= (unsigned int)v4 )                                                                            | 
              {                                                                                                          | 
                p_Src = (unsigned int *)v30;                                                                             | 
                goto LABEL_97;                                                                                           | 
              }                                                                                                          | 
LABEL_68:                                                                                                                | 
              ProcessActivePatches = -1073741789;                                                                        | 
              goto LABEL_59;                                                                                             | 
            }                                                                                                            | 
            break;                                                                                                       | 
          case 9:                                                                                                        | 
            if ( Src == 1 && !Sid )                                                                                      | 
            {                                                                                                            | 
              ProcessActivePatches = 0;                                                                                  | 
              v23 = 0;                                                                                                   | 
LABEL_71:                                                                                                                | 
              p_Src = 0LL;                                                                                               | 
              goto LABEL_97;                                                                                             | 
            }                                                                                                            | 
            break;                                                                                                       | 
          default:                                                                                                       | 
            LOBYTE(v15) = PreviousMode;                                                                                  | 
            ProcessActivePatches = MiCreatePatchSectionRequest(&Src, v15);                                               | 
            v23 = ProcessActivePatches;                                                                                  | 
            if ( ProcessActivePatches < 0 )                                                                              | 
            {                                                                                                            | 
              goto LABEL_101;                                                                                            | 
            }                                                                                                            | 
            goto LABEL_96;                                                                                               | 
        }                                                                                                                | 
LABEL_58:                                                                                                                | 
        ProcessActivePatches = -1073741811;                                                                              | 
LABEL_59:                                                                                                                | 
        v23 = ProcessActivePatches;                                                                                      | 
        goto LABEL_101;                                                                                                  | 
      }                                                                                                                  | 
LABEL_78:                                                                                                                | 
      if ( Src != 2 )                                                                                                    | 
      {                                                                                                                  | 
        goto LABEL_58;                                                                                                   | 
      }                                                                                                                  | 
      p_Sid = 0LL;                                                                                                       | 
      if ( a1 == 5 )                                                                                                     | 
      {                                                                                                                  | 
        if ( !RtlValidSid(&Sid) )                                                                                        | 
        {                                                                                                                | 
LABEL_81:                                                                                                                | 
          ProcessActivePatches = -1073741704;                                                                            | 
          goto LABEL_59;                                                                                                 | 
        }                                                                                                                | 
        p_Sid = &Sid;                                                                                                    | 
      }                                                                                                                  | 
      ProcessActivePatches = MiQueryLoadedPatches(p_Sid, v4);                                                            | 
      v23 = ProcessActivePatches;                                                                                        | 
      if ( ProcessActivePatches < 0 )                                                                                    | 
      {                                                                                                                  | 
        goto LABEL_101;                                                                                                  | 
      }                                                                                                                  | 
      if ( Size <= (unsigned int)v4 )                                                                                    | 
      {                                                                                                                  | 
        p_Src = (unsigned int *)P;                                                                                       | 
        goto LABEL_97;                                                                                                   | 
      }                                                                                                                  | 
      goto LABEL_68;                                                                                                     | 
    }                                                                                                                    | 
  }                                                                                                                      | 
  if ( Src != 2 || !StringIn.Length )                                                                                    | 
  {                                                                                                                      | 
    goto LABEL_58;                                                                                                       | 
  }                                                                                                                      | 
  if ( PreviousMode && ((__int64)StringIn.Buffer & 1) != 0 )                                                             | 
  {                                                                                                                      | 
    ExRaiseDatatypeMisalignment();                                                                                       | 
  }                                                                                                                      | 
  ProcessActivePatches = RtlDuplicateUnicodeString(0, &StringIn, &DestinationString);                                    | 
  v23 = ProcessActivePatches;                                                                                            | 
  if ( ProcessActivePatches < 0 )                                                                                        | 
  {                                                                                                                      | 
    goto LABEL_101;                                                                                                      | 
  }                                                                                                                      | 
  if ( a1 == 3 )                                                                                                         | 
  {                                                                                                                      | 
    if ( !RtlValidSid(v36) )                                                                                             | 
    {                                                                                                                    | 
      goto LABEL_81;                                                                                                     | 
    }                                                                                                                    | 
    v18 = MiLoadHotPatchForUserSid(&DestinationString, v36, v37);                                                        | 
  }                                                                                                                      | 
  else                                                                                                                   | 
  {                                                                                                                      | 
    v18 = MiLoadHotPatch(&DestinationString, 1LL, v37, v38);                                                             | 
  }                                                                                                                      | 
  v23 = v18;                                                                                                             | 
  ProcessActivePatches = v18;                                                                                            | 
  MiLogHotPatchManagement((unsigned int)a1, &Src, &DestinationString, v18);                                              | 
LABEL_96:                                                                                                                | 
  Size = v12;                                                                                                            | 
  p_Src = &Src;                                                                                                          | 
LABEL_97:                                                                                                                | 
  v19 = v22;                                                                                                             | 
  if ( !Size_4 )                                                                                                         | 
  {                                                                                                                      | 
    if ( v22 )                                                                                                           | 
    {                                                                                                                    | 
      memmove((void *)a2, p_Src, Size);                                                                                  | 
      v20 = (_DWORD *)a4;                                                                                                | 
      goto LABEL_103;                                                                                                    | 
    }                                                                                                                    | 
    memmove((void *)a2, p_Src, Size);                                                                                    | 
  }                                                                                                                      | 
LABEL_102:                                                                                                               | 
  v20 = (_DWORD *)a4;                                                                                                    | 
LABEL_103:                                                                                                               | 
  if ( v19 )                                                                                                             | 
  {                                                                                                                      | 
    if ( (unsigned __int64)v20 < 0x7FFFFFFF0000LL )                                                                      | 
    {                                                                                                                    | 
      v9 = v32;                                                                                                          | 
    }                                                                                                                    | 
    *(_DWORD *)v9 = *(_DWORD *)v9;                                                                                       | 
    *v20 = Size;                                                                                                         | 
    ProcessActivePatches = v23;                                                                                          | 
  }                                                                                                                      | 
  else                                                                                                                   | 
  {                                                                                                                      | 
    *v20 = Size;                                                                                                         | 
  }                                                                                                                      | 
  RtlFreeAnsiString(&DestinationString);                                                                                 | 
  if ( P )                                                                                                               | 
  {                                                                                                                      | 
    ExFreePoolWithTag(P, 0);                                                                                             | 
  }                                                                                                                      | 
  if ( v29 )                                                                                                             | 
  {                                                                                                                      | 
    ExFreePoolWithTag(v29, 0);                                                                                           | 
  }                                                                                                                      | 
  if ( v30 )                                                                                                             | 
  {                                                                                                                      | 
    ExFreePoolWithTag(v30, 0);                                                                                           | 
  }                                                                                                                      | 
  return (unsigned int)ProcessActivePatches;                                                                             | 
}                                                                                                                        | 
335 row(s)
