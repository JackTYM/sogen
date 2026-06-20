__int64 __fastcall NtSetTimer2(void *a1, __int64 a2)           
{                                                              
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
                                                               
  if ( a2 )                                                    
  {                                                            
    return ExpSetTimer2(a1);                                   
  }                                                            
  else                                                         
  {                                                            
    return 3221225712LL;                                       
  }                                                            
}                                                              
