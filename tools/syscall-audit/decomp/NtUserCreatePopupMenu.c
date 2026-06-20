__int64 NtUserCreatePopupMenu()                                 | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  Menu = InternalCreateMenu(1);                                 | 
  v1 = 0LL;                                                     | 
  if ( Menu )                                                   | 
  {                                                             | 
    v1 = *(_QWORD *)Menu;                                       | 
  }                                                             | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v1;                                                    | 
}                                                               | 
14 row(s)
