__int64 __fastcall NtUserSetWindowPos(__int64 a1, HWND a2, int a3, int a4, int a5, int a6, unsigned int a7) | 
{                                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                             | 
                                                                                                            | 
  v28 = a4;                                                                                                 | 
  v27 = a3;                                                                                                 | 
  v9 = 0;                                                                                                   | 
  v25[0] = 0LL;                                                                                             | 
  v10 = EnterCrit(0LL, 0LL);                                                                                | 
  v11 = ValidateHwnd(a1);                                                                                   | 
  v12 = (struct tagWND *)v11;                                                                               | 
  if ( v11 && (((*(_WORD *)(*(_QWORD *)(v11 + 40) + 42LL) & 0x2FFF) - 669) & 0xFFFFFFFD) != 0 )             | 
  {                                                                                                         | 
    Win32HM_LockIntoThread<0>(v10, v11, BugCheckParameter3);                                                | 
    v13 = a7;                                                                                               | 
    if ( (a7 & 0xFFFD9800) != 0 )                                                                           | 
    {                                                                                                       | 
      v14 = 1004LL;                                                                                         | 
    }                                                                                                       | 
    else                                                                                                    | 
    {                                                                                                       | 
      if ( !(unsigned int)ValidateHWNDIA(a2, (struct tagWND **)v25) )                                       | 
      {                                                                                                     | 
        goto LABEL_30;                                                                                      | 
      }                                                                                                     | 
      if ( *(char *)(*((_QWORD *)v12 + 5) + 20LL) >= 0 )                                                    | 
      {                                                                                                     | 
        if ( (v13 & 2) == 0 )                                                                               | 
        {                                                                                                   | 
          ConstrainWindowPos(&v27, &v28);                                                                   | 
        }                                                                                                   | 
        if ( (v13 & 1) == 0 )                                                                               | 
        {                                                                                                   | 
          ConstrainWindowSize(&a5, &a6);                                                                    | 
        }                                                                                                   | 
        v15 = 0LL;                                                                                          | 
        if ( v25[0] - 2 <= 0xFFFFFFFFFFFFFFFBuLL )                                                          | 
        {                                                                                                   | 
          v15 = v25[0];                                                                                     | 
        }                                                                                                   | 
        CurrentThreadDpiAwarenessContext = W32GetCurrentThreadDpiAwarenessContext();                        | 
        if ( (unsigned __int8)ShouldVirtualizeWindowRect(v12, CurrentThreadDpiAwarenessContext) )           | 
        {                                                                                                   | 
          TransformSWPCoords(v12, &v27, &v28, &a5, &a6, v13);                                               | 
        }                                                                                                   | 
        if ( (v13 & 0x43) == 0x43                                                                           | 
          && (*(_BYTE *)(*((_QWORD *)v12 + 5) + 31LL) & 0x10) == 0                                          | 
          && ShellWindowManagement::ExtendedBehaviorEnabled((ShellWindowManagement *)0x40000000, v17)       | 
          && (unsigned __int8)ShellWindowManagement::WindowSubjectToBehavior(v12, 1LL, 0LL) )               | 
        {                                                                                                   | 
          if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                 | 
            || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 0x800) == 0                            | 
            || (v19 = 1, *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 4u) )                          | 
          {                                                                                                 | 
            v19 = 0;                                                                                        | 
          }                                                                                                 | 
          v20 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                  | 
          if ( v19 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )            | 
          {                                                                                                 | 
            UserSessionState = W32GetUserSessionState();                                                    | 
            LOBYTE(v22) = v20;                                                                              | 
            LOBYTE(v23) = v19;                                                                              | 
            WPP_RECORDER_AND_TRACE_SF_q(                                                                    | 
              *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                           | 
              v23,                                                                                          | 
              v22,                                                                                          | 
              *(_QWORD *)(UserSessionState + 69512),                                                        | 
              4,                                                                                            | 
              12,                                                                                           | 
              11,                                                                                           | 
              (__int64)&WPP_ee910b509ed73fb465c6a84e13eeb380_Traceguids,                                    | 
              (char)v12);                                                                                   | 
          }                                                                                                 | 
          LOBYTE(v18) = 5;                                                                                  | 
          xxxShowWindowEx(v12, v18, 0LL);                                                                   | 
          v9 = 1;                                                                                           | 
        }                                                                                                   | 
        else                                                                                                | 
        {                                                                                                   | 
          Win32HM_LockIntoThread<1>(v10, v15, v25);                                                         | 
          v9 = xxxSetWindowPos(v12, a5, a6, v13);                                                           | 
          Win32HMThreadLock<tagCURSOR>::~Win32HMThreadLock<tagCURSOR>((ULONG_PTR)v25);                      | 
        }                                                                                                   | 
        goto LABEL_30;                                                                                      | 
      }                                                                                                     | 
      v14 = 87LL;                                                                                           | 
    }                                                                                                       | 
    UserSetLastError(v14);                                                                                  | 
LABEL_30:                                                                                                   | 
    Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3);       | 
  }                                                                                                         | 
  UserSessionSwitchLeaveCrit();                                                                             | 
  return v9;                                                                                                | 
}                                                                                                           | 
94 row(s)
