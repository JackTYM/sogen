__int64 __fastcall NtGdiGetRandomRgn(HDC a1, __int64 a2, int a3) | 
{                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]  | 
                                                                 | 
  if ( a3 == 5 )                                                 | 
  {                                                              | 
    return 0LL;                                                  | 
  }                                                              | 
  if ( a3 == 4 )                                                 | 
  {                                                              | 
    v4 = 1;                                                      | 
    UserEnterUserCritSecShared();                                | 
  }                                                              | 
  else                                                           | 
  {                                                              | 
    v4 = 0;                                                      | 
  }                                                              | 
  RandomRgn = GreGetRandomRgn(a1);                               | 
  if ( v4 )                                                      | 
  {                                                              | 
    UserLeaveUserCritSec();                                      | 
  }                                                              | 
  return RandomRgn;                                              | 
}                                                                | 
24 row(s)
