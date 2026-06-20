__int64 __fastcall NtQuerySystemEnvironmentValue(_OWORD *a1, volatile void *a2, unsigned __int16 a3, _WORD *a4) | 
{                                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                 | 
                                                                                                                | 
  v5 = a3;                                                                                                      | 
  v23 = 0;                                                                                                      | 
  *(_QWORD *)&DestinationString.Length = 0LL;                                                                   | 
  *(_OWORD *)UnicodeString = 0LL;                                                                               | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                                       | 
  {                                                                                                             | 
    return 3221225474LL;                                                                                        | 
  }                                                                                                             | 
  DestinationString.Buffer = 0LL;                                                                               | 
  CurrentThread = KeGetCurrentThread();                                                                         | 
  PreviousMode = CurrentThread->PreviousMode;                                                                   | 
  if ( PreviousMode )                                                                                           | 
  {                                                                                                             | 
    if ( ((unsigned __int8)a1 & 3) != 0 )                                                                       | 
    {                                                                                                           | 
      ExRaiseDatatypeMisalignment();                                                                            | 
    }                                                                                                           | 
    v9 = 0x7FFFFFFF0000LL;                                                                                      | 
    *(_OWORD *)UnicodeString = *a1;                                                                             | 
    v10 = _mm_cvtsi128_si32(*(__m128i *)UnicodeString);                                                         | 
    if ( !v10 )                                                                                                 | 
    {                                                                                                           | 
      return 3221225477LL;                                                                                      | 
    }                                                                                                           | 
    if ( ((__int64)UnicodeString[1] & 1) != 0 )                                                                 | 
    {                                                                                                           | 
      ExRaiseDatatypeMisalignment();                                                                            | 
    }                                                                                                           | 
    v11 = UnicodeString[1];                                                                                     | 
    v12 = (PCWCH)((char *)UnicodeString[1] + v10);                                                              | 
    if ( (unsigned __int64)v12 > 0x7FFFFFFF0000LL || v12 < UnicodeString[1] )                                   | 
    {                                                                                                           | 
      v11 = UnicodeString[1];                                                                                   | 
      v10 = (unsigned __int16)UnicodeString[0];                                                                 | 
    }                                                                                                           | 
    ProbeForWrite(a2, v5, 2u);                                                                                  | 
    if ( a4 )                                                                                                   | 
    {                                                                                                           | 
      if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )                                                            | 
      {                                                                                                         | 
        v9 = (__int64)a4;                                                                                       | 
      }                                                                                                         | 
      *(_WORD *)v9 = *(_WORD *)v9;                                                                              | 
      v11 = UnicodeString[1];                                                                                   | 
      v10 = (unsigned __int16)UnicodeString[0];                                                                 | 
    }                                                                                                           | 
    if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                                  | 
    {                                                                                                           | 
      return 3221225569LL;                                                                                      | 
    }                                                                                                           | 
  }                                                                                                             | 
  else                                                                                                          | 
  {                                                                                                             | 
    *(_OWORD *)UnicodeString = *a1;                                                                             | 
    v11 = (const WCHAR *)_mm_srli_si128(*(__m128i *)UnicodeString, 8).m128i_u64[0];                             | 
    v10 = _mm_cvtsi128_si32(*(__m128i *)UnicodeString);                                                         | 
  }                                                                                                             | 
  BytesInMultiByteString = 0;                                                                                   | 
  RtlUnicodeToMultiByteSize(&BytesInMultiByteString, v11, v10);                                                 | 
  v13 = BytesInMultiByteString + 1;                                                                             | 
  v23 = BytesInMultiByteString + 1;                                                                             | 
  DestinationString.Buffer = (char *)ExAllocatePool2(0x40uLL, BytesInMultiByteString + 1, 0x72766E45u);         | 
  if ( !DestinationString.Buffer )                                                                              | 
  {                                                                                                             | 
    return 3221225626LL;                                                                                        | 
  }                                                                                                             | 
  DestinationString.MaximumLength = v13;                                                                        | 
  v14 = RtlUnicodeStringToAnsiString(&DestinationString, (PCUNICODE_STRING)UnicodeString, 0);                   | 
  v24 = v14;                                                                                                    | 
  if ( v14 >= 0 )                                                                                               | 
  {                                                                                                             | 
    Pool2 = (void *)ExAllocatePool2(0x40uLL, 0x400uLL, 0x72766E45u);                                            | 
    P = Pool2;                                                                                                  | 
    if ( Pool2 )                                                                                                | 
    {                                                                                                           | 
      v16 = KeGetCurrentThread();                                                                               | 
      --v16->KernelApcDisable;                                                                                  | 
      ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                            | 
      EnvironmentVariable = HalGetEnvironmentVariable(DestinationString.Buffer, 1024LL, Pool2);                 | 
      ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                            | 
      KeLeaveCriticalRegion();                                                                                  | 
      ExFreePoolWithTag(DestinationString.Buffer, 0);                                                           | 
      if ( EnvironmentVariable )                                                                                | 
      {                                                                                                         | 
        ExFreePoolWithTag(Pool2, 0);                                                                            | 
        return 3221225473LL;                                                                                    | 
      }                                                                                                         | 
      else                                                                                                      | 
      {                                                                                                         | 
        RtlInitAnsiString(&DestinationString, (PCSZ)Pool2);                                                     | 
        UnicodeString[1] = (PCWCH)a2;                                                                           | 
        WORD1(UnicodeString[0]) = v5;                                                                           | 
        LOWORD(UnicodeString[0]) = 0;                                                                           | 
        v18 = RtlAnsiStringToUnicodeString((PUNICODE_STRING)UnicodeString, &DestinationString, 0);              | 
        v24 = v18;                                                                                              | 
        if ( a4 )                                                                                               | 
        {                                                                                                       | 
          *a4 = UnicodeString[0];                                                                               | 
        }                                                                                                       | 
        ExFreePoolWithTag(Pool2, 0);                                                                            | 
        return v18;                                                                                             | 
      }                                                                                                         | 
    }                                                                                                           | 
    else                                                                                                        | 
    {                                                                                                           | 
      ExFreePoolWithTag(DestinationString.Buffer, 0);                                                           | 
      return 3221225626LL;                                                                                      | 
    }                                                                                                           | 
  }                                                                                                             | 
  else                                                                                                          | 
  {                                                                                                             | 
    ExFreePoolWithTag(DestinationString.Buffer, 0);                                                             | 
    return (unsigned int)v14;                                                                                   | 
  }                                                                                                             | 
}                                                                                                               | 
119 row(s)
