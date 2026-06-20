__int64 __fastcall NtUserGetProcessUIContextInformation(HANDLE Handle, void *a2)                   | 
{                                                                                                  | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                    | 
                                                                                                   | 
  v4 = 0LL;                                                                                        | 
  v17 = 0LL;                                                                                       | 
  Src = 0LL;                                                                                       | 
  v5 = 1;                                                                                          | 
  EnterLeaveCritShared::EnterLeaveCritShared(v14, 1LL);                                            | 
  if ( Handle == (HANDLE)-1LL )                                                                    | 
  {                                                                                                | 
    CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                                | 
    v7 = CurrentProcessWin32Process;                                                               | 
    if ( CurrentProcessWin32Process )                                                              | 
    {                                                                                              | 
      v7 = -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL) & CurrentProcessWin32Process;  | 
    }                                                                                              | 
  }                                                                                                | 
  else                                                                                             | 
  {                                                                                                | 
    Object = 0LL;                                                                                  | 
    v8 = ObReferenceObjectByHandle(Handle, 0x1000u, (POBJECT_TYPE)PsProcessType, 1, &Object, 0LL); | 
    v4 = Object;                                                                                   | 
    v17 = Object;                                                                                  | 
    if ( v8 < 0 )                                                                                  | 
    {                                                                                              | 
      UserSetLastError(87LL);                                                                      | 
      v5 = 0;                                                                                      | 
      goto LABEL_21;                                                                               | 
    }                                                                                              | 
    ProcessSessionId = PsGetProcessSessionIdEx(Object);                                            | 
    if ( ProcessSessionId != (unsigned int)W32GetCurrentWin32kSessionId() )                        | 
    {                                                                                              | 
      v10 = 87LL;                                                                                  | 
      goto LABEL_18;                                                                               | 
    }                                                                                              | 
    ProcessWin32Process = PsGetProcessWin32Process(v4);                                            | 
    v7 = ProcessWin32Process;                                                                      | 
    if ( ProcessWin32Process )                                                                     | 
    {                                                                                              | 
      v7 = -(__int64)(*(_QWORD *)ProcessWin32Process != 0LL) & ProcessWin32Process;                | 
    }                                                                                              | 
  }                                                                                                | 
  if ( v7 && (*(_DWORD *)(v7 + 12) & 0x21) != 0 )                                                  | 
  {                                                                                                | 
    Src = (*(_DWORD *)(v7 + 800) >> 4) & 3LL;                                                      | 
    v12 = (*(_BYTE *)(v7 + 800) & 0x40) != 0;                                                      | 
    HIDWORD(Src) = v12;                                                                            | 
    if ( (*(_DWORD *)(v7 + 800) & 0x100LL) != 0 )                                                  | 
    {                                                                                              | 
      HIDWORD(Src) = v12 | 2;                                                                      | 
    }                                                                                              | 
    if ( (unsigned __int64)a2 >= MmUserProbeAddress )                                              | 
    {                                                                                              | 
      a2 = (void *)MmUserProbeAddress;                                                             | 
    }                                                                                              | 
    RtlCopyVolatileMemory(a2, &Src, 8uLL);                                                         | 
    goto LABEL_19;                                                                                 | 
  }                                                                                                | 
  v10 = 1471LL;                                                                                    | 
LABEL_18:                                                                                          | 
  UserSetLastError(v10);                                                                           | 
  v5 = 0;                                                                                          | 
LABEL_19:                                                                                          | 
  if ( v4 )                                                                                        | 
  {                                                                                                | 
    ObfDereferenceObject(v4);                                                                      | 
  }                                                                                                | 
LABEL_21:                                                                                          | 
  UserSessionSwitchLeaveCrit();                                                                    | 
  return v5;                                                                                       | 
}                                                                                                  | 
72 row(s)
