__int64 __fastcall NtUserRegisterRawInputDevices(volatile void *a1, unsigned int a2, int a3)                        | 
{                                                                                                                   | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                     | 
                                                                                                                    | 
  EnterCrit(0LL, 0LL);                                                                                              | 
  AtomicExecutionCheck::AtomicExecutionCheck((AtomicExecutionCheck *)v16);                                          | 
  PsGetCurrentProcessWin32Process();                                                                                | 
  v5 = PtiCurrent();                                                                                                | 
  if ( a1 && a2 && a3 == 16 )                                                                                       | 
  {                                                                                                                 | 
    tagTLBLOCK::_unnamed_type_list_::_unnamed_type_list_((tagTLBLOCK::_unnamed_type_list_ *)BugCheckParameter2);    | 
    v6 = a2;                                                                                                        | 
    v7 = 16LL * a2;                                                                                                 | 
    ProbeForRead(a1, v7, 8u);                                                                                       | 
    if ( v7 > 0xFFFFFFFF )                                                                                          | 
    {                                                                                                               | 
      v12 = 0;                                                                                                      | 
      UserSetLastError(87LL);                                                                                       | 
      Win32RawLockedNtObject<tagDESKTOP>::~Win32RawLockedNtObject<tagDESKTOP>(BugCheckParameter2);                  | 
    }                                                                                                               | 
    else                                                                                                            | 
    {                                                                                                               | 
      v8 = v7;                                                                                                      | 
      v9 = (char *)Win32AllocPoolWithQuotaZInit((unsigned int)v7, 1769108309LL);                                    | 
      v15 = v9;                                                                                                     | 
      if ( !v9 )                                                                                                    | 
      {                                                                                                             | 
        ExRaiseStatus(-1073741801);                                                                                 | 
      }                                                                                                             | 
      if ( v18 != (void (*)(void *))-1LL )                                                                          | 
      {                                                                                                             | 
        BugCheckParameter4 = PtiCurrent();                                                                          | 
        KeBugCheckEx(0x164u, 0x12uLL, (ULONG_PTR)BugCheckParameter2, (ULONG_PTR)v9, (ULONG_PTR)BugCheckParameter4); | 
      }                                                                                                             | 
      v10 = PtiCurrent();                                                                                           | 
      BugCheckParameter2[0] = *((_QWORD *)v10 + 48);                                                                | 
      *((_QWORD *)v10 + 48) = BugCheckParameter2;                                                                   | 
      BugCheckParameter2[1] = (ULONG_PTR)v9;                                                                        | 
      v18 = Win32FreePool;                                                                                          | 
      if ( (unsigned int)v7 >= v7 )                                                                                 | 
      {                                                                                                             | 
        v8 = 16 * a2;                                                                                               | 
      }                                                                                                             | 
      memmove(v9, (const void *)a1, v8);                                                                            | 
      v12 = RegisterRawInputDevices(v9, a2, 0LL);                                                                   | 
      v13 = (unsigned int *)(v15 + 4);                                                                              | 
      do                                                                                                            | 
      {                                                                                                             | 
        EtwTraceAuditApiRegisterRawInputDevices(                                                                    | 
          v12,                                                                                                      | 
          *((unsigned __int16 *)v13 - 2),                                                                           | 
          *((unsigned __int16 *)v13 - 1),                                                                           | 
          *v13,                                                                                                     | 
          *(_QWORD *)(v13 + 1),                                                                                     | 
          v5,                                                                                                       | 
          *(_QWORD *)(*(_QWORD *)v5 + 1376LL),                                                                      | 
          *(_QWORD *)(*(_QWORD *)v5 + 1216LL),                                                                      | 
          *(_DWORD *)(*(_QWORD *)v5 + 1296LL));                                                                     | 
        v13 += 4;                                                                                                   | 
        --v6;                                                                                                       | 
      }                                                                                                             | 
      while ( v6 );                                                                                                 | 
      Win32RawLockedNtObject<tagDESKTOP>::~Win32RawLockedNtObject<tagDESKTOP>(BugCheckParameter2);                  | 
    }                                                                                                               | 
  }                                                                                                                 | 
  else                                                                                                              | 
  {                                                                                                                 | 
    v12 = 0;                                                                                                        | 
    UserSetLastError(87LL);                                                                                         | 
  }                                                                                                                 | 
  AtomicExecutionCheck::Disarm((AtomicExecutionCheck *)v16);                                                        | 
  UserSessionSwitchLeaveCrit();                                                                                     | 
  return (int)v12;                                                                                                  | 
}                                                                                                                   | 
74 row(s)
