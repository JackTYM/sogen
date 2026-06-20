__int64 __fastcall NtCreateTimer2(__int64 *a1, __int64 a2, __int64 a3, unsigned int a4)                         
{                                                                                                               
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                 
                                                                                                                
  v17 = 0LL;                                                                                                    
  Object = 0LL;                                                                                                 
  if ( !(unsigned __int8)ExpExTimerAttributesAreValid(a4) )                                                     
  {                                                                                                             
    return 3221225714LL;                                                                                        
  }                                                                                                             
  if ( v10 )                                                                                                    
  {                                                                                                             
    return 3221225713LL;                                                                                        
  }                                                                                                             
  if ( v9 && (a4 & 2) == 0 )                                                                                    
  {                                                                                                             
    return 3221225712LL;                                                                                        
  }                                                                                                             
  v15 = 0;                                                                                                      
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                            
  if ( PreviousMode )                                                                                           
  {                                                                                                             
    v6 = 0x7FFFFFFF0000LL;                                                                                      
    v7 = 0x7FFFFFFF0000LL;                                                                                      
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                                              
    {                                                                                                           
      v7 = (__int64)a1;                                                                                         
    }                                                                                                           
    *(_QWORD *)v7 = *(_QWORD *)v7;                                                                              
    if ( v9 )                                                                                                   
    {                                                                                                           
      if ( (unsigned __int64)v9 < 0x7FFFFFFF0000LL )                                                            
      {                                                                                                         
        v6 = (__int64)v9;                                                                                       
      }                                                                                                         
      RtlCopyVolatileMemory(&v15, (const void *)v6, 4uLL);                                                      
    }                                                                                                           
  }                                                                                                             
  else if ( v9 )                                                                                                
  {                                                                                                             
    v15 = *v9;                                                                                                  
  }                                                                                                             
  if ( (a4 & 2) == 0 || (LOBYTE(v6) = PreviousMode, result = ExpCheckIRTimerAccess(v15, v6), (int)result >= 0) )
  {                                                                                                             
    LOBYTE(v8) = PreviousMode;                                                                                  
    LOBYTE(v7) = PreviousMode;                                                                                  
    inserted = ObCreateObjectEx(v7, (_DWORD)ExpIRTimerObjectType, 0, v8);                                       
    if ( inserted >= 0 )                                                                                        
    {                                                                                                           
      v13 = Object;                                                                                             
      if ( (a4 & 2) != 0 )                                                                                      
      {                                                                                                         
        KeInitializeIRTimer((_DWORD)Object, 0, 0, (unsigned int)&v15, a4);                                      
      }                                                                                                         
      else                                                                                                      
      {                                                                                                         
        KeInitializeTimer2(Object, 0LL, 0LL, a4);                                                               
      }                                                                                                         
      v13[17] = 0LL;                                                                                            
      *((_DWORD *)v13 + 40) = a4;                                                                               
      inserted = ObInsertObjectEx(v13, 0, 0LL, (__int64)&v17);                                                  
      LODWORD(Object) = inserted;                                                                               
      if ( inserted >= 0 )                                                                                      
      {                                                                                                         
        *a1 = v17;                                                                                              
      }                                                                                                         
    }                                                                                                           
    return (unsigned int)inserted;                                                                              
  }                                                                                                             
  return result;                                                                                                
}                                                                                                               
