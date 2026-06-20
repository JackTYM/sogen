__int64 __fastcall NtSetIntervalProfile(unsigned int a1, unsigned int a2)                      | 
{                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                | 
                                                                                               | 
  if ( !SeSinglePrivilegeCheck(SeSystemProfilePrivilege, KeGetCurrentThread()->PreviousMode) ) | 
  {                                                                                            | 
    return 3221225569LL;                                                                       | 
  }                                                                                            | 
  KeSetIntervalProfile(a1, a2);                                                                | 
  return 0LL;                                                                                  | 
}                                                                                              | 
11 row(s)
