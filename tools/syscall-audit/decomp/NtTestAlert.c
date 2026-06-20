__int64 __fastcall NtTestAlert(__int64 a1)                        | 
{                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]   | 
                                                                  | 
  LOBYTE(a1) = KeGetCurrentThread()->PreviousMode;                | 
  return (unsigned __int8)KeTestAlertThread(a1) != 0 ? 0x101 : 0; | 
}                                                                 | 
7 row(s)
