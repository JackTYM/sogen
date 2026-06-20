__int64 __fastcall NtSetSystemEnvironmentValue(_OWORD *a1, __m128i *a2)                                 | 
{                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                         | 
                                                                                                        | 
  v18 = 0;                                                                                              | 
  v19 = 0;                                                                                              | 
  *(_QWORD *)&DestinationString.Length = 0LL;                                                           | 
  *(_QWORD *)&v17.Length = 0LL;                                                                         | 
  *(_OWORD *)UnicodeString = 0LL;                                                                       | 
  v22 = 0LL;                                                                                            | 
  if ( (unsigned __int8)PsIsCurrentThreadInServerSilo() )                                               | 
  {                                                                                                     | 
    return 3221225474LL;                                                                                | 
  }                                                                                                     | 
  DestinationString.Buffer = 0LL;                                                                       | 
  v17.Buffer = 0LL;                                                                                     | 
  CurrentThread = KeGetCurrentThread();                                                                 | 
  PreviousMode = CurrentThread->PreviousMode;                                                           | 
  if ( PreviousMode )                                                                                   | 
  {                                                                                                     | 
    if ( ((unsigned __int8)a1 & 3) != 0 )                                                               | 
    {                                                                                                   | 
      goto LABEL_19;                                                                                    | 
    }                                                                                                   | 
    *(_OWORD *)UnicodeString = *a1;                                                                     | 
    if ( !(unsigned __int16)_mm_cvtsi128_si32(*(__m128i *)UnicodeString) )                              | 
    {                                                                                                   | 
      return 3221225626LL;                                                                              | 
    }                                                                                                   | 
    if ( ((__int64)UnicodeString[1] & 1) != 0 )                                                         | 
    {                                                                                                   | 
      ExRaiseDatatypeMisalignment();                                                                    | 
    }                                                                                                   | 
    if ( ((unsigned __int8)a2 & 3) != 0 )                                                               | 
    {                                                                                                   | 
LABEL_19:                                                                                               | 
      ExRaiseDatatypeMisalignment();                                                                    | 
    }                                                                                                   | 
    v22 = *a2;                                                                                          | 
    v5 = _mm_cvtsi128_si32(v22);                                                                        | 
    if ( !v5 )                                                                                          | 
    {                                                                                                   | 
      return 3221225626LL;                                                                              | 
    }                                                                                                   | 
    v6 = (const WCHAR *)v22.m128i_i64[1];                                                               | 
    if ( (v22.m128i_i8[8] & 1) != 0 )                                                                   | 
    {                                                                                                   | 
      ExRaiseDatatypeMisalignment();                                                                    | 
    }                                                                                                   | 
    v7 = (const WCHAR *)(v22.m128i_i64[1] + v5);                                                        | 
    if ( (unsigned __int64)v7 > 0x7FFFFFFF0000LL || (unsigned __int64)v7 < v22.m128i_i64[1] )           | 
    {                                                                                                   | 
      v6 = (const WCHAR *)v22.m128i_i64[1];                                                             | 
      v5 = v22.m128i_i16[0];                                                                            | 
    }                                                                                                   | 
    if ( !SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, PreviousMode) )                          | 
    {                                                                                                   | 
      return 3221225569LL;                                                                              | 
    }                                                                                                   | 
  }                                                                                                     | 
  else                                                                                                  | 
  {                                                                                                     | 
    *(_OWORD *)UnicodeString = *a1;                                                                     | 
    v22 = *a2;                                                                                          | 
    v6 = (const WCHAR *)_mm_srli_si128(v22, 8).m128i_u64[0];                                            | 
    v5 = _mm_cvtsi128_si32(v22);                                                                        | 
  }                                                                                                     | 
  BytesInMultiByteString = 0;                                                                           | 
  RtlUnicodeToMultiByteSize(&BytesInMultiByteString, UnicodeString[1], LOWORD(UnicodeString[0]));       | 
  v8 = BytesInMultiByteString + 1;                                                                      | 
  v18 = BytesInMultiByteString + 1;                                                                     | 
  DestinationString.Buffer = (char *)ExAllocatePool2(0x40uLL, BytesInMultiByteString + 1, 0x72766E45u); | 
  if ( !DestinationString.Buffer )                                                                      | 
  {                                                                                                     | 
    return 3221225626LL;                                                                                | 
  }                                                                                                     | 
  DestinationString.MaximumLength = v8;                                                                 | 
  v9 = RtlUnicodeStringToAnsiString(&DestinationString, (PCUNICODE_STRING)UnicodeString, 0);            | 
  v20 = v9;                                                                                             | 
  if ( v9 >= 0 )                                                                                        | 
  {                                                                                                     | 
    v15 = 0;                                                                                            | 
    RtlUnicodeToMultiByteSize(&v15, v6, v5);                                                            | 
    v10 = v15 + 1;                                                                                      | 
    v19 = v15 + 1;                                                                                      | 
    v17.Buffer = (char *)ExAllocatePool2(0x40uLL, v15 + 1, 0x72766E45u);                                | 
    if ( v17.Buffer )                                                                                   | 
    {                                                                                                   | 
      v17.MaximumLength = v10;                                                                          | 
      v11 = RtlUnicodeStringToAnsiString(&v17, (PCUNICODE_STRING)&v22, 0);                              | 
      v20 = v11;                                                                                        | 
      if ( v11 >= 0 )                                                                                   | 
      {                                                                                                 | 
        v12 = KeGetCurrentThread();                                                                     | 
        --v12->KernelApcDisable;                                                                        | 
        ExAcquireFastMutexUnsafe(&ExpEnvironmentLock);                                                  | 
        v13 = HalSetEnvironmentVariable(DestinationString.Buffer, v17.Buffer);                          | 
        ExReleaseFastMutexUnsafe(&ExpEnvironmentLock);                                                  | 
        KeLeaveCriticalRegion();                                                                        | 
        ExFreePoolWithTag(DestinationString.Buffer, 0);                                                 | 
        ExFreePoolWithTag(v17.Buffer, 0);                                                               | 
        return v13 != 0 ? 0xC000009A : 0;                                                               | 
      }                                                                                                 | 
      else                                                                                              | 
      {                                                                                                 | 
        ExFreePoolWithTag(DestinationString.Buffer, 0);                                                 | 
        ExFreePoolWithTag(v17.Buffer, 0);                                                               | 
        return (unsigned int)v11;                                                                       | 
      }                                                                                                 | 
    }                                                                                                   | 
    else                                                                                                | 
    {                                                                                                   | 
      ExFreePoolWithTag(DestinationString.Buffer, 0);                                                   | 
      return 3221225626LL;                                                                              | 
    }                                                                                                   | 
  }                                                                                                     | 
  else                                                                                                  | 
  {                                                                                                     | 
    ExFreePoolWithTag(DestinationString.Buffer, 0);                                                     | 
    return (unsigned int)v9;                                                                            | 
  }                                                                                                     | 
}                                                                                                       | 
122 row(s)
