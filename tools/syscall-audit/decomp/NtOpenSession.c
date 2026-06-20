__int64 __fastcall NtOpenSession(_QWORD *a1, int a2, int a3)                         | 
{                                                                                    | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                      | 
                                                                                     | 
  v12[0] = 0LL;                                                                      | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                 | 
  if ( PreviousMode )                                                                | 
  {                                                                                  | 
    v7 = 0x7FFFFFFF0000LL;                                                           | 
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                   | 
    {                                                                                | 
      v7 = (__int64)a1;                                                              | 
    }                                                                                | 
    *(_QWORD *)v7 = *(_QWORD *)v7;                                                   | 
  }                                                                                  | 
  v8 = (int)MmSessionObjectType;                                                     | 
  CurrentSilo = PsGetCurrentSilo();                                                  | 
  LOBYTE(v10) = PreviousMode;                                                        | 
  result = ObOpenObjectByNameEx(a3, v8, v10, 0, a2, 0LL, CurrentSilo, (__int64)v12); | 
  *a1 = v12[0];                                                                      | 
  return result;                                                                     | 
}                                                                                    | 
22 row(s)
