__int64 __fastcall NtWaitForSingleObject(int a1, unsigned __int8 a2, LARGE_INTEGER *a3)  | 
{                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                          | 
                                                                                         | 
  v3 = a2;                                                                               | 
  v7 = 0LL;                                                                              | 
  PreviousMode = (unsigned __int8)KeGetCurrentThread()->PreviousMode;                    | 
  if ( a3 && (_BYTE)PreviousMode )                                                       | 
  {                                                                                      | 
    v5 = 0x7FFFFFFF0000LL;                                                               | 
    if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )                                       | 
    {                                                                                    | 
      v5 = (__int64)a3;                                                                  | 
    }                                                                                    | 
    v7 = *(_QWORD *)v5;                                                                  | 
    a3 = (LARGE_INTEGER *)&v7;                                                           | 
  }                                                                                      | 
  return ObWaitForSingleObject(a1, PreviousMode, (unsigned __int8)PreviousMode, v3, a3); | 
}                                                                                        | 
