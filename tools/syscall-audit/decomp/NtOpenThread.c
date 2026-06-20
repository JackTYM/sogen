__int64 __fastcall NtOpenThread(int a1, int a2, int a3, int a4)    | 
{                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]    | 
                                                                   | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;               | 
  return PsOpenThread(a1, a2, a3, a4, PreviousMode, PreviousMode); | 
}                                                                  | 
