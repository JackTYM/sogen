__int64 __fastcall NtAlpcImpersonateClientContainerOfPort(HANDLE Handle, __int64 a2, int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v21 = 0;
  v16 = 0;
  BugCheckParameter2 = 0LL;
  v22 = 0;
  v20[0] = 0LL;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v5 = 0LL;
  v19 = 0LL;
  if ( a3 )
  {
    v6 = -1073741811;
  }
  else
  {
    AlpcpCaptureIdMessage(a2, &v22, &v21);
    PreviousMode = KeGetCurrentThread()->PreviousMode;
    Object = 0LL;
    v6 = ObReferenceObjectByHandle(Handle, 0x20000u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
    v5 = Object;
    v19 = Object;
    if ( v6 >= 0 )
    {
      if ( (*((_BYTE *)Object + 416) & 6) == 6
        && KeGetCurrentThread()->ApcState.Process == (_KPROCESS *)*((_QWORD *)Object + 3) )
      {
        v6 = AlpcpLookupMessage((_DWORD)Object, v22, v21, v8, (__int64)&BugCheckParameter2);
        v21 = v6;
        if ( v6 >= 0 )
        {
          if ( (*(_DWORD *)(BugCheckParameter2 + 40) & 0x80u) == 0 )
          {
            v9 = *(struct _KTHREAD **)(BugCheckParameter2 + 32);
            if ( v9 )
            {
              WorkOnBehalfThread = (struct _KTHREAD *)PsGetWorkOnBehalfThread(
                                                        *(_QWORD *)(BugCheckParameter2 + 32),
                                                        &v16);
              v11 = WorkOnBehalfThread;
              if ( WorkOnBehalfThread )
              {
                PsImpersonateContainerOfThread(WorkOnBehalfThread);
                if ( v16 )
                {
                  ObDereferenceObjectDeferDelete(v11);
                }
              }
              else if ( IoThreadToProcess(v9)[3].UserWaitTime || (unsigned __int8)PoEnergyEstimationEnabled() )
              {
                v11 = v9;
                PsImpersonateContainerOfThread(v9);
              }
              else
              {
                v11 = 0LL;
              }
              if ( v11 )
              {
                v12 = KeGetCurrentThread();
                if ( (v12->MiscFlags & 0x400) != 0 || v12->ApcStateIndex == 1 )
                {
                  Teb = 0LL;
                }
                else
                {
                  Teb = v12->Teb;
                }
                if ( Teb )
                {
                  PsEncodeThreadWorkOnBehalfTicket(v11, v20);
                  *(_QWORD *)(v14 + 696) = v20[0];
                }
              }
            }
            AlpcpUnlockMessage(BugCheckParameter2);
          }
          else
          {
            AlpcpUnlockMessage(BugCheckParameter2);
            v6 = -1073740029;
          }
        }
      }
      else
      {
        v6 = -1073741790;
      }
    }
  }
  if ( v5 )
  {
    ObfDereferenceObject(v5);
  }
  KeLeaveCriticalRegion();
  return (unsigned int)v6;
}
100 row(s)
