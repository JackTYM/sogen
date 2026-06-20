__int64 __fastcall NtUserMoveWindow(__int64 a1, int a2, int a3, int a4, int a5, int a6)         | 
{                                                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                 | 
                                                                                                | 
  v10 = EnterCrit(0LL, 0LL);                                                                    | 
  v11 = ValidateHwnd(a1);                                                                       | 
  v12 = 0;                                                                                      | 
  v13 = (struct tagWND *)v11;                                                                   | 
  if ( v11 && (((*(_WORD *)(*(_QWORD *)(v11 + 40) + 42LL) & 0x2FFF) - 669) & 0xFFFFFFFD) != 0 ) | 
  {                                                                                             | 
    Win32HM_LockIntoThread<0>(v10, v11, BugCheckParameter3);                                    | 
    if ( a2 > 0x7FFF )                                                                          | 
    {                                                                                           | 
      a2 = 0x7FFF;                                                                              | 
    }                                                                                           | 
    else if ( a2 < -32768 )                                                                     | 
    {                                                                                           | 
      a2 = -32768;                                                                              | 
    }                                                                                           | 
    if ( a3 > 0x7FFF )                                                                          | 
    {                                                                                           | 
      a3 = 0x7FFF;                                                                              | 
    }                                                                                           | 
    else if ( a3 < -32768 )                                                                     | 
    {                                                                                           | 
      a3 = -32768;                                                                              | 
    }                                                                                           | 
    if ( a4 < 0 )                                                                               | 
    {                                                                                           | 
      a4 = 0;                                                                                   | 
    }                                                                                           | 
    else if ( a4 > 0x7FFF )                                                                     | 
    {                                                                                           | 
      a4 = 0x7FFF;                                                                              | 
    }                                                                                           | 
    v14 = a5;                                                                                   | 
    if ( a5 < 0 )                                                                               | 
    {                                                                                           | 
      v14 = 0;                                                                                  | 
    }                                                                                           | 
    else if ( a5 > 0x7FFF )                                                                     | 
    {                                                                                           | 
      v14 = 0x7FFF;                                                                             | 
    }                                                                                           | 
    CurrentThreadDpiAwarenessContext = W32GetCurrentThreadDpiAwarenessContext(4294934528LL);    | 
    if ( (unsigned __int8)ShouldVirtualizeWindowRect(v13, CurrentThreadDpiAwarenessContext) )   | 
    {                                                                                           | 
      v19 = a2;                                                                                 | 
      v21 = a4 + a2;                                                                            | 
      v20 = a3;                                                                                 | 
      v22 = a3 + v14;                                                                           | 
      TransformRectBetweenCoordinateSpaces(&v19, &v19, v13, 0LL);                               | 
      a4 = v21 - v19;                                                                           | 
      v14 = v22 - v20;                                                                          | 
    }                                                                                           | 
    v12 = xxxSetWindowPos(v13, a4, v14, a6 != 0 ? 20 : 28);                                     | 
    v16 = PtiCurrent();                                                                         | 
    Win32HM_UnlockFromThread<0>((ULONG_PTR)v16, (ULONG_PTR)BugCheckParameter3);                 | 
  }                                                                                             | 
  UserSessionSwitchLeaveCrit();                                                                 | 
  return v12;                                                                                   | 
}                                                                                               | 
62 row(s)
