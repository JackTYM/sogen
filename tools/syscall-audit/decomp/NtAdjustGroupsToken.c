NTSTATUS __stdcall NtAdjustGroupsToken(                                                                                                     | 
        HANDLE TokenHandle,                                                                                                                 | 
        BOOLEAN ResetToDefault,                                                                                                             | 
        PTOKEN_GROUPS NewState,                                                                                                             | 
        ULONG BufferLength,                                                                                                                 | 
        PTOKEN_GROUPS PreviousState,                                                                                                        | 
        PULONG ReturnLength)                                                                                                                | 
{                                                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                             | 
                                                                                                                                            | 
  v37 = BufferLength;                                                                                                                       | 
  v6 = NewState;                                                                                                                            | 
  GroupCount = 0;                                                                                                                           | 
  v35 = 0LL;                                                                                                                                | 
  v33 = 0;                                                                                                                                  | 
  v30 = 0LL;                                                                                                                                | 
  LOBYTE(v36) = 0;                                                                                                                          | 
  DestinationSid = 0LL;                                                                                                                     | 
  if ( !ResetToDefault && !NewState )                                                                                                       | 
  {                                                                                                                                         | 
    return -1073741811;                                                                                                                     | 
  }                                                                                                                                         | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                                                        | 
  v29 = PreviousMode;                                                                                                                       | 
  if ( PreviousMode )                                                                                                                       | 
  {                                                                                                                                         | 
    if ( ResetToDefault )                                                                                                                   | 
    {                                                                                                                                       | 
      v11 = 0x7FFFFFFF0000LL;                                                                                                               | 
    }                                                                                                                                       | 
    else                                                                                                                                    | 
    {                                                                                                                                       | 
      if ( ((unsigned __int8)NewState & 3) != 0 )                                                                                           | 
      {                                                                                                                                     | 
        ExRaiseDatatypeMisalignment();                                                                                                      | 
      }                                                                                                                                     | 
      v11 = 0x7FFFFFFF0000LL;                                                                                                               | 
    }                                                                                                                                       | 
    v12 = PreviousState;                                                                                                                    | 
    if ( PreviousState )                                                                                                                    | 
    {                                                                                                                                       | 
      ProbeForWrite(PreviousState, BufferLength, 4u);                                                                                       | 
      if ( (unsigned __int64)ReturnLength < 0x7FFFFFFF0000LL )                                                                              | 
      {                                                                                                                                     | 
        v11 = (__int64)ReturnLength;                                                                                                        | 
      }                                                                                                                                     | 
      *(_DWORD *)v11 = *(_DWORD *)v11;                                                                                                      | 
    }                                                                                                                                       | 
  }                                                                                                                                         | 
  else                                                                                                                                      | 
  {                                                                                                                                         | 
    v12 = PreviousState;                                                                                                                    | 
  }                                                                                                                                         | 
  if ( ResetToDefault                                                                                                                       | 
    || (GroupCount = v6->GroupCount,                                                                                                        | 
        v28 = (__int64)&v33,                                                                                                                | 
        v27 = (__int64)&v35,                                                                                                                | 
        LOBYTE(NewState) = PreviousMode,                                                                                                    | 
        result = SeCaptureSidAndAttributesArray(v6->Groups, (unsigned int)GroupCount, NewState, 0LL, 0),                                    | 
        v31 = result,                                                                                                                       | 
        result >= 0) )                                                                                                                      | 
  {                                                                                                                                         | 
    Object = 0LL;                                                                                                                           | 
    v15 = ObReferenceObjectByHandle(                                                                                                        | 
            TokenHandle,                                                                                                                    | 
            v12 != 0LL ? 72 : 64,                                                                                                           | 
            (POBJECT_TYPE)SeTokenObjectType,                                                                                                | 
            PreviousMode,                                                                                                                   | 
            &Object,                                                                                                                        | 
            0LL);                                                                                                                           | 
    if ( v15 < 0 )                                                                                                                          | 
    {                                                                                                                                       | 
      if ( v35 )                                                                                                                            | 
      {                                                                                                                                     | 
        LOBYTE(v14) = PreviousMode;                                                                                                         | 
        SeReleaseLuidAndAttributesArray(v35, v14);                                                                                          | 
      }                                                                                                                                     | 
      return v15;                                                                                                                           | 
    }                                                                                                                                       | 
    else                                                                                                                                    | 
    {                                                                                                                                       | 
      CurrentThread = KeGetCurrentThread();                                                                                                 | 
      --CurrentThread->KernelApcDisable;                                                                                                    | 
      v17 = (PERESOURCE *)Object;                                                                                                           | 
      ExAcquireResourceExclusiveLite(*((PERESOURCE *)Object + 6), 1u);                                                                      | 
      _InterlockedOr(v26, 0);                                                                                                               | 
      v18 = v35;                                                                                                                            | 
      LOBYTE(v19) = ResetToDefault;                                                                                                         | 
      v20 = Object;                                                                                                                         | 
      v21 = SepAdjustGroups(                                                                                                                | 
              (int)Object,                                                                                                                  | 
              0,                                                                                                                            | 
              v19,                                                                                                                          | 
              GroupCount,                                                                                                                   | 
              v35,                                                                                                                          | 
              (__int64)v12,                                                                                                                 | 
              0LL,                                                                                                                          | 
              (__int64)&v30,                                                                                                                | 
              (__int64)&v30 + 4,                                                                                                            | 
              (__int64)&v36);                                                                                                               | 
      v31 = v21;                                                                                                                            | 
      if ( v12 )                                                                                                                            | 
      {                                                                                                                                     | 
        v22 = (int)ReturnLength;                                                                                                            | 
        *ReturnLength = v30;                                                                                                                | 
      }                                                                                                                                     | 
      if ( v21 < 0 )                                                                                                                        | 
      {                                                                                                                                     | 
        goto LABEL_24;                                                                                                                      | 
      }                                                                                                                                     | 
      if ( !v12 )                                                                                                                           | 
      {                                                                                                                                     | 
LABEL_20:                                                                                                                                   | 
        LOBYTE(v23) = ResetToDefault;                                                                                                       | 
        LOBYTE(v22) = 1;                                                                                                                    | 
        v31 = SepAdjustGroups(                                                                                                              | 
                (int)v20,                                                                                                                   | 
                v22,                                                                                                                        | 
                v23,                                                                                                                        | 
                GroupCount,                                                                                                                 | 
                v18,                                                                                                                        | 
                (__int64)v12,                                                                                                               | 
                DestinationSid,                                                                                                             | 
                (__int64)&v30,                                                                                                              | 
                (__int64)&v30 + 4,                                                                                                          | 
                (__int64)&v36);                                                                                                             | 
        if ( v12 )                                                                                                                          | 
        {                                                                                                                                   | 
          v12->GroupCount = HIDWORD(v30);                                                                                                   | 
        }                                                                                                                                   | 
        if ( (_BYTE)v36 )                                                                                                                   | 
        {                                                                                                                                   | 
          v20[7] = ExpLuidIncrement + _InterlockedExchangeAdd64(&ExpLuid, ExpLuidIncrement);                                                | 
        }                                                                                                                                   | 
LABEL_24:                                                                                                                                   | 
        _InterlockedOr(v26, 0);                                                                                                             | 
        ExReleaseResourceLite(v17[6]);                                                                                                      | 
        KeLeaveCriticalRegion();                                                                                                            | 
        ObfDereferenceObject(Object);                                                                                                       | 
        if ( v35 )                                                                                                                          | 
        {                                                                                                                                   | 
          LOBYTE(v24) = PreviousMode;                                                                                                       | 
          SeReleaseLuidAndAttributesArray(v35, v24);                                                                                        | 
        }                                                                                                                                   | 
        return v31;                                                                                                                         | 
      }                                                                                                                                     | 
      if ( (unsigned int)v30 <= v37 )                                                                                                       | 
      {                                                                                                                                     | 
        DestinationSid = (void *)(((unsigned __int64)&v12->Groups[0].Sid + (unsigned int)(16 * HIDWORD(v30)) + 3) & 0xFFFFFFFFFFFFFFFCuLL); | 
        goto LABEL_20;                                                                                                                      | 
      }                                                                                                                                     | 
      _InterlockedOr(v26, 0);                                                                                                               | 
      ExReleaseResourceLite(v17[6]);                                                                                                        | 
      KeLeaveCriticalRegion();                                                                                                              | 
      ObfDereferenceObject(Object);                                                                                                         | 
      if ( v35 )                                                                                                                            | 
      {                                                                                                                                     | 
        LOBYTE(v25) = PreviousMode;                                                                                                         | 
        SeReleaseLuidAndAttributesArray(v35, v25);                                                                                          | 
      }                                                                                                                                     | 
      return -1073741789;                                                                                                                   | 
    }                                                                                                                                       | 
  }                                                                                                                                         | 
  return result;                                                                                                                            | 
}                                                                                                                                           | 
165 row(s)
