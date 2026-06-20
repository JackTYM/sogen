__int64 __fastcall NtCreatePartition(int a1, int a2, int a3, int a4)               | 
{                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                    | 
                                                                                   | 
  return PsCreatePartition(a1, a2, a3, a4, KeGetCurrentThread()->PreviousMode, 0); | 
}                                                                                  | 
6 row(s)
