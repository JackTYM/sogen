NTSTATUS __fastcall NtWaitForKeyedEvent(HANDLE Handle, unsigned __int64 a2, BOOLEAN a3, unsigned __int64 a4)
{                                                                                                           
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             
                                                                                                            
  Timeout = (LARGE_INTEGER *)a4;                                                                            
  v6 = 0;                                                                                                   
  v30 = 0LL;                                                                                                
  InitialStack = 0LL;                                                                                       
  v7 = 1;                                                                                                   
  if ( (a2 & 1) != 0 )                                                                                      
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
    v30 = *v10;                                                                                             
    Timeout = (LARGE_INTEGER *)&v30;                                                                        
  }                                                                                                         
  if ( Handle )                                                                                             
  {                                                                                                         
    Object = 0LL;                                                                                           
    result = ObReferenceObjectByHandle(Handle, 1u, ExpKeyedEventObjectType, PreviousMode, &Object, 0LL);    
    v6 = result;                                                                                            
    v11 = Object;                                                                                           
    v33 = Object;                                                                                           
    if ( result < 0 )                                                                                       
    {                                                                                                       
      return result;                                                                                        
    }                                                                                                       
  }                                                                                                         
  else                                                                                                      
  {                                                                                                         
    v11 = (PVOID)ExpCritSecOutOfMemoryEvent;                                                                
    v33 = (PVOID)ExpCritSecOutOfMemoryEvent;                                                                
  }                                                                                                         
  *((_DWORD *)&CurrentThread[1].SwapListEntry + 3) |= 0x20u;                                                
  Process = CurrentThread->ApcState.Process;                                                                
  v12 = (ULONG_PTR)v11 + 24 * ((a2 >> 5) & 0x3F);                                                           
  v13 = (struct _LIST_ENTRY *)(v12 + 8);                                                                    
  --CurrentThread->KernelApcDisable;                                                                        
  v14 = KeAbPreAcquire(v12, 0LL, 0LL);                                                                      
  v16 = v14;                                                                                                
  if ( _interlockedbittestandset64((volatile signed __int32 *)v12, 0LL) )                                   
  {                                                                                                         
    ExfAcquirePushLockExclusiveEx(v12, v14, v12);                                                           
  }                                                                                                         
  if ( v16 )                                                                                                
  {                                                                                                         
    *(_BYTE *)(v16 + 10) = 1;                                                                               
  }                                                                                                         
  Flink = v13->Flink;                                                                                       
  v18 = Process;                                                                                            
  while ( 1 )                                                                                               
  {                                                                                                         
    p_Blink = &Flink[-77].Blink;                                                                            
    if ( Flink == v13 || (v15 = p_Blink[157], ((unsigned __int8)v15 & 1) == 0) )                            
    {                                                                                                       
      InitialStack = CurrentThread[1].InitialStack;                                                         
      CurrentThread[1].InitialStack = (void *)a2;                                                           
      p_WaitListHead = &CurrentThread[1].Header.WaitListHead;                                               
      v21 = *(struct _LIST_ENTRY **)(v12 + 16);                                                             
      if ( v21->Flink == v13 )                                                                              
      {                                                                                                     
        p_WaitListHead->Flink = v13;                                                                        
        CurrentThread[1].Header.WaitListHead.Blink = v21;                                                   
        v21->Flink = p_WaitListHead;                                                                        
        *(_QWORD *)(v12 + 16) = p_WaitListHead;                                                             
        p_Blink = 0LL;                                                                                      
        goto LABEL_18;                                                                                      
      }                                                                                                     
      goto LABEL_13;                                                                                        
    }                                                                                                       
    if ( v15 == (struct _LIST_ENTRY *)(a2 | 1) && p_Blink[68] == (struct _LIST_ENTRY *)Process )            
    {                                                                                                       
      break;                                                                                                
    }                                                                                                       
    Flink = Flink->Flink;                                                                                   
  }                                                                                                         
  v15 = Flink->Flink;                                                                                       
  Blink = Flink->Blink;                                                                                     
  if ( Flink->Flink->Blink != Flink || Blink->Flink != Flink )                                              
  {                                                                                                         
    goto LABEL_13;                                                                                          
  }                                                                                                         
  Blink->Flink = v15;                                                                                       
  v15->Blink = Blink;                                                                                       
  Flink->Blink = Flink;                                                                                     
  Flink->Flink = Flink;                                                                                     
