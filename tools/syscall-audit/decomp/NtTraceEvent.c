__int64 __fastcall NtTraceEvent(__int128 *a1, __int64 a2, unsigned int a3, __int64 a4)                                        
{                                                                                                                             
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                               
                                                                                                                              
  v6 = a2;                                                                                                                    
  v7 = a2 & 0xFF00;                                                                                                           
  if ( v7 == 768 )                                                                                                            
  {                                                                                                                           
    CurrentThread = KeGetCurrentThread();                                                                                     
    PreviousMode = CurrentThread->PreviousMode;                                                                               
    if ( PreviousMode && (a4 & 3) != 0 )                                                                                      
    {                                                                                                                         
      ExRaiseDatatypeMisalignment();                                                                                          
    }                                                                                                                         
    v77 = *(_QWORD *)(a4 + 88);                                                                                               
    v91 = v77;                                                                                                                
    v70 = *(_DWORD *)(a4 + 84);                                                                                               
    v86 = v70;                                                                                                                
    v8 = *(unsigned __int16 *)(a4 + 82);                                                                                      
    v92 = *(unsigned __int16 *)(a4 + 82);                                                                                     
    LODWORD(v68) = *(_DWORD *)(a4 + 112);                                                                                     
    v87 = v68;                                                                                                                
    v79 = *(_QWORD *)(a4 + 48);                                                                                               
    v93 = v79;                                                                                                                
    v63 = *(_BYTE *)(a4 + 44);                                                                                                
    v72 = v63;                                                                                                                
    v9 = 0;                                                                                                                   
    v10 = 0LL;                                                                                                                
    v88 = 0LL;                                                                                                                
    if ( *(_BYTE *)(a4 + 80) )                                                                                                
    {                                                                                                                         
      v10 = a4 + 96;                                                                                                          
      v88 = a4 + 96;                                                                                                          
    }                                                                                                                         
    Object = 0LL;                                                                                                             
    v11 = ObReferenceObjectByHandle(a1, 0x800u, EtwpRegistrationObjectType, 1, &Object, 0LL);                                 
    v67 = v11;                                                                                                                
    if ( v11 >= 0 )                                                                                                           
    {                                                                                                                         
      v78 = 0LL;                                                                                                              
      v12 = (unsigned __int16 *)Object;                                                                                       
      v13 = *((_QWORD *)Object + 4);                                                                                          
      v14 = *((_QWORD *)Object + 5);                                                                                          
      v85 = v14;                                                                                                              
      v15 = v6 >> 31;                                                                                                         
      v98 = 0LL;                                                                                                              
      v99 = 0LL;                                                                                                              
      v16 = *((_QWORD *)Object + 14);                                                                                         
      v17 = v16 + 28;                                                                                                         
      if ( !v16 )                                                                                                             
      {                                                                                                                       
        v17 = 0LL;                                                                                                            
      }                                                                                                                       
      v82 = v17;                                                                                                              
      v18 = *((unsigned __int16 *)Object + 50);                                                                               
      if ( (_WORD)v18 )                                                                                                       
      {                                                                                                                       
        v67 = EtwpWriteUserEvent(                                                                                             
                v13,                                                                                                          
                v18,                                                                                                          
                (unsigned __int16)v15,                                                                                        
                v8,                                                                                                           
                v68,                                                                                                          
                a4,                                                                                                           
                v10,                                                                                                          
                v63,                                                                                                          
                v79,                                                                                                          
                v70,                                                                                                          
                v77,                                                                                                          
                (__int64)&v98,                                                                                                
                0LL,                                                                                                          
                *((_WORD *)Object + 49),                                                                                      
                v17,                                                                                                          
                (__int64)&v78);                                                                                               
        v17 = v82;                                                                                                            
        v14 = v85;                                                                                                            
      }                                                                                                                       
      v19 = v12[51];                                                                                                          
      if ( (_WORD)v19 )                                                                                                       
      {                                                                                                                       
        v67 = EtwpWriteUserEvent(                                                                                             
                v13,                                                                                                          
                v19,                                                                                                          
                (unsigned __int16)v15,                                                                                        
                v8,                                                                                                           
                v68,                                                                                                          
                a4,                                                                                                           
                v10,                                                                                                          
                v63,                                                                                                          
                v79,                                                                                                          
                v70,                                                                                                          
                v77,                                                                                                          
                (__int64)&v98,                                                                                                
                v14,                                                                                                          
                v12[49],                                                                                                      
                v17,                                                                                                          
                (__int64)&v78);                                                                                               
      }                                                                                                                       
      if ( *(_QWORD *)(v13 + 656) )                                                                                           
      {                                                                                                                       
        v98 = 0LL;                                                                                                            
        v99 = 0LL;                                                                                                            
        v61 = v12[52];                                                                                                        
        if ( (_WORD)v61 )                                                                                                     
        {                                                                                                                     
          v67 = EtwpWriteUserEvent(                                                                                           
                  *(_QWORD *)(v13 + 656),                                                                                     
                  v61,                                                                                                        
                  (unsigned __int16)v15,                                                                                      
                  0,                                                                                                          
                  v68,                                                                                                        
                  a4,                                                                                                         
                  v10,                                                                                                        
                  v63,                                                                                                        
                  v79,                                                                                                        
                  v70,                                                                                                        
                  v77,                                                                                                        
                  (__int64)&v98,                                                                                              
                  0LL,                                                                                                        
                  v12[49],                                                                                                    
                  v82,                                                                                                        
                  (__int64)&v78);                                                                                             
        }                                                                                                                     
        v62 = v12[53];                                                                                                        
        if ( (_WORD)v62 )                                                                                                     
        {                                                                                                                     
          v67 = EtwpWriteUserEvent(                                                                                           
                  *(_QWORD *)(v13 + 656),                                                                                     
                  v62,                                                                                                        
                  (unsigned __int16)v15,                                                                                      
                  0,                                                                                                          
                  v68,                                                                                                        
                  a4,                                                                                                         
                  v10,                                                                                                        
                  v63,                                                                                                        
                  v79,                                                                                                        
                  v70,                                                                                                        
                  v77,                                                                                                        
                  (__int64)&v98,                                                                                              
                  *(_QWORD *)(v85 + 656),                                                                                     
                  v12[49],                                                                                                    
                  v82,                                                                                                        
                  (__int64)&v78);                                                                                             
        }                                                                                                                     
      }                                                                                                                       
      v20 = (signed __int64)(v12 - 24);                                                                                       
      if ( ObpTraceFlags )                                                                                                    
      {                                                                                                                       
        ObpPushStackInfo((_DWORD)v12 - 48);                                                                                   
      }                                                                                                                       
      v21 = _InterlockedExchangeAdd64((volatile signed __int64 *)v20, 0xFFFFFFFFFFFFFFFFuLL);                                 
      v22 = v21 <= 1;                                                                                                         
      v23 = v21 - 1;                                                                                                          
      if ( v22 )                                                                                                              
      {                                                                                                                       
        if ( *(_QWORD *)(v20 + 8) )                                                                                           
        {                                                                                                                     
          KeBugCheckEx(                                                                                                       
            0x18u,                                                                                                            
            ObTypeIndexTable[(unsigned __int8)ObHeaderCookie ^ *(unsigned __int8 *)(v20 + 24) ^ (unsigned __int64)BYTE1(v20)],
            (ULONG_PTR)Object,                                                                                                
            1uLL,                                                                                                             
            *(_QWORD *)(v20 + 8));                                                                                            
        }                                                                                                                     
        if ( v23 < 0 )                                                                                                        
        {                                                                                                                     
          KeBugCheckEx(0x18u, 0LL, (ULONG_PTR)Object, 2uLL, v23);                                                             
        }                                                                                                                     
        if ( KeGetCurrentThread()->SpecialApcDisable || !(unsigned __int8)KeAreInterruptsEnabled() || KeGetCurrentIrql() )    
        {                                                                                                                     
          _m_prefetchw(&ObpRemoveObjectList);                                                                                 
          v44 = ObpRemoveObjectList;                                                                                          
          *(_QWORD *)(v20 + 8) = ObpRemoveObjectList;                                                                         
          for ( i = _InterlockedCompareExchange64(&ObpRemoveObjectList, v20, v44);                                            
                i != v44;                                                                                                     
                i = _InterlockedCompareExchange64(&ObpRemoveObjectList, v20, i) )                                             
          {                                                                                                                   
            v44 = i;                                                                                                          
            *(_QWORD *)(v20 + 8) = i;                                                                                         
          }                                                                                                                   
          if ( !v44 )                                                                                                         
          {                                                                                                                   
            if ( (unsigned __int8)ObGetCurrentIrql() > 2u )                                                                   
            {                                                                                                                 
              KeInsertQueueDpc(&ObpRemoveObjectDpc, 0LL, 0LL);                                                                
            }                                                                                                                 
            else                                                                                                              
            {                                                                                                                 
              ExQueueWorkItem(&ObpRemoveObjectWorkItem, CriticalWorkQueue);                                                   
            }                                                                                                                 
          }                                                                                                                   
        }                                                                                                                     
        else                                                                                                                  
        {                                                                                                                     
          if ( (*(_BYTE *)(v20 + 26) & 0x40) != 0                                                                             
            && *(_BYTE *)(*(_QWORD *)(v20 - ObpInfoMaskToOffset[*(_BYTE *)(v20 + 26) & 0x7F]) + 24LL) )                       
          {                                                                                                                   
            ObpHandleRevocationBlockRemoveObject();                                                                           
          }                                                                                                                   
          LOBYTE(v9) = ObpTraceFlags != 0;                                                                                    
          if ( v9 )                                                                                                           
          {                                                                                                                   
            ObpDeregisterObject(v12 - 24);                                                                                    
          }                                                                                                                   
          ObpRemoveObjectRoutine(v12 - 24, 0LL);                                                                              
        }                                                                                                                     
      }                                                                                                                       
      return v67;                                                                                                             
    }                                                                                                                         
    return (unsigned int)v11;                                                                                                 
  }                                                                                                                           
  if ( v7 == 512 )                                                                                                            
  {                                                                                                                           
    v94 = a1;                                                                                                                 
    v76 = 0LL;                                                                                                                
    if ( a3 == 40 )                                                                                                           
    {                                                                                                                         
      if ( KeGetCurrentThread()->PreviousMode == 1 )                                                                          
      {                                                                                                                       
        v68 = a4;                                                                                                             
        if ( (a4 & 3) != 0 )                                                                                                  
        {                                                                                                                     
          ExRaiseDatatypeMisalignment();                                                                                      
        }                                                                                                                     
        v69 = *(_DWORD *)(v68 + 24);                                                                                          
        if ( (int)a2 < 0 )                                                                                                    
        {                                                                                                                     
          LODWORD(v76) = *(_DWORD *)(v68 + 32);                                                                               
          v69 |= 0x80u;                                                                                                       
        }                                                                                                                     
        else                                                                                                                  
        {                                                                                                                     
          v76 = *(_QWORD *)(v68 + 32);                                                                                        
        }                                                                                                                     
        v25 = *(unsigned int *)(v68 + 28);                                                                                    
        v81 = v25;                                                                                                            
        if ( (unsigned int)v25 <= 0x10000 )                                                                                   
        {                                                                                                                     
          if ( (_DWORD)v25 && (v76 + v25 > 0x7FFFFFFF0000LL || v76 + v25 < v76) )                                             
          {                                                                                                                   
            LODWORD(v25) = v81;                                                                                               
          }                                                                                                                   
          HIDWORD(v94) = v25;                                                                                                 
          return EtwpTraceMessageVa((_DWORD)v94, v69 | 0x40u, (int)v68 + 8, *(unsigned __int16 *)(v68 + 4), v76, 1);          
        }                                                                                                                     
        v11 = -1073741811;                                                                                                    
        v67 = -1073741811;                                                                                                    
        return (unsigned int)v11;                                                                                             
      }                                                                                                                       
      return 3221225659LL;                                                                                                    
    }                                                                                                                         
    return 3221225485LL;                                                                                                      
  }                                                                                                                           
  if ( v7 <= 0x600 )                                                                                                          
  {                                                                                                                           
    switch ( v7 )                                                                                                             
    {                                                                                                                         
      case 0x600u:                                                                                                            
        v64 = 0;                                                                                                              
        v47 = *(_QWORD *)(((__int64 (*)(void))PsGetCurrentServerSiloGlobals)() + 832);                                        
        v48 = v46;                                                                                                            
        if ( v46 == 0xFFFF || !v46 )                                                                                          
        {                                                                                                                     
          v48 = *(unsigned __int8 *)(v47 + 4504);                                                                             
        }                                                                                                                     
        if ( v48 < *(_DWORD *)(v47 + 16)                                                                                      
          && (v53 = KeGetCurrentThread()->PreviousMode, v54 = EtwpOpenLogger(v48, v47, v53, &v64), v56 = v54, v54) )          
        {                                                                                                                     
          if ( (*(_DWORD *)(v54 + 12) & 0x2000000) != 0 )                                                                     
          {                                                                                                                   
            v57 = v47 + 32LL * *(unsigned __int8 *)(v54 + 818) + 4556;                                                        
            if ( v57 && (*(_DWORD *)(v57 + 4) & 0x28) != 0 )                                                                  
            {                                                                                                                 
              LOBYTE(v55) = 1;                                                                                                
            }                                                                                                                 
            else                                                                                                              
            {                                                                                                                 
              v55 = 0;                                                                                                        
            }                                                                                                                 
            v11 = EtwpSetMark(v56, a4, a3, v55, v53);                                                                         
          }                                                                                                                   
          else                                                                                                                
          {                                                                                                                   
            v11 = -1073741811;                                                                                                
          }                                                                                                                   
          EtwpCloseLogger(v48, v47, v64);                                                                                     
        }                                                                                                                     
        else                                                                                                                  
        {                                                                                                                     
          return (unsigned int)-1073741816;                                                                                   
        }                                                                                                                     
        return (unsigned int)v11;                                                                                             
      case 0x400u:                                                                                                            
        v27 = *(_QWORD *)(PsGetCurrentServerSiloGlobals(a1, a2) + 832);                                                       
        v28 = 0;                                                                                                              
        if ( v26 != 0xFFFF )                                                                                                  
        {                                                                                                                     
          v28 = v26;                                                                                                          
        }                                                                                                                     
        if ( v28 < *(_DWORD *)(v27 + 16) )                                                                                    
        {                                                                                                                     
          if ( a3 > 0xFFDF )                                                                                                  
          {                                                                                                                   
            return 3221225621LL;                                                                                              
          }                                                                                                                   
          if ( KeGetCurrentThread()->PreviousMode == 1 )                                                                      
          {                                                                                                                   
            if ( (a4 & 3) != 0 )                                                                                              
            {                                                                                                                 
              ExRaiseDatatypeMisalignment();                                                                                  
            }                                                                                                                 
            v89 = a4 + 32;                                                                                                    
            v73 = *(_WORD *)(a4 + 6);                                                                                         
            v94 = (__int128 *)(a4 + 32);                                                                                      
            v95 = a3;                                                                                                         
            v96 = 0;                                                                                                          
            EtwpLogSystemEventUnsafe(                                                                                         
              v27,                                                                                                            
              (unsigned int)&v94,                                                                                             
              (unsigned int)KeGetCurrentThread(),                                                                             
              v28,                                                                                                            
              1,                                                                                                              
              v73,                                                                                                            
              v6 & 0xC00F00FF | 0x3100,                                                                                       
              1);                                                                                                             
            return 0;                                                                                                         
          }                                                                                                                   
          return 3221225659LL;                                                                                                
        }                                                                                                                     
        return 3221225485LL;                                                                                                  
      case 0x100u:                                                                                                            
        return (unsigned int)EtwTraceEvent(                                                                                   
                               (_DWORD)a1,                                                                                    
                               a4,                                                                                            
                               48,                                                                                            
                               (((int)a2 >> 31) & 0xFFF60000) - 1072431104,                                                   
                               KeGetCurrentThread()->PreviousMode);                                                           
      case 0x500u:                                                                                                            
        v30 = *(_QWORD *)(PsGetCurrentServerSiloGlobals(a1, KeGetCurrentThread()[1].CycleTime) + 832);                        
        if ( *(_DWORD *)(v30 + 4344) == v31 )                                                                                 
        {                                                                                                                     
          if ( (a4 & 3) != 0 )                                                                                                
          {                                                                                                                   
            ExRaiseDatatypeMisalignment();                                                                                    
          }                                                                                                                   
          return (unsigned int)EtwpWriteUserEvent(                                                                            
                                 (int)v30 + 24,                                                                               
                                 *(unsigned __int16 *)(v30 + 4336),                                                           
                                 0,                                                                                           
                                 0,                                                                                           
                                 *(_DWORD *)(v29 + 112),                                                                      
                                 a4,                                                                                          
                                 0LL,                                                                                         
                                 *(_BYTE *)(a4 + 44),                                                                         
                                 *(_QWORD *)(a4 + 48),                                                                        
                                 *(_DWORD *)(v29 + 84),                                                                       
                                 *(_QWORD *)(v29 + 88),                                                                       
                                 0LL,                                                                                         
                                 0LL,                                                                                         
                                 0,                                                                                           
                                 0LL,                                                                                         
                                 0LL);                                                                                        
        }                                                                                                                     
        return 3221225506LL;                                                                                                  
    }                                                                                                                         
    return (unsigned int)-1073741811;                                                                                         
  }                                                                                                                           
  if ( v7 != 1792 )                                                                                                           
  {                                                                                                                           
    if ( v7 == 2048 )                                                                                                         
    {                                                                                                                         
      return (unsigned int)EtwTraceEvent(                                                                                     
                             (_DWORD)a1,                                                                                      
                             a4,                                                                                              
                             72,                                                                                              
                             (((int)a2 >> 31) & 0xFFF60000) - 1072365568,                                                     
                             KeGetCurrentThread()->PreviousMode);                                                             
    }                                                                                                                         
    if ( v7 == 2304 )                                                                                                         
    {                                                                                                                         
      if ( (_DWORD)a2 == 2304 && a3 && a4 )                                                                                   
      {                                                                                                                       
        return (unsigned int)EtwTraceRaw(a1, a4, a3, (unsigned __int8)KeGetCurrentThread()->PreviousMode);                    
      }                                                                                                                       
      return 3221225485LL;                                                                                                    
    }                                                                                                                         
    return (unsigned int)-1073741811;                                                                                         
  }                                                                                                                           
  if ( !a1 )                                                                                                                  
  {                                                                                                                           
    return 3221225485LL;                                                                                                      
  }                                                                                                                           
  if ( (a4 & 3) != 0 )                                                                                                        
  {                                                                                                                           
    ExRaiseDatatypeMisalignment();                                                                                            
  }                                                                                                                           
  if ( ((unsigned __int8)a1 & 3) != 0 )                                                                                       
  {                                                                                                                           
    ExRaiseDatatypeMisalignment();                                                                                            
  }                                                                                                                           
  v83 = *(_QWORD *)(a4 + 88);                                                                                                 
  v74 = *(_DWORD *)(a4 + 84);                                                                                                 
  v75 = *(_DWORD *)(a4 + 112);                                                                                                
  v84 = *(_QWORD *)(a4 + 48);                                                                                                 
  v65 = *(_BYTE *)(a4 + 44);                                                                                                  
  v97 = *a1;                                                                                                                  
  v32 = ((__int64 (*)(void))PsGetCurrentServerSiloGlobals)();                                                                 
  GuidEntryByGuid = EtwpFindGuidEntryByGuid(*(_QWORD *)(v32 + 832), &v97, 0LL);                                               
  v34 = GuidEntryByGuid;                                                                                                      
  if ( GuidEntryByGuid )                                                                                                      
  {                                                                                                                           
    if ( *(_DWORD *)(GuidEntryByGuid + 96) )                                                                                  
    {                                                                                                                         
      v11 = EtwpAccessCheck(*(PSECURITY_DESCRIPTOR *)(GuidEntryByGuid + 72), 0x800u);                                         
      v67 = v11;                                                                                                              
      if ( v11 >= 0 )                                                                                                         
      {                                                                                                                       
        v66[0] = 0;                                                                                                           
        v35 = KeGetCurrentThread();                                                                                           
        --v35->KernelApcDisable;                                                                                              
        v36 = (signed __int64 *)(v34 + 664);                                                                                  
        v37 = KeAbPreAcquire(v34 + 664, 0LL, 0LL);                                                                            
        v39 = v37;                                                                                                            
        if ( _interlockedbittestandset64((volatile signed __int32 *)(v34 + 664), 0LL) )                                       
        {                                                                                                                     
          ExfAcquirePushLockExclusiveEx(v34 + 664, v37, v34 + 664);                                                           
        }                                                                                                                     
        if ( v39 )                                                                                                            
        {                                                                                                                     
          *(_BYTE *)(v39 + 10) = 1;                                                                                           
        }                                                                                                                     
        *(_QWORD *)(v34 + 672) = KeGetCurrentThread();                                                                        
        LOBYTE(v38) = 1;                                                                                                      
        EtwpUpdateEnableMask(v34, 0, 0, v38, (__int64)v66);                                                                   
        *(_QWORD *)(v34 + 672) = 0LL;                                                                                         
        _m_prefetchw(v36);                                                                                                    
        v40 = *v36;                                                                                                           
        v41 = *v36 - 16;                                                                                                      
        if ( (*v36 & 0xFFFFFFFFFFFFFFF0uLL) <= 0x10 )                                                                         
        {                                                                                                                     
          v41 = 0LL;                                                                                                          
        }                                                                                                                     
        if ( (v40 & 2) != 0 || (v42 = *v36, v42 != _InterlockedCompareExchange64(v36, v41, v40)) )                            
        {                                                                                                                     
          ExfReleasePushLock(v34 + 664, v41);                                                                                 
        }                                                                                                                     
        KeAbPostRelease(v34 + 664);                                                                                           
        KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                                    
        v43 = v6 >> 31;                                                                                                       
        v11 = EtwpWriteUserEvent(                                                                                             
                v34,                                                                                                          
                v66[0],                                                                                                       
                (unsigned __int16)v43,                                                                                        
                0,                                                                                                            
                v75,                                                                                                          
                a4,                                                                                                           
                0LL,                                                                                                          
                v65,                                                                                                          
                v84,                                                                                                          
                v74,                                                                                                          
                v83,                                                                                                          
                0LL,                                                                                                          
                0LL,                                                                                                          
                0,                                                                                                            
                0LL,                                                                                                          
                0LL);                                                                                                         
        v67 = v11;                                                                                                            
        if ( *(_QWORD *)(v34 + 656) )                                                                                         
        {                                                                                                                     
          v66[0] = 0;                                                                                                         
          v49 = KeGetCurrentThread();                                                                                         
          --v49->KernelApcDisable;                                                                                            
          v50 = KeAbPreAcquire(v34 + 664, 0LL, 0LL);                                                                          
          v52 = v50;                                                                                                          
          if ( _interlockedbittestandset64((volatile signed __int32 *)v36, 0LL) )                                             
          {                                                                                                                   
            ExfAcquirePushLockExclusiveEx(v34 + 664, v50, v34 + 664);                                                         
          }                                                                                                                   
          if ( v52 )                                                                                                          
          {                                                                                                                   
            *(_BYTE *)(v52 + 10) = 1;                                                                                         
          }                                                                                                                   
          *(_QWORD *)(v34 + 672) = KeGetCurrentThread();                                                                      
          LOBYTE(v51) = 1;                                                                                                    
          EtwpUpdateEnableMask(*(_QWORD *)(v34 + 656), 0, 1, v51, (__int64)v66);                                              
          *(_QWORD *)(v34 + 672) = 0LL;                                                                                       
          _m_prefetchw(v36);                                                                                                  
          v58 = *v36;                                                                                                         
          v59 = *v36 - 16;                                                                                                    
          if ( (*v36 & 0xFFFFFFFFFFFFFFF0uLL) <= 0x10 )                                                                       
          {                                                                                                                   
            v59 = 0LL;                                                                                                        
          }                                                                                                                   
          if ( (v58 & 2) != 0 || (v60 = *v36, v60 != _InterlockedCompareExchange64(v36, v59, v58)) )                          
          {                                                                                                                   
            ExfReleasePushLock(v34 + 664, v59);                                                                               
          }                                                                                                                   
          KeAbPostRelease(v34 + 664);                                                                                         
          KeLeaveCriticalRegionThread(KeGetCurrentThread());                                                                  
          v11 = EtwpWriteUserEvent(                                                                                           
                  *(_QWORD *)(v34 + 656),                                                                                     
                  v66[0],                                                                                                     
                  (unsigned __int16)v43,                                                                                      
                  0,                                                                                                          
                  v75,                                                                                                        
                  a4,                                                                                                         
                  0LL,                                                                                                        
                  v65,                                                                                                        
                  v84,                                                                                                        
                  v74,                                                                                                        
                  v83,                                                                                                        
                  0LL,                                                                                                        
                  0LL,                                                                                                        
                  0,                                                                                                          
                  0LL,                                                                                                        
                  0LL);                                                                                                       
        }                                                                                                                     
      }                                                                                                                       
      EtwpUnreferenceGuidEntry(v34);                                                                                          
      return (unsigned int)v11;                                                                                               
    }                                                                                                                         
    EtwpUnreferenceGuidEntry(GuidEntryByGuid);                                                                                
  }                                                                                                                           
  return 3221226242LL;                                                                                                        
}                                                                                                                             
