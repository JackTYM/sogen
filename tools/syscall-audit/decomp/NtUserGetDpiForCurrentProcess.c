__int64 NtUserGetDpiForCurrentProcess()                                                     | 
{                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                             | 
                                                                                            | 
  CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                           | 
  if ( CurrentProcessWin32Process )                                                         | 
  {                                                                                         | 
    CurrentProcessWin32Process &= -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL); | 
  }                                                                                         | 
  return *(unsigned __int16 *)(CurrentProcessWin32Process + 272);                           | 
}                                                                                           | 
11 row(s)
