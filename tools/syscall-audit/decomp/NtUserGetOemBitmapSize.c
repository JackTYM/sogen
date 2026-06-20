__int64 __fastcall NtUserGetOemBitmapSize(unsigned int a1, _QWORD *a2)    | 
{                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]           | 
                                                                          | 
  EnterCrit(0LL, 0LL);                                                    | 
  v4 = 0;                                                                 | 
  if ( a1 < 0x5D )                                                        | 
  {                                                                       | 
    OemBitmapInfoForCurrentThread = GetOemBitmapInfoForCurrentThread(a1); | 
    v6 = *(__int16 *)(OemBitmapInfoForCurrentThread + 4);                 | 
    v7 = *(__int16 *)(OemBitmapInfoForCurrentThread + 6);                 | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )                     | 
    {                                                                     | 
      a2 = (_QWORD *)MmUserProbeAddress;                                  | 
    }                                                                     | 
    HIDWORD(v9) = *(__int16 *)(OemBitmapInfoForCurrentThread + 6);        | 
    LODWORD(v9) = *(__int16 *)(OemBitmapInfoForCurrentThread + 4);        | 
    *a2 = v9;                                                             | 
    if ( v6 && v7 )                                                       | 
    {                                                                     | 
      v4 = 1;                                                             | 
    }                                                                     | 
  }                                                                       | 
  else                                                                    | 
  {                                                                       | 
    UserSetLastError(87LL);                                               | 
  }                                                                       | 
  UserSessionSwitchLeaveCrit();                                           | 
  return v4;                                                              | 
}                                                                         | 
30 row(s)
