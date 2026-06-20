__int64 __fastcall NtSetCachedSigningLevel2(                                                              | 
        __int64 a1,                                                                                       | 
        unsigned __int64 a2,                                                                              | 
        const void *a3,                                                                                   | 
        unsigned int a4,                                                                                  | 
        __int64 a5,                                                                                       | 
        char *a6)                                                                                         | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  v30 = a2;                                                                                               | 
  v6 = a4;                                                                                                | 
  v8 = a1;                                                                                                | 
  Pool2 = 0LL;                                                                                            | 
  v25 = 0;                                                                                                | 
  SourceString[0] = 0LL;                                                                                  | 
  P = 0LL;                                                                                                | 
  *(_OWORD *)Src = 0LL;                                                                                   | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                      | 
  if ( (a1 & 0x2000) != 0 )                                                                               | 
  {                                                                                                       | 
    if ( qword_140F04A48 )                                                                                | 
    {                                                                                                     | 
      a2 = (unsigned __int64)a6;                                                                          | 
      if ( !a6 || !a5 )                                                                                   | 
      {                                                                                                   | 
        v12 = -1073741811;                                                                                | 
        goto LABEL_12;                                                                                    | 
      }                                                                                                   | 
      if ( PreviousMode == 1 )                                                                            | 
      {                                                                                                   | 
        if ( (BYTE2(KeGetCurrentThread()->ApcState.Process[3].ActiveGroupsMask.Masks[1]) & 7) != 1 )      | 
        {                                                                                                 | 
          goto LABEL_69;                                                                                  | 
        }                                                                                                 | 
        if ( ((unsigned __int8)a6 & 3) != 0 )                                                             | 
        {                                                                                                 | 
          ExRaiseDatatypeMisalignment();                                                                  | 
        }                                                                                                 | 
        v18 = *(_OWORD *)(a6 + 8);                                                                        | 
        *(_OWORD *)Src = v18;                                                                             | 
        v19 = (void *)v18;                                                                                | 
        if ( (_QWORD)v18 )                                                                                | 
        {                                                                                                 | 
          if ( ((__int64)Src[1] & 3) != 0 )                                                               | 
          {                                                                                               | 
            ExRaiseDatatypeMisalignment();                                                                | 
          }                                                                                               | 
          a2 = (unsigned __int64)Src[1] + v18;                                                            | 
          if ( (char *)Src[1] + (unsigned __int64)v18 > (void *)0x7FFFFFFF0000LL || (void *)a2 < Src[1] ) | 
          {                                                                                               | 
            v19 = Src[0];                                                                                 | 
          }                                                                                               | 
        }                                                                                                 | 
        v29 = v19;                                                                                        | 
        if ( !Src[1] || !v19 )                                                                            | 
        {                                                                                                 | 
          v12 = -1073741811;                                                                              | 
          goto LABEL_12;                                                                                  | 
        }                                                                                                 | 
        if ( (unsigned __int64)v19 > 0xFFFF )                                                             | 
        {                                                                                                 | 
          v12 = -1073741811;                                                                              | 
          goto LABEL_12;                                                                                  | 
        }                                                                                                 | 
        P = (PVOID)ExAllocatePool2(0x103uLL, (ULONG_PTR)v19, 0x63734943u);                                | 
        if ( !P )                                                                                         | 
        {                                                                                                 | 
          v12 = -1073741670;                                                                              | 
          goto LABEL_12;                                                                                  | 
        }                                                                                                 | 
        memmove(P, Src[1], (size_t)v19);                                                                  | 
      }                                                                                                   | 
      else                                                                                                | 
      {                                                                                                   | 
        v19 = (void *)*((_QWORD *)a6 + 1);                                                                | 
        P = (PVOID)*((_QWORD *)a6 + 2);                                                                   | 
      }                                                                                                   | 
      LOBYTE(a1) = PreviousMode;                                                                          | 
      v17 = guard_dispatch_icall_no_overrides(a1, a5, v19, P);                                            | 
      goto LABEL_37;                                                                                      | 
    }                                                                                                     | 
LABEL_39:                                                                                                 | 
    v12 = -1073741822;                                                                                    | 
    goto LABEL_12;                                                                                        | 
  }                                                                                                       | 
  if ( !qword_140F04968 )                                                                                 | 
  {                                                                                                       | 
    goto LABEL_39;                                                                                        | 
  }                                                                                                       | 
  if ( (a2 & 0x30) != 0 )                                                                                 | 
  {                                                                                                       | 
    goto LABEL_43;                                                                                        | 
  }                                                                                                       | 
  if ( a4 - 1 > 0xFFF )                                                                                   | 
  {                                                                                                       | 
    goto LABEL_42;                                                                                        | 
  }                                                                                                       | 
  if ( (a1 & 6) == 0 && (_BYTE)a2 )                                                                       | 
  {                                                                                                       | 
LABEL_43:                                                                                                 | 
    v12 = -1073741584;                                                                                    | 
    goto LABEL_12;                                                                                        | 
  }                                                                                                       | 
  if ( (a1 & 3) == 3 )                                                                                    | 
  {                                                                                                       | 
    goto LABEL_16;                                                                                        | 
  }                                                                                                       | 
  if ( PreviousMode == 1 )                                                                                | 
  {                                                                                                       | 
    if ( (a1 & 2) != 0 )                                                                                  | 
    {                                                                                                     | 
      goto LABEL_16;                                                                                      | 
    }                                                                                                     | 
    v8 = a1 | 1;                                                                                          | 
    if ( (a1 & 4) != 0 )                                                                                  | 
    {                                                                                                     | 
      goto LABEL_10;                                                                                      | 
    }                                                                                                     | 
    Process = KeGetCurrentThread()->ApcState.Process;                                                     | 
    if ( (BYTE2(Process[3].ActiveGroupsMask.Masks[1]) & 7) == 1 )                                         | 
    {                                                                                                     | 
      v21 = Process[3].ActiveGroupsMask.Masks[1] & 0xF;                                                   | 
      v22 = BYTE1(Process[3].ActiveGroupsMask.Masks[1]) & 0xF;                                            | 
      LOBYTE(a2) = v21;                                                                                   | 
      LOBYTE(Process) = v22;                                                                              | 
      v23 = SeCompareSigningLevels(Process, a2);                                                          | 
      v24 = v21;                                                                                          | 
      if ( !v23 )                                                                                         | 
      {                                                                                                   | 
        v24 = v22;                                                                                        | 
      }                                                                                                   | 
      v25 = v24;                                                                                          | 
      goto LABEL_10;                                                                                      | 
    }                                                                                                     | 
LABEL_69:                                                                                                 | 
    v12 = -1073741790;                                                                                    | 
    goto LABEL_12;                                                                                        | 
  }                                                                                                       | 
  if ( (a1 & 1) == 0 )                                                                                    | 
  {                                                                                                       | 
    if ( (a1 & 2) != 0 )                                                                                  | 
    {                                                                                                     | 
      v25 = 8;                                                                                            | 
      goto LABEL_10;                                                                                      | 
    }                                                                                                     | 
LABEL_16:                                                                                                 | 
    v12 = -1073741585;                                                                                    | 
    goto LABEL_12;                                                                                        | 
  }                                                                                                       | 
  v25 = 15;                                                                                               | 
