__int64 __fastcall NtUserGetKeyboardType(unsigned int a1)       | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v4, 1LL);         | 
  KeyboardType = (int)GetKeyboardType(a1);                      | 
  UserSessionSwitchLeaveCritWithNonPaged();                     | 
  return KeyboardType;                                          | 
}                                                               | 
9 row(s)
