__int64 NtUserGetCursor()                                       | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v2, 1LL);         | 
  v0 = 0LL;                                                     | 
  if ( *(_QWORD *)(*((_QWORD *)PtiCurrent() + 59) + 496LL) )    | 
  {                                                             | 
    v0 = **(_QWORD **)(*((_QWORD *)PtiCurrent() + 59) + 496LL); | 
  }                                                             | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v0;                                                    | 
}                                                               | 
13 row(s)
