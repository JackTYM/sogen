__int64 __fastcall NtUserMsgWaitForMultipleObjectsEx(unsigned int a1, volatile void *a2, int a3, int a4, int a5) | 
{                                                                                                                | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                  | 
                                                                                                                 | 
  v47 = a3;                                                                                                      | 
  v6 = a1;                                                                                                       | 
  v46 = a5;                                                                                                      | 
  v7 = -1;                                                                                                       | 
  v48 = -1;                                                                                                      | 
  EnterCrit(0LL, 0LL);                                                                                           | 
  CurrentThreadNonPaged = (__int64 *)W32GetCurrentThreadNonPaged();                                              | 
  v9 = 0LL;                                                                                                      | 
  if ( CurrentThreadNonPaged )                                                                                   | 
  {                                                                                                              | 
    v10 = *CurrentThreadNonPaged;                                                                                | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    v10 = 0LL;                                                                                                   | 
  }                                                                                                              | 
  memset_0(v53, 0, 0x448uLL);                                                                                    | 
  v50 = 0LL;                                                                                                     | 
  if ( (a5 & 0xFFFFFFE0) != 0 || (unsigned int)v6 > 0x3F )                                                       | 
  {                                                                                                              | 
    UserSetLastError(87LL);                                                                                      | 
    goto LABEL_53;                                                                                               | 
  }                                                                                                              | 
  v12 = *(_QWORD *)(v10 + 488);                                                                                  | 
  if ( !v12 || (a5 & 1) != 0 && (_DWORD)v6 )                                                                     | 
  {                                                                                                              | 
    v19 = a4;                                                                                                    | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    if ( (a5 & 4) != 0 )                                                                                         | 
    {                                                                                                            | 
      v43 = *(_DWORD *)(v12 + 8);                                                                                | 
      v13 = (__int64 *)&v43;                                                                                     | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      LODWORD(ProcessPeb) = 0;                                                                                   | 
      v13 = &ProcessPeb;                                                                                         | 
    }                                                                                                            | 
    v14 = INTERLOCKEDINT::operator int(v12 + 4, v11, v13);                                                       | 
    v16 = INTERLOCKEDINT::operator int(v15, v14, v15);                                                           | 
    v18 = v17 | v16;                                                                                             | 
    v19 = a4;                                                                                                    | 
    if ( (v18 & (unsigned __int16)a4) != 0 )                                                                     | 
    {                                                                                                            | 
      v7 = v6;                                                                                                   | 
      goto LABEL_53;                                                                                             | 
    }                                                                                                            | 
  }                                                                                                              | 
  InputEvent = xxxGetInputEvent(v19 | ((unsigned __int16)a5 << 16));                                             | 
  if ( !InputEvent )                                                                                             | 
  {                                                                                                              | 
    goto LABEL_53;                                                                                               | 
  }                                                                                                              | 
  if ( (a5 & 1) != 0 )                                                                                           | 
  {                                                                                                              | 
    v49 = 0LL;                                                                                                   | 
    v20 = (__int64 *)W32GetCurrentThreadNonPaged();                                                              | 
    if ( v20 )                                                                                                   | 
    {                                                                                                            | 
      v21 = *v20;                                                                                                | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v21 = 0LL;                                                                                                 | 
    }                                                                                                            | 
    if ( (unsigned int)ZwCancelWaitCompletionPacket(*(_QWORD *)(v21 + 1640), 0LL) == 259 )                       | 
    {                                                                                                            | 
      LOBYTE(v22) = 1;                                                                                           | 
      ZwCancelWaitCompletionPacket(*(_QWORD *)(v21 + 1640), v22);                                                | 
      v23 = 1;                                                                                                   | 
      KeSetEvent(*(PRKEVENT *)(v21 + 768), 1, 0);                                                                | 
      goto LABEL_27;                                                                                             | 
    }                                                                                                            | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    v24 = (__int64 *)W32GetCurrentThreadNonPaged();                                                              | 
    if ( v24 )                                                                                                   | 
    {                                                                                                            | 
      v25 = *v24;                                                                                                | 
    }                                                                                                            | 
    else                                                                                                         | 
    {                                                                                                            | 
      v25 = 0LL;                                                                                                 | 
    }                                                                                                            | 
    v49 = *(_QWORD *)(v25 + 1624);                                                                               | 
    if ( !v49 )                                                                                                  | 
    {                                                                                                            | 
      goto LABEL_53;                                                                                             | 
    }                                                                                                            | 
  }                                                                                                              | 
  v23 = 1;                                                                                                       | 
LABEL_27:                                                                                                        | 
  CurrentProcess = PsGetCurrentProcess();                                                                        | 
  ProcessPeb = PsGetProcessPeb(CurrentProcess);                                                                  | 
  v27 = *(void **)(ProcessPeb + 32);                                                                             | 
  memset_0(v57, 0, 0x448uLL);                                                                                    | 
  RtlCopyFromUser(v57, v27, 0x448uLL);                                                                           | 
  v28 = v53;                                                                                                     | 
  v29 = v57;                                                                                                     | 
  v30 = 8LL;                                                                                                     | 
  do                                                                                                             | 
  {                                                                                                              | 
    *v28 = *v29;                                                                                                 | 
    v28[1] = v29[1];                                                                                             | 
    v28[2] = v29[2];                                                                                             | 
    v28[3] = v29[3];                                                                                             | 
    v28[4] = v29[4];                                                                                             | 
    v28[5] = v29[5];                                                                                             | 
    v28[6] = v29[6];                                                                                             | 
    v28 += 8;                                                                                                    | 
    *(v28 - 1) = v29[7];                                                                                         | 
    v29 += 8;                                                                                                    | 
    --v30;                                                                                                       | 
  }                                                                                                              | 
  while ( v30 );                                                                                                 | 
  *v28 = *v29;                                                                                                   | 
  v28[1] = v29[1];                                                                                               | 
  v28[2] = v29[2];                                                                                               | 
  v28[3] = v29[3];                                                                                               | 
  *((_QWORD *)v28 + 8) = *((_QWORD *)v29 + 8);                                                                   | 
  if ( (_DWORD)v6 )                                                                                              | 
  {                                                                                                              | 
    if ( !PsGetCurrentProcessWow64Process() )                                                                    | 
    {                                                                                                            | 
      v23 = 4;                                                                                                   | 
    }                                                                                                            | 
    ProbeForRead(a2, 8 * v6, v23);                                                                               | 
  }                                                                                                              | 
  v31 = (_QWORD *)Win32AllocPoolWithQuotaZInit(8LL * (unsigned int)(v6 + 1), 2037609301LL);                      | 
  v32 = v31;                                                                                                     | 
  v50 = v31;                                                                                                     | 
  if ( v31 )                                                                                                     | 
  {                                                                                                              | 
    memmove(v31, (const void *)a2, 8 * v6);                                                                      | 
    v34 = 0LL;                                                                                                   | 
    v45 = 0;                                                                                                     | 
    v35 = v56;                                                                                                   | 
    v36 = v55;                                                                                                   | 
    v37 = v54;                                                                                                   | 
    while ( (unsigned int)v34 < (unsigned int)v6 )                                                               | 
    {                                                                                                            | 
      v33 = LODWORD(v32[(unsigned int)v34]);                                                                     | 
      switch ( (_DWORD)v33 )                                                                                     | 
      {                                                                                                          | 
        case 0xFFFFFFF4:                                                                                         | 
          v32[(unsigned int)v34] = v35;                                                                          | 
          break;                                                                                                 | 
        case 0xFFFFFFF5:                                                                                         | 
          v32[(unsigned int)v34] = v36;                                                                          | 
          break;                                                                                                 | 
        case 0xFFFFFFF6:                                                                                         | 
          v32[(unsigned int)v34] = v37;                                                                          | 
          break;                                                                                                 | 
      }                                                                                                          | 
      v34 = (unsigned int)(v34 + 1);                                                                             | 
      v45 = v34;                                                                                                 | 
    }                                                                                                            | 
    v38 = v46;                                                                                                   | 
    if ( (v46 & 8) != 0 )                                                                                        | 
    {                                                                                                            | 
      CurrentThreadWin32Thread = (__int64 *)PsGetCurrentThreadWin32Thread(v34, v33, v35, v36);                   | 
      if ( CurrentThreadWin32Thread )                                                                            | 
      {                                                                                                          | 
        v9 = *CurrentThreadWin32Thread;                                                                          | 
      }                                                                                                          | 
      *(_QWORD *)(v9 + 1360) |= 0x400uLL;                                                                        | 
    }                                                                                                            | 
    v40 = v49;                                                                                                   | 
    if ( (a5 & 1) != 0 )                                                                                         | 
    {                                                                                                            | 
      v40 = InputEvent;                                                                                          | 
    }                                                                                                            | 
    v32[v6] = v40;                                                                                               | 
    Win32RawLockedItemNoCleanup<_ACCESS_ALLOWED_ACE,0>::Win32RawLockedItemNoCleanup<_ACCESS_ALLOWED_ACE,0>(      | 
      v52,                                                                                                       | 
      v32,                                                                                                       | 
      Win32FreePool,                                                                                             | 
      v36);                                                                                                      | 
    v7 = xxxMsgWaitForMultipleObjectsEx(v6, (_DWORD)v32, v47, a4, v38);                                          | 
    xxxClearWakeMask();                                                                                          | 
    Win32RawLockedItem<tagEVENT_PACKET_TARGETS,0>::~Win32RawLockedItem<tagEVENT_PACKET_TARGETS,0>(v52);          | 
    if ( (a5 & 1) != 0 )                                                                                         | 
    {                                                                                                            | 
      ReassociateQueueEventCompletionPacket();                                                                   | 
    }                                                                                                            | 
  }                                                                                                              | 
  else                                                                                                           | 
  {                                                                                                              | 
    UserSetLastError(8LL);                                                                                       | 
  }                                                                                                              | 
LABEL_53:                                                                                                        | 
  UserSessionSwitchLeaveCrit();                                                                                  | 
  return v7;                                                                                                     | 
}                                                                                                                | 
201 row(s)
