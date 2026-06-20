__int64 NtGetCurrentProcessorNumber()                            | 
{                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]  | 
                                                                 | 
  CurrentThread = KeGetCurrentThread();                          | 
  v6 = 0LL;                                                      | 
  GroupIndex = KeGetCurrentPrcb()->GroupIndex;                   | 
  KeQueryPrimaryGroupAffinityThread(CurrentThread, &v6);         | 
  v2 = v6;                                                       | 
  if ( !_bittest64(&v2, GroupIndex) )                            | 
  {                                                              | 
    LODWORD(GroupIndex) = KeFindBiasedSetMember(v6, GroupIndex); | 
  }                                                              | 
  v3 = IoThreadToProcess(CurrentThread);                         | 
  if ( v3[1].ReadyTime )                                         | 
  {                                                              | 
    v5 = WORD2(v3[3].PerProcessorCycleTimes);                    | 
    if ( v5 == 332 || v5 == 452 )                                | 
    {                                                            | 
      LODWORD(GroupIndex) = GroupIndex & 0x1F;                   | 
    }                                                            | 
  }                                                              | 
  return (unsigned int)GroupIndex;                               | 
}                                                                | 
24 row(s)
