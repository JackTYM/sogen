__int64 __fastcall NtSubscribeWnfStateChange(__int64 a1, unsigned int a2, int a3, _QWORD *a4)                          | 
{                                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                        | 
                                                                                                                       | 
  v5 = a2;                                                                                                             | 
  CurrentThread = KeGetCurrentThread();                                                                                | 
  --CurrentThread->KernelApcDisable;                                                                                   | 
  v14 = 0LL;                                                                                                           | 
  if ( a4 )                                                                                                            | 
  {                                                                                                                    | 
    v8 = (unsigned __int64)a4;                                                                                         | 
    v9 = (unsigned __int64)a4 + 7;                                                                                     | 
    if ( a4 >= (_QWORD *)((char *)a4 + 7) || v9 >= 0x7FFFFFFF0000LL )                                                  | 
    {                                                                                                                  | 
      ExRaiseAccessViolation();                                                                                        | 
    }                                                                                                                  | 
    v10 = (v9 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                         | 
    do                                                                                                                 | 
    {                                                                                                                  | 
      *(_BYTE *)v8 = *(_BYTE *)v8;                                                                                     | 
      v8 = (v8 & 0xFFFFFFFFFFFFF000uLL) + 4096;                                                                        | 
    }                                                                                                                  | 
    while ( v8 != v10 );                                                                                               | 
    *a4 = 0LL;                                                                                                         | 
  }                                                                                                                    | 
  v13 = 1;                                                                                                             | 
  v11 = ExpWnfSubscribeWnfStateChange((unsigned __int64)&v14 & -(__int64)(a4 != 0LL), 0LL, a1, v5, 0LL, 0LL, a3, v13); | 
  if ( v11 >= 0 && a4 )                                                                                                | 
  {                                                                                                                    | 
    *a4 = v14;                                                                                                         | 
  }                                                                                                                    | 
  KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                                   | 
  return (unsigned int)v11;                                                                                            | 
}                                                                                                                      | 
34 row(s)
