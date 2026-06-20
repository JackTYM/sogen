__int64 __fastcall NtUserTrackMouseEvent(__m128i *a1)           | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  v5 = 0LL;                                                     | 
  v6 = 0LL;                                                     | 
  EnterCrit(0LL, 0LL);                                          | 
  v1 = (__m128i *)MmUserProbeAddress;                           | 
  if ( (unsigned __int64)a1 < MmUserProbeAddress )              | 
  {                                                             | 
    v1 = a1;                                                    | 
  }                                                             | 
  v5 = *v1;                                                     | 
  v6 = v1[1].m128i_i64[0];                                      | 
  if ( _mm_cvtsi128_si32(v5) == 24 )                            | 
  {                                                             | 
    if ( (v5.m128i_i32[1] & 0x3FFFFFEC) != 0 )                  | 
    {                                                           | 
      UserSetLastError(1004LL);                                 | 
      v2 = 0;                                                   | 
    }                                                           | 
    else if ( (v5.m128i_i32[1] & 0x40000000) != 0 )             | 
    {                                                           | 
      v4 = QueryTrackMouseEvent(&v5);                           | 
      *a1 = v5;                                                 | 
      a1[1].m128i_i64[0] = v6;                                  | 
      v2 = v4;                                                  | 
    }                                                           | 
    else                                                        | 
    {                                                           | 
      v2 = TrackMouseEvent(&v5);                                | 
    }                                                           | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    UserSetLastError(87LL);                                     | 
    v2 = 0;                                                     | 
  }                                                             | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v2;                                                    | 
}                                                               | 
41 row(s)
