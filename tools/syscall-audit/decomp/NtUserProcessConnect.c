NTSTATUS __fastcall NtUserProcessConnect(void *a1, unsigned int a2, volatile void *a3)                           | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  Handle = a1;                                                                                                   | 
  v25 = a2;                                                                                                      | 
  Address = a3;                                                                                                  | 
  memset(&ApcState, 0, sizeof(ApcState));                                                                        | 
  memset_0(&Src, 0, 0x248uLL);                                                                                   | 
  v28 = 0LL;                                                                                                     | 
  ProcessPeb = 0LL;                                                                                              | 
  Length = a2 + 8;                                                                                               | 
  if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                              | 
    || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 4) == 0                                             | 
    || *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 4u )                                                  | 
  {                                                                                                              | 
    v4 = 0;                                                                                                      | 
    v5 = 1;                                                                                                      | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    v5 = 1;                                                                                                      | 
    v4 = 1;                                                                                                      | 
  }                                                                                                              | 
  v6 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                                | 
  if ( v4 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                          | 
  {                                                                                                              | 
    UserSessionState = W32GetUserSessionState();                                                                 | 
    LOBYTE(v8) = v6;                                                                                             | 
    LOBYTE(v9) = v4;                                                                                             | 
    WPP_RECORDER_AND_TRACE_SF_(                                                                                  | 
      *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                                        | 
      v9,                                                                                                        | 
      v8,                                                                                                        | 
      *(_QWORD *)(UserSessionState + 69512),                                                                     | 
      4,                                                                                                         | 
      3,                                                                                                         | 
      16,                                                                                                        | 
      (__int64)&WPP_8175567c5340347c36ff9ce21a4803c0_Traceguids);                                                | 
  }                                                                                                              | 
  if ( a2 > 0x248 )                                                                                              | 
  {                                                                                                              | 
    return -1073741823;                                                                                          | 
  }                                                                                                              | 
  v10 = *(_QWORD *)(W32GetUserGdiSessionState() + 40);                                                           | 
  if ( v10 != PsGetCurrentProcess() )                                                                            | 
  {                                                                                                              | 
    return -1073741823;                                                                                          | 
  }                                                                                                              | 
  v11 = **(__int64 (***)(void))(W32GetWin32kFullApiSetTable()                                                    | 
                              + 24LL * ext_ms_win_moderncore_win32k_base_sysentry_l1_ApiSetTableIndex);          | 
  if ( v11 )                                                                                                     | 
  {                                                                                                              | 
    v12 = v11();                                                                                                 | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    v12 = -1073741637;                                                                                           | 
  }                                                                                                              | 
  if ( v12 < 0 )                                                                                                 | 
  {                                                                                                              | 
    return -1073741637;                                                                                          | 
  }                                                                                                              | 
  ProbeForWrite(Address, Length, 4u);                                                                            | 
  v13 = *(_DWORD *)Address;                                                                                      | 
  Src = *(_DWORD *)Address;                                                                                      | 
  v14 = *(__int64 (**)(void))(*(_QWORD *)(W32GetWin32kFullApiSetTable()                                          | 
                                        + 24LL * ext_ms_win_moderncore_win32k_base_sysentry_l1_ApiSetTableIndex) | 
                            + 8LL);                                                                              | 
  if ( v14 )                                                                                                     | 
  {                                                                                                              | 
    LODWORD(v14) = v14();                                                                                        | 
  }                                                                                                              | 
  if ( v13 != (_DWORD)v14 )                                                                                      | 
  {                                                                                                              | 
    return -1073741811;                                                                                          | 
  }                                                                                                              | 
  Object = 0LL;                                                                                                  | 
  result = ObReferenceObjectByHandle(Handle, 0, (POBJECT_TYPE)PsProcessType, 1, &Object, 0LL);                   | 
  v16 = Object;                                                                                                  | 
  Handle = Object;                                                                                               | 
  if ( result >= 0 )                                                                                             | 
  {                                                                                                              | 
    if ( v16 == (PVOID)PsGetCurrentProcess()                                                                     | 
      || !(unsigned int)PsIsProtectedProcess(v16)                                                                | 
      || (unsigned int)PsIsProtectedProcessLight(v16) )                                                          | 
    {                                                                                                            | 
      v24 = 0;                                                                                                   | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v27 = 0LL;                                                                                                 | 
      v33 = 0LL;                                                                                                 | 
      inited = PsAcquireProcessExitSynchronization(v16);                                                         | 
      if ( inited < 0 )                                                                                          | 
      {                                                                                                          | 
LABEL_22:                                                                                                        | 
        ObfDereferenceObject(v16);                                                                               | 
        if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                        | 
          || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 4) == 0                                       | 
          || *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 4u )                                            | 
        {                                                                                                        | 
          v5 = 0;                                                                                                | 
        }                                                                                                        | 
        v18 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                         | 
        if ( v5 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                    | 
        {                                                                                                        | 
          v19 = W32GetUserSessionState();                                                                        | 
          LOBYTE(v20) = v18;                                                                                     | 
          LOBYTE(v21) = v5;                                                                                      | 
          WPP_RECORDER_AND_TRACE_SF_(                                                                            | 
            *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                                  | 
            v21,                                                                                                 | 
            v20,                                                                                                 | 
            *(_QWORD *)(v19 + 69512),                                                                            | 
            4,                                                                                                   | 
            3,                                                                                                   | 
            17,                                                                                                  | 
            (__int64)&WPP_8175567c5340347c36ff9ce21a4803c0_Traceguids);                                          | 
        }                                                                                                        | 
        return inited;                                                                                           | 
      }                                                                                                          | 
      v24 = 1;                                                                                                   | 
      ProcessPeb = PsGetProcessPeb(v16);                                                                         | 
      KeStackAttachProcess((PRKPROCESS)v16, &ApcState);                                                          | 
      _m_prefetchw((const void *)(ProcessPeb + 88));                                                             | 
      v22 = _InterlockedAnd64((volatile signed __int64 *)(ProcessPeb + 88), 0xFFFFFFFFFFFFFFFEuLL);              | 
      v27 = v22;                                                                                                 | 
      if ( (v22 & 1) == 0 )                                                                                      | 
      {                                                                                                          | 
        inited = -1073741823;                                                                                    | 
        goto LABEL_20;                                                                                           | 
      }                                                                                                          | 
      v27 = v22 & 0xFFFFFFFFFFFFFFFEuLL;                                                                         | 
      ProbeForRead((volatile void *)(v22 & 0xFFFFFFFFFFFFFFFEuLL), 0x10uLL, 4u);                                 | 
      v33 = *(_OWORD *)(v22 & 0xFFFFFFFFFFFFFFFEuLL);                                                            | 
      if ( (_DWORD)v33 != a2 )                                                                                   | 
      {                                                                                                          | 
        inited = -1073741823;                                                                                    | 
        goto LABEL_20;                                                                                           | 
      }                                                                                                          | 
      v28 = (volatile void *)*((_QWORD *)&v33 + 1);                                                              | 
      ProbeForWrite(*((volatile void **)&v33 + 1), a2, 4u);                                                      | 
    }                                                                                                            | 
    EnterCrit(1LL, 0LL);                                                                                         | 
    inited = InitMapSharedSection((struct _EPROCESS *)v16, (struct _USERCONNECT *)&Src);                         | 
    UserSessionSwitchLeaveCrit();                                                                                | 
    if ( inited >= 0 )                                                                                           | 
    {                                                                                                            | 
      if ( v24 )                                                                                                 | 
      {                                                                                                          | 
        memmove((void *)v28, v35, a2);                                                                           | 
        _InterlockedOr(v23, 0);                                                                                  | 
        *(_QWORD *)(ProcessPeb + 88) = 0LL;                                                                      | 
LABEL_21:                                                                                                        | 
        KeUnstackDetachProcess(&ApcState);                                                                       | 
        PsReleaseProcessExitSynchronization(v16);                                                                | 
        goto LABEL_22;                                                                                           | 
      }                                                                                                          | 
      memmove((void *)Address, &Src, Length);                                                                    | 
    }                                                                                                            | 
LABEL_20:                                                                                                        | 
    if ( !v24 )                                                                                                  | 
    {                                                                                                            | 
      goto LABEL_22;                                                                                             | 
    }                                                                                                            | 
    goto LABEL_21;                                                                                               | 
  }                                                                                                              | 
  return result;                                                                                                 | 
}                                                                                                                | 
170 row(s)
