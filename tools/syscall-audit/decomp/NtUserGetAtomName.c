__int64 __fastcall NtUserGetAtomName(unsigned __int16 a1, ULONG64 a2) | 
{                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]       | 
                                                                      | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v8, 1LL);               | 
  if ( a2 >= MmUserProbeAddress )                                     | 
  {                                                                   | 
    a2 = MmUserProbeAddress;                                          | 
  }                                                                   | 
  v6 = *(_DWORD *)a2;                                                 | 
  v7 = *(volatile void **)(a2 + 8);                                   | 
  ProbeForWrite(v7, (unsigned __int16)HIWORD(*(_DWORD *)a2), 2u);     | 
  AtomName = UserGetAtomName(a1, v7, HIWORD(v6) >> 1);                | 
  UserSessionSwitchLeaveCrit();                                       | 
  return AtomName;                                                    | 
}                                                                     | 
16 row(s)
