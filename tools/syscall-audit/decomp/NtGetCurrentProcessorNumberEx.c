__int64 __fastcall NtGetCurrentProcessorNumberEx(_BYTE *a1)     | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  if ( KeGetCurrentThread()->PreviousMode )                     | 
  {                                                             | 
    ProbeForWrite(a1, 4uLL, 1u);                                | 
  }                                                             | 
  CurrentPrcb = KeGetCurrentPrcb();                             | 
  *(_WORD *)a1 = CurrentPrcb->Group;                            | 
  a1[2] = CurrentPrcb->GroupIndex;                              | 
  a1[3] = 0;                                                    | 
  return 0LL;                                                   | 
}                                                               | 
