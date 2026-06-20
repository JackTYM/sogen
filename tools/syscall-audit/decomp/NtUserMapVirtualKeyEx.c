__int64 __fastcall NtUserMapVirtualKeyEx(unsigned int a1, unsigned int a2, __int64 a3, int a4) | 
{                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                | 
                                                                                               | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v12, 1LL);                                       | 
  v8 = 0;                                                                                      | 
  v9 = PtiCurrent();                                                                           | 
  if ( a4 )                                                                                    | 
  {                                                                                            | 
    v10 = HKLtoPKL(v9, a3);                                                                    | 
  }                                                                                            | 
  else                                                                                         | 
  {                                                                                            | 
    v10 = *((_QWORD *)v9 + 60);                                                                | 
  }                                                                                            | 
  if ( v10 )                                                                                   | 
  {                                                                                            | 
    v8 = InternalMapVirtualKeyEx(a1, a2, *(_QWORD *)(*(_QWORD *)(v10 + 48) + 32LL));           | 
  }                                                                                            | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                    | 
  return v8;                                                                                   | 
}                                                                                              | 
22 row(s)
