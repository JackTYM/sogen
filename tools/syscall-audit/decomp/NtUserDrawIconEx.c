__int64 __fastcall NtUserDrawIconEx(                                                                           | 
        __int64 a1,                                                                                            | 
        unsigned int a2,                                                                                       | 
        unsigned int a3,                                                                                       | 
        __int64 a4,                                                                                            | 
        int a5,                                                                                                | 
        int a6,                                                                                                | 
        int a7,                                                                                                | 
        __int64 a8,                                                                                            | 
        int a9,                                                                                                | 
        int a10,                                                                                               | 
        ULONG64 a11)                                                                                           | 
{                                                                                                              | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                | 
                                                                                                               | 
  EnterCrit(0LL, 1LL);                                                                                         | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v22);                                     | 
  LOBYTE(v15) = 3;                                                                                             | 
  v16 = (struct tagCURSOR *)HMValidateHandleWithDescriptor(a4, v15);                                           | 
  v17 = 0LL;                                                                                                   | 
  if ( v16 )                                                                                                   | 
  {                                                                                                            | 
    VirtualizedDpiCursor = GetVirtualizedDpiCursor(v16);                                                       | 
    AnimatedCursorFrame = VirtualizedDpiCursor;                                                                | 
    if ( a10 )                                                                                                 | 
    {                                                                                                          | 
      if ( (*((_DWORD *)VirtualizedDpiCursor + 20) & 8) == 0                                                   | 
        || (AnimatedCursorFrame = GetAnimatedCursorFrame(VirtualizedDpiCursor, 0)) != 0LL )                    | 
      {                                                                                                        | 
        if ( !PsGetCurrentProcessWow64Process() )                                                              | 
        {                                                                                                      | 
          v17 = 3LL;                                                                                           | 
        }                                                                                                      | 
        if ( (v17 & a11) != 0 )                                                                                | 
        {                                                                                                      | 
          ExRaiseDatatypeMisalignment();                                                                       | 
        }                                                                                                      | 
        v21 = (_BYTE *)a11;                                                                                    | 
        if ( a11 >= MmUserProbeAddress )                                                                       | 
        {                                                                                                      | 
          v21 = (_BYTE *)MmUserProbeAddress;                                                                   | 
        }                                                                                                      | 
        *v21 = *v21;                                                                                           | 
        v21[39] = v21[39];                                                                                     | 
        *(_QWORD *)a11 = *((_QWORD *)AnimatedCursorFrame + 11);                                                | 
        *(_QWORD *)(a11 + 8) = *((_QWORD *)AnimatedCursorFrame + 12);                                          | 
        *(_QWORD *)(a11 + 32) = *((_QWORD *)AnimatedCursorFrame + 16);                                         | 
        GetVirtualizedCursorSize(AnimatedCursorFrame, (volatile int *)(a11 + 24), (volatile int *)(a11 + 28)); | 
        *(_DWORD *)(a11 + 16) = *((_DWORD *)AnimatedCursorFrame + 35);                                         | 
        *(_DWORD *)(a11 + 20) = *((_DWORD *)AnimatedCursorFrame + 36);                                         | 
        LODWORD(v17) = 1;                                                                                      | 
      }                                                                                                        | 
    }                                                                                                          | 
    else                                                                                                       | 
    {                                                                                                          | 
      LODWORD(v17) = DrawIconEx(a1, a2, a3, VirtualizedDpiCursor, a5, a6, a7, a8, a9);                         | 
    }                                                                                                          | 
  }                                                                                                            | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v22);                                                   | 
  UserSessionSwitchLeaveCrit();                                                                                | 
  return (int)v17;                                                                                             | 
}                                                                                                              | 
62 row(s)
