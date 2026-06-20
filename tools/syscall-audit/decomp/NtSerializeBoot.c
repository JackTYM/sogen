__int64 NtSerializeBoot()                                       | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  if ( KeGetCurrentThread()->PreviousMode != 1 )                | 
  {                                                             | 
    return 3221225506LL;                                        | 
  }                                                             | 
  if ( SeSinglePrivilegeCheck(SeTcbPrivilege, 1) )              | 
  {                                                             | 
    return PnpSerializeBoot();                                  | 
  }                                                             | 
  return 3221225569LL;                                          | 
}                                                               | 
14 row(s)
