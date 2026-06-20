NTSTATUS __stdcall NtAdjustPrivilegesToken(
        HANDLE TokenHandle,
        BOOLEAN DisableAllPrivileges,
        PTOKEN_PRIVILEGES NewState,
        ULONG BufferLength,
        PTOKEN_PRIVILEGES PreviousState,
        PULONG ReturnLength)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  PrivilegeCount = 0;
  P = 0LL;
  v36[0] = 0;
  v31 = 0;
  v33 = 0;
  v37 = 0;
  if ( !DisableAllPrivileges && !NewState )
  {
    return -1073741811;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v29 = PreviousMode;
  if ( PreviousMode )
  {
    if ( DisableAllPrivileges )
    {
      v11 = 0x7FFFFFFF0000LL;
    }
    else
    {
      if ( ((unsigned __int8)NewState & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v11 = 0x7FFFFFFF0000LL;
      PrivilegeCount = NewState->PrivilegeCount;
      v36[1] = 12 * (PrivilegeCount - 1) + 16;
    }
    v12 = PreviousState;
    if ( PreviousState )
    {
      ProbeForWrite(PreviousState, BufferLength, 4u);
      if ( (unsigned __int64)ReturnLength < 0x7FFFFFFF0000LL )
      {
        v11 = (__int64)ReturnLength;
      }
      *(_DWORD *)v11 = *(_DWORD *)v11;
    }
LABEL_13:
    if ( !DisableAllPrivileges )
    {
      result = SeCaptureLuidAndAttributesArray(
                 NewState->Privileges,
                 (int)Object,
                 (int)HandleInformation,
                 v28,
                 (__int64)&P,
                 (__int64)v36);
      v35 = result;
      if ( result < 0 )
      {
        return result;
      }
    }
    goto LABEL_15;
  }
  v12 = PreviousState;
  if ( !DisableAllPrivileges )
  {
    PrivilegeCount = NewState->PrivilegeCount;
    goto LABEL_13;
  }
LABEL_15:
  v32 = 0LL;
  v15 = ObReferenceObjectByHandle(
          TokenHandle,
          v12 != 0LL ? 40 : 32,
          (POBJECT_TYPE)SeTokenObjectType,
          PreviousMode,
          &v32,
          0LL);
  if ( v15 < 0 )
  {
    if ( P )
    {
      LOBYTE(v14) = PreviousMode;
      SeReleaseLuidAndAttributesArray(P, v14);
    }
    return v15;
  }
  else
  {
    CurrentThread = KeGetCurrentThread();
    --CurrentThread->KernelApcDisable;
    v17 = (PERESOURCE *)v32;
    ExAcquireResourceExclusiveLite(*((PERESOURCE *)v32 + 6), 1u);
    _InterlockedOr(v25, 0);
    v18 = P;
    LOBYTE(v19) = DisableAllPrivileges;
    v20 = v32;
    SepAdjustPrivileges(
      (_DWORD)v32,
      0,
      v19,
      PrivilegeCount,
      (__int64)P,
      (__int64)v12,
      (__int64)&v31,
      (__int64)&v33,
      (__int64)&v37);
    if ( v12 && (*ReturnLength = v31, v31 > BufferLength) )
    {
      _InterlockedOr(v25, 0);
      ExReleaseResourceLite(v17[6]);
      KeLeaveCriticalRegion();
      ObfDereferenceObject(v32);
      if ( P )
      {
        LOBYTE(v24) = PreviousMode;
        SeReleaseLuidAndAttributesArray(P, v24);
      }
      return -1073741789;
    }
    else
    {
      LOBYTE(v22) = DisableAllPrivileges;
      LOBYTE(v21) = 1;
      v35 = SepAdjustPrivileges(
              (_DWORD)v20,
              v21,
              v22,
              PrivilegeCount,
              (__int64)v18,
              (__int64)v12,
              (__int64)&v31,
              (__int64)&v33,
              (__int64)&v37);
      if ( v12 )
      {
        v23 = v33;
        v12->PrivilegeCount = v33;
        if ( !v23 )
        {
          v12->Privileges[0].Luid = 0LL;
          v12->Privileges[0].Attributes = 0;
        }
      }
      if ( v37 )
      {
        v20[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);
      }
      _InterlockedOr(v25, 0);
      ExReleaseResourceLite(v17[6]);
      KeLeaveCriticalRegion();
      ObfDereferenceObject(v32);
      if ( P )
      {
        if ( (unsigned __int8)PreviousMode <= 1u )
        {
          ExFreePoolWithTag(P, 0);
        }
      }
      return v35;
    }
  }
}
