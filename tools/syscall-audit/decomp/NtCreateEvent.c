__int64 __fastcall NtCreateEvent(unsigned __int64 a1, __int64 a2, int a3, EVENT_TYPE a4, BOOLEAN a5)           
{                                                                                                              
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                
                                                                                                               
  v44 = (_QWORD *)a1;                                                                                          
  v41 = 0LL;                                                                                                   
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                           
  if ( PreviousMode )                                                                                          
  {                                                                                                            
    v9 = 0x7FFFFFFF0000LL;                                                                                     
    if ( a1 < 0x7FFFFFFF0000LL )                                                                               
    {                                                                                                          
      v9 = a1;                                                                                                 
    }                                                                                                          
    *(_QWORD *)v9 = *(_QWORD *)v9;                                                                             
  }                                                                                                            
  if ( (unsigned int)a4 > SynchronizationEvent )                                                               
  {                                                                                                            
    return 3221225485LL;                                                                                       
  }                                                                                                            
  v10 = ExEventObjectType;                                                                                     
  *(_OWORD *)ListEntry = 0LL;                                                                                  
  v40 = 0LL;                                                                                                   
  CurrentPrcb = KeGetCurrentPrcb();                                                                            
  P = CurrentPrcb->PPLookasideList[4].P;                                                                       
  ++P->TotalAllocates;                                                                                         
  v13 = RtlpInterlockedPopEntrySList(&P->ListHead);                                                            
  if ( !v13 )                                                                                                  
  {                                                                                                            
    ++P->AllocateMisses;                                                                                       
    L = CurrentPrcb->PPLookasideList[4].L;                                                                     
    ++L->TotalAllocates;                                                                                       
    v13 = RtlpInterlockedPopEntrySList(&L->ListHead);                                                          
    if ( !v13 )                                                                                                
    {                                                                                                          
      ++L->AllocateMisses;                                                                                     
      v13 = (PSLIST_ENTRY)guard_dispatch_icall_no_overrides((unsigned int)L->Type, L->Size, L->Tag);           
    }                                                                                                          
  }                                                                                                            
  if ( !v13 )                                                                                                  
  {                                                                                                            
    Information = -1073741670;                                                                                 
    v23 = 0LL;                                                                                                 
    goto LABEL_25;                                                                                             
  }                                                                                                            
  LODWORD(v13->Next) = CurrentPrcb->Number;                                                                    
  Information = ObpCaptureObjectCreateInformation(                                                             
                  PreviousMode,                                                                                
                  PreviousMode,                                                                                
                  a3,                                                                                          
                  (unsigned int)ListEntry,                                                                     
                  (__int64)v13,                                                                                
                  0);                                                                                          
  if ( Information >= 0 )                                                                                      
  {                                                                                                            
    if ( ((__int64)v13->Next & (_DWORD)v10[9]) != 0 )                                                          
    {                                                                                                          
      Information = -1073741811;                                                                               
      goto LABEL_14;                                                                                           
    }                                                                                                          
    if ( ((__int64)v13->Next & 0x10) == 0 )                                                                    
    {                                                                                                          
LABEL_13:                                                                                                      
      v17 = *((_DWORD *)v10 + 27);                                                                             
      HIDWORD(v13[1].Next) = *((_DWORD *)v10 + 26);                                                            
      *((_DWORD *)&v13[1].Next + 2) = v17;                                                                     
      Information = ObpAllocateObject(                                                                         
                      (_DWORD)v13,                                                                             
                      PreviousMode,                                                                            
                      (_DWORD)v10,                                                                             
                      (unsigned int)ListEntry,                                                                 
                      24,                                                                                      
                      (__int64)&v40,                                                                           
                      0LL);                                                                                    
      if ( Information >= 0 )                                                                                  
      {                                                                                                        
        v31 = v40;                                                                                             
        if ( ObpTraceFlags )                                                                                   
        {                                                                                                      
          ObpRegisterObject(v40);                                                                              
          ObpPushStackInfo((_DWORD)v31);                                                                       
        }                                                                                                      
        v23 = v31 + 2;                                                                                         
        goto LABEL_25;                                                                                         
      }                                                                                                        
LABEL_14:                                                                                                      
      if ( ListEntry[1] )                                                                                      
      {                                                                                                        
        if ( WORD1(ListEntry[0]) == 248 )                                                                      
        {                                                                                                      
          v18 = KeGetCurrentPrcb();                                                                            
          v19 = v18->PPLookasideList[5].P;                                                                     
          ++v19->TotalFrees;                                                                                   
          if ( LOWORD(v19->ListHead.Alignment) < v19->Depth                                                    
            || (++v19->FreeMisses,                                                                             
                v19 = v18->PPLookasideList[5].L,                                                               
                ++v19->TotalFrees,                                                                             
                LOWORD(v19->ListHead.Alignment) < v19->Depth) )                                                
          {                                                                                                    
            RtlpInterlockedPushEntrySList(&v19->ListHead, ListEntry[1]);                                       
          }                                                                                                    
          else                                                                                                 
          {                                                                                                    
            ++v19->FreeMisses;                                                                                 
            if ( (void (__stdcall *)(PVOID))v19->FreeEx == ExFreePool )                                        
            {                                                                                                  
              ExFreePool(ListEntry[1]);                                                                        
            }                                                                                                  
            else                                                                                               
            {                                                                                                  
              guard_dispatch_icall_no_overrides(ListEntry[1], v18, v16);                                       
            }                                                                                                  
          }                                                                                                    
        }                                                                                                      
        else                                                                                                   
        {                                                                                                      
          ExFreePoolWithTag(ListEntry[1], 0);                                                                  
        }                                                                                                      
      }                                                                                                        
      Next = v13[2].Next;                                                                                      
      if ( Next )                                                                                              
      {                                                                                                        
        if ( LOBYTE(v13[1].Next) <= 1u )                                                                       
        {                                                                                                      
          ExFreePoolWithTag(Next, 0);                                                                          
        }                                                                                                      
        v13[2].Next = 0LL;                                                                                     
      }                                                                                                        
      v21 = KeGetCurrentPrcb();                                                                                
      v22 = v21->PPLookasideList[4].P;                                                                         
      ++v22->TotalFrees;                                                                                       
      if ( LOWORD(v22->ListHead.Alignment) >= v22->Depth )                                                     
      {                                                                                                        
        ++v22->FreeMisses;                                                                                     
        v22 = v21->PPLookasideList[4].L;                                                                       
        ++v22->TotalFrees;                                                                                     
        if ( LOWORD(v22->ListHead.Alignment) >= v22->Depth )                                                   
        {                                                                                                      
          goto LABEL_41;                                                                                       
        }                                                                                                      
      }                                                                                                        
      goto LABEL_23;                                                                                           
    }                                                                                                          
    v25 = SeCreatePermanentPrivilege;                                                                          
    memset(&SubjectContext, 0, sizeof(SubjectContext));                                                        
    SeCaptureSubjectContext(&SubjectContext);                                                                  
    v26 = 1;                                                                                                   
    v47[0] = 1;                                                                                                
    v47[1] = 1;                                                                                                
    v48 = v25;                                                                                                 
    v49 = 0;                                                                                                   
    ClientToken = SubjectContext.ClientToken;                                                                  
    if ( PreviousMode )                                                                                        
    {                                                                                                          
      if ( !SubjectContext.ClientToken )                                                                       
      {                                                                                                        
        LODWORD(ClientToken) = SubjectContext.PrimaryToken;                                                    
LABEL_32:                                                                                                      
        v28 = SepPrivilegeCheck((_DWORD)ClientToken, (unsigned int)&v48, 1, 1, PreviousMode);                  
        ClientToken = SubjectContext.ClientToken;                                                              
        v26 = v47[0];                                                                                          
        goto LABEL_33;                                                                                         
      }                                                                                                        
      if ( SubjectContext.ImpersonationLevel >= SecurityImpersonation )                                        
      {                                                                                                        
        goto LABEL_32;                                                                                         
      }                                                                                                        
      v28 = 0;                                                                                                 
    }                                                                                                          
    else                                                                                                       
    {                                                                                                          
      v28 = 1;                                                                                                 
    }                                                                                                          
LABEL_33:                                                                                                      
    if ( PreviousMode )                                                                                        
    {                                                                                                          
      PrimaryToken = SubjectContext.PrimaryToken;                                                              
      if ( ClientToken )                                                                                       
      {                                                                                                        
        PrimaryToken = ClientToken;                                                                            
      }                                                                                                        
      Sid2 = (PSID)**((_QWORD **)PrimaryToken + 19);                                                           
      if ( !RtlEqualSid(SeLocalSystemSid, Sid2) )                                                              
      {                                                                                                        
        v46 = SeExports;                                                                                       
        if ( !RtlEqualSid(SeExports->SeNetworkServiceSid, Sid2) && !RtlEqualSid(v46->SeLocalServiceSid, Sid2) )
        {                                                                                                      
          goto LABEL_46;                                                                                       
        }                                                                                                      
        v32 = 0;                                                                                               
        if ( v26 )                                                                                             
        {                                                                                                      
          v33 = &v48;                                                                                          
          v34 = v26;                                                                                           
          do                                                                                                   
          {                                                                                                    
            v35 = SepFilterPrivileges;                                                                         
            while ( v33->LowPart != *(_DWORD *)*v35 || v33->HighPart != *((_DWORD *)*v35 + 1) )                
            {                                                                                                  
              if ( !*++v35 )                                                                                   
              {                                                                                                
                goto LABEL_61;                                                                                 
              }                                                                                                
            }                                                                                                  
            ++v32;                                                                                             
LABEL_61:                                                                                                      
            v33 = (LUID *)((char *)v33 + 12);                                                                  
            --v34;                                                                                             
          }                                                                                                    
          while ( v34 );                                                                                       
          v36 = &v48;                                                                                          
          v37 = v26;                                                                                           
          do                                                                                                   
          {                                                                                                    
            v38 = &SepServicesFilterPrivileges;                                                                
            v39 = SepServicesFilterPrivileges;                                                                 
            while ( v36->LowPart != v39->LowPart || v36->HighPart != v39->HighPart )                           
            {                                                                                                  
              v39 = *++v38;                                                                                    
              if ( !*v38 )                                                                                     
              {                                                                                                
                goto LABEL_66;                                                                                 
              }                                                                                                
            }                                                                                                  
            ++v32;                                                                                             
LABEL_66:                                                                                                      
            v36 = (LUID *)((char *)v36 + 12);                                                                  
            --v37;                                                                                             
          }                                                                                                    
          while ( v37 );                                                                                       
          if ( v32 != v26 )                                                                                    
          {                                                                                                    
LABEL_46:                                                                                                      
            SepAdtPrivilegedServiceAuditAlarm(                                                                 
              (unsigned int)&SubjectContext,                                                                   
              (unsigned int)&SeSubsystemName,                                                                  
              0,                                                                                               
              (_DWORD)ClientToken,                                                                             
              (__int64)SubjectContext.PrimaryToken,                                                            
              (__int64)v47,                                                                                    
              v28);                                                                                            
          }                                                                                                    
        }                                                                                                      
      }                                                                                                        
    }                                                                                                          
    SeReleaseSubjectContext(&SubjectContext);                                                                  
    if ( !v28 )                                                                                                
    {                                                                                                          
      Information = -1073741727;                                                                               
      goto LABEL_14;                                                                                           
    }                                                                                                          
    goto LABEL_13;                                                                                             
  }                                                                                                            
  v21 = KeGetCurrentPrcb();                                                                                    
  v22 = v21->PPLookasideList[4].P;                                                                             
  ++v22->TotalFrees;                                                                                           
  if ( LOWORD(v22->ListHead.Alignment) >= v22->Depth )                                                         
  {                                                                                                            
    ++v22->FreeMisses;                                                                                         
    v22 = v21->PPLookasideList[4].L;                                                                           
    ++v22->TotalFrees;                                                                                         
    if ( LOWORD(v22->ListHead.Alignment) >= v22->Depth )                                                       
    {                                                                                                          
LABEL_41:                                                                                                      
      FreeEx = (void (__stdcall *)(PVOID))v22->FreeEx;                                                         
      ++v22->FreeMisses;                                                                                       
      if ( FreeEx == ExFreePool )                                                                              
      {                                                                                                        
        ExFreePool(v13);                                                                                       
      }                                                                                                        
      else                                                                                                     
      {                                                                                                        
        guard_dispatch_icall_no_overrides(v13, v21, v16);                                                      
      }                                                                                                        
      goto LABEL_24;                                                                                           
    }                                                                                                          
  }                                                                                                            
LABEL_23:                                                                                                      
  RtlpInterlockedPushEntrySList(&v22->ListHead, v13);                                                          
LABEL_24:                                                                                                      
  v23 = 0LL;                                                                                                   
LABEL_25:                                                                                                      
  if ( Information >= 0 )                                                                                      
  {                                                                                                            
    KeInitializeEvent(v23, a4, a5);                                                                            
    Information = ObInsertObjectEx(v23, 0, 0LL, (__int64)&v41);                                                
    if ( Information >= 0 )                                                                                    
    {                                                                                                          
      *v44 = v41;                                                                                              
    }                                                                                                          
  }                                                                                                            
  return (unsigned int)Information;                                                                            
}                                                                                                              
