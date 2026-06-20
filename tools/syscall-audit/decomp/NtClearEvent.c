__int64 __fastcall NtClearEvent(HANDLE Handle)                                                            
{                                                                                                         
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           
                                                                                                          
  Object = 0LL;                                                                                           
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                      
  v3 = ObReferenceObjectByHandle(Handle, 2u, (POBJECT_TYPE)ExEventObjectType, PreviousMode, &Object, 0LL);
  v4 = Object;                                                                                            
  v5 = v3;                                                                                                
  if ( v3 < 0 )                                                                                           
  {                                                                                                       
    if ( v3 == -1073741788 )                                                                              
    {                                                                                                     
      if ( ExCrossVmEventObjectType )                                                                     
      {                                                                                                   
        Object = 0LL;                                                                                     
        v7 = ObReferenceObjectByHandle(Handle, 2u, ExCrossVmEventObjectType, PreviousMode, &Object, 0LL); 
        v4 = Object;                                                                                      
        v5 = v7;                                                                                          
        if ( v7 >= 0 )                                                                                    
        {                                                                                                 
          v5 = ExpResetCrossVmEvent(Object, 0LL);                                                         
        }                                                                                                 
      }                                                                                                   
    }                                                                                                     
  }                                                                                                       
  else                                                                                                    
  {                                                                                                       
    KeResetEvent((PRKEVENT)Object);                                                                       
  }                                                                                                       
  if ( v4 )                                                                                               
  {                                                                                                       
    ObfDereferenceObject(v4);                                                                             
  }                                                                                                       
  return v5;                                                                                              
}                                                                                                         
