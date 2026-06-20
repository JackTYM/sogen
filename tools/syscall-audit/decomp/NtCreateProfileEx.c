__int64 __fastcall NtCreateProfileEx(                                      | 
        int a1,                                                            | 
        int a2,                                                            | 
        int a3,                                                            | 
        int a4,                                                            | 
        int a5,                                                            | 
        __int64 a6,                                                        | 
        SIZE_T a7,                                                         | 
        int a8,                                                            | 
        __int16 a9,                                                        | 
        __int64 a10)                                                       | 
{                                                                          | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]            | 
                                                                           | 
  LODWORD(Length) = a7;                                                    | 
  return ExpProfileCreate(a1, a2, a3, a4, a5, a6, Length, a8, a9, a10, 0); | 
}                                                                          | 
17 row(s)
