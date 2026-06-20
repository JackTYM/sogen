__int64 __fastcall NtdllRunOnceInitMuiCrits(PRTL_RUN_ONCE RunOnce, struct _KMUTANT **Parameter, PVOID *Context) | 
{                                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                 | 
                                                                                                                | 
  KiInitializeMutant(*Parameter, 0, 1u, 0);                                                                     | 
  return 1LL;                                                                                                   | 
}                                                                                                               | 
7 row(s)