LABEL_10:                                                                                                 | 
  v11 = 8 * v6;                                                                                           | 
  Pool2 = (UNICODE_STRING *)ExAllocatePool2(0x100uLL, v11, 0x63734943u);                                  | 
  SourceString[1] = Pool2;                                                                                | 
  if ( !Pool2 )                                                                                           | 
  {                                                                                                       | 
    v12 = -1073741670;                                                                                    | 
    goto LABEL_12;                                                                                        | 
  }                                                                                                       | 
  if ( PreviousMode == 1 )                                                                                | 
  {                                                                                                       | 
    if ( v11 && ((unsigned __int8)a3 & 7) != 0 )                                                          | 
    {                                                                                                     | 
      ExRaiseDatatypeMisalignment();                                                                      | 
    }                                                                                                     | 
    v14 = a6;                                                                                             | 
    if ( a6 && ((unsigned __int8)a6 & 3) != 0 )                                                           | 
    {                                                                                                     | 
      ExRaiseDatatypeMisalignment();                                                                      | 
    }                                                                                                     | 
  }                                                                                                       | 
  else                                                                                                    | 
  {                                                                                                       | 
    v14 = a6;                                                                                             | 
  }                                                                                                       | 
  memmove(Pool2, a3, v11);                                                                                | 
  if ( v14 )                                                                                              | 
  {                                                                                                       | 
    if ( *(_DWORD *)v14 < 0x18u )                                                                         | 
    {                                                                                                     | 
      v12 = -1073741580;                                                                                  | 
      goto LABEL_12;                                                                                      | 
    }                                                                                                     | 
    if ( *((_WORD *)v14 + 4) )                                                                            | 
    {                                                                                                     | 
      LOBYTE(v15) = PreviousMode;                                                                         | 
      v12 = SepCaptureUnicodeStringArray(v14 + 8, 1LL, v15, SourceString);                                | 
      if ( v12 < 0 )                                                                                      | 
      {                                                                                                   | 
        goto LABEL_12;                                                                                    | 
      }                                                                                                   | 
      v12 = RtlUnicodeStringValidateEx(SourceString[0], a2);                                              | 
      if ( v12 < 0 )                                                                                      | 
      {                                                                                                   | 
        goto LABEL_12;                                                                                    | 
      }                                                                                                   | 
    }                                                                                                     | 
  }                                                                                                       | 
  a2 = a4;                                                                                                | 
  if ( (v8 & 6) == 0 )                                                                                    | 
  {                                                                                                       | 
LABEL_36:                                                                                                 | 
    LOBYTE(v16) = v25;                                                                                    | 
    LOBYTE(v15) = v30;                                                                                    | 
    LOBYTE(a2) = PreviousMode;                                                                            | 
    v17 = guard_dispatch_icall_no_overrides(v8 & 0x807, a2, v15, v16);                                    | 
LABEL_37:                                                                                                 | 
    v12 = v17;                                                                                            | 
    goto LABEL_12;                                                                                        | 
  }                                                                                                       | 
  if ( a4 != 1 )                                                                                          | 
  {                                                                                                       | 
LABEL_42:                                                                                                 | 
    v12 = -1073741582;                                                                                    | 
    goto LABEL_12;                                                                                        | 
  }                                                                                                       | 
  if ( a5 == *(_QWORD *)&Pool2->Length )                                                                  | 
  {                                                                                                       | 
    goto LABEL_36;                                                                                        | 
  }                                                                                                       | 
  v12 = -1073741581;                                                                                      | 
LABEL_12:                                                                                                 | 
  LOBYTE(a2) = PreviousMode;                                                                              | 
  SepReleaseUnicodeStringArray(SourceString[0], a2);                                                      | 
  if ( Pool2 )                                                                                            | 
  {                                                                                                       | 
    ExFreePoolWithTag(Pool2, 0x63734943u);                                                                | 
  }                                                                                                       | 
  if ( P && PreviousMode == 1 )                                                                           | 
  {                                                                                                       | 
    ExFreePoolWithTag(P, 0x63734943u);                                                                    | 
  }                                                                                                       | 
  return (unsigned int)v12;                                                                               | 
}                                                                                                         | 
235 row(s)
