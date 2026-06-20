__int64 __fastcall NtUserGetRawInputData(struct HRAWINPUT__ *a1, unsigned int a2, void *a3, unsigned int *a4, int a5) | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  if ( a5 == 24 )                                                                                                     | 
  {                                                                                                                   | 
    return GetRawInputData_NoUserCrit(a1, a2, a3, a4);                                                                | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v5 = -1;                                                                                                          | 
    UserSetLastError(87LL);                                                                                           | 
  }                                                                                                                   | 
  return v5;                                                                                                          | 
}                                                                                                                     | 
15 row(s)
