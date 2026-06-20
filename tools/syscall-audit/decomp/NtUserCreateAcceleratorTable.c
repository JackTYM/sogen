__int64 __fastcall NtUserCreateAcceleratorTable(volatile void *a1, int a2)            | 
{                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                       | 
                                                                                      | 
  v2 = a2;                                                                            | 
  EnterCrit(0LL, 0LL);                                                                | 
  if ( (unsigned int)(v2 - 1) > 0x7FFE )                                              | 
  {                                                                                   | 
    v5 = 0LL;                                                                         | 
    UserSetLastError(87LL);                                                           | 
  }                                                                                   | 
  else                                                                                | 
  {                                                                                   | 
    if ( v2 > 0x2AAAAAAAAAAAAAAALL )                                                  | 
    {                                                                                 | 
      ExRaiseAccessViolation();                                                       | 
    }                                                                                 | 
    CurrentProcessWow64Process = PsGetCurrentProcessWow64Process();                   | 
    ProbeForRead(a1, 6 * v2, 2 - (CurrentProcessWow64Process != 0));                  | 
    AcceleratorTable = (__int64 *)CreateAcceleratorTable(a1, (unsigned int)(6 * v2)); | 
    v5 = 0LL;                                                                         | 
    if ( AcceleratorTable )                                                           | 
    {                                                                                 | 
      v5 = *AcceleratorTable;                                                         | 
    }                                                                                 | 
  }                                                                                   | 
  UserSessionSwitchLeaveCrit();                                                       | 
  return v5;                                                                          | 
}                                                                                     | 
29 row(s)
