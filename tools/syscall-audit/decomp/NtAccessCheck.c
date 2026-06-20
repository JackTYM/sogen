__int64 __fastcall NtAccessCheck(                                              | 
        __int64 a1,                                                            | 
        __int64 a2,                                                            | 
        unsigned int a3,                                                       | 
        __int64 a4,                                                            | 
        __int64 a5,                                                            | 
        __int64 a6,                                                            | 
        __int64 a7,                                                            | 
        __int64 a8)                                                            | 
{                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                | 
                                                                               | 
  v9 = 0;                                                                      | 
  return SeAccessCheckByType(a1, 0LL, a2, a3, 0LL, 0, a4, a5, a6, a7, a8, v9); | 
}                                                                              | 
15 row(s)
