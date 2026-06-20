_BOOL8 __fastcall NtUserReleaseDC(__int64 a1)                   | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v3, 1LL);         | 
  LODWORD(a1) = ReleaseCacheDC(a1, 0LL);                        | 
  UserSessionSwitchLeaveCritWithNonPaged();                     | 
  return (_DWORD)a1 != 2;                                       | 
}                                                               | 
9 row(s)
