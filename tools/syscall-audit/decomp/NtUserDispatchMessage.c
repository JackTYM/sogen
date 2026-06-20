__int64 __fastcall NtUserDispatchMessage(__int128 *a1)                                        | 
{                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                               | 
                                                                                              | 
  v7 = 0LL;                                                                                   | 
  v8 = 0LL;                                                                                   | 
  v9 = 0LL;                                                                                   | 
  EnterCrit(0LL, 0LL);                                                                        | 
  if ( (unsigned __int64)a1 >= MmUserProbeAddress )                                           | 
  {                                                                                           | 
    a1 = (__int128 *)MmUserProbeAddress;                                                      | 
  }                                                                                           | 
  v7 = *a1;                                                                                   | 
  v8 = a1[1];                                                                                 | 
  v9 = a1[2];                                                                                 | 
  v2 = 0LL;                                                                                   | 
  if ( (DWORD2(v7) & 0xFFFE0000) != 0 )                                                       | 
  {                                                                                           | 
    v3 = 87LL;                                                                                | 
  }                                                                                           | 
  else                                                                                        | 
  {                                                                                           | 
    if ( (!(_QWORD)v7                                                                         | 
       || (v4 = ValidateHwnd()) == 0                                                          | 
       || (*(_BYTE *)(*(_QWORD *)(v4 + 40) + 18LL) & 4) == 0                                  | 
       || *(_QWORD *)(*(_QWORD *)(v4 + 16) + 464LL) == *((_QWORD *)PtiCurrent() + 58))        | 
      && (DWORD2(v7) >= 0x400 || !(((unsigned __int16)MessageTable[DWORD2(v7)] >> 13) & 1)) ) | 
    {                                                                                         | 
      v2 = xxxDispatchMessage((struct tagMSG *)&v7);                                          | 
      goto LABEL_16;                                                                          | 
    }                                                                                         | 
    v3 = 5LL;                                                                                 | 
  }                                                                                           | 
  UserSetLastError(v3);                                                                       | 
LABEL_16:                                                                                     | 
  UserSessionSwitchLeaveCrit();                                                               | 
  return v2;                                                                                  | 
}                                                                                             | 
38 row(s)
