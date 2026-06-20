__int64 NtUserTraceLoggingSendMixedModeTelemetry()              | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  EnterCrit(0LL, 0LL);                                          | 
  v0 = (int)TraceLoggingSendMixedModeTelemetry();               | 
  UserSessionSwitchLeaveCrit();                                 | 
  return v0;                                                    | 
}                                                               | 
9 row(s)
