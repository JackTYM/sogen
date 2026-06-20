__int64 __fastcall NtUserGetClassInfoEx(int a1, ULONG64 a2, _OWORD *a3, _QWORD *a4, int a5)                       | 
{                                                                                                                 | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                   | 
                                                                                                                  | 
  v23[0] = 0LL;                                                                                                   | 
  memset_0(&v25, 0, 0x50uLL);                                                                                     | 
  EnterCrit(0LL, 0LL);                                                                                            | 
  v8 = PtiCurrent();                                                                                              | 
  if ( (*(_DWORD *)(*((_QWORD *)v8 + 58) + 12LL) & 0x10000000) != 0 || (unsigned int)RegisterIconTitleClass() )   | 
  {                                                                                                               | 
    tagTLBLOCK::_unnamed_type_list_::_unnamed_type_list_((tagTLBLOCK::_unnamed_type_list_ *)BugCheckParameter2);  | 
    if ( a2 >= MmUserProbeAddress )                                                                               | 
    {                                                                                                             | 
      a2 = MmUserProbeAddress;                                                                                    | 
    }                                                                                                             | 
    v10 = *(_DWORD *)a2;                                                                                          | 
    v21 = *(_DWORD *)a2;                                                                                          | 
    v11 = *(wchar_t **)(a2 + 8);                                                                                  | 
    Str1_8 = v11;                                                                                                 | 
    if ( ((unsigned __int64)v11 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                   | 
    {                                                                                                             | 
      if ( ((unsigned __int8)v11 & 1) != 0 )                                                                      | 
      {                                                                                                           | 
        ExRaiseDatatypeMisalignment();                                                                            | 
      }                                                                                                           | 
      v12 = (ULONG64)v11 + (unsigned __int16)v10 + 2;                                                             | 
      if ( v12 <= (unsigned __int64)v11 || v12 >= MmUserProbeAddress )                                            | 
      {                                                                                                           | 
        ExRaiseAccessViolation();                                                                                 | 
      }                                                                                                           | 
      if ( (unsigned __int16)v10 > HIWORD(v21) || (v10 & 1) != 0 )                                                | 
      {                                                                                                           | 
        if ( (v10 & 1) != 0 )                                                                                     | 
        {                                                                                                         | 
          MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 9725LL);                             | 
        }                                                                                                         | 
        ExRaiseAccessViolation();                                                                                 | 
      }                                                                                                           | 
    }                                                                                                             | 
    if ( ((unsigned __int64)v11 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                   | 
    {                                                                                                             | 
      v13 = (unsigned __int16)v10;                                                                                | 
      v14 = Win32AllocPoolWithQuotaZInit((unsigned __int16)v10 + 2LL, 2020897621LL);                              | 
      v15 = (void *)v14;                                                                                          | 
      Str1_8 = (wchar_t *)v14;                                                                                    | 
      if ( !v14 )                                                                                                 | 
      {                                                                                                           | 
        ExRaiseStatus(-1073741801);                                                                               | 
      }                                                                                                           | 
      Win32RawLockedItemBase<unsigned short,&void Win32FreePool(void *),1,1,1>::ManualLock<void>(                 | 
        (ULONG_PTR)BugCheckParameter2,                                                                            | 
        v14);                                                                                                     | 
      memmove(v15, v11, (unsigned int)v13);                                                                       | 
      *((_WORD *)v15 + (v13 >> 1)) = 0;                                                                           | 
    }                                                                                                             | 
    v16 = -PsGetCurrentProcessWow64Process();                                                                     | 
    v23[2] = (-(__int64)(v16 != 0) & 0xFFFFFFFFFFFFFFFDuLL) + 4;                                                  | 
    v17 = a3;                                                                                                     | 
    if ( ((v16 != 0 ? 0 : 3) & (unsigned int)a3) != 0 )                                                           | 
    {                                                                                                             | 
      ExRaiseDatatypeMisalignment();                                                                              | 
    }                                                                                                             | 
    if ( (unsigned __int64)a3 >= MmUserProbeAddress )                                                             | 
    {                                                                                                             | 
      v17 = (_BYTE *)MmUserProbeAddress;                                                                          | 
    }                                                                                                             | 
    *v17 = *v17;                                                                                                  | 
    v17[79] = v17[79];                                                                                            | 
    v18 = a4;                                                                                                     | 
    if ( (unsigned __int64)a4 >= MmUserProbeAddress )                                                             | 
    {                                                                                                             | 
      v18 = (_DWORD *)MmUserProbeAddress;                                                                         | 
    }                                                                                                             | 
    *v18 = *v18;                                                                                                  | 
    v25 = *a3;                                                                                                    | 
    v26 = a3[1];                                                                                                  | 
    v27 = a3[2];                                                                                                  | 
    v28 = a3[3];                                                                                                  | 
    v29 = a3[4];                                                                                                  | 
    if ( (*(_DWORD *)(*((_QWORD *)v8 + 58) + 12LL) & 0x2000) != 0 || (unsigned int)RegisterDefaultClass(Str1_8) ) | 
    {                                                                                                             | 
      ClassInfo = GetClassInfoEx(a1, (_DWORD)Str1_8, (unsigned int)&v25, (unsigned int)v23, a5);                  | 
      v9 = ClassInfo;                                                                                             | 
      if ( ClassInfo )                                                                                            | 
      {                                                                                                           | 
        *a3 = v25;                                                                                                | 
        a3[1] = v26;                                                                                              | 
        a3[2] = v27;                                                                                              | 
        a3[3] = v28;                                                                                              | 
        a3[4] = v29;                                                                                              | 
        *a4 = v23[0];                                                                                             | 
      }                                                                                                           | 
    }                                                                                                             | 
    else                                                                                                          | 
    {                                                                                                             | 
      v9 = 0;                                                                                                     | 
      UserSetLastError(87LL);                                                                                     | 
    }                                                                                                             | 
    Win32RawLockedNtObject<tagDESKTOP>::~Win32RawLockedNtObject<tagDESKTOP>(BugCheckParameter2);                  | 
  }                                                                                                               | 
  else                                                                                                            | 
  {                                                                                                               | 
    v9 = 0;                                                                                                       | 
    UserSetLastError(87LL);                                                                                       | 
  }                                                                                                               | 
  UserSessionSwitchLeaveCrit();                                                                                   | 
  return v9;                                                                                                      | 
}                                                                                                                 | 
108 row(s)
