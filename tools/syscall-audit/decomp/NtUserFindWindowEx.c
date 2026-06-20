__int64 __fastcall NtUserFindWindowEx(__int64 a1, __int64 a2, ULONG64 a3, int *a4)                       | 
{                                                                                                        | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                          | 
                                                                                                         | 
  EnterLeaveCritShared::EnterLeaveCritShared(v27, 1LL);                                                  | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v28);                               | 
  if ( a1 == -3 )                                                                                        | 
  {                                                                                                      | 
    v8 = 0LL;                                                                                            | 
    v10 = PtiCurrent();                                                                                  | 
    if ( v10 )                                                                                           | 
    {                                                                                                    | 
      v11 = *((_QWORD *)v10 + 62);                                                                       | 
      if ( v11 )                                                                                         | 
      {                                                                                                  | 
        v8 = *(struct tagWND **)(v11 + 112);                                                             | 
      }                                                                                                  | 
    }                                                                                                    | 
  }                                                                                                      | 
  else if ( a1 )                                                                                         | 
  {                                                                                                      | 
    v8 = (struct tagWND *)ValidateHwnd(a1);                                                              | 
    if ( !v8 )                                                                                           | 
    {                                                                                                    | 
LABEL_4:                                                                                                 | 
      v9 = 0LL;                                                                                          | 
      goto LABEL_59;                                                                                     | 
    }                                                                                                    | 
  }                                                                                                      | 
  else                                                                                                   | 
  {                                                                                                      | 
    v8 = 0LL;                                                                                            | 
  }                                                                                                      | 
  if ( a2 )                                                                                              | 
  {                                                                                                      | 
    v12 = (struct tagWND *)ValidateHwnd(a2);                                                             | 
    if ( !v12 )                                                                                          | 
    {                                                                                                    | 
      goto LABEL_4;                                                                                      | 
    }                                                                                                    | 
  }                                                                                                      | 
  else                                                                                                   | 
  {                                                                                                      | 
    v12 = 0LL;                                                                                           | 
  }                                                                                                      | 
  Atom = 0;                                                                                              | 
  v26 = 0LL;                                                                                             | 
  v24 = 0;                                                                                               | 
  if ( a3 >= MmUserProbeAddress )                                                                        | 
  {                                                                                                      | 
    a3 = MmUserProbeAddress;                                                                             | 
  }                                                                                                      | 
  v13 = *(_DWORD *)a3;                                                                                   | 
  v14 = *(const unsigned __int16 **)(a3 + 8);                                                            | 
  if ( ((unsigned __int64)v14 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                            | 
  {                                                                                                      | 
    if ( ((unsigned __int8)v14 & 1) != 0 )                                                               | 
    {                                                                                                    | 
      ExRaiseDatatypeMisalignment();                                                                     | 
    }                                                                                                    | 
    v15 = (ULONG64)v14 + (unsigned __int16)v13 + 2;                                                      | 
    if ( v15 <= (unsigned __int64)v14 || (v16 = (int **)MmUserProbeAddress, v15 >= MmUserProbeAddress) ) | 
    {                                                                                                    | 
      ExRaiseAccessViolation();                                                                          | 
    }                                                                                                    | 
    if ( (unsigned __int16)v13 > (unsigned __int16)HIWORD(*(_DWORD *)a3) || (v13 & 1) != 0 )             | 
    {                                                                                                    | 
      if ( (v13 & 1) != 0 )                                                                              | 
      {                                                                                                  | 
        MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 9617LL);                      | 
      }                                                                                                  | 
      ExRaiseAccessViolation();                                                                          | 
    }                                                                                                    | 
  }                                                                                                      | 
  else                                                                                                   | 
  {                                                                                                      | 
    v16 = (int **)MmUserProbeAddress;                                                                    | 
  }                                                                                                      | 
  if ( v14 && ((unsigned __int64)v14 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                     | 
  {                                                                                                      | 
    if ( (int)RtlStringCchCopyW(v29, 0x100uLL, v14) >= 0 )                                               | 
    {                                                                                                    | 
      Atom = UserFindAtom(v29);                                                                          | 
    }                                                                                                    | 
    if ( !Atom )                                                                                         | 
    {                                                                                                    | 
      v9 = 0LL;                                                                                          | 
      goto LABEL_59;                                                                                     | 
    }                                                                                                    | 
    v16 = (int **)MmUserProbeAddress;                                                                    | 
  }                                                                                                      | 
  else                                                                                                   | 
  {                                                                                                      | 
    Atom = *(_QWORD *)(a3 + 8);                                                                          | 
  }                                                                                                      | 
  v17 = *v16;                                                                                            | 
  if ( a4 >= v17 )                                                                                       | 
  {                                                                                                      | 
    a4 = v17;                                                                                            | 
  }                                                                                                      | 
  v18 = *a4;                                                                                             | 
  v19 = (const void *)*((_QWORD *)a4 + 1);                                                               | 
  if ( ((unsigned __int8)v19 & 1) != 0 )                                                                 | 
  {                                                                                                      | 
    ExRaiseDatatypeMisalignment();                                                                       | 
  }                                                                                                      | 
  v20 = (ULONG64)v19 + (unsigned __int16)v18 + 2;                                                        | 
  if ( v20 <= (unsigned __int64)v19 || v20 >= MmUserProbeAddress )                                       | 
  {                                                                                                      | 
    ExRaiseAccessViolation();                                                                            | 
  }                                                                                                      | 
  if ( (unsigned __int16)v18 > (unsigned __int16)HIWORD(*a4) )                                           | 
  {                                                                                                      | 
    if ( (v18 & 1) == 0 )                                                                                | 
    {                                                                                                    | 
      goto LABEL_55;                                                                                     | 
    }                                                                                                    | 
    goto LABEL_54;                                                                                       | 
  }                                                                                                      | 
  if ( (v18 & 1) != 0 )                                                                                  | 
  {                                                                                                      | 
LABEL_54:                                                                                                | 
    MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 9642LL);                          | 
LABEL_55:                                                                                                | 
    ExRaiseAccessViolation();                                                                            | 
  }                                                                                                      | 
  if ( v19 )                                                                                             | 
  {                                                                                                      | 
    v21 = (unsigned __int16)v18 >> 1;                                                                    | 
    if ( (unsigned int)(v21 + 1) > 0x100 )                                                               | 
    {                                                                                                    | 
      v26 = (unsigned __int16 *)Win32AllocPoolWithQuotaZInit((unsigned __int16)v18 + 2LL, 2020897621LL); | 
      if ( !v26 )                                                                                        | 
      {                                                                                                  | 
        ExRaiseStatus(-1073741801);                                                                      | 
      }                                                                                                  | 
      v24 = 1;                                                                                           | 
    }                                                                                                    | 
    else                                                                                                 | 
    {                                                                                                    | 
      v26 = v29;                                                                                         | 
    }                                                                                                    | 
    memmove(v26, v19, (unsigned __int16)v18);                                                            | 
    v26[v21] = 0;                                                                                        | 
  }                                                                                                      | 
  Window = _FindWindowEx(v8, v12, Atom, v26);                                                            | 
  if ( Window )                                                                                          | 
  {                                                                                                      | 
    v9 = *(_QWORD *)Window;                                                                              | 
  }                                                                                                      | 
  else                                                                                                   | 
  {                                                                                                      | 
    v9 = 0LL;                                                                                            | 
  }                                                                                                      | 
  if ( v24 )                                                                                             | 
  {                                                                                                      | 
    Win32FreePool(v26);                                                                                  | 
  }                                                                                                      | 
LABEL_59:                                                                                                | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v28);                                             | 
  UserSessionSwitchLeaveCrit();                                                                          | 
  return v9;                                                                                             | 
}                                                                                                        | 
163 row(s)
