__int64 __fastcall NtUnsubscribeWnfStateChange(void *a1)                     | 
{                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]              | 
                                                                             | 
  P = 0LL;                                                                   | 
  CurrentThread = KeGetCurrentThread();                                      | 
  --CurrentThread->KernelApcDisable;                                         | 
  v2 = ExpCaptureWnfStateName(a1);                                           | 
  if ( v2 >= 0 )                                                             | 
  {                                                                          | 
    v3 = *(_QWORD *)&KeGetCurrentThread()->ApcState.Process[3].BasePriority; | 
    if ( v3 )                                                                | 
    {                                                                        | 
      v2 = ExpWnfAcquireSubscriptionByName(0LL, v3, &P);                     | 
      if ( v2 >= 0 )                                                         | 
      {                                                                      | 
        ExpWnfDeleteSubscription(P);                                         | 
        v2 = 0;                                                              | 
      }                                                                      | 
    }                                                                        | 
    else                                                                     | 
    {                                                                        | 
      v2 = -1073741772;                                                      | 
    }                                                                        | 
  }                                                                          | 
  KeLeaveCriticalRegion();                                                   | 
  return (unsigned int)v2;                                                   | 
}                                                                            | 
28 row(s)
