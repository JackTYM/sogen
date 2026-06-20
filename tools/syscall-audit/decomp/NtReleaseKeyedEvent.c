NTSTATUS __fastcall NtReleaseKeyedEvent(HANDLE Handle, struct _LIST_ENTRY *a2, BOOLEAN a3, unsigned __int64 a4)
{                                                                                                              
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                
                                                                                                               
  Timeout = (LARGE_INTEGER *)a4;                                                                               
  v6 = 0;                                                                                                      
  v28 = 0LL;                                                                                                   
  InitialStack = 0LL;                                                                                          
  v7 = 1;                                                                                                      
  if ( ((unsigned __int8)a2 & 1) != 0 )                                                                        
  {                                                                                                            
    return -1073741585;                                                                                        
  }                                                                                                            
  CurrentThread = KeGetCurrentThread();                                                                        
  PreviousMode = CurrentThread->PreviousMode;                                                                  
  WaitMode = PreviousMode;                                                                                     
  v10 = (__int64 *)a4;                                                                                         
  if ( a4 )                                                                                                    
  {                                                                                                            
    if ( PreviousMode )                                                                                        
    {                                                                                                          
      if ( a4 + 8 > 0x7FFFFFFF0000LL || a4 + 8 < a4 )                                                          
      {                                                                                                        
        v10 = (__int64 *)a4;                                                                                   
      }                                                                                                        
      PreviousMode = CurrentThread->PreviousMode;                                                              
    }                                                                                                          
    v28 = *v10;                                                                                                
    Timeout = (LARGE_INTEGER *)&v28;                                                                           
  }                                                                                                            
  if ( Handle )                                                                                                
  {                                                                                                            
    Object = 0LL;                                                                                              
    result = ObReferenceObjectByHandle(Handle, 2u, ExpKeyedEventObjectType, PreviousMode, &Object, 0LL);       
    v6 = result;                                                                                               
    v12 = (char *)Object;                                                                                      
    v31 = Object;                                                                                              
    if ( result < 0 )                                                                                          
    {                                                                                                          
      return result;                                                                                           
    }                                                                                                          
  }                                                                                                            
  else                                                                                                         
  {                                                                                                            
    v12 = (char *)ExpCritSecOutOfMemoryEvent;                                                                  
    v31 = (PVOID)ExpCritSecOutOfMemoryEvent;                                                                   
  }                                                                                                            
  *((_DWORD *)&CurrentThread[1].SwapListEntry + 3) |= 0x20u;                                                   
  Process = CurrentThread->ApcState.Process;                                                                   
  v13 = (volatile signed __int32 *)&v12[24 * (((unsigned __int64)a2 >> 5) & 0x3F)];                            
  v14 = (struct _LIST_ENTRY *)(v13 + 2);                                                                       
  --CurrentThread->KernelApcDisable;                                                                           
  v15 = KeAbPreAcquire(v13, 0LL, 0LL);                                                                         
  v16 = v15;                                                                                                   
  if ( _interlockedbittestandset64(v13, 0LL) )                                                                 
  {                                                                                                            
    ExfAcquirePushLockExclusiveEx(v13, v15, v13);                                                              
  }                                                                                                            
  if ( v16 )                                                                                                   
  {                                                                                                            
    *(_BYTE *)(v16 + 10) = 1;                                                                                  
  }                                                                                                            
  for ( i = v14->Flink; ; i = i->Flink )                                                                       
  {                                                                                                            
    if ( i == v14 )                                                                                            
    {                                                                                                          
      InitialStack = CurrentThread[1].InitialStack;                                                            
      CurrentThread[1].InitialStack = (void *)((unsigned __int64)a2 | 1);                                      
      p_WaitListHead = &CurrentThread[1].Header.WaitListHead;                                                  
      Flink = v14->Flink;                                                                                      
      if ( v14->Flink->Blink != v14 )                                                                          
      {                                                                                                        
        goto LABEL_15;                                                                                         
      }                                                                                                        
      p_WaitListHead->Flink = Flink;                                                                           
      CurrentThread[1].Header.WaitListHead.Blink = v14;                                                        
      Flink->Blink = p_WaitListHead;                                                                           
      v14->Flink = p_WaitListHead;                                                                             
      p_Blink = 0LL;                                                                                           
      goto LABEL_21;                                                                                           
    }                                                                                                          
    p_Blink = &i[-77].Blink;                                                                                   
    if ( i[2].Flink == a2 && p_Blink[68] == (struct _LIST_ENTRY *)Process )                                    
    {                                                                                                          
      break;                                                                                                   
    }                                                                                                          
  }                                                                                                            
  v19 = i->Flink;                                                                                              
  Blink = i->Blink;                                                                                            
  if ( i->Flink->Blink != i || Blink->Flink != i )                                                             
  {                                                                                                            
LABEL_15:                                                                                                      
    __fastfail(3u);                                                                                            
  }                                                                                                            
  Blink->Flink = v19;                                                                                          
  v19->Blink = Blink;                                                                                          
  i->Blink = i;                                                                                                
  i->Flink = i;                                                                                                
LABEL_21:                                                                                                      
  if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v13, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )           
  {                                                                                                            
    ExfTryToWakePushLock(v13);                                                                                 
  }                                                                                                            
  KeAbPostRelease((ULONG_PTR)v13);                                                                             
  if ( p_Blink )                                                                                               
  {                                                                                                            
    KeReleaseSemaphore((PRKSEMAPHORE)(p_Blink + 163), 1, 1, 0);                                                
    KiLeaveCriticalRegionUnsafe(CurrentThread);                                                                
  }                                                                                                            
  else                                                                                                         
  {                                                                                                            
    KiLeaveCriticalRegionUnsafe(CurrentThread);                                                                
    v6 = KeWaitForSingleObject(&CurrentThread[1].KernelStack, WrKeyedEvent, WaitMode, a3, Timeout);            
    if ( v6 )                                                                                                  
    {                                                                                                          
      --CurrentThread->KernelApcDisable;                                                                       
      v23 = KeAbPreAcquire(v13, 0LL, 0LL);                                                                     
      v24 = v23;                                                                                               
      if ( _interlockedbittestandset64(v13, 0LL) )                                                             
      {                                                                                                        
        ExfAcquirePushLockExclusiveEx(v13, v23, v13);                                                          
      }                                                                                                        
      if ( v24 )                                                                                               
      {                                                                                                        
        *(_BYTE *)(v24 + 10) = 1;                                                                              
      }                                                                                                        
      v25 = &CurrentThread[1].Header.WaitListHead;                                                             
      v26 = (struct _KTHREAD *)CurrentThread[1].Header.WaitListHead.Flink;                                     
      if ( v26 != (struct _KTHREAD *)&CurrentThread[1].Header.WaitListHead )                                   
      {                                                                                                        
        v27 = CurrentThread[1].Header.WaitListHead.Blink;                                                      
        if ( v26->Header.WaitListHead.Flink != v25 || v27->Flink != v25 )                                      
        {                                                                                                      
          goto LABEL_15;                                                                                       
        }                                                                                                      
        v27->Flink = (struct _LIST_ENTRY *)v26;                                                                
        v26->Header.WaitListHead.Flink = v27;                                                                  
        CurrentThread[1].Header.WaitListHead.Blink = &CurrentThread[1].Header.WaitListHead;                    
        v25->Flink = v25;                                                                                      
        v7 = 0;                                                                                                
      }                                                                                                        
      if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v13, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )       
      {                                                                                                        
        ExfTryToWakePushLock(v13);                                                                             
      }                                                                                                        
      KeAbPostRelease((ULONG_PTR)v13);                                                                         
      KiLeaveCriticalRegionUnsafe(CurrentThread);                                                              
      if ( v7 )                                                                                                
      {                                                                                                        
        v6 = KeWaitForSingleObject(&CurrentThread[1].KernelStack, WrKeyedEvent, 0, 0, 0LL);                    
      }                                                                                                        
    }                                                                                                          
    CurrentThread[1].InitialStack = InitialStack;                                                              
  }                                                                                                            
  *((_DWORD *)&CurrentThread[1].SwapListEntry + 3) &= ~0x20u;                                                  
  if ( Handle )                                                                                                
  {                                                                                                            
    ObfDereferenceObject(v31);                                                                                 
  }                                                                                                            
  return v6;                                                                                                   
}                                                                                                              
