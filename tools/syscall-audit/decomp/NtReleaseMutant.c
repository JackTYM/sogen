__int64 __fastcall NtReleaseMutant(HANDLE Handle, _DWORD *a2)                                            
{                                                                                                        
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                          
                                                                                                         
  v11 = 0;                                                                                               
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                     
  if ( a2 && PreviousMode )                                                                              
  {                                                                                                      
    v9 = 0x7FFFFFFF0000LL;                                                                               
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                                       
    {                                                                                                    
      v9 = (__int64)a2;                                                                                  
    }                                                                                                    
    *(_DWORD *)v9 = *(_DWORD *)v9;                                                                       
  }                                                                                                      
  Object = 0LL;                                                                                          
  v5 = ObReferenceObjectByHandle(Handle, 0, ExMutantObjectType, PreviousMode, &Object, 0LL);             
  v6 = v5;                                                                                               
  v7 = Object;                                                                                           
  v12 = Object;                                                                                          
  LODWORD(Object) = v5;                                                                                  
  if ( v5 < 0 )                                                                                          
  {                                                                                                      
    if ( v5 == -1073741788 )                                                                             
    {                                                                                                    
      if ( ExCrossVmMutantObjectType )                                                                   
      {                                                                                                  
        Object = 0LL;                                                                                    
        v6 = ObReferenceObjectByHandle(Handle, 0, ExCrossVmMutantObjectType, PreviousMode, &Object, 0LL);
        v7 = Object;                                                                                     
        v12 = Object;                                                                                    
        LODWORD(Object) = v6;                                                                            
        if ( v6 >= 0 )                                                                                   
        {                                                                                                
          v6 = ExpReleaseCrossVmMutant(v7, &v11);                                                        
          LODWORD(Object) = v6;                                                                          
        }                                                                                                
      }                                                                                                  
    }                                                                                                    
  }                                                                                                      
  else                                                                                                   
  {                                                                                                      
    v6 = KeReleaseMutantEx((ULONG_PTR)v7);                                                               
    LODWORD(Object) = v6;                                                                                
    if ( v6 == 128 || v6 == -1073741754 )                                                                
    {                                                                                                    
LABEL_4:                                                                                                 
      ObfDereferenceObject(v7);                                                                          
      return (unsigned int)v6;                                                                           
    }                                                                                                    
  }                                                                                                      
  if ( v6 >= 0 && a2 )                                                                                   
  {                                                                                                      
    *a2 = v11;                                                                                           
  }                                                                                                      
  if ( v7 )                                                                                              
  {                                                                                                      
    goto LABEL_4;                                                                                        
  }                                                                                                      
  return (unsigned int)v6;                                                                               
}                                                                                                        
