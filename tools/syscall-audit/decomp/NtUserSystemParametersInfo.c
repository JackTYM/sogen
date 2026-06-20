__int64 __fastcall NtUserSystemParametersInfo(unsigned int a1, unsigned int a2, ULONG64 a3, char a4)                                                             | 
{                                                                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                                                                  | 
                                                                                                                                                                 | 
  v5 = a2;                                                                                                                                                       | 
  v63 = a2;                                                                                                                                                      | 
  v75 = 0LL;                                                                                                                                                     | 
  Size = 0;                                                                                                                                                      | 
  Address = 0LL;                                                                                                                                                 | 
  v68 = 0;                                                                                                                                                       | 
  UserSessionState = W32GetUserSessionState();                                                                                                                   | 
  v8 = UserCritInternal::_anonymous_namespace_::EnterCritInternal(                                                                                               | 
         UserSessionState,                                                                                                                                       | 
         0LL,                                                                                                                                                    | 
         0LL,                                                                                                                                                    | 
         _lambda_63b61c2369133a205197eda5bd671ee7_::_lambda_invoker_cdecl_);                                                                                     | 
  *(_QWORD *)(UserSessionState + 16) = v8;                                                                                                                       | 
  if ( v8 )                                                                                                                                                      | 
  {                                                                                                                                                              | 
    DestroySharedUserCritDeferredUnlockList(UserSessionState + 19832);                                                                                           | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19888);                                                                                         | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19872);                                                                                         | 
  }                                                                                                                                                              | 
  Win32RawLockedNtObject<tagDESKTOP>::Win32RawLockedNtObject<tagDESKTOP>(BugCheckParameter2);                                                                    | 
  Win32RawLockedNtObject<tagDESKTOP>::Win32RawLockedNtObject<tagDESKTOP>(v64);                                                                                   | 
  v69 = a4 & 3;                                                                                                                                                  | 
  if ( (unsigned int)Feature_UIPIModernization__private_IsEnabledDeviceUsageNoInline() )                                                                         | 
  {                                                                                                                                                              | 
    CurrentProcessWin32Process = PsGetCurrentProcessWin32Process();                                                                                              | 
    if ( CurrentProcessWin32Process )                                                                                                                            | 
    {                                                                                                                                                            | 
      CurrentProcessWin32Process &= -(__int64)(*(_QWORD *)CurrentProcessWin32Process != 0LL);                                                                    | 
    }                                                                                                                                                            | 
    v11 = UIPrivilegeIsolation::MeetsUIPIThreshold(                                                                                                              | 
            (UIPrivilegeIsolation *)(CurrentProcessWin32Process + 856),                                                                                          | 
            (const struct tagUIPI_INFO *)0x2000,                                                                                                                 | 
            v10);                                                                                                                                                | 
  }                                                                                                                                                              | 
  else                                                                                                                                                           | 
  {                                                                                                                                                              | 
    v72[0] = 0x2000;                                                                                                                                             | 
    v72[1] = -1;                                                                                                                                                 | 
    v72[2] = 0;                                                                                                                                                  | 
    v12 = PsGetCurrentProcessWin32Process();                                                                                                                     | 
    if ( v12 )                                                                                                                                                   | 
    {                                                                                                                                                            | 
      v12 &= -(__int64)(*(_QWORD *)v12 != 0LL);                                                                                                                  | 
    }                                                                                                                                                            | 
    v11 = UIPrivilegeIsolation::CheckAccessEx(v12 + 856, v72, 0LL);                                                                                              | 
  }                                                                                                                                                              | 
  v13 = v11;                                                                                                                                                     | 
  if ( (unsigned int)Feature_LearningModeExpansion__private_IsEnabledDeviceUsageNoInline() )                                                                     | 
  {                                                                                                                                                              | 
    LOBYTE(v14) = v13;                                                                                                                                           | 
    if ( (unsigned __int8)CheckSystemParametersAccess(a1, v14) )                                                                                                 | 
    {                                                                                                                                                            | 
      goto LABEL_126;                                                                                                                                            | 
    }                                                                                                                                                            | 
LABEL_432:                                                                                                                                                       | 
    v31 = 0;                                                                                                                                                     | 
    v32 = 5LL;                                                                                                                                                   | 
    goto LABEL_433;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( !tagPROCESSINFO::HasUILimit(*(tagPROCESSINFO **)(v8 + 464), 8u) && v13 )                                                                                  | 
  {                                                                                                                                                              | 
    goto LABEL_126;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 <= 0x6D )                                                                                                                                              | 
  {                                                                                                                                                              | 
    if ( a1 == 109 )                                                                                                                                             | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 <= 0x37 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      if ( a1 == 55 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 <= 0x1D )                                                                                                                                          | 
      {                                                                                                                                                          | 
        if ( a1 == 29 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          goto LABEL_429;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 <= 0x13 )                                                                                                                                        | 
        {                                                                                                                                                        | 
          v16 = 698452;                                                                                                                                          | 
          v17 = _bittest(&v16, a1);                                                                                                                              | 
LABEL_24:                                                                                                                                                        | 
          if ( !v17 )                                                                                                                                            | 
          {                                                                                                                                                      | 
            goto LABEL_126;                                                                                                                                      | 
          }                                                                                                                                                      | 
          goto LABEL_429;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 == 20 || a1 == 21 || a1 == 23 )                                                                                                                  | 
        {                                                                                                                                                        | 
          goto LABEL_429;                                                                                                                                        | 
        }                                                                                                                                                        | 
        v19 = a1 - 24;                                                                                                                                           | 
        v18 = a1 == 24;                                                                                                                                          | 
        goto LABEL_121;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 <= 0x2A )                                                                                                                                          | 
      {                                                                                                                                                          | 
        if ( a1 != 42 && a1 != 30 && a1 != 32 && a1 != 33 && a1 != 34 && a1 != 36 )                                                                              | 
        {                                                                                                                                                        | 
          v20 = a1 == 37;                                                                                                                                        | 
          goto LABEL_125;                                                                                                                                        | 
        }                                                                                                                                                        | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 == 44 || a1 == 46 )                                                                                                                                | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v22 = a1 - 47;                                                                                                                                             | 
      v21 = a1 == 47;                                                                                                                                            | 
      goto LABEL_119;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 <= 0x55 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      if ( a1 == 85 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 <= 0x49 )                                                                                                                                          | 
      {                                                                                                                                                          | 
        if ( a1 - 57 > 0x10 )                                                                                                                                    | 
        {                                                                                                                                                        | 
          goto LABEL_126;                                                                                                                                        | 
        }                                                                                                                                                        | 
        v15 = 87317LL;                                                                                                                                           | 
        v17 = _bittest((const int *)&v15, a1 - 57);                                                                                                              | 
        goto LABEL_24;                                                                                                                                           | 
      }                                                                                                                                                          | 
      if ( a1 == 75 || a1 == 76 || a1 == 77 || a1 == 78 || a1 == 81 )                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v23 = a1 == 82;                                                                                                                                            | 
LABEL_52:                                                                                                                                                        | 
      if ( !v23 )                                                                                                                                                | 
      {                                                                                                                                                          | 
        goto LABEL_126;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 > 0x60 )                                                                                                                                             | 
    {                                                                                                                                                            | 
      if ( a1 - 97 > 0xA )                                                                                                                                       | 
      {                                                                                                                                                          | 
        goto LABEL_126;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v23 = (((_BYTE)a1 - 97) & 1) == 0;                                                                                                                         | 
      goto LABEL_52;                                                                                                                                             | 
    }                                                                                                                                                            | 
    if ( a1 == 96 || a1 == 86 || a1 == 87 || a1 == 88 || a1 == 90 )                                                                                              | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    v25 = a1 - 91;                                                                                                                                               | 
    v24 = a1 == 91;                                                                                                                                              | 
LABEL_123:                                                                                                                                                       | 
    if ( !v24 )                                                                                                                                                  | 
    {                                                                                                                                                            | 
      v20 = v25 == 2;                                                                                                                                            | 
LABEL_125:                                                                                                                                                       | 
      if ( !v20 )                                                                                                                                                | 
      {                                                                                                                                                          | 
LABEL_126:                                                                                                                                                       | 
        v26 = 4LL;                                                                                                                                               | 
        goto LABEL_127;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    goto LABEL_429;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 > 0x1017 )                                                                                                                                             | 
  {                                                                                                                                                              | 
    if ( a1 <= 0x2003 )                                                                                                                                          | 
    {                                                                                                                                                            | 
      if ( a1 == 8195 )                                                                                                                                          | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 > 0x1049 )                                                                                                                                         | 
      {                                                                                                                                                          | 
        if ( a1 != 4171 && a1 != 4173 && a1 != 4175 && a1 != 4177 && a1 != 4179 )                                                                                | 
        {                                                                                                                                                        | 
          v20 = a1 == 8193;                                                                                                                                      | 
          goto LABEL_125;                                                                                                                                        | 
        }                                                                                                                                                        | 
LABEL_429:                                                                                                                                                       | 
        v59 = PsGetCurrentProcessWin32Process();                                                                                                                 | 
        if ( v59 )                                                                                                                                               | 
        {                                                                                                                                                        | 
          v59 &= -(__int64)(*(_QWORD *)v59 != 0LL);                                                                                                              | 
        }                                                                                                                                                        | 
        EtwTraceUIPISystemError((struct tagPROCESSINFO *)v59);                                                                                                   | 
        goto LABEL_432;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 == 4169 )                                                                                                                                          | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 == 4121 )                                                                                                                                          | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v26 = 4LL;                                                                                                                                                 | 
      if ( a1 == 4125 || a1 == 4127 || a1 == 4135 )                                                                                                              | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v28 = a1 - 4161;                                                                                                                                           | 
      v27 = a1 == 4161;                                                                                                                                          | 
      goto LABEL_113;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 > 0x2019 )                                                                                                                                           | 
    {                                                                                                                                                            | 
      if ( a1 == 8219 || a1 == 8221 )                                                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v22 = a1 - 8241;                                                                                                                                           | 
      v21 = a1 == 8241;                                                                                                                                          | 
LABEL_119:                                                                                                                                                       | 
      if ( v21 )                                                                                                                                                 | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v19 = v22 - 2;                                                                                                                                             | 
      v18 = v19 == 0;                                                                                                                                            | 
      goto LABEL_121;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 == 8217 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 == 8197 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    v26 = 4LL;                                                                                                                                                   | 
    if ( a1 == 8201 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 == 8207 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    v29 = a1 - 8209;                                                                                                                                             | 
    if ( a1 == 8209 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
LABEL_112:                                                                                                                                                       | 
    v28 = v29 - 4;                                                                                                                                               | 
    v27 = v28 == 0;                                                                                                                                              | 
    goto LABEL_113;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 == 4119 )                                                                                                                                              | 
  {                                                                                                                                                              | 
    goto LABEL_429;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 > 0xAD )                                                                                                                                               | 
  {                                                                                                                                                              | 
    if ( a1 <= 0x1007 )                                                                                                                                          | 
    {                                                                                                                                                            | 
      if ( a1 == 4103 || a1 == 175 || a1 == 177 || a1 == 179 )                                                                                                   | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v19 = a1 - 4097;                                                                                                                                           | 
      v18 = a1 == 4097;                                                                                                                                          | 
LABEL_121:                                                                                                                                                       | 
      if ( v18 )                                                                                                                                                 | 
      {                                                                                                                                                          | 
        goto LABEL_429;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v25 = v19 - 2;                                                                                                                                             | 
      v24 = v25 == 0;                                                                                                                                            | 
      goto LABEL_123;                                                                                                                                            | 
    }                                                                                                                                                            | 
    switch ( a1 )                                                                                                                                                | 
    {                                                                                                                                                            | 
      case 0x1009u:                                                                                                                                              | 
        goto LABEL_429;                                                                                                                                          | 
      case 0x100Bu:                                                                                                                                              | 
        goto LABEL_429;                                                                                                                                          | 
      case 0x100Du:                                                                                                                                              | 
        goto LABEL_429;                                                                                                                                          | 
    }                                                                                                                                                            | 
    v29 = a1 - 4111;                                                                                                                                             | 
    if ( a1 == 4111 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    v26 = 4LL;                                                                                                                                                   | 
    goto LABEL_112;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 == 173 )                                                                                                                                               | 
  {                                                                                                                                                              | 
    goto LABEL_429;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 > 0x99 )                                                                                                                                               | 
  {                                                                                                                                                              | 
    if ( a1 == 155 || a1 == 161 || a1 == 163 || a1 == 166 )                                                                                                      | 
    {                                                                                                                                                            | 
      goto LABEL_429;                                                                                                                                            | 
    }                                                                                                                                                            | 
    v25 = a1 - 169;                                                                                                                                              | 
    v24 = a1 == 169;                                                                                                                                             | 
    goto LABEL_123;                                                                                                                                              | 
  }                                                                                                                                                              | 
  switch ( a1 )                                                                                                                                                  | 
  {                                                                                                                                                              | 
    case 0x99u:                                                                                                                                                  | 
      goto LABEL_429;                                                                                                                                            | 
    case 0x6Fu:                                                                                                                                                  | 
      goto LABEL_429;                                                                                                                                            | 
    case 0x71u:                                                                                                                                                  | 
      goto LABEL_429;                                                                                                                                            | 
  }                                                                                                                                                              | 
  v26 = 4LL;                                                                                                                                                     | 
  if ( a1 == 117 || a1 == 119 || a1 == 147 )                                                                                                                     | 
  {                                                                                                                                                              | 
    goto LABEL_429;                                                                                                                                              | 
  }                                                                                                                                                              | 
  v28 = a1 - 149;                                                                                                                                                | 
  v27 = a1 == 149;                                                                                                                                               | 
LABEL_113:                                                                                                                                                       | 
  if ( v27 || v28 == 2 )                                                                                                                                         | 
  {                                                                                                                                                              | 
    goto LABEL_429;                                                                                                                                              | 
  }                                                                                                                                                              | 
LABEL_127:                                                                                                                                                       | 
  v30 = (unsigned __int16 *)Win32AllocPoolWithQuotaZInitImpl(v15, 0x228uLL, 0x79747355u);                                                                        | 
  if ( !v30 )                                                                                                                                                    | 
  {                                                                                                                                                              | 
    v31 = 0;                                                                                                                                                     | 
    v32 = 8LL;                                                                                                                                                   | 
LABEL_433:                                                                                                                                                       | 
    UserSetLastError(v32);                                                                                                                                       | 
LABEL_434:                                                                                                                                                       | 
    Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>::~Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>(v64);                | 
    Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>::~Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>(BugCheckParameter2); | 
    goto LABEL_435;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( v67 != (__int64 (__fastcall *)(PVOID))-1LL )                                                                                                              | 
  {                                                                                                                                                              | 
    BugCheckParameter4 = PtiCurrent();                                                                                                                           | 
    KeBugCheckEx(0x164u, 0x12uLL, (ULONG_PTR)BugCheckParameter2, (ULONG_PTR)v30, (ULONG_PTR)BugCheckParameter4);                                                 | 
  }                                                                                                                                                              | 
  v34 = PtiCurrent();                                                                                                                                            | 
  BugCheckParameter2[0] = *((_QWORD *)v34 + 48);                                                                                                                 | 
  *((_QWORD *)v34 + 48) = BugCheckParameter2;                                                                                                                    | 
  BugCheckParameter2[1] = (ULONG_PTR)v30;                                                                                                                        | 
  v67 = GreDeleteFastMutex;                                                                                                                                      | 
  if ( a1 > 0x1026 )                                                                                                                                             | 
  {                                                                                                                                                              | 
    if ( a1 == 4135 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      goto LABEL_427;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 == 8221 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      if ( a3 <= 2 )                                                                                                                                             | 
      {                                                                                                                                                          | 
        goto LABEL_427;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_426;                                                                                                                                            | 
    }                                                                                                                                                            | 
    goto LABEL_407;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 == 4134 )                                                                                                                                              | 
  {                                                                                                                                                              | 
    goto LABEL_422;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 > 0x62 )                                                                                                                                               | 
  {                                                                                                                                                              | 
    if ( a1 <= 0x94 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      if ( a1 == 148 )                                                                                                                                           | 
      {                                                                                                                                                          | 
        if ( v5 != 40 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          goto LABEL_426;                                                                                                                                        | 
        }                                                                                                                                                        | 
        Size = 40;                                                                                                                                               | 
      }                                                                                                                                                          | 
      else                                                                                                                                                       | 
      {                                                                                                                                                          | 
        if ( a1 <= 0x7A )                                                                                                                                        | 
        {                                                                                                                                                        | 
          if ( a1 == 122 )                                                                                                                                       | 
          {                                                                                                                                                      | 
            goto LABEL_422;                                                                                                                                      | 
          }                                                                                                                                                      | 
          if ( a1 <= 0x70 )                                                                                                                                      | 
          {                                                                                                                                                      | 
            if ( ((a1 - 100) & 0xFFFFFFF1) == 0 )                                                                                                                | 
            {                                                                                                                                                    | 
              goto LABEL_422;                                                                                                                                    | 
            }                                                                                                                                                    | 
            goto LABEL_407;                                                                                                                                      | 
          }                                                                                                                                                      | 
          switch ( a1 )                                                                                                                                          | 
          {                                                                                                                                                      | 
            case 'r':                                                                                                                                            | 
              goto LABEL_422;                                                                                                                                    | 
            case 's':                                                                                                                                            | 
              Address = (volatile void *)a3;                                                                                                                     | 
              a3 = (ULONG64)v30;                                                                                                                                 | 
              ProbeForWrite(Address, 2LL * v63, 2u);                                                                                                             | 
              if ( v5 >= 0x104 )                                                                                                                                 | 
              {                                                                                                                                                  | 
                v5 = 260;                                                                                                                                        | 
              }                                                                                                                                                  | 
              v63 = v5;                                                                                                                                          | 
              Size = 2 * v5;                                                                                                                                     | 
              goto LABEL_229;                                                                                                                                    | 
            case 't':                                                                                                                                            | 
              goto LABEL_149;                                                                                                                                    | 
            case 'u':                                                                                                                                            | 
              goto LABEL_148;                                                                                                                                    | 
          }                                                                                                                                                      | 
          v49 = a1 - 118;                                                                                                                                        | 
          v48 = a1 == 118;                                                                                                                                       | 
LABEL_281:                                                                                                                                                       | 
          if ( v48 )                                                                                                                                             | 
          {                                                                                                                                                      | 
            goto LABEL_422;                                                                                                                                      | 
          }                                                                                                                                                      | 
          v47 = v49 == 2;                                                                                                                                        | 
          goto LABEL_283;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 <= 0x88 )                                                                                                                                        | 
        {                                                                                                                                                        | 
          if ( a1 == 136 || a1 == 124 || a1 == 126 || a1 == 128 || a1 == 130 )                                                                                   | 
          {                                                                                                                                                      | 
            goto LABEL_422;                                                                                                                                      | 
          }                                                                                                                                                      | 
          v49 = a1 - 132;                                                                                                                                        | 
          v48 = a1 == 132;                                                                                                                                       | 
          goto LABEL_281;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 == 138 || a1 == 140 || a1 == 142 || a1 == 144 )                                                                                                  | 
        {                                                                                                                                                        | 
          goto LABEL_422;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 != 146 )                                                                                                                                         | 
        {                                                                                                                                                        | 
          if ( a1 != 147 )                                                                                                                                       | 
          {                                                                                                                                                      | 
            goto LABEL_407;                                                                                                                                      | 
          }                                                                                                                                                      | 
          if ( v5 != 68 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          Size = 68;                                                                                                                                             | 
          goto LABEL_397;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( v5 != 68 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          goto LABEL_426;                                                                                                                                        | 
        }                                                                                                                                                        | 
        Size = 68;                                                                                                                                               | 
      }                                                                                                                                                          | 
LABEL_404:                                                                                                                                                       | 
      Address = (volatile void *)a3;                                                                                                                             | 
      a3 = (ULONG64)v30;                                                                                                                                         | 
      CurrentProcessWow64Process = PsGetCurrentProcessWow64Process();                                                                                            | 
      v57 = Address;                                                                                                                                             | 
      ProbeForWrite(Address, Size, CurrentProcessWow64Process != 0 ? 1 : 4);                                                                                     | 
      v68 = 1;                                                                                                                                                   | 
      *(_DWORD *)v30 = *v57;                                                                                                                                     | 
      goto LABEL_427;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 <= 0xA5 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      if ( a1 != 165 )                                                                                                                                           | 
      {                                                                                                                                                          | 
        if ( a1 > 0x9B )                                                                                                                                         | 
        {                                                                                                                                                        | 
          switch ( a1 )                                                                                                                                          | 
          {                                                                                                                                                      | 
            case 0x9Cu:                                                                                                                                          | 
              if ( v5 != 16 )                                                                                                                                    | 
              {                                                                                                                                                  | 
                goto LABEL_426;                                                                                                                                  | 
              }                                                                                                                                                  | 
              break;                                                                                                                                             | 
            case 0x9Du:                                                                                                                                          | 
              goto LABEL_335;                                                                                                                                    | 
            case 0x9Eu:                                                                                                                                          | 
            case 0xA0u:                                                                                                                                          | 
              goto LABEL_422;                                                                                                                                    | 
            case 0xA2u:                                                                                                                                          | 
              break;                                                                                                                                             | 
            case 0xA3u:                                                                                                                                          | 
              goto LABEL_335;                                                                                                                                    | 
            default:                                                                                                                                             | 
              goto LABEL_407;                                                                                                                                    | 
          }                                                                                                                                                      | 
LABEL_202:                                                                                                                                                       | 
          Size = 16;                                                                                                                                             | 
          goto LABEL_404;                                                                                                                                        | 
        }                                                                                                                                                        | 
        switch ( a1 )                                                                                                                                            | 
        {                                                                                                                                                        | 
          case 0x9Bu:                                                                                                                                            | 
            if ( v5 != 548 )                                                                                                                                     | 
            {                                                                                                                                                    | 
              goto LABEL_426;                                                                                                                                    | 
            }                                                                                                                                                    | 
            Size = 548;                                                                                                                                          | 
            memset(v78, 0, 0x224uLL);                                                                                                                            | 
            RtlCopyFromUser(v78, (void *)a3, 0x224uLL);                                                                                                          | 
            v53 = v30;                                                                                                                                           | 
            v54 = v78;                                                                                                                                           | 
            do                                                                                                                                                   | 
            {                                                                                                                                                    | 
              *v53 = *v54;                                                                                                                                       | 
              v53[1] = v54[1];                                                                                                                                   | 
              v53[2] = v54[2];                                                                                                                                   | 
              v53[3] = v54[3];                                                                                                                                   | 
              v53[4] = v54[4];                                                                                                                                   | 
              v53[5] = v54[5];                                                                                                                                   | 
              v53[6] = v54[6];                                                                                                                                   | 
              v53 += 8;                                                                                                                                          | 
              *(v53 - 1) = v54[7];                                                                                                                               | 
              v54 += 8;                                                                                                                                          | 
              --v26;                                                                                                                                             | 
            }                                                                                                                                                    | 
            while ( v26 );                                                                                                                                       | 
            *v53 = *v54;                                                                                                                                         | 
            v53[1] = v54[1];                                                                                                                                     | 
            *((_DWORD *)v53 + 8) = *((_DWORD *)v54 + 8);                                                                                                         | 
            a3 = (ULONG64)v30;                                                                                                                                   | 
            if ( *((_DWORD *)v30 + 6) > 0x104u )                                                                                                                 | 
            {                                                                                                                                                    | 
              goto LABEL_426;                                                                                                                                    | 
            }                                                                                                                                                    | 
            goto LABEL_427;                                                                                                                                      | 
          case 0x95u:                                                                                                                                            | 
            if ( v5 != 40 )                                                                                                                                      | 
            {                                                                                                                                                    | 
              goto LABEL_426;                                                                                                                                    | 
            }                                                                                                                                                    | 
            Size = 40;                                                                                                                                           | 
            goto LABEL_397;                                                                                                                                      | 
          case 0x96u:                                                                                                                                            | 
            if ( v5 != 28 )                                                                                                                                      | 
            {                                                                                                                                                    | 
              goto LABEL_426;                                                                                                                                    | 
            }                                                                                                                                                    | 
            goto LABEL_219;                                                                                                                                      | 
          case 0x97u:                                                                                                                                            | 
            if ( v5 != 28 )                                                                                                                                      | 
            {                                                                                                                                                    | 
              goto LABEL_426;                                                                                                                                    | 
            }                                                                                                                                                    | 
            goto LABEL_218;                                                                                                                                      | 
        }                                                                                                                                                        | 
        if ( a1 != 152 )                                                                                                                                         | 
        {                                                                                                                                                        | 
          if ( a1 == 153 )                                                                                                                                       | 
          {                                                                                                                                                      | 
            if ( v5 != 128 )                                                                                                                                     | 
            {                                                                                                                                                    | 
              goto LABEL_426;                                                                                                                                    | 
            }                                                                                                                                                    | 
            Size = 128;                                                                                                                                          | 
            goto LABEL_397;                                                                                                                                      | 
          }                                                                                                                                                      | 
          if ( v5 != 548 )                                                                                                                                       | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          Size = 548;                                                                                                                                            | 
          v50 = PsGetCurrentProcessWow64Process();                                                                                                               | 
          ProbeForWrite((volatile void *)a3, 0x224uLL, v50 != 0 ? 1 : 4);                                                                                        | 
          Address = (volatile void *)a3;                                                                                                                         | 
          v51 = (_OWORD *)a3;                                                                                                                                    | 
          v52 = v30;                                                                                                                                             | 
          do                                                                                                                                                     | 
          {                                                                                                                                                      | 
            *v52 = *v51;                                                                                                                                         | 
            v52[1] = v51[1];                                                                                                                                     | 
            v52[2] = v51[2];                                                                                                                                     | 
            v52[3] = v51[3];                                                                                                                                     | 
            v52[4] = v51[4];                                                                                                                                     | 
            v52[5] = v51[5];                                                                                                                                     | 
            v52[6] = v51[6];                                                                                                                                     | 
            v52 += 8;                                                                                                                                            | 
            *(v52 - 1) = v51[7];                                                                                                                                 | 
            v51 += 8;                                                                                                                                            | 
            --v26;                                                                                                                                               | 
          }                                                                                                                                                      | 
          while ( v26 );                                                                                                                                         | 
          *v52 = *v51;                                                                                                                                           | 
          v52[1] = v51[1];                                                                                                                                       | 
          *((_DWORD *)v52 + 8) = *((_DWORD *)v51 + 8);                                                                                                           | 
          a3 = (ULONG64)v30;                                                                                                                                     | 
LABEL_229:                                                                                                                                                       | 
          v68 = 1;                                                                                                                                               | 
          goto LABEL_427;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( v5 != 128 )                                                                                                                                         | 
        {                                                                                                                                                        | 
          goto LABEL_426;                                                                                                                                        | 
        }                                                                                                                                                        | 
        Size = 128;                                                                                                                                              | 
        goto LABEL_404;                                                                                                                                          | 
      }                                                                                                                                                          | 
LABEL_367:                                                                                                                                                       | 
      if ( v5 != 12 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_426;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_149;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 > 0xAF )                                                                                                                                             | 
    {                                                                                                                                                            | 
      switch ( a1 )                                                                                                                                              | 
      {                                                                                                                                                          | 
        case 0xB0u:                                                                                                                                              | 
          if ( v5 )                                                                                                                                              | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          break;                                                                                                                                                 | 
        case 0xB2u:                                                                                                                                              | 
          if ( v5 != 4 )                                                                                                                                         | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          break;                                                                                                                                                 | 
        case 0xB3u:                                                                                                                                              | 
          if ( v5 != 4 )                                                                                                                                         | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          Size = 4;                                                                                                                                              | 
          goto LABEL_396;                                                                                                                                        | 
        case 0xB4u:                                                                                                                                              | 
          if ( v5 != 56 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
LABEL_231:                                                                                                                                                       | 
          Size = 56;                                                                                                                                             | 
          goto LABEL_404;                                                                                                                                        | 
        case 0xB5u:                                                                                                                                              | 
          if ( v5 != 56 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          goto LABEL_230;                                                                                                                                        | 
        default:                                                                                                                                                 | 
          goto LABEL_407;                                                                                                                                        | 
      }                                                                                                                                                          | 
    }                                                                                                                                                            | 
    else                                                                                                                                                         | 
    {                                                                                                                                                            | 
      switch ( a1 )                                                                                                                                              | 
      {                                                                                                                                                          | 
        case 0xAFu:                                                                                                                                              | 
          if ( ((PsGetCurrentProcessWow64Process() == 0 ? 3 : 0) & (unsigned __int8)a3) != 0 )                                                                   | 
          {                                                                                                                                                      | 
            ExRaiseDatatypeMisalignment();                                                                                                                       | 
          }                                                                                                                                                      | 
          if ( *(_DWORD *)a3 )                                                                                                                                   | 
          {                                                                                                                                                      | 
            switch ( *(_DWORD *)a3 )                                                                                                                             | 
            {                                                                                                                                                    | 
              case 1:                                                                                                                                            | 
                Size = 44;                                                                                                                                       | 
                break;                                                                                                                                           | 
              case 2:                                                                                                                                            | 
                Size = 48;                                                                                                                                       | 
                break;                                                                                                                                           | 
              case 0xFFFFFFFF:                                                                                                                                   | 
                Size = 64;                                                                                                                                       | 
                break;                                                                                                                                           | 
              default:                                                                                                                                           | 
                goto LABEL_426;                                                                                                                                  | 
            }                                                                                                                                                    | 
          }                                                                                                                                                      | 
          else                                                                                                                                                   | 
          {                                                                                                                                                      | 
            Size = 24;                                                                                                                                           | 
          }                                                                                                                                                      | 
          if ( (unsigned int)Feature_TouchpadSPIRelaxedCheck__private_IsEnabledDeviceUsageNoInline() )                                                           | 
          {                                                                                                                                                      | 
            if ( v63 < Size )                                                                                                                                    | 
            {                                                                                                                                                    | 
              goto LABEL_426;                                                                                                                                    | 
            }                                                                                                                                                    | 
          }                                                                                                                                                      | 
          else if ( v63 != Size )                                                                                                                                | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          goto LABEL_396;                                                                                                                                        | 
        case 0xA7u:                                                                                                                                              | 
          goto LABEL_367;                                                                                                                                        | 
        case 0xA8u:                                                                                                                                              | 
        case 0xAAu:                                                                                                                                              | 
          goto LABEL_422;                                                                                                                                        | 
      }                                                                                                                                                          | 
      if ( a1 != 172 )                                                                                                                                           | 
      {                                                                                                                                                          | 
        if ( a1 == 173 )                                                                                                                                         | 
        {                                                                                                                                                        | 
          if ( v5 != 4 )                                                                                                                                         | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          Size = 4;                                                                                                                                              | 
          goto LABEL_397;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 != 174 )                                                                                                                                         | 
        {                                                                                                                                                        | 
          goto LABEL_407;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( ((PsGetCurrentProcessWow64Process() == 0 ? 3 : 0) & (unsigned __int8)a3) != 0 )                                                                     | 
        {                                                                                                                                                        | 
          ExRaiseDatatypeMisalignment();                                                                                                                         | 
        }                                                                                                                                                        | 
        if ( *(_DWORD *)a3 )                                                                                                                                     | 
        {                                                                                                                                                        | 
          switch ( *(_DWORD *)a3 )                                                                                                                               | 
          {                                                                                                                                                      | 
            case 1:                                                                                                                                              | 
              v55 = 44;                                                                                                                                          | 
              break;                                                                                                                                             | 
            case 2:                                                                                                                                              | 
              v55 = 48;                                                                                                                                          | 
              break;                                                                                                                                             | 
            case 0xFFFFFFFF:                                                                                                                                     | 
              v55 = 64;                                                                                                                                          | 
              break;                                                                                                                                             | 
            default:                                                                                                                                             | 
              goto LABEL_426;                                                                                                                                    | 
          }                                                                                                                                                      | 
        }                                                                                                                                                        | 
        else                                                                                                                                                     | 
        {                                                                                                                                                        | 
          v55 = 24;                                                                                                                                              | 
        }                                                                                                                                                        | 
        Size = v55;                                                                                                                                              | 
        if ( (unsigned int)Feature_TouchpadSPIRelaxedCheck__private_IsEnabledDeviceUsageNoInline() )                                                             | 
        {                                                                                                                                                        | 
          if ( v63 < v55 )                                                                                                                                       | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
        }                                                                                                                                                        | 
        else if ( v63 != v55 )                                                                                                                                   | 
        {                                                                                                                                                        | 
          goto LABEL_426;                                                                                                                                        | 
        }                                                                                                                                                        | 
        *(_DWORD *)v30 = *(_DWORD *)a3;                                                                                                                          | 
        goto LABEL_404;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( v5 != 4 )                                                                                                                                             | 
      {                                                                                                                                                          | 
        goto LABEL_426;                                                                                                                                          | 
      }                                                                                                                                                          | 
    }                                                                                                                                                            | 
    Size = 4;                                                                                                                                                    | 
    goto LABEL_404;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 == 98 )                                                                                                                                                | 
  {                                                                                                                                                              | 
    goto LABEL_422;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 > 0x32 )                                                                                                                                               | 
  {                                                                                                                                                              | 
    if ( a1 > 0x43 )                                                                                                                                             | 
    {                                                                                                                                                            | 
      if ( a1 > 0x50 )                                                                                                                                           | 
      {                                                                                                                                                          | 
        if ( a1 == 83 || a1 == 84 )                                                                                                                              | 
        {                                                                                                                                                        | 
          goto LABEL_422;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 != 89 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          if ( a1 != 90 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            v42 = a1 - 94;                                                                                                                                       | 
            if ( a1 == 94 )                                                                                                                                      | 
            {                                                                                                                                                    | 
              goto LABEL_422;                                                                                                                                    | 
            }                                                                                                                                                    | 
LABEL_257:                                                                                                                                                       | 
            v47 = v42 == 1;                                                                                                                                      | 
LABEL_283:                                                                                                                                                       | 
            if ( v47 )                                                                                                                                           | 
            {                                                                                                                                                    | 
              goto LABEL_422;                                                                                                                                    | 
            }                                                                                                                                                    | 
            goto LABEL_407;                                                                                                                                      | 
          }                                                                                                                                                      | 
          goto LABEL_220;                                                                                                                                        | 
        }                                                                                                                                                        | 
      }                                                                                                                                                          | 
      else                                                                                                                                                       | 
      {                                                                                                                                                          | 
        if ( a1 == 80 || a1 == 68 || a1 == 70 )                                                                                                                  | 
        {                                                                                                                                                        | 
          goto LABEL_422;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 != 72 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          if ( a1 != 73 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            if ( a1 == 74 )                                                                                                                                      | 
            {                                                                                                                                                    | 
              goto LABEL_422;                                                                                                                                    | 
            }                                                                                                                                                    | 
            v47 = a1 == 79;                                                                                                                                      | 
            goto LABEL_283;                                                                                                                                      | 
          }                                                                                                                                                      | 
          goto LABEL_220;                                                                                                                                        | 
        }                                                                                                                                                        | 
      }                                                                                                                                                          | 
LABEL_258:                                                                                                                                                       | 
      Size = 8;                                                                                                                                                  | 
      goto LABEL_404;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 != 67 )                                                                                                                                              | 
    {                                                                                                                                                            | 
      if ( a1 <= 0x3A )                                                                                                                                          | 
      {                                                                                                                                                          | 
        if ( a1 != 58 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          if ( a1 == 51 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            Size = 24;                                                                                                                                           | 
            goto LABEL_397;                                                                                                                                      | 
          }                                                                                                                                                      | 
          if ( a1 != 52 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            if ( a1 != 53 )                                                                                                                                      | 
            {                                                                                                                                                    | 
              if ( a1 != 54 )                                                                                                                                    | 
              {                                                                                                                                                  | 
                v42 = a1 - 55;                                                                                                                                   | 
                if ( a1 == 55 )                                                                                                                                  | 
                {                                                                                                                                                | 
LABEL_218:                                                                                                                                                       | 
                  Size = 28;                                                                                                                                     | 
                  goto LABEL_397;                                                                                                                                | 
                }                                                                                                                                                | 
                goto LABEL_257;                                                                                                                                  | 
              }                                                                                                                                                  | 
LABEL_219:                                                                                                                                                       | 
              Size = 28;                                                                                                                                         | 
              goto LABEL_404;                                                                                                                                    | 
            }                                                                                                                                                    | 
LABEL_220:                                                                                                                                                       | 
            Size = 8;                                                                                                                                            | 
            goto LABEL_397;                                                                                                                                      | 
          }                                                                                                                                                      | 
        }                                                                                                                                                        | 
        goto LABEL_258;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 == 59 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_220;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( a1 != 60 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        if ( a1 != 61 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          if ( a1 != 64 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            if ( a1 != 65 )                                                                                                                                      | 
            {                                                                                                                                                    | 
              if ( a1 != 66 )                                                                                                                                    | 
              {                                                                                                                                                  | 
                goto LABEL_407;                                                                                                                                  | 
              }                                                                                                                                                  | 
              Size = 16;                                                                                                                                         | 
              v43 = PsGetCurrentProcessWow64Process();                                                                                                           | 
              ProbeForWrite((volatile void *)a3, 0x10uLL, v43 != 0 ? 1 : 4);                                                                                     | 
              Address = (volatile void *)a3;                                                                                                                     | 
              *(_OWORD *)v30 = *(_OWORD *)a3;                                                                                                                    | 
              a3 = (ULONG64)v30;                                                                                                                                 | 
              ProbeForWrite(*((volatile void **)v30 + 1), 0x100uLL, 2u);                                                                                         | 
              goto LABEL_229;                                                                                                                                    | 
            }                                                                                                                                                    | 
LABEL_230:                                                                                                                                                       | 
            Size = 56;                                                                                                                                           | 
            goto LABEL_397;                                                                                                                                      | 
          }                                                                                                                                                      | 
          goto LABEL_231;                                                                                                                                        | 
        }                                                                                                                                                        | 
LABEL_148:                                                                                                                                                       | 
        Size = 12;                                                                                                                                               | 
        goto LABEL_397;                                                                                                                                          | 
      }                                                                                                                                                          | 
LABEL_149:                                                                                                                                                       | 
      Size = 12;                                                                                                                                                 | 
      goto LABEL_404;                                                                                                                                            | 
    }                                                                                                                                                            | 
    v73 = 0LL;                                                                                                                                                   | 
    v74 = 0LL;                                                                                                                                                   | 
    RtlCopyFromUser(&v73, (void *)a3, 0x18uLL);                                                                                                                  | 
    *(_OWORD *)v30 = v73;                                                                                                                                        | 
    *((_QWORD *)v30 + 2) = v74;                                                                                                                                  | 
    a3 = (ULONG64)v30;                                                                                                                                           | 
    v44 = *((_QWORD *)v30 + 2);                                                                                                                                  | 
    if ( (v44 & 1) != 0 )                                                                                                                                        | 
    {                                                                                                                                                            | 
      ExRaiseDatatypeMisalignment();                                                                                                                             | 
    }                                                                                                                                                            | 
    v45 = v30[4];                                                                                                                                                | 
    v46 = v44 + v45 + 2;                                                                                                                                         | 
    if ( v46 <= v44 || v46 >= MmUserProbeAddress )                                                                                                               | 
    {                                                                                                                                                            | 
      ExRaiseAccessViolation();                                                                                                                                  | 
    }                                                                                                                                                            | 
    if ( (unsigned __int16)v45 > v30[5] )                                                                                                                        | 
    {                                                                                                                                                            | 
      if ( (v45 & 1) == 0 )                                                                                                                                      | 
      {                                                                                                                                                          | 
        goto LABEL_242;                                                                                                                                          | 
      }                                                                                                                                                          | 
    }                                                                                                                                                            | 
    else if ( (v45 & 1) == 0 )                                                                                                                                   | 
    {                                                                                                                                                            | 
      if ( !(_WORD)v45 )                                                                                                                                         | 
      {                                                                                                                                                          | 
        *((_QWORD *)v30 + 2) = 0LL;                                                                                                                              | 
      }                                                                                                                                                          | 
      goto LABEL_427;                                                                                                                                            | 
    }                                                                                                                                                            | 
    v70 = 0x20000;                                                                                                                                               | 
    MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 487LL);                                                                                   | 
LABEL_242:                                                                                                                                                       | 
    ExRaiseAccessViolation();                                                                                                                                    | 
  }                                                                                                                                                              | 
  if ( a1 == 50 )                                                                                                                                                | 
  {                                                                                                                                                              | 
    v5 = 24;                                                                                                                                                     | 
LABEL_208:                                                                                                                                                       | 
    Size = v5;                                                                                                                                                   | 
    goto LABEL_404;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 > 0x19 )                                                                                                                                               | 
  {                                                                                                                                                              | 
    if ( a1 <= 0x2A )                                                                                                                                            | 
    {                                                                                                                                                            | 
      if ( a1 != 42 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        if ( a1 == 27 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          goto LABEL_422;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 == 31 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          Size = 92;                                                                                                                                             | 
          goto LABEL_404;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 != 34 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          if ( a1 == 35 || a1 == 38 )                                                                                                                            | 
          {                                                                                                                                                      | 
            goto LABEL_422;                                                                                                                                      | 
          }                                                                                                                                                      | 
          if ( a1 != 41 )                                                                                                                                        | 
          {                                                                                                                                                      | 
            goto LABEL_407;                                                                                                                                      | 
          }                                                                                                                                                      | 
          if ( ((v5 - 500) & 0xFFFFFFFB) != 0 )                                                                                                                  | 
          {                                                                                                                                                      | 
            goto LABEL_426;                                                                                                                                      | 
          }                                                                                                                                                      | 
          goto LABEL_208;                                                                                                                                        | 
        }                                                                                                                                                        | 
        Size = 92;                                                                                                                                               | 
LABEL_397:                                                                                                                                                       | 
        if ( (a3 & 3) != 0 )                                                                                                                                     | 
        {                                                                                                                                                        | 
          ExRaiseDatatypeMisalignment();                                                                                                                         | 
        }                                                                                                                                                        | 
        goto LABEL_399;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( ((v5 - 500) & 0xFFFFFFFB) != 0 )                                                                                                                      | 
      {                                                                                                                                                          | 
        goto LABEL_426;                                                                                                                                          | 
      }                                                                                                                                                          | 
      Size = v5;                                                                                                                                                 | 
LABEL_396:                                                                                                                                                       | 
      if ( !Size )                                                                                                                                               | 
      {                                                                                                                                                          | 
LABEL_399:                                                                                                                                                       | 
        memmove(v30, (const void *)a3, Size);                                                                                                                    | 
        a3 = (ULONG64)v30;                                                                                                                                       | 
        goto LABEL_427;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_397;                                                                                                                                            | 
    }                                                                                                                                                            | 
    switch ( a1 )                                                                                                                                                | 
    {                                                                                                                                                            | 
      case '+':                                                                                                                                                  | 
        Size = 20;                                                                                                                                               | 
        goto LABEL_404;                                                                                                                                          | 
      case ',':                                                                                                                                                  | 
        Size = 20;                                                                                                                                               | 
        goto LABEL_397;                                                                                                                                          | 
      case '-':                                                                                                                                                  | 
        Size = 108;                                                                                                                                              | 
        goto LABEL_404;                                                                                                                                          | 
      case '.':                                                                                                                                                  | 
        Size = 108;                                                                                                                                              | 
        goto LABEL_397;                                                                                                                                          | 
    }                                                                                                                                                            | 
    if ( a1 != 47 )                                                                                                                                              | 
    {                                                                                                                                                            | 
      if ( a1 != 48 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_407;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_202;                                                                                                                                            | 
    }                                                                                                                                                            | 
LABEL_335:                                                                                                                                                       | 
    Size = 16;                                                                                                                                                   | 
    goto LABEL_397;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 == 25 )                                                                                                                                                | 
  {                                                                                                                                                              | 
    goto LABEL_422;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 <= 0xE )                                                                                                                                               | 
  {                                                                                                                                                              | 
    if ( a1 == 14 || a1 == 1 )                                                                                                                                   | 
    {                                                                                                                                                            | 
      goto LABEL_422;                                                                                                                                            | 
    }                                                                                                                                                            | 
    if ( a1 != 3 )                                                                                                                                               | 
    {                                                                                                                                                            | 
      if ( a1 != 4 )                                                                                                                                             | 
      {                                                                                                                                                          | 
        if ( a1 == 5 || a1 == 10 )                                                                                                                               | 
        {                                                                                                                                                        | 
          goto LABEL_422;                                                                                                                                        | 
        }                                                                                                                                                        | 
        if ( a1 != 13 )                                                                                                                                          | 
        {                                                                                                                                                        | 
          goto LABEL_407;                                                                                                                                        | 
        }                                                                                                                                                        | 
LABEL_156:                                                                                                                                                       | 
        if ( (a3 & 0xFFFFFFFFFFFF0000uLL) == 0 )                                                                                                                 | 
        {                                                                                                                                                        | 
          goto LABEL_427;                                                                                                                                        | 
        }                                                                                                                                                        | 
LABEL_422:                                                                                                                                                       | 
        Size = 4;                                                                                                                                                | 
        Address = (volatile void *)a3;                                                                                                                           | 
        a3 = (ULONG64)v30;                                                                                                                                       | 
        v58 = Address;                                                                                                                                           | 
        if ( (unsigned __int64)Address >= MmUserProbeAddress )                                                                                                   | 
        {                                                                                                                                                        | 
          v58 = (_DWORD *)MmUserProbeAddress;                                                                                                                    | 
        }                                                                                                                                                        | 
        *v58 = *v58;                                                                                                                                             | 
        v68 = 1;                                                                                                                                                 | 
        goto LABEL_427;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_148;                                                                                                                                            | 
    }                                                                                                                                                            | 
    goto LABEL_149;                                                                                                                                              | 
  }                                                                                                                                                              | 
  switch ( a1 )                                                                                                                                                  | 
  {                                                                                                                                                              | 
    case 0x10u:                                                                                                                                                  | 
    case 0x12u:                                                                                                                                                  | 
      goto LABEL_422;                                                                                                                                            | 
    case 0x14u:                                                                                                                                                  | 
      if ( !a3 )                                                                                                                                                 | 
      {                                                                                                                                                          | 
        a3 = -1LL;                                                                                                                                               | 
      }                                                                                                                                                          | 
      if ( a3 == -2LL || a3 == -1LL )                                                                                                                            | 
      {                                                                                                                                                          | 
LABEL_160:                                                                                                                                                       | 
        v63 = -1;                                                                                                                                                | 
        goto LABEL_427;                                                                                                                                          | 
      }                                                                                                                                                          | 
LABEL_165:                                                                                                                                                       | 
      v35 = (unsigned int *)a3;                                                                                                                                  | 
      if ( a3 >= MmUserProbeAddress )                                                                                                                            | 
      {                                                                                                                                                          | 
        v35 = (unsigned int *)MmUserProbeAddress;                                                                                                                | 
      }                                                                                                                                                          | 
      v36 = *v35;                                                                                                                                                | 
      v70 = v36;                                                                                                                                                 | 
      LODWORD(v75) = v36;                                                                                                                                        | 
      v37 = (const void *)*((_QWORD *)v35 + 1);                                                                                                                  | 
      *((_QWORD *)&v75 + 1) = v37;                                                                                                                               | 
      if ( ((unsigned __int8)v37 & 1) != 0 )                                                                                                                     | 
      {                                                                                                                                                          | 
        ExRaiseDatatypeMisalignment();                                                                                                                           | 
      }                                                                                                                                                          | 
      v38 = (unsigned __int16)v36;                                                                                                                               | 
      v39 = (ULONG64)v37 + (unsigned __int16)v36 + 2;                                                                                                            | 
      if ( v39 <= (unsigned __int64)v37 || v39 >= MmUserProbeAddress )                                                                                           | 
      {                                                                                                                                                          | 
        ExRaiseAccessViolation();                                                                                                                                | 
      }                                                                                                                                                          | 
      if ( (unsigned __int16)v36 > HIWORD(v70) )                                                                                                                 | 
      {                                                                                                                                                          | 
        if ( (v36 & 1) == 0 )                                                                                                                                    | 
        {                                                                                                                                                        | 
          goto LABEL_180;                                                                                                                                        | 
        }                                                                                                                                                        | 
      }                                                                                                                                                          | 
      else if ( (v36 & 1) == 0 )                                                                                                                                 | 
      {                                                                                                                                                          | 
        a3 = (ULONG64)Win32AllocPoolWithQuotaZInitImpl(v36, (unsigned __int16)v36 + 2LL, 0x79747355u);                                                           | 
        if ( !a3 )                                                                                                                                               | 
        {                                                                                                                                                        | 
          ExRaiseStatus(-1073741801);                                                                                                                            | 
        }                                                                                                                                                        | 
        if ( v65 != (__int64 (__fastcall *)(PVOID))-1LL )                                                                                                        | 
        {                                                                                                                                                        | 
          v40 = PtiCurrent();                                                                                                                                    | 
          KeBugCheckEx(0x164u, 0x12uLL, (ULONG_PTR)v64, a3, (ULONG_PTR)v40);                                                                                     | 
        }                                                                                                                                                        | 
        v41 = PtiCurrent();                                                                                                                                      | 
        v64[0] = *((_QWORD *)v41 + 48);                                                                                                                          | 
        *((_QWORD *)v41 + 48) = v64;                                                                                                                             | 
        v64[1] = a3;                                                                                                                                             | 
        v65 = GreDeleteFastMutex;                                                                                                                                | 
        memmove((void *)a3, v37, v38);                                                                                                                           | 
        *(_WORD *)(a3 + 2 * (v38 >> 1)) = 0;                                                                                                                     | 
        goto LABEL_427;                                                                                                                                          | 
      }                                                                                                                                                          | 
      v69 = 0x20000;                                                                                                                                             | 
      MicrosoftTelemetryAssertTriggeredArgsKM("IXPTelAssert", 0x20000LL, 345LL);                                                                                 | 
LABEL_180:                                                                                                                                                       | 
      ExRaiseAccessViolation();                                                                                                                                  | 
    case 0x15u:                                                                                                                                                  | 
      if ( v5 == -1 )                                                                                                                                            | 
      {                                                                                                                                                          | 
        goto LABEL_427;                                                                                                                                          | 
      }                                                                                                                                                          | 
      if ( !a3 )                                                                                                                                                 | 
      {                                                                                                                                                          | 
        goto LABEL_160;                                                                                                                                          | 
      }                                                                                                                                                          | 
      goto LABEL_165;                                                                                                                                            | 
    case 0x16u:                                                                                                                                                  | 
      goto LABEL_422;                                                                                                                                            | 
    case 0x18u:                                                                                                                                                  | 
      goto LABEL_156;                                                                                                                                            | 
  }                                                                                                                                                              | 
LABEL_407:                                                                                                                                                       | 
  if ( a1 < 0xB6 )                                                                                                                                               | 
  {                                                                                                                                                              | 
    goto LABEL_427;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 - 4096 > 0x55 && a1 - 0x2000 > 0x37                                                                                                                    | 
    || v5 && (*((_DWORD *)PtiCurrent() + 168) > 0x400u || a1 != 4159 || v5 != 1) )                                                                               | 
  {                                                                                                                                                              | 
    goto LABEL_426;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( (a1 & 1) == 0 )                                                                                                                                           | 
  {                                                                                                                                                              | 
    goto LABEL_422;                                                                                                                                              | 
  }                                                                                                                                                              | 
  if ( a1 != 8203 )                                                                                                                                              | 
  {                                                                                                                                                              | 
    if ( a1 == 8211 )                                                                                                                                            | 
    {                                                                                                                                                            | 
      v76 = a3;                                                                                                                                                  | 
      if ( (a3 & 0xFFFFFFFE) != 0 )                                                                                                                              | 
      {                                                                                                                                                          | 
        goto LABEL_426;                                                                                                                                          | 
      }                                                                                                                                                          | 
    }                                                                                                                                                            | 
LABEL_427:                                                                                                                                                       | 
    v31 = xxxSystemParametersInfo(a1, v63);                                                                                                                      | 
    if ( v68 )                                                                                                                                                   | 
    {                                                                                                                                                            | 
      memmove((void *)Address, (const void *)a3, Size);                                                                                                          | 
    }                                                                                                                                                            | 
    goto LABEL_434;                                                                                                                                              | 
  }                                                                                                                                                              | 
  v77 = a3;                                                                                                                                                      | 
  if ( (a3 & 0xFFFFFFFC) == 0 )                                                                                                                                  | 
  {                                                                                                                                                              | 
    goto LABEL_427;                                                                                                                                              | 
  }                                                                                                                                                              | 
LABEL_426:                                                                                                                                                       | 
  UserSetLastError(87LL);                                                                                                                                        | 
  v31 = 0;                                                                                                                                                       | 
  Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>::~Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>(v64);                  | 
  Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>::~Win32RawOptionalLockedItemAlways<void,&void Win32FreePool(void *)>(BugCheckParameter2);   | 
LABEL_435:                                                                                                                                                       | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                                                                                      | 
  return v31;                                                                                                                                                    | 
}                                                                                                                                                                | 
1213 row(s)
