__int64 __fastcall NtUserDefSetText(__int64 a1, __m128i *a2)       | 
{                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]    | 
                                                                   | 
  v13 = 0LL;                                                       | 
  EnterCrit(0LL, 0LL);                                             | 
  v7 = ValidateReceivingHwnd(a1, 1LL);                             | 
  v8 = 0;                                                          | 
  if ( v7 )                                                        | 
  {                                                                | 
    if ( a2 )                                                      | 
    {                                                              | 
      if ( (unsigned __int64)a2 >= MmUserProbeAddress )            | 
      {                                                            | 
        a2 = (__m128i *)MmUserProbeAddress;                        | 
      }                                                            | 
      v13 = *a2;                                                   | 
      v10 = _mm_srli_si128(v13, 8).m128i_u64[0];                   | 
      if ( v10 )                                                   | 
      {                                                            | 
        if ( v13.m128i_i32[0] > (v13.m128i_i32[1] & 0x7FFFFFFFu) ) | 
        {                                                          | 
          ExRaiseAccessViolation();                                | 
        }                                                          | 
        if ( v13.m128i_i32[1] < 0 )                                | 
        {                                                          | 
          v12 = v10 + v13.m128i_u32[0] + 1LL;                      | 
          if ( v12 < v10 || v12 > MmUserProbeAddress )             | 
          {                                                        | 
            ExRaiseAccessViolation();                              | 
          }                                                        | 
        }                                                          | 
        else                                                       | 
        {                                                          | 
          if ( (v10 & 1) != 0 )                                    | 
          {                                                        | 
            ExRaiseDatatypeMisalignment();                         | 
          }                                                        | 
          v11 = v10 + v13.m128i_u32[0] + 2LL;                      | 
          if ( v11 < v10 || v11 > MmUserProbeAddress )             | 
          {                                                        | 
            ExRaiseAccessViolation();                              | 
          }                                                        | 
        }                                                          | 
      }                                                            | 
      else                                                         | 
      {                                                            | 
        v13.m128i_i32[0] = 0;                                      | 
      }                                                            | 
      a2 = &v13;                                                   | 
    }                                                              | 
    v8 = DefSetText(v7, a2);                                       | 
  }                                                                | 
  UserSessionSwitchLeaveCrit(v5, v4, v6, v7);                      | 
  return v8;                                                       | 
}                                                                  | 
56 row(s)
