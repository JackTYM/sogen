__int64 __fastcall NtUserMessageCall(__int64 a1, unsigned int a2, __int64 a3, __int64 a4, __int64 a5, int a6, int a7)                                 | 
{                                                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                       | 
                                                                                                                                                      | 
  v27 = a3;                                                                                                                                           | 
  v7 = 0LL;                                                                                                                                           | 
  v8 = a2;                                                                                                                                            | 
  v9 = 0;                                                                                                                                             | 
  v12 = 0LL;                                                                                                                                          | 
  if ( (a2 & 0xFFFE0000) != 0 )                                                                                                                       | 
  {                                                                                                                                                   | 
    UserSetLastError(87LL);                                                                                                                           | 
    UserSetLastError(87LL);                                                                                                                           | 
    return 0LL;                                                                                                                                       | 
  }                                                                                                                                                   | 
  else                                                                                                                                                | 
  {                                                                                                                                                   | 
    v13 = a6;                                                                                                                                         | 
    if ( a6 == 672 || a6 == 694 )                                                                                                                     | 
    {                                                                                                                                                 | 
      return 0LL;                                                                                                                                     | 
    }                                                                                                                                                 | 
    else                                                                                                                                              | 
    {                                                                                                                                                 | 
      if ( ((a1 + 1) & 0xFFFFFFFFFFFEFFFFuLL) != 0 || (unsigned int)(a6 - 695) > 1 )                                                                  | 
      {                                                                                                                                               | 
        if ( a6 == 689 && a2 - 992 > 8 )                                                                                                              | 
        {                                                                                                                                             | 
          goto LABEL_7;                                                                                                                               | 
        }                                                                                                                                             | 
      }                                                                                                                                               | 
      else                                                                                                                                            | 
      {                                                                                                                                               | 
        v12 = -1LL;                                                                                                                                   | 
      }                                                                                                                                               | 
      v9 = 1;                                                                                                                                         | 
LABEL_7:                                                                                                                                              | 
      while ( 1 )                                                                                                                                     | 
      {                                                                                                                                               | 
        EnterLeaveCritSharedOrExclusive::EnterLeaveCritSharedOrExclusive(&v26, v9);                                                                   | 
        CurrentThreadNonPaged = (_QWORD *)W32GetCurrentThreadNonPaged();                                                                              | 
        v15 = CurrentThreadNonPaged ? *CurrentThreadNonPaged : 0LL;                                                                                   | 
        if ( v12 == -1 )                                                                                                                              | 
        {                                                                                                                                             | 
          break;                                                                                                                                      | 
        }                                                                                                                                             | 
        v16 = ValidateHwnd(a1);                                                                                                                       | 
        v12 = v16;                                                                                                                                    | 
        if ( !v16 )                                                                                                                                   | 
        {                                                                                                                                             | 
          LeaveEditionCrit(&v26);                                                                                                                     | 
          return 0LL;                                                                                                                                 | 
        }                                                                                                                                             | 
        if ( *(_QWORD *)(v16 + 16) == v15 && (*(_BYTE *)(*(_QWORD *)(v16 + 40) + 18LL) & 4) == 0 || v9 )                                              | 
        {                                                                                                                                             | 
          break;                                                                                                                                      | 
        }                                                                                                                                             | 
        v9 = 1;                                                                                                                                       | 
        LeaveEditionCrit(&v26);                                                                                                                       | 
      }                                                                                                                                               | 
      v24 = 0LL;                                                                                                                                      | 
      BugCheckParameter3 = -1LL;                                                                                                                      | 
      if ( v12 != -1 )                                                                                                                                | 
      {                                                                                                                                               | 
        BugCheckParameter3 = *(_QWORD *)(v15 + 456);                                                                                                  | 
        *(_QWORD *)(v15 + 456) = &BugCheckParameter3;                                                                                                 | 
        v24 = v12;                                                                                                                                    | 
        HMLockObject(v12);                                                                                                                            | 
      }                                                                                                                                               | 
      if ( v9 )                                                                                                                                       | 
      {                                                                                                                                               | 
        if ( (unsigned int)v8 >= 0x400 )                                                                                                              | 
        {                                                                                                                                             | 
          UserSessionState = W32GetUserSessionState();                                                                                                | 
          v17 = (*(__int64 (__fastcall **)(__int64, _QWORD, __int64, __int64, __int64))(UserSessionState                                              | 
                                                                                      + 8LL * (((_BYTE)v13 + 6) & 0x1F)                               | 
                                                                                      + 71272))(                                                      | 
                  v12,                                                                                                                                | 
                  (unsigned int)v8,                                                                                                                   | 
                  v27,                                                                                                                                | 
                  a4,                                                                                                                                 | 
                  a5);                                                                                                                                | 
        }                                                                                                                                             | 
        else                                                                                                                                          | 
        {                                                                                                                                             | 
          _mm_lfence();                                                                                                                               | 
          v17 = ((__int64 (__fastcall *)(__int64, _QWORD, __int64, __int64, __int64, int, bool))gapfnMessageCall[(unsigned __int8)MessageTable[v8]])( | 
                  v12,                                                                                                                                | 
                  (unsigned int)v8,                                                                                                                   | 
                  v27,                                                                                                                                | 
                  a4,                                                                                                                                 | 
                  a5,                                                                                                                                 | 
                  v13,                                                                                                                                | 
                  a7 != 0);                                                                                                                           | 
        }                                                                                                                                             | 
      }                                                                                                                                               | 
      else                                                                                                                                            | 
      {                                                                                                                                               | 
        CDisableILCheckAuto::CDisableILCheckAuto((CDisableILCheckAuto *)v25);                                                                         | 
        if ( (unsigned int)v8 < 0x400 )                                                                                                               | 
        {                                                                                                                                             | 
          _mm_lfence();                                                                                                                               | 
          v19 = ((__int64 (__fastcall *)(__int64, _QWORD, __int64, __int64, __int64, int, bool))gapfnMessageCall[(unsigned __int8)MessageTable[v8]])( | 
                  v12,                                                                                                                                | 
                  (unsigned int)v8,                                                                                                                   | 
                  v27,                                                                                                                                | 
                  a4,                                                                                                                                 | 
                  a5,                                                                                                                                 | 
                  v13,                                                                                                                                | 
                  a7 != 0);                                                                                                                           | 
        }                                                                                                                                             | 
        else                                                                                                                                          | 
        {                                                                                                                                             | 
          v18 = W32GetUserSessionState();                                                                                                             | 
          v19 = (*(__int64 (__fastcall **)(__int64, _QWORD, __int64, __int64, __int64))(v18                                                           | 
                                                                                      + 8LL * (((_BYTE)v13 + 6) & 0x1F)                               | 
                                                                                      + 71272))(                                                      | 
                  v12,                                                                                                                                | 
                  (unsigned int)v8,                                                                                                                   | 
                  v27,                                                                                                                                | 
                  a4,                                                                                                                                 | 
                  a5);                                                                                                                                | 
        }                                                                                                                                             | 
        v17 = v19;                                                                                                                                    | 
        CDisableILCheckAuto::~CDisableILCheckAuto((CDisableILCheckAuto *)v25);                                                                        | 
      }                                                                                                                                               | 
      if ( BugCheckParameter3 != -1LL )                                                                                                               | 
      {                                                                                                                                               | 
        v20 = (ULONG_PTR *)W32GetCurrentThreadNonPaged();                                                                                             | 
        if ( v20 )                                                                                                                                    | 
        {                                                                                                                                             | 
          v7 = *v20;                                                                                                                                  | 
        }                                                                                                                                             | 
        Win32HM_UnlockFromThread<0>(v7, (ULONG_PTR)&BugCheckParameter3);                                                                              | 
      }                                                                                                                                               | 
      LeaveEditionCrit(&v26);                                                                                                                         | 
      return v17;                                                                                                                                     | 
    }                                                                                                                                                 | 
  }                                                                                                                                                   | 
}                                                                                                                                                     | 
140 row(s)
