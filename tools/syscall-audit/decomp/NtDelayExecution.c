NTSTATUS __fastcall NtDelayExecution(BOOLEAN a1, LARGE_INTEGER *a2) | 
{                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]     | 
                                                                    | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                | 
  if ( PreviousMode )                                               | 
  {                                                                 | 
    if ( ((unsigned __int8)a2 & 3) != 0 )                           | 
    {                                                               | 
      ExRaiseDatatypeMisalignment();                                | 
    }                                                               | 
    Interval = *a2;                                                 | 
  }                                                                 | 
  else                                                              | 
  {                                                                 | 
    Interval = *a2;                                                 | 
  }                                                                 | 
  return KeDelayExecutionThread(PreviousMode, a1, &Interval);       | 
}                                                                   | 
