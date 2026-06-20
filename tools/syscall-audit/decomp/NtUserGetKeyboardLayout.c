__int64 __fastcall NtUserGetKeyboardLayout(unsigned int a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  EnterLeaveCritShared::EnterLeaveCritShared(&v4, 1LL);
  KeyboardLayout = GetKeyboardLayout(a1);
  UserSessionSwitchLeaveCritWithNonPaged();
  return KeyboardLayout;
}
