__int64 __fastcall NtOpenMutant(_QWORD *a1, int a2, int a3)                                        
{                                                                                                  
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                    
                                                                                                   
  v14[0] = 0LL;                                                                                    
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                               
  if ( PreviousMode )                                                                              
  {                                                                                                
    v7 = 0x7FFFFFFF0000LL;                                                                         
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )                                                 
    {                                                                                              
      v7 = (__int64)a1;                                                                            
    }                                                                                              
    *(_QWORD *)v7 = *(_QWORD *)v7;                                                                 
  }                                                                                                
  v8 = (int)ExMutantObjectType;                                                                    
  CurrentSilo = PsGetCurrentSilo();                                                                
  LOBYTE(v10) = PreviousMode;                                                                      
  v11 = ObOpenObjectByNameEx(a3, v8, v10, 0, a2, 0LL, CurrentSilo, (__int64)v14);                  
  if ( v11 == -1073741788 && ExCrossVmMutantObjectType )                                           
  {                                                                                                
    LOBYTE(v12) = PreviousMode;                                                                    
    v11 = ObOpenObjectByName(a3, (_DWORD)ExCrossVmMutantObjectType, v12, 0, a2, 0LL, (__int64)v14);
  }                                                                                                
  if ( v11 >= 0 )                                                                                  
  {                                                                                                
    *a1 = v14[0];                                                                                  
  }                                                                                                
  return (unsigned int)v11;                                                                        
}                                                                                                  
