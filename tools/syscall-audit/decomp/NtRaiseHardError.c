__int64 __fastcall NtRaiseHardError(__int64 a1, unsigned int a2, int a3, const void *a4, unsigned int a5, _DWORD *a6)
{                                                                                                                    
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                      
                                                                                                                     
  v7 = a2;                                                                                                           
  v8 = a1;                                                                                                           
  v15 = 0;                                                                                                           
  if ( a2 > 5 )                                                                                                      
  {                                                                                                                  
    return 3221225712LL;                                                                                             
  }                                                                                                                  
  if ( !a4 )                                                                                                         
  {                                                                                                                  
    if ( !a2 )                                                                                                       
    {                                                                                                                
      goto LABEL_4;                                                                                                  
    }                                                                                                                
    return 3221225712LL;                                                                                             
  }                                                                                                                  
  if ( !a2 )                                                                                                         
  {                                                                                                                  
    return 3221225712LL;                                                                                             
  }                                                                                                                  
LABEL_4:                                                                                                             
  if ( !KeGetCurrentThread()->PreviousMode )                                                                         
  {                                                                                                                  
    v13 = ExRaiseHardError(a1, a2);                                                                                  
    *a6 = v15;                                                                                                       
    return v13;                                                                                                      
  }                                                                                                                  
  if ( a5 <= 6 || a5 - 7 <= 1 )                                                                                      
  {                                                                                                                  
    v9 = 0x7FFFFFFF0000LL;                                                                                           
    if ( (unsigned __int64)a6 < 0x7FFFFFFF0000LL )                                                                   
    {                                                                                                                
      v9 = (__int64)a6;                                                                                              
    }                                                                                                                
    *(_DWORD *)v9 = *(_DWORD *)v9;                                                                                   
    if ( a4 )                                                                                                        
    {                                                                                                                
      if ( 8LL * a2 && ((unsigned __int8)a4 & 7) != 0 )                                                              
      {                                                                                                              
        ExRaiseDatatypeMisalignment();                                                                               
      }                                                                                                              
      memmove(Src, a4, 8LL * a2);                                                                                    
      memmove(v19, Src, 8 * v7);                                                                                     
      if ( a3 )                                                                                                      
      {                                                                                                              
        for ( i = 0; ; ++i )                                                                                         
        {                                                                                                            
          v16 = i;                                                                                                   
          if ( i >= (unsigned int)v7 )                                                                               
          {                                                                                                          
            break;                                                                                                   
          }                                                                                                          
          if ( _bittest(&a3, i) )                                                                                    
          {                                                                                                          
            v11 = (_OWORD *)Src[i];                                                                                  
            if ( ((unsigned __int8)v11 & 7) != 0 )                                                                   
            {                                                                                                        
              ExRaiseDatatypeMisalignment();                                                                         
            }                                                                                                        
            v12 = &v19[8 * i + 20];                                                                                  
            *v12 = *v11;                                                                                             
            *(_QWORD *)&v19[4 * i] = v12;                                                                            
            i = v16;                                                                                                 
          }                                                                                                          
        }                                                                                                            
      }                                                                                                              
    }                                                                                                                
    v13 = ExpRaiseHardError(v8, v7, a3, (unsigned int)Src, (__int64)v19, a5, (__int64)&v15);                         
    v17 = v13;                                                                                                       
    *a6 = v15;                                                                                                       
    return v13;                                                                                                      
  }                                                                                                                  
  return 3221225714LL;                                                                                               
}                                                                                                                    
