__int64 __fastcall NtCancelTimer(HANDLE Handle, _BYTE *a2)                                                                                       
{                                                                                                                                                
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                  
                                                                                                                                                 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                                             
  if ( a2 && PreviousMode )                                                                                                                      
  {                                                                                                                                              
    v18 = 0x7FFFFFFF0000LL;                                                                                                                      
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )                                                                                               
    {                                                                                                                                            
      v18 = (__int64)a2;                                                                                                                         
    }                                                                                                                                            
    *(_BYTE *)v18 = *(_BYTE *)v18;                                                                                                               
  }                                                                                                                                              
  v5 = 0;                                                                                                                                        
  Object = 0LL;                                                                                                                                  
  v6 = ObReferenceObjectByHandle(Handle, 2u, 0LL, PreviousMode, &Object, 0LL);                                                                   
  if ( v6 < 0 )                                                                                                                                  
  {                                                                                                                                              
    return (unsigned int)v6;                                                                                                                     
  }                                                                                                                                              
  v7 = Object;                                                                                                                                   
  v26 = (unsigned __int8)((unsigned __int16)((_WORD)Object - 48) >> 8);                                                                          
  v8 = (struct _OBJECT_TYPE *)ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ v26 ^ (unsigned __int8)*((char *)Object - 24)];                 
  if ( v8 == ExpIRTimerObjectType )                                                                                                              
  {                                                                                                                                              
    if ( a2 )                                                                                                                                    
    {                                                                                                                                            
      ObfDereferenceObjectWithTag(Object, 0x746C6644u);                                                                                          
      return 3221225485LL;                                                                                                                       
    }                                                                                                                                            
    else                                                                                                                                         
    {                                                                                                                                            
      return ExpSetTimerObject2((ULONG_PTR)Object);                                                                                              
    }                                                                                                                                            
  }                                                                                                                                              
  else                                                                                                                                           
  {                                                                                                                                              
    if ( v8 == ExTimerObjectType )                                                                                                               
    {                                                                                                                                            
      CurrentThread = 0LL;                                                                                                                       
      v30 = 0;                                                                                                                                   
      v9 = (volatile signed __int64 *)((char *)Object + 64);                                                                                     
      v10 = KeAcquireSpinLockRaiseToDpc((PKSPIN_LOCK)Object + 8);                                                                                
      v28 = v10;                                                                                                                                 
      P = (PVOID)*((_QWORD *)v7 + 32);                                                                                                           
      v25 = P;                                                                                                                                   
      if ( P )                                                                                                                                   
      {                                                                                                                                          
        KeReleaseSpinLock((PKSPIN_LOCK)v7 + 8, v10);                                                                                             
        CurrentThread = KeGetCurrentThread();                                                                                                    
        --CurrentThread->KernelApcDisable;                                                                                                       
        v16 = KeAbPreAcquire(&ExpWakeTimerLock, 0LL, 0LL);                                                                                       
        v17 = v16;                                                                                                                               
        if ( _interlockedbittestandset64((volatile signed __int32 *)&ExpWakeTimerLock, 0LL) )                                                    
        {                                                                                                                                        
          ExfAcquirePushLockExclusiveEx(&ExpWakeTimerLock, v16, &ExpWakeTimerLock);                                                              
        }                                                                                                                                        
        if ( v17 )                                                                                                                               
        {                                                                                                                                        
          *(_BYTE *)(v17 + 10) = 1;                                                                                                              
        }                                                                                                                                        
        v30 = 1;                                                                                                                                 
        v28 = KeAcquireSpinLockRaiseToDpc((PKSPIN_LOCK)v7 + 8);                                                                                  
        P = (PVOID)*((_QWORD *)v7 + 32);                                                                                                         
        v25 = P;                                                                                                                                 
        *((_QWORD *)v7 + 32) = 0LL;                                                                                                              
        v7 = Object;                                                                                                                             
      }                                                                                                                                          
      v11 = ExpCancelTimer((PKTIMER)v7) + 1;                                                                                                     
      if ( (BYTE6(PerfGlobalGroupMask) & 1) == 0 || PopHibernateInProgress )                                                                     
      {                                                                                                                                          
        _InterlockedAnd64(v9, 0LL);                                                                                                              
      }                                                                                                                                          
      else                                                                                                                                       
      {                                                                                                                                          
        KiReleaseSpinLockInstrumented(v9, retaddr);                                                                                              
      }                                                                                                                                          
      if ( KiIrqlFlags )                                                                                                                         
      {                                                                                                                                          
        KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v28);                                                                                    
      }                                                                                                                                          
      __writecr8(v28);                                                                                                                           
      v12 = P;                                                                                                                                   
      if ( v30 )                                                                                                                                 
      {                                                                                                                                          
        if ( P )                                                                                                                                 
        {                                                                                                                                        
          v19 = (char *)Object + 264;                                                                                                            
          v20 = *((_QWORD *)Object + 33);                                                                                                        
          v21 = (_QWORD *)*((_QWORD *)Object + 34);                                                                                              
          if ( *(PVOID *)(v20 + 8) != (char *)Object + 264 || (_QWORD *)*v21 != v19 )                                                            
          {                                                                                                                                      
            __fastfail(3u);                                                                                                                      
          }                                                                                                                                      
          *v21 = v20;                                                                                                                            
          *(_QWORD *)(v20 + 8) = v21;                                                                                                            
          *v19 = 0LL;                                                                                                                            
        }                                                                                                                                        
        if ( (_InterlockedExchangeAdd64((volatile signed __int64 *)&ExpWakeTimerLock, 0xFFFFFFFFFFFFFFFFuLL) & 6) == 2 )                         
        {                                                                                                                                        
          ExfTryToWakePushLock(&ExpWakeTimerLock);                                                                                               
        }                                                                                                                                        
        KeAbPostRelease((ULONG_PTR)&ExpWakeTimerLock);                                                                                           
        KiLeaveCriticalRegionUnsafe(CurrentThread);                                                                                              
      }                                                                                                                                          
      v29 = *((_BYTE *)Object + 4);                                                                                                              
      v13 = (char *)Object - 48;                                                                                                                 
      LOBYTE(v5) = ObpTraceFlags != 0;                                                                                                           
      if ( v5 )                                                                                                                                  
      {                                                                                                                                          
        ObpPushStackInfo((_DWORD)Object - 48);                                                                                                   
      }                                                                                                                                          
      v14 = _InterlockedExchangeAdd64((volatile signed __int64 *)v13, -v11) - v11;                                                               
      if ( v14 <= 0 )                                                                                                                            
      {                                                                                                                                          
        if ( *((_QWORD *)v13 + 1) )                                                                                                              
        {                                                                                                                                        
          KeBugCheckEx(                                                                                                                          
            0x18u,                                                                                                                               
            ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ (unsigned __int8)v26 ^ (unsigned __int64)(unsigned __int8)*((char *)Object - 24)],
            (ULONG_PTR)Object,                                                                                                                   
            6uLL,                                                                                                                                
            *((_QWORD *)v13 + 1));                                                                                                               
        }                                                                                                                                        
        if ( v14 < 0 )                                                                                                                           
        {                                                                                                                                        
          KeBugCheckEx(0x18u, 0LL, (ULONG_PTR)Object, 5uLL, v14);                                                                                
        }                                                                                                                                        
        ObpDeferObjectDeletion(v13);                                                                                                             
      }                                                                                                                                          
      if ( a2 )                                                                                                                                  
      {                                                                                                                                          
        *a2 = v29;                                                                                                                               
      }                                                                                                                                          
      if ( v12 )                                                                                                                                 
      {                                                                                                                                          
        PoDestroyReasonContext(v12);                                                                                                             
      }                                                                                                                                          
      return (unsigned int)v6;                                                                                                                   
    }                                                                                                                                            
    ObfDereferenceObjectWithTag(Object, 0x746C6644u);                                                                                            
    return 3221225508LL;                                                                                                                         
  }                                                                                                                                              
}                                                                                                                                                
