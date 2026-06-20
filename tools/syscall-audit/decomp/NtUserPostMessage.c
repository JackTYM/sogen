__int64 __fastcall NtUserPostMessage(__int64 a1, unsigned int a2, unsigned __int64 a3, volatile void *a4) | 
{                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                           | 
                                                                                                          | 
  v4 = a2;                                                                                                | 
  EnterCrit(0LL, 1LL);                                                                                    | 
  if ( (v4 & 0xFFFE0000) != 0 )                                                                           | 
  {                                                                                                       | 
    UserSetLastError(87LL);                                                                               | 
    v8 = 0;                                                                                               | 
    goto LABEL_56;                                                                                        | 
  }                                                                                                       | 
  if ( (unsigned int)v4 < 0x400 && (MessageTable[v4] & 0x2000) != 0 )                                     | 
  {                                                                                                       | 
    v8 = 0;                                                                                               | 
    v9 = 5LL;                                                                                             | 
    goto LABEL_55;                                                                                        | 
  }                                                                                                       | 
  v8 = 0;                                                                                                 | 
  if ( a1 )                                                                                               | 
  {                                                                                                       | 
    if ( ((a1 + 1) & 0xFFFFFFFFFFFEFFFFuLL) != 0 )                                                        | 
    {                                                                                                     | 
      v10 = ValidateHwnd(a1);                                                                             | 
      if ( !v10 )                                                                                         | 
      {                                                                                                   | 
        LOBYTE(v8) = (_DWORD)v4 == 993;                                                                   | 
        goto LABEL_56;                                                                                    | 
      }                                                                                                   | 
    }                                                                                                     | 
    else                                                                                                  | 
    {                                                                                                     | 
      v10 = -1LL;                                                                                         | 
    }                                                                                                     | 
  }                                                                                                       | 
  else                                                                                                    | 
  {                                                                                                       | 
    v10 = 0LL;                                                                                            | 
  }                                                                                                       | 
  if ( (_DWORD)v4 == 576 )                                                                                | 
  {                                                                                                       | 
    if ( (unsigned __int64)(v10 - 1) <= 0xFFFFFFFFFFFFFFFDuLL )                                           | 
    {                                                                                                     | 
      v11 = PtiCurrent();                                                                                 | 
      v8 = ForwardTouchMessage(v11, a4, 1, 0LL, 0, 0LL);                                                  | 
      goto LABEL_56;                                                                                      | 
    }                                                                                                     | 
    v9 = 1400LL;                                                                                          | 
LABEL_55:                                                                                                 | 
    UserSetLastError(v9);                                                                                 | 
    goto LABEL_56;                                                                                        | 
  }                                                                                                       | 
  if ( (((_DWORD)v4 - 281) & 0xFFFFFFFD) == 0                                                             | 
    || (unsigned int)(v4 - 577) <= 3                                                                      | 
    || (unsigned int)IsPointerInputClientMessage((unsigned int)v4) )                                      | 
  {                                                                                                       | 
    v9 = 1002LL;                                                                                          | 
    goto LABEL_55;                                                                                        | 
  }                                                                                                       | 
  v12 = 0;                                                                                                | 
  if ( (unsigned int)(v4 - 992) <= 8 )                                                                    | 
  {                                                                                                       | 
    v12 = ValidateDDEConvPair(a3, v10) != 0;                                                              | 
  }                                                                                                       | 
  if ( (_DWORD)v4 == 274 && a3 == 61488 )                                                                 | 
  {                                                                                                       | 
    v13 = PtiCurrent();                                                                                   | 
    if ( (unsigned int)IAMThreadAccessGranted(v13) )                                                      | 
    {                                                                                                     | 
      v12 = 1;                                                                                            | 
    }                                                                                                     | 
  }                                                                                                       | 
  if ( (unsigned __int64)(v10 - 1) <= 0xFFFFFFFFFFFFFFFDuLL && !v12 )                                     | 
  {                                                                                                       | 
    v14 = *(_QWORD *)(*(_QWORD *)(v10 + 16) + 464LL);                                                     | 
    CurrentProcessWin32Process = (_QWORD *)PsGetCurrentProcessWin32Process();                             | 
    v16 = CurrentProcessWin32Process;                                                                     | 
    if ( CurrentProcessWin32Process && !*CurrentProcessWin32Process )                                     | 
    {                                                                                                     | 
      v16 = 0LL;                                                                                          | 
    }                                                                                                     | 
    if ( (_QWORD *)v14 != v16 )                                                                           | 
    {                                                                                                     | 
      if ( (_DWORD)v4 == 717 )                                                                            | 
      {                                                                                                   | 
        goto LABEL_56;                                                                                    | 
      }                                                                                                   | 
      if ( !IsMessageAllowedAcrossILByReceiver((struct tagPROCESSINFO *)v14, (struct tagWND *)v10, v4)    | 
        && !(unsigned int)IsMessageAlwaysAllowedAcrossIL(v4)                                              | 
        && ((_DWORD)v4 != 274 || ((a3 - 61472) & 0xFFFFFFFFFFFFFEBFuLL) != 0 || a3 == 61792) )            | 
      {                                                                                                   | 
        if ( (unsigned int)Feature_MsgUseCSRSSTrueIL__private_IsEnabledDeviceUsageNoInline() )            | 
        {                                                                                                 | 
          v18 = (const struct tagUIPI_INFO *)(v14 + 856);                                                 | 
        }                                                                                                 | 
        else                                                                                              | 
        {                                                                                                 | 
          if ( *(_QWORD *)v14 == *(_QWORD *)(W32GetUserGdiSessionState() + 40) )                          | 
          {                                                                                               | 
            v24 = 0x2000LL;                                                                               | 
            v25 = 0;                                                                                      | 
          }                                                                                               | 
          else                                                                                            | 
          {                                                                                               | 
            v24 = *(_QWORD *)(v14 + 856);                                                                 | 
            v25 = *(_DWORD *)(v14 + 864);                                                                 | 
          }                                                                                               | 
          v18 = (const struct tagUIPI_INFO *)&v24;                                                        | 
        }                                                                                                 | 
        if ( !UIPrivilegeIsolation::CheckAccess((UIPrivilegeIsolation *)(v16 + 107), v18, v17) )          | 
        {                                                                                                 | 
          if ( (_DWORD)v4 != 793                                                                          | 
            || (v19 = *(_QWORD *)(v10 + 16), *(_QWORD *)(v19 + 1584) != v10)                              | 
            || ((CurrentThreadWin32Thread = (__int64 *)PsGetCurrentThreadWin32Thread()) == 0LL            | 
              ? (v21 = 0LL)                                                                               | 
              : (v21 = *CurrentThreadWin32Thread),                                                        | 
                *(_QWORD *)(v19 + 472) != *(_QWORD *)(v21 + 472)) )                                       | 
          {                                                                                               | 
            EtwTraceUIPIMsgError(v16, v14, (unsigned int)v4, a3, a4);                                     | 
            v9 = 5LL;                                                                                     | 
            goto LABEL_55;                                                                                | 
          }                                                                                               | 
        }                                                                                                 | 
      }                                                                                                   | 
    }                                                                                                     | 
  }                                                                                                       | 
  v22 = _PostTransformableMessageExtended((struct tagWND *)v10, v4, a3, (__int64)a4, 0LL, 1);             | 
  if ( v22 >= MmSystemRangeStart )                                                                        | 
  {                                                                                                       | 
    LOBYTE(v8) = v22 != 0LL;                                                                              | 
    LODWORD(v22) = v8;                                                                                    | 
  }                                                                                                       | 
  v8 = (int)v22;                                                                                          | 
LABEL_56:                                                                                                 | 
  UserSessionSwitchLeaveCrit();                                                                           | 
  return v8;                                                                                              | 
}                                                                                                         | 
137 row(s)
