__int64 __fastcall NtUserBuildHwndList(                                                             | 
        __int64 a1,                                                                                 | 
        __int64 a2,                                                                                 | 
        int a3,                                                                                     | 
        int a4,                                                                                     | 
        int a5,                                                                                     | 
        unsigned int a6,                                                                            | 
        volatile void *Address,                                                                     | 
        unsigned int *a8)                                                                           | 
{                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                     | 
                                                                                                    | 
  v35 = a3;                                                                                         | 
  Object = 0LL;                                                                                     | 
  v11 = 0LL;                                                                                        | 
  v25[0] = 0LL;                                                                                     | 
  v25[1] = 0LL;                                                                                     | 
  v27 = 0;                                                                                          | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v32, 1LL);                                            | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v33);                          | 
  v13 = 2 * (**(_DWORD **)(W32GetUserSessionState() + 20016) & 4 | 1);                              | 
  if ( a4 && !(unsigned int)CallerCanSeeImmersiveWindows() )                                        | 
  {                                                                                                 | 
    v13 |= 0x10u;                                                                                   | 
  }                                                                                                 | 
  if ( a2 )                                                                                         | 
  {                                                                                                 | 
    ThreadDesktopWindow = (struct tagWND *)ValidateHwnd(a2);                                        | 
    if ( !ThreadDesktopWindow )                                                                     | 
    {                                                                                               | 
LABEL_6:                                                                                            | 
      v15 = -1073741816;                                                                            | 
      goto LABEL_38;                                                                                | 
    }                                                                                               | 
  }                                                                                                 | 
  else                                                                                              | 
  {                                                                                                 | 
    ThreadDesktopWindow = 0LL;                                                                      | 
  }                                                                                                 | 
  if ( a5 )                                                                                         | 
  {                                                                                                 | 
    v16 = PtiFromThreadId();                                                                        | 
    v17 = v16;                                                                                      | 
    if ( !v16 )                                                                                     | 
    {                                                                                               | 
      goto LABEL_22;                                                                                | 
    }                                                                                               | 
    v18 = *(_QWORD *)(v16 + 496);                                                                   | 
    if ( !v18 )                                                                                     | 
    {                                                                                               | 
      goto LABEL_22;                                                                                | 
    }                                                                                               | 
    ThreadDesktopWindow = *(struct tagWND **)(*(_QWORD *)(*(_QWORD *)(v18 + 8) + 24LL) + 112LL);    | 
  }                                                                                                 | 
  else                                                                                              | 
  {                                                                                                 | 
    v17 = 0LL;                                                                                      | 
  }                                                                                                 | 
  if ( a1 )                                                                                         | 
  {                                                                                                 | 
    LOBYTE(v12) = 1;                                                                                | 
    if ( (int)ValidateHdesk(a1, v12, 1LL, &Object) < 0 )                                            | 
    {                                                                                               | 
      goto LABEL_6;                                                                                 | 
    }                                                                                               | 
    v30 = 0;                                                                                        | 
    v31 = 0;                                                                                        | 
    v29 = Object;                                                                                   | 
    v26 = 1;                                                                                        | 
    CurrentProcess = PsGetCurrentProcess();                                                         | 
    v15 = MapDesktop(v25);                                                                          | 
    if ( v15 < 0 )                                                                                  | 
    {                                                                                               | 
      v19 = 6LL;                                                                                    | 
      goto LABEL_17;                                                                                | 
    }                                                                                               | 
    ThreadDesktopWindow = *(struct tagWND **)(*(_QWORD *)(*((_QWORD *)Object + 1) + 24LL) + 112LL); | 
  }                                                                                                 | 
  if ( ThreadDesktopWindow )                                                                        | 
  {                                                                                                 | 
    if ( v35 )                                                                                      | 
    {                                                                                               | 
      v13 |= 1u;                                                                                    | 
LABEL_25:                                                                                           | 
      ThreadDesktopWindow = (struct tagWND *)*((_QWORD *)ThreadDesktopWindow + 14);                 | 
    }                                                                                               | 
  }                                                                                                 | 
  else if ( !Object )                                                                               | 
  {                                                                                                 | 
    ThreadDesktopWindow = (struct tagWND *)GetThreadDesktopWindow(v17);                             | 
    if ( !ThreadDesktopWindow )                                                                     | 
    {                                                                                               | 
LABEL_22:                                                                                           | 
      v15 = -1073741816;                                                                            | 
      UserSetLastError(87LL);                                                                       | 
      goto LABEL_38;                                                                                | 
    }                                                                                               | 
    goto LABEL_25;                                                                                  | 
  }                                                                                                 | 
  v20 = BuildHwndList(ThreadDesktopWindow, v13);                                                    | 
  v11 = v20;                                                                                        | 
  if ( v20 )                                                                                        | 
  {                                                                                                 | 
    v21 = ((*(_QWORD *)(v20 + 8) - v20 - 32) >> 3) + 1;                                             | 
    ProbeForWrite(Address, 8LL * a6, 4u);                                                           | 
    v22 = a8;                                                                                       | 
    if ( (unsigned __int64)a8 >= MmUserProbeAddress )                                               | 
    {                                                                                               | 
      v22 = (_DWORD *)MmUserProbeAddress;                                                           | 
    }                                                                                               | 
    *v22 = *v22;                                                                                    | 
    if ( v21 > a6 )                                                                                 | 
    {                                                                                               | 
      v24 = -1073741789;                                                                            | 
    }                                                                                               | 
    else                                                                                            | 
    {                                                                                               | 
      RtlCopyVolatileMemory((void *)Address, (const void *)(v11 + 32), 8LL * v21);                  | 
      v24 = 0;                                                                                      | 
    }                                                                                               | 
    *a8 = v21;                                                                                      | 
    v15 = v24;                                                                                      | 
    goto LABEL_34;                                                                                  | 
  }                                                                                                 | 
  v15 = -1073741816;                                                                                | 
  v19 = 8LL;                                                                                        | 
LABEL_17:                                                                                           | 
  UserSetLastError(v19);                                                                            | 
LABEL_34:                                                                                           | 
  if ( v11 )                                                                                        | 
  {                                                                                                 | 
    FreeHwndList(v11);                                                                              | 
  }                                                                                                 | 
  if ( Object )                                                                                     | 
  {                                                                                                 | 
    ObfDereferenceObject(Object);                                                                   | 
  }                                                                                                 | 
LABEL_38:                                                                                           | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v33);                                        | 
  UserSessionSwitchLeaveCrit();                                                                     | 
  return (unsigned int)v15;                                                                         | 
}                                                                                                   | 
142 row(s)
