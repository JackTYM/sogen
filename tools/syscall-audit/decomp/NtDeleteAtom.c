__int64 __fastcall NtDeleteAtom(unsigned __int16 a1)                                                                            | 
{                                                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                 | 
                                                                                                                                | 
  v5 = 0LL;                                                                                                                     | 
  v6 = 0LL;                                                                                                                     | 
  if ( (unsigned int)((__int64 (__fastcall *)(__int64, __int128 *, _QWORD, _QWORD, _QWORD, _DWORD))PspUpdateCalloutParameters)( | 
                       2LL,                                                                                                     | 
                       &v5,                                                                                                     | 
                       0LL,                                                                                                     | 
                       0LL,                                                                                                     | 
                       0LL,                                                                                                     | 
                       0) )                                                                                                     | 
  {                                                                                                                             | 
    Win32Callouts = PsSessionGetWin32Callouts();                                                                                | 
    ExCallCallBack(Win32Callouts, v3, &v5);                                                                                     | 
  }                                                                                                                             | 
  if ( v6 )                                                                                                                     | 
  {                                                                                                                             | 
    return RtlDeleteAtomFromAtomTable(v6, a1);                                                                                  | 
  }                                                                                                                             | 
  else                                                                                                                          | 
  {                                                                                                                             | 
    return 3221225506LL;                                                                                                        | 
  }                                                                                                                             | 
}                                                                                                                               | 
26 row(s)
