__int64 __fastcall NtUserFindExistingCursorIcon(int *a1, ULONG64 a2, ULONG64 a3)                   | 
{                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                    | 
                                                                                                   | 
  v26 = 0LL;                                                                                       | 
  v23 = 0LL;                                                                                       | 
  v24 = 0LL;                                                                                       | 
  v25 = 0LL;                                                                                       | 
  EnterLeaveCritShared::EnterLeaveCritShared(v27, 1LL);                                            | 
  v6 = (int **)MmUserProbeAddress;                                                                 | 
  if ( a3 >= MmUserProbeAddress )                                                                  | 
  {                                                                                                | 
    a3 = MmUserProbeAddress;                                                                       | 
  }                                                                                                | 
  v24 = *(_OWORD *)a3;                                                                             | 
  v25 = *(_QWORD *)(a3 + 16);                                                                      | 
  v7 = 0LL;                                                                                        | 
  if ( (_QWORD)v24 )                                                                               | 
  {                                                                                                | 
    LOBYTE(v6) = 3;                                                                                | 
    v22 = (struct tagCURSOR *)HMValidateHandleWithDescriptor(v24, v6);                             | 
    if ( !v22 )                                                                                    | 
    {                                                                                              | 
      goto LABEL_37;                                                                               | 
    }                                                                                              | 
    v6 = (int **)MmUserProbeAddress;                                                               | 
  }                                                                                                | 
  else                                                                                             | 
  {                                                                                                | 
    v22 = 0LL;                                                                                     | 
  }                                                                                                | 
  if ( a1 >= *v6 )                                                                                 | 
  {                                                                                                | 
    a1 = *v6;                                                                                      | 
  }                                                                                                | 
  v10 = *a1;                                                                                       | 
  v20 = *a1;                                                                                       | 
  LODWORD(v26) = *a1;                                                                              | 
  v11 = (const unsigned __int16 *)*((_QWORD *)a1 + 1);                                             | 
  *((_QWORD *)&v26 + 1) = v11;                                                                     | 
  if ( ((unsigned __int8)v11 & 1) != 0 )                                                           | 
  {                                                                                                | 
    ExRaiseDatatypeMisalignment();                                                                 | 
  }                                                                                                | 
  v12 = (ULONG64)v11 + (unsigned __int16)v10 + 2;                                                  | 
  if ( v12 <= (unsigned __int64)v11 || v12 >= MmUserProbeAddress )                                 | 
  {                                                                                                | 
    ExRaiseAccessViolation();                                                                      | 
  }                                                                                                | 
  if ( (unsigned __int16)v10 > HIWORD(v20) )                                                       | 
  {                                                                                                | 
    if ( (v10 & 1) == 0 )                                                                          | 
    {                                                                                              | 
      goto LABEL_35;                                                                               | 
    }                                                                                              | 
    goto LABEL_34;                                                                                 | 
  }                                                                                                | 
  if ( (v10 & 1) != 0 )                                                                            | 
  {                                                                                                | 
LABEL_34:                                                                                          | 
    MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 7837LL);                    | 
LABEL_35:                                                                                          | 
    ExRaiseAccessViolation();                                                                      | 
  }                                                                                                | 
  if ( a2 >= MmUserProbeAddress )                                                                  | 
  {                                                                                                | 
    a2 = MmUserProbeAddress;                                                                       | 
  }                                                                                                | 
  v13 = *(_DWORD *)a2;                                                                             | 
  v21 = *(_DWORD *)a2;                                                                             | 
  *(_DWORD *)&v23.Length = *(_DWORD *)a2;                                                          | 
  v14 = *(WCHAR **)(a2 + 8);                                                                       | 
  v23.Buffer = v14;                                                                                | 
  if ( ((unsigned __int64)v14 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                      | 
  {                                                                                                | 
    if ( ((unsigned __int8)v14 & 1) != 0 )                                                         | 
    {                                                                                              | 
      ExRaiseDatatypeMisalignment();                                                               | 
    }                                                                                              | 
    v15 = (ULONG64)v14 + (unsigned __int16)v13 + 2;                                                | 
    if ( v15 <= (unsigned __int64)v14 || v15 >= MmUserProbeAddress )                               | 
    {                                                                                              | 
      ExRaiseAccessViolation();                                                                    | 
    }                                                                                              | 
    if ( (unsigned __int16)v13 > HIWORD(v21) || (v13 & 1) != 0 )                                   | 
    {                                                                                              | 
      if ( (v13 & 1) != 0 )                                                                        | 
      {                                                                                            | 
        MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 7840LL);                | 
      }                                                                                            | 
      ExRaiseAccessViolation();                                                                    | 
    }                                                                                              | 
  }                                                                                                | 
  v16 = RtlStringCchCopyW(v28, 0x100uLL, v11);                                                     | 
  if ( v16 < 0 )                                                                                   | 
  {                                                                                                | 
    UserSetLastStatus(v16);                                                                        | 
  }                                                                                                | 
  else                                                                                             | 
  {                                                                                                | 
    Atom = UserFindAtom(v28);                                                                      | 
    if ( Atom )                                                                                    | 
    {                                                                                              | 
      ExistingCursorIcon = _FindExistingCursorIcon(Atom, &v23, v22, (struct tagCURSORFIND *)&v24); | 
      if ( ExistingCursorIcon )                                                                    | 
      {                                                                                            | 
        v7 = *(_QWORD *)ExistingCursorIcon;                                                        | 
      }                                                                                            | 
    }                                                                                              | 
  }                                                                                                | 
LABEL_37:                                                                                          | 
  UserSessionSwitchLeaveCrit(v9, v8);                                                              | 
  return v7;                                                                                       | 
}                                                                                                  | 
114 row(s)
