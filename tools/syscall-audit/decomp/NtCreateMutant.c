__int64 __fastcall NtCreateMutant(__int64 *a1, __int64 a2, int a3, char a4)                                   
{                                                                                                             
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                               
                                                                                                              
  v26 = 0LL;                                                                                                  
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
  v8 = ExMutantObjectType;                                                                                    
  v27 = 0LL;                                                                                                  
  v25 = 0LL;                                                                                                  
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
    v22 = 0LL;                                                                                                
    goto LABEL_22;                                                                                            
  }                                                                                                           
  LODWORD(v13->Next) = CurrentPrcb->Number;                                                                   
  LOBYTE(v11) = PreviousMode;                                                                                 
  LOBYTE(v12) = PreviousMode;                                                                                 
  Information = ObpCaptureObjectCreateInformation(v12, v11, a3, (unsigned int)&v27, (__int64)v13, 0);         
  if ( Information < 0 )                                                                                      
  {                                                                                                           
    goto LABEL_19;                                                                                            
  }                                                                                                           
  if ( ((__int64)v13->Next & v8->TypeInfo.InvalidAttributes) != 0 )                                           
  {                                                                                                           
    Information = -1073741811;                                                                                
    goto LABEL_13;                                                                                            
  }                                                                                                           
  if ( ((__int64)v13->Next & 0x10) != 0 && !SeSinglePrivilegeCheck(SeCreatePermanentPrivilege, PreviousMode) )
  {                                                                                                           
    Information = -1073741727;                                                                                
    goto LABEL_13;                                                                                            
  }                                                                                                           
  DefaultNonPagedPoolCharge = v8->TypeInfo.DefaultNonPagedPoolCharge;                                         
  HIDWORD(v13[1].Next) = v8->TypeInfo.DefaultPagedPoolCharge;                                                 
  *((_DWORD *)&v13[1].Next + 2) = DefaultNonPagedPoolCharge;                                                  
  LOBYTE(v15) = PreviousMode;                                                                                 
  Information = ObpAllocateObject((_DWORD)v13, v15, (_DWORD)v8, (unsigned int)&v27, 56, (__int64)&v25, 0LL);  
  if ( Information < 0 )                                                                                      
  {                                                                                                           
LABEL_13:                                                                                                     
    if ( *((_QWORD *)&v27 + 1) )                                                                              
    {                                                                                                         
      ObpFreeObjectNameBuffer(&v27);                                                                          
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
LABEL_19:                                                                                                     
    v20 = KeGetCurrentPrcb();                                                                                 
    v21 = v20->PPLookasideList[4].P;                                                                          
    ++v21->TotalFrees;                                                                                        
    if ( LOWORD(v21->ListHead.Alignment) < v21->Depth                                                         
      || (++v21->FreeMisses,                                                                                  
          v21 = v20->PPLookasideList[4].L,                                                                    
          ++v21->TotalFrees,                                                                                  
          LOWORD(v21->ListHead.Alignment) < v21->Depth) )                                                     
    {                                                                                                         
      RtlpInterlockedPushEntrySList(&v21->ListHead, v13);                                                     
    }                                                                                                         
    else                                                                                                      
    {                                                                                                         
      ++v21->FreeMisses;                                                                                      
      FreeEx = (void (__stdcall *)(PVOID))v21->FreeEx;                                                        
      if ( FreeEx == ExFreePool )                                                                             
      {                                                                                                       
        ExFreePool(v13);                                                                                      
      }                                                                                                       
      else                                                                                                    
      {                                                                                                       
        guard_dispatch_icall_no_overrides(v13, FreeEx, v17);                                                  
      }                                                                                                       
    }                                                                                                         
    v22 = 0LL;                                                                                                
    goto LABEL_22;                                                                                            
  }                                                                                                           
  if ( ObpTraceFlags )                                                                                        
  {                                                                                                           
    ObpRegisterObject(v25);                                                                                   
    ObpPushStackInfo(v25);                                                                                    
  }                                                                                                           
  v22 = (void *)(v25 + 48);                                                                                   
LABEL_22:                                                                                                     
  if ( Information >= 0 )                                                                                     
  {                                                                                                           
    LOBYTE(v11) = a4;                                                                                         
    KeInitializeMutantEx(v22, v11, ExpForceEnableMutantAutoboost != 0);                                       
    Information = ObInsertObjectEx(v22, 0, 0LL, (__int64)&v26);                                               
    LODWORD(v25) = Information;                                                                               
    if ( Information >= 0 )                                                                                   
    {                                                                                                         
      *a1 = v26;                                                                                              
    }                                                                                                         
  }                                                                                                           
  return (unsigned int)Information;                                                                           
}                                                                                                             
