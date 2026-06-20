line
__int64 __fastcall NtSaveKeyEx(int a1, void *a2, int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  Object = 0LL;
  Handle = 0LL;
  v26 = 0LL;
  v24[1] = v24;
  v24[0] = v24;
  v25 = 0LL;
  v27 = 0LL;
  v28 = 0LL;
  v29 = 0LL;
  memset(&ApcState, 0, sizeof(ApcState));
  CmpInitializeThreadInfo(&v25);
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( !(unsigned __int8)CmpAcquireShutdownRundown() )
  {
    v7 = -1073741431;
    goto LABEL_10;
  }
  v7 = CmCheckNoTxContext();
  if ( v7 >= 0 )
  {
    if ( SeSinglePrivilegeCheck(SeBackupPrivilege, PreviousMode) )
    {
      if ( ((a3 - 1) & 0xFFFFFFFC) == 0 && a3 != 3 )
      {
        if ( PreviousMode == 1 )
        {
          LOBYTE(v8) = 1;
          v11 = 0LL;
          v7 = IoConvertFileHandleToKernelHandle(a2, v8, 2LL);
          if ( v7 < 0 )
          {
LABEL_8:
            if ( v11 && v11 != a2 )
            {
              ZwClose(v11);
            }
            goto LABEL_9;
          }
        }
        else
        {
          v11 = a2;
          Handle = a2;
        }
        LOBYTE(v10) = PreviousMode;
        v7 = CmObReferenceObjectByHandle(a1, 0, v9, v10, (__int64)&Object, 0LL);
        if ( v7 < 0 )
        {
          v14 = Object;
        }
        else
        {
          CurrentThread = KeGetCurrentThread();
          v14 = Object;
          v11 = Handle;
          --CurrentThread->KernelApcDisable;
          if ( !CmpCallBackCount
            || (unsigned int)CmpIsRegistryLockAcquired()
            || (*(_QWORD *)&v27 = v14,
                *((_QWORD *)&v27 + 1) = Handle,
                LOBYTE(v15) = 1,
                LODWORD(v28) = a3,
                v16 = CmpCallCallBacksEx(43, (unsigned int)&v27, 0, v15, 44, 0LL, (__int64)v24),
                v16 >= 0) )
          {
            CmpAttachToRegistryProcess(&ApcState);
            if ( a3 == 4 )
            {
              LOBYTE(v17) = PreviousMode;
              v19 = CmDumpKeyToFile(v14, v17, Handle);
            }
            else
            {
              v20 = 5LL;
              LOBYTE(v18) = PreviousMode;
              if ( a3 != 2 )
              {
                v20 = 3LL;
              }
              v19 = CmSaveKey(v14, Handle, v20, v18);
            }
            v21 = v19;
            CmpDetachFromRegistryProcess(&ApcState);
            v7 = CmPostCallbackNotificationEx(44, (_DWORD)v14, v21, (unsigned int)&v27, 0LL, (__int64)v24);
          }
          else
          {
            v7 = 0;
            if ( v16 != -1073740541 )
            {
              v7 = v16;
            }
          }
          KeLeaveCriticalRegion();
        }
        if ( v14 )
        {
          ObfDereferenceObject(v14);
        }
        goto LABEL_8;
      }
      v7 = -1073741811;
    }
    else
    {
      v7 = -1073741727;
    }
  }
LABEL_9:
  CmpReleaseShutdownRundown();
LABEL_10:
  CmpCleanupThreadInfo(&v25);
  return (unsigned int)v7;
}
