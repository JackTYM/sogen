__int64 NtFlushProcessWriteBuffers()                            | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  KeFlushProcessWriteBuffers(0LL);                              | 
  return 0LL;                                                   | 
}                                                               | 