LABEL_18:                                                                                                   
  if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v12, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )        
  {                                                                                                         
    ExfTryToWakePushLock(v12, v15, v18);                                                                    
  }                                                                                                         
  KeAbPostRelease(v12);                                                                                     
  if ( p_Blink )                                                                                            
  {                                                                                                         
    KeReleaseSemaphore((PRKSEMAPHORE)(p_Blink + 163), 1, 1, 0);                                             
    KiLeaveCriticalRegionUnsafe(CurrentThread);                                                             
    goto LABEL_23;                                                                                          
  }                                                                                                         
  KiLeaveCriticalRegionUnsafe(CurrentThread);                                                               
  v6 = KeWaitForSingleObject(&CurrentThread[1].KernelStack, WrKeyedEvent, WaitMode, a3, Timeout);           
  if ( !v6 )                                                                                                
  {                                                                                                         
    goto LABEL_22;                                                                                          
  }                                                                                                         
  --CurrentThread->KernelApcDisable;                                                                        
  v23 = KeAbPreAcquire(v12, 0LL, 0LL);                                                                      
  v26 = v23;                                                                                                
  if ( _interlockedbittestandset64((volatile signed __int32 *)v12, 0LL) )                                   
  {                                                                                                         
    ExfAcquirePushLockExclusiveEx(v12, v23, v12);                                                           
  }                                                                                                         
  if ( v26 )                                                                                                
  {                                                                                                         
    *(_BYTE *)(v26 + 10) = 1;                                                                               
  }                                                                                                         
  v27 = &CurrentThread[1].Header.WaitListHead;                                                              
  v28 = (struct _KTHREAD *)CurrentThread[1].Header.WaitListHead.Flink;                                      
  if ( v28 != (struct _KTHREAD *)&CurrentThread[1].Header.WaitListHead )                                    
  {                                                                                                         
    v24 = CurrentThread[1].Header.WaitListHead.Blink;                                                       
    if ( v28->Header.WaitListHead.Flink == v27 && v24->Flink == v27 )                                       
    {                                                                                                       
      v24->Flink = (struct _LIST_ENTRY *)v28;                                                               
      v28->Header.WaitListHead.Flink = v24;                                                                 
      CurrentThread[1].Header.WaitListHead.Blink = &CurrentThread[1].Header.WaitListHead;                   
      v27->Flink = v27;                                                                                     
      v7 = 0;                                                                                               
      goto LABEL_33;                                                                                        
    }                                                                                                       
LABEL_13:                                                                                                   
    __fastfail(3u);                                                                                         
  }                                                                                                         
LABEL_33:                                                                                                   
  if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)v12, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )        
  {                                                                                                         
    ExfTryToWakePushLock(v12, v24, v25);                                                                    
  }                                                                                                         
  KeAbPostRelease(v12);                                                                                     
  KiLeaveCriticalRegionUnsafe(CurrentThread);                                                               
  if ( v7 )                                                                                                 
  {                                                                                                         
    v6 = KeWaitForSingleObject(&CurrentThread[1].KernelStack, WrKeyedEvent, 0, 0, 0LL);                     
  }                                                                                                         
LABEL_22:                                                                                                   
  CurrentThread[1].InitialStack = InitialStack;                                                             
LABEL_23:                                                                                                   
  *((_DWORD *)&CurrentThread[1].SwapListEntry + 3) &= ~0x20u;                                               
  if ( Handle )                                                                                             
  {                                                                                                         
    ObfDereferenceObject(v33);                                                                              
  }                                                                                                         
  return v6;                                                                                                
}                                                                                                           
