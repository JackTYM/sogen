__int64 __fastcall NtUserGetCursorPos(struct tagPOINT *a1, unsigned int a2) | 
{                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]             | 
                                                                            | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v6, 1LL);                     | 
  CursorPos = (int)xxxGetCursorPos(a1, a2);                                 | 
  UserSessionSwitchLeaveCrit();                                             | 
  return CursorPos;                                                         | 
}                                                                           | 
9 row(s)
