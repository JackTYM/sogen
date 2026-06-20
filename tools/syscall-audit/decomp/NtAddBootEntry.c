__int64 __fastcall NtAddBootEntry(__int64 a1, __int64 a2)       | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  if ( dword_140EFED30 == 2 )                                   | 
  {                                                             | 
    return ExpSetBootEntry(1LL, a1, a2);                        | 
  }                                                             | 
  else                                                          | 
  {                                                             | 
    return 3221225474LL;                                        | 
  }                                                             | 
}                                                               | 
13 row(s)
