__int64 __fastcall NtUserGetIconSize(__int64 a1, int a2, _DWORD *a3, _DWORD *a4)            | 
{                                                                                           | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                             | 
                                                                                            | 
  v8 = 0;                                                                                   | 
  v13 = 0;                                                                                  | 
  v14[0] = 0;                                                                               | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v15, 1LL);                                    | 
  LOBYTE(v9) = 3;                                                                           | 
  v10 = (struct tagCURSOR *)HMValidateHandleWithDescriptor(a1, v9);                         | 
  if ( v10 )                                                                                | 
  {                                                                                         | 
    VirtualizedDpiCursor = GetVirtualizedDpiCursor(v10);                                    | 
    if ( (*((_DWORD *)VirtualizedDpiCursor + 20) & 8) == 0                                  | 
      || (VirtualizedDpiCursor = GetAnimatedCursorFrame(VirtualizedDpiCursor, a2)) != 0LL ) | 
    {                                                                                       | 
      GetVirtualizedCursorSize(VirtualizedDpiCursor, &v13, v14);                            | 
      if ( (unsigned __int64)a3 >= MmUserProbeAddress )                                     | 
      {                                                                                     | 
        a3 = (_DWORD *)MmUserProbeAddress;                                                  | 
      }                                                                                     | 
      *a3 = v13;                                                                            | 
      if ( (unsigned __int64)a4 >= MmUserProbeAddress )                                     | 
      {                                                                                     | 
        a4 = (_DWORD *)MmUserProbeAddress;                                                  | 
      }                                                                                     | 
      *a4 = v14[0];                                                                         | 
      v8 = 1;                                                                               | 
      v14[1] = 1;                                                                           | 
    }                                                                                       | 
  }                                                                                         | 
  UserSessionSwitchLeaveCrit();                                                             | 
  return v8;                                                                                | 
}                                                                                           | 
34 row(s)
