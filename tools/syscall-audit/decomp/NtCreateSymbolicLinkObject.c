__int64 __fastcall NtCreateSymbolicLinkObject(unsigned __int64 a1, int a2, __int64 a3, UNICODE_STRING *a4) | 
{                                                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                            | 
                                                                                                           | 
  v26 = 0LL;                                                                                               | 
  DestinationString = 0LL;                                                                                 | 
  HIDWORD(v31) = 0;                                                                                        | 
  RtlInitUnicodeString(&DestinationString, 0LL);                                                           | 
  RtlInitUnicodeString(&v26, 0LL);                                                                         | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                       | 
  if ( PreviousMode )                                                                                      | 
  {                                                                                                        | 
    if ( (a3 & 3) != 0 )                                                                                   | 
    {                                                                                                      | 
      ExRaiseDatatypeMisalignment();                                                                       | 
    }                                                                                                      | 
    v9 = 0x7FFFFFFF0000LL;                                                                                 | 
    v30 = *(UNICODE_STRING **)(a3 + 16);                                                                   | 
    if ( v30 )                                                                                             | 
    {                                                                                                      | 
      DestinationString = *v30;                                                                            | 
    }                                                                                                      | 
    v26 = *a4;                                                                                             | 
    if ( a1 < 0x7FFFFFFF0000LL )                                                                           | 
    {                                                                                                      | 
      v9 = a1;                                                                                             | 
    }                                                                                                      | 
    *(_QWORD *)v9 = *(_QWORD *)v9;                                                                         | 
    v10 = (__m128i)v26;                                                                                    | 
  }                                                                                                        | 
  else                                                                                                     | 
  {                                                                                                        | 
    v10 = *(__m128i *)a4;                                                                                  | 
    v26 = *a4;                                                                                             | 
    v24 = *(UNICODE_STRING **)(a3 + 16);                                                                   | 
    if ( v24 )                                                                                             | 
    {                                                                                                      | 
      DestinationString = *v24;                                                                            | 
    }                                                                                                      | 
  }                                                                                                        | 
  if ( (v10.m128i_i8[2] & 1) != 0 )                                                                        | 
  {                                                                                                        | 
    epi16 = v10.m128i_i16[1] & 0xFFFE;                                                                     | 
    v26.MaximumLength = v10.m128i_i16[1] & 0xFFFE;                                                         | 
    v10 = (__m128i)v26;                                                                                    | 
  }                                                                                                        | 
  else                                                                                                     | 
  {                                                                                                        | 
    epi16 = _mm_extract_epi16(v10, 1);                                                                     | 
  }                                                                                                        | 
  if ( !epi16 )                                                                                            | 
  {                                                                                                        | 
    return 3221225485LL;                                                                                   | 
  }                                                                                                        | 
  v12 = _mm_cvtsi128_si32(v10);                                                                            | 
  if ( v12 > epi16 || (_mm_cvtsi128_si32(v10) & 1) != 0 )                                                  | 
  {                                                                                                        | 
    return 3221225485LL;                                                                                   | 
  }                                                                                                        | 
  v31 = 4LL;                                                                                               | 
  v32 = v10;                                                                                               | 
  SymbolicLink = ObCreateSymbolicLink(a1, a2, a3, (unsigned int)&v31, PreviousMode);                       | 
  v28 = SymbolicLink;                                                                                      | 
  v27 = a2;                                                                                                | 
  v14 = EtwApiCallsProvRegHandle;                                                                          | 
  if ( EtwApiCallsProvRegHandle )                                                                          | 
  {                                                                                                        | 
    v25 = 0;                                                                                               | 
    v15 = 0;                                                                                               | 
    if ( DestinationString.Buffer )                                                                        | 
    {                                                                                                      | 
      UserData[0].Ptr = (ULONGLONG)DestinationString.Buffer;                                               | 
      UserData[0].Size = DestinationString.Length;                                                         | 
      UserData[0].Reserved = 0;                                                                            | 
      v15 = 1;                                                                                             | 
    }                                                                                                      | 
    v16 = v15;                                                                                             | 
    UserData[v16].Ptr = (ULONGLONG)&v25;                                                                   | 
    *(_QWORD *)&UserData[v16].Size = 2LL;                                                                  | 
    v17 = v15 + 1;                                                                                         | 
    if ( v26.Buffer )                                                                                      | 
    {                                                                                                      | 
      v18 = v17;                                                                                           | 
      UserData[v18].Ptr = (ULONGLONG)v26.Buffer;                                                           | 
      UserData[v18].Size = v12;                                                                            | 
      *(&UserData[0].Reserved + 1 * v18) = 0;                                                              | 
      ++v17;                                                                                               | 
    }                                                                                                      | 
    v19 = v17;                                                                                             | 
    UserData[v19].Ptr = (ULONGLONG)&v25;                                                                   | 
    *(_QWORD *)&UserData[v19].Size = 2LL;                                                                  | 
    v20 = v17 + 1;                                                                                         | 
    v21 = v20;                                                                                             | 
    UserData[v21].Ptr = (ULONGLONG)&v27;                                                                   | 
    *(_QWORD *)&UserData[v21].Size = 4LL;                                                                  | 
    v22 = ++v20;                                                                                           | 
    UserData[v22].Ptr = (ULONGLONG)&v28;                                                                   | 
    *(_QWORD *)&UserData[v22].Size = 4LL;                                                                  | 
    EtwWrite(v14, &KERNEL_AUDIT_API_CREATESYMBOLICLINKOBJECT, 0LL, v20 + 1, UserData);                     | 
  }                                                                                                        | 
  return SymbolicLink;                                                                                     | 
}                                                                                                          | 
102 row(s)
