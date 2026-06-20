__int64 __fastcall NtModifyBootEntry(__int64 a1)                | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  if ( dword_140EFED30 == 2 )                                   | 
  {                                                             | 
    return ExpSetBootEntry(0LL, a1, 0LL);                       | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    return 3221225474LL;                                        | 
  }                                                             | 
}                                                               | 
13 row(s)
