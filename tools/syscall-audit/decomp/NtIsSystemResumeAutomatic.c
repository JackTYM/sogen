bool NtIsSystemResumeAutomatic()                                | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  return (PopFullWake & 3) == 0;                                | 
}                                                               | 
6 row(s)
