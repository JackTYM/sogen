__int64 __fastcall NtSetSystemEnvironmentValueEx(__m128i *a1, __int128 *a2, __int64 a3, int a4, int a5) | 
{                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                         | 
                                                                                                        | 
  v17 = 0LL;                                                                                            | 
  if ( dword_140EFED30 != 2 || (unsigned __int8)PsIsCurrentThreadInServerSilo() )                       | 
  {                                                                                                     | 
    return 3221225474LL;                                                                                | 
  }                                                                                                     | 
  if ( !KeGetCurrentThread()->PreviousMode )                                                            | 
  {                                                                                                     | 
    return ExSetFirmwareEnvironmentVariable((_DWORD)a1, (_DWORD)a2, a3, a4, a5);                        | 
  }                                                                                                     | 
  if ( ((unsigned __int8)a1 & 3) != 0 )                                                                 | 
  {                                                                                                     | 
    goto LABEL_24;                                                                                      | 
  }                                                                                                     | 
  Src = *a1;                                                                                            | 
  if ( !(unsigned __int16)_mm_cvtsi128_si32(*a1) )                                                      | 
  {                                                                                                     | 
    return 3221225477LL;                                                                                | 
  }                                                                                                     | 
  if ( (Src.m128i_i8[8] & 1) != 0 )                                                                     | 
  {                                                                                                     | 
    ExRaiseDatatypeMisalignment();                                                                      | 
  }                                                                                                     | 
  if ( ((unsigned __int8)a2 & 3) != 0 )                                                                 | 
  {                                                                                                     | 
LABEL_24:                                                                                               | 
    ExRaiseDatatypeMisalignment();                                                                      | 
  }                                                                                                     | 
  v15 = a3 != 0 ? a4 : 0;                                                                               | 
  v13 = SeSinglePrivilegeCheck(SeSystemEnvironmentPrivilege, 1);                                        | 
  if ( v13 )                                                                                            | 
  {                                                                                                     | 
    goto LABEL_16;                                                                                      | 
  }                                                                                                     | 
  if ( (unsigned __int8)PsIsProcessAppContainer(KeGetCurrentThread()->ApcState.Process) )               | 
  {                                                                                                     | 
    v13 = ExpFirmwareAccessAppContainerCheck(2LL);                                                      | 
  }                                                                                                     | 
  if ( !v13 )                                                                                           | 
  {                                                                                                     | 
    return 3221225569LL;                                                                                | 
  }                                                                                                     | 
LABEL_16:                                                                                               | 
  v17 = *a2;                                                                                            | 
  Pool2 = (_WORD *)ExAllocatePool2(0x40uLL, Src.m128i_u16[0] + 2LL, 0x72766E45u);                       | 
  v10 = Pool2;                                                                                          | 
  if ( !Pool2 )                                                                                         | 
  {                                                                                                     | 
    return 3221225626LL;                                                                                | 
  }                                                                                                     | 
  memmove(Pool2, (const void *)Src.m128i_i64[1], Src.m128i_u16[0]);                                     | 
  v10[(unsigned __int64)Src.m128i_u16[0] >> 1] = 0;                                                     | 
  v11 = v17 - ExpSecureBootVendorGuid;                                                                  | 
  if ( (_QWORD)v17 == ExpSecureBootVendorGuid )                                                         | 
  {                                                                                                     | 
    v11 = *((_QWORD *)&v17 + 1) - 0x4B788FE7F42860BDLL;                                                 | 
  }                                                                                                     | 
  if ( v11 || wcsnicmp(v10, L"Kernel_", 7uLL) )                                                         | 
  {                                                                                                     | 
    v12 = ExpSetFirmwareEnvironmentVariable((_DWORD)v10, (unsigned int)&v17, a3, v15, a5, 1);           | 
    ExFreePoolWithTag(v10, 0);                                                                          | 
    return v12;                                                                                         | 
  }                                                                                                     | 
  else                                                                                                  | 
  {                                                                                                     | 
    ExFreePoolWithTag(v10, 0);                                                                          | 
    return 3221225506LL;                                                                                | 
  }                                                                                                     | 
}                                                                                                       | 
72 row(s)
