__int64 __fastcall NtWaitForAlertByThreadId(__int64 a1, __int64 *a2)  | 
{                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]       | 
                                                                      | 
  v6 = 0LL;                                                           | 
  PreviousMode = (unsigned __int8)KeGetCurrentThread()->PreviousMode; | 
  if ( a2 && (_BYTE)PreviousMode )                                    | 
  {                                                                   | 
    v5 = 0x7FFFFFFF0000LL;                                            | 
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                    | 
    {                                                                 | 
      v5 = (__int64)a2;                                               | 
    }                                                                 | 
    v6 = *(_QWORD *)v5;                                               | 
    a2 = &v6;                                                         | 
  }                                                                   | 
  return KeWaitForAlertByThreadId(PreviousMode, a2, a1);              | 
}                                                                     | 
