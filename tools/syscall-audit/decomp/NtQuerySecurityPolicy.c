__int64 __fastcall NtQuerySecurityPolicy(                                                    | 
        __int64 a1,                                                                          | 
        __int64 a2,                                                                          | 
        __int64 a3,                                                                          | 
        _DWORD *a4,                                                                          | 
        volatile void *a5,                                                                   | 
        _DWORD *Address)                                                                     | 
{                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                              | 
                                                                                             | 
  v7 = a3;                                                                                   | 
  v24 = 0LL;                                                                                 | 
  v25 = 0LL;                                                                                 | 
  Pool2 = 0LL;                                                                               | 
  v23 = 0LL;                                                                                 | 
  v26[0] = 0LL;                                                                              | 
  LODWORD(Length) = 0;                                                                       | 
  v22 = 0;                                                                                   | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                         | 
  LOBYTE(a3) = PreviousMode;                                                                 | 
  v12 = SepCaptureUnicodeStringArray(a1, 1LL, a3, &v24);                                     | 
  if ( v12 >= 0 )                                                                            | 
  {                                                                                          | 
    LOBYTE(v13) = PreviousMode;                                                              | 
    v12 = SepCaptureUnicodeStringArray(a2, 1LL, v13, &v25);                                  | 
    if ( v12 >= 0 )                                                                          | 
    {                                                                                        | 
      LOBYTE(v14) = PreviousMode;                                                            | 
      v12 = SepCaptureUnicodeStringArray(v7, 1LL, v14, v26);                                 | 
      HIDWORD(Length) = v12;                                                                 | 
      if ( v12 >= 0 )                                                                        | 
      {                                                                                      | 
        if ( PreviousMode == 1 )                                                             | 
        {                                                                                    | 
          ProbeForWrite(a4, 4uLL, 4u);                                                       | 
          v15 = Address;                                                                     | 
          if ( ((unsigned __int8)Address & 3) != 0 )                                         | 
          {                                                                                  | 
            ExRaiseDatatypeMisalignment();                                                   | 
          }                                                                                  | 
          LODWORD(Length) = *Address;                                                        | 
          if ( !(_DWORD)Length )                                                             | 
          {                                                                                  | 
            Length = 0xC000000D00000000uLL;                                                  | 
            v12 = -1073741811;                                                               | 
            goto LABEL_18;                                                                   | 
          }                                                                                  | 
          ProbeForWrite(Address, 4uLL, 4u);                                                  | 
          v16 = (void *)a5;                                                                  | 
          if ( a5 )                                                                          | 
          {                                                                                  | 
            ProbeForWrite(a5, (unsigned int)Length, 1u);                                     | 
            Pool2 = (void *)ExAllocatePool2(0x101uLL, (unsigned int)Length, 0x20206553u);    | 
            v23 = Pool2;                                                                     | 
            if ( !Pool2 )                                                                    | 
            {                                                                                | 
              v12 = -1073741670;                                                             | 
              goto LABEL_18;                                                                 | 
            }                                                                                | 
          }                                                                                  | 
        }                                                                                    | 
        else                                                                                 | 
        {                                                                                    | 
          v16 = (void *)a5;                                                                  | 
          Pool2 = (void *)a5;                                                                | 
          v23 = (void *)a5;                                                                  | 
          v15 = Address;                                                                     | 
          LODWORD(Length) = *Address;                                                        | 
        }                                                                                    | 
        if ( qword_140F049F0 )                                                               | 
        {                                                                                    | 
          if ( Pool2 && !(_DWORD)Length )                                                    | 
          {                                                                                  | 
            v12 = -1073741811;                                                               | 
          }                                                                                  | 
          else                                                                               | 
          {                                                                                  | 
            v12 = guard_dispatch_icall_no_overrides(v24, v25, v26[0], &v22, Pool2, &Length); | 
          }                                                                                  | 
        }                                                                                    | 
        else                                                                                 | 
        {                                                                                    | 
          v12 = -1073741637;                                                                 | 
        }                                                                                    | 
        HIDWORD(Length) = v12;                                                               | 
        if ( (int)(v12 + 0x80000000) < 0 || v12 == -1073741789 )                             | 
        {                                                                                    | 
          *a4 = v22;                                                                         | 
          v20 = Length;                                                                      | 
          *v15 = Length;                                                                     | 
          if ( v16 )                                                                         | 
          {                                                                                  | 
            if ( PreviousMode == 1 && v12 >= 0 )                                             | 
            {                                                                                | 
              memmove(v16, Pool2, v20);                                                      | 
            }                                                                                | 
          }                                                                                  | 
        }                                                                                    | 
      }                                                                                      | 
    }                                                                                        | 
  }                                                                                          | 
LABEL_18:                                                                                    | 
  LOBYTE(v11) = PreviousMode;                                                                | 
  SepReleaseUnicodeStringArray(v24, v11);                                                    | 
  LOBYTE(v17) = PreviousMode;                                                                | 
  SepReleaseUnicodeStringArray(v25, v17);                                                    | 
  LOBYTE(v18) = PreviousMode;                                                                | 
  SepReleaseUnicodeStringArray(v26[0], v18);                                                 | 
  if ( Pool2 && PreviousMode == 1 )                                                          | 
  {                                                                                          | 
    ExFreePoolWithTag(Pool2, 0);                                                             | 
  }                                                                                          | 
  return (unsigned int)v12;                                                                  | 
}                                                                                            | 
114 row(s)
