__int64 __fastcall NtCreateSemaphore(__int64 *a1, __int64 a2, int a3, LONG a4, int Limit)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v25 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    v8 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )
    {
      v8 = (__int64)a1;
    }
    *(_QWORD *)v8 = *(_QWORD *)v8;
  }
  if ( Limit <= 0 || a4 < 0 || a4 > Limit )
  {
    return 3221225485LL;
  }
  v9 = ExSemaphoreObjectType;
  v26[0] = 0LL;
  v24 = 0LL;
  CurrentPrcb = KeGetCurrentPrcb();
  P = CurrentPrcb->PPLookasideList[4].P;
  ++P->TotalAllocates;
  v12 = RtlpInterlockedPopEntrySList(&P->ListHead);
  if ( !v12 )
  {
    ++P->AllocateMisses;
    L = CurrentPrcb->PPLookasideList[4].L;
    ++L->TotalAllocates;
    v12 = RtlpInterlockedPopEntrySList(&L->ListHead);
    if ( !v12 )
    {
      ++L->AllocateMisses;
      v12 = (PSLIST_ENTRY)guard_dispatch_icall_no_overrides((unsigned int)L->Type, L->Size, L->Tag);
    }
  }
  if ( v12 )
  {
    LODWORD(v12->Next) = CurrentPrcb->Number;
    Information = ObpCaptureObjectCreateInformation(PreviousMode, PreviousMode, a3, (unsigned int)v26, (__int64)v12, 0);
    if ( Information >= 0 )
    {
      if ( ((__int64)v12->Next & (_DWORD)v9[9]) != 0 )
      {
        Information = -1073741811;
      }
      else if ( ((__int64)v12->Next & 0x10) == 0 || SeSinglePrivilegeCheck(SeCreatePermanentPrivilege, PreviousMode) )
      {
        v16 = *((_DWORD *)v9 + 27);
        HIDWORD(v12[1].Next) = *((_DWORD *)v9 + 26);
        *((_DWORD *)&v12[1].Next + 2) = v16;
        Information = ObpAllocateObject(
                        (_DWORD)v12,
                        PreviousMode,
                        (_DWORD)v9,
                        (unsigned int)v26,
                        32,
                        (__int64)&v24,
                        0LL);
        if ( Information >= 0 )
        {
          v22 = v24;
          if ( ObpTraceFlags )
          {
            ObpRegisterObject(v24);
            ObpPushStackInfo(v22);
          }
          v20 = (struct _KSEMAPHORE *)(v22 + 48);
          goto LABEL_25;
        }
      }
      else
      {
        Information = -1073741727;
      }
      if ( *((_QWORD *)&v26[0] + 1) )
      {
        ObpFreeObjectNameBuffer(v26);
      }
      Next = v12[2].Next;
      if ( Next )
      {
        if ( LOBYTE(v12[1].Next) <= 1u )
        {
          ExFreePoolWithTag(Next, 0);
        }
        v12[2].Next = 0LL;
      }
    }
    v18 = KeGetCurrentPrcb();
    v19 = v18->PPLookasideList[4].P;
    ++v19->TotalFrees;
    if ( LOWORD(v19->ListHead.Alignment) < v19->Depth
      || (++v19->FreeMisses,
          v19 = v18->PPLookasideList[4].L,
          ++v19->TotalFrees,
          LOWORD(v19->ListHead.Alignment) < v19->Depth) )
    {
      RtlpInterlockedPushEntrySList(&v19->ListHead, v12);
    }
    else
    {
      ++v19->FreeMisses;
      FreeEx = (void (__stdcall *)(PVOID))v19->FreeEx;
      if ( FreeEx == ExFreePool )
      {
        ExFreePool(v12);
      }
      else
      {
        guard_dispatch_icall_no_overrides(v12, FreeEx, v15);
      }
    }
    v20 = 0LL;
  }
  else
  {
    Information = -1073741670;
    v20 = 0LL;
  }
LABEL_25:
  if ( Information >= 0 )
  {
    KeInitializeSemaphore(v20, a4, Limit);
    Information = ObInsertObjectEx(v20, 0, 0LL, (__int64)&v25);
    if ( Information >= 0 )
    {
      *a1 = v25;
    }
  }
  return (unsigned int)Information;
}
