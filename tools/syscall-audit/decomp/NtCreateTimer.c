__int64 __fastcall NtCreateTimer(__int64 a1, __int64 a2, int a3, unsigned int a4)                    
{                                                                                                    
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                      
                                                                                                     
  v4 = a4;                                                                                           
  v5 = (_QWORD *)a1;                                                                                 
  v14 = 0LL;                                                                                         
  if ( a4 > 1 )                                                                                      
  {                                                                                                  
    return 3221225714LL;                                                                             
  }                                                                                                  
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                 
  if ( PreviousMode )                                                                                
  {                                                                                                  
    a1 = 0x7FFFFFFF0000LL;                                                                           
    if ( (unsigned __int64)v5 < 0x7FFFFFFF0000LL )                                                   
    {                                                                                                
      a1 = (__int64)v5;                                                                              
    }                                                                                                
    *(_QWORD *)a1 = *(_QWORD *)a1;                                                                   
  }                                                                                                  
  LOBYTE(a4) = PreviousMode;                                                                         
  LOBYTE(a1) = PreviousMode;                                                                         
  Object = ObCreateObjectEx(a1, (_DWORD)ExTimerObjectType, a3, a4);                                  
  if ( Object >= 0 )                                                                                 
  {                                                                                                  
    KeInitializeDpc((PRKDPC)0xA0, ExpTimerDpcRoutine, 0LL);                                          
    KeInitializeTimerEx(0LL, v4);                                                                    
    MEMORY[0x40] = 0LL;                                                                              
    MEMORY[0x130] = 0;                                                                               
    MEMORY[0x100] = 0LL;                                                                             
    MEMORY[0x108] = 0LL;                                                                             
    MEMORY[0x118] = 0LL;                                                                             
    if ( PreviousMode )                                                                              
    {                                                                                                
      Process = KeGetCurrentThread()->ApcState.Process;                                              
      if ( (*(_DWORD *)&Process->0 & 0x10) != 0 )                                                    
      {                                                                                              
        v10 = Process[1].Padding[3];                                                                 
        if ( v10 )                                                                                   
        {                                                                                            
          CurrentThread = KeGetCurrentThread();                                                      
          --CurrentThread->SpecialApcDisable;                                                        
          ExAcquireResourceExclusiveLite((PERESOURCE)(v10 + 56), 1u);                                
        }                                                                                            
        PsInsertVirtualizedTimer(Process, 280LL);                                                    
        if ( v10 )                                                                                   
        {                                                                                            
          ExReleaseResourceLite((PERESOURCE)(v10 + 56));                                             
          v12 = KeGetCurrentThread();                                                                
          if ( v12->SpecialApcDisable++ == -1                                                        
            && ($727077A9B6E167EAE1398C74674DC5A5 *)v12->ApcState.ApcListHead[0].Flink != &v12->152 )
          {                                                                                          
            KiCheckForKernelApcDelivery();                                                           
          }                                                                                          
        }                                                                                            
      }                                                                                              
    }                                                                                                
    Object = ObInsertObjectEx(0LL, 0, 0LL, (__int64)&v14);                                           
    if ( Object >= 0 )                                                                               
    {                                                                                                
      *v5 = v14;                                                                                     
    }                                                                                                
  }                                                                                                  
  return (unsigned int)Object;                                                                       
}                                                                                                    
