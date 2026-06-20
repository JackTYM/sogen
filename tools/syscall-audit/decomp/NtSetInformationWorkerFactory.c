NTSTATUS __fastcall NtSetInformationWorkerFactory(HANDLE Handle, int a2, unsigned __int64 a3, unsigned int a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  memset(&Size_4, 0, sizeof(Size_4));
  v37 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  memset_0(Src, 0, sizeof(Src));
  if ( a2 == 11 )
  {
LABEL_2:
    v9 = 4;
LABEL_3:
    Size = v9;
    if ( a4 != v9 )
    {
      return -1073741820;
    }
    if ( a2 == 9 )
    {
      if ( PreviousMode && (a3 & 3) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
LABEL_7:
      v10 = *(_DWORD *)a3;
    }
    else
    {
      switch ( a2 )
      {
        case 2:
          if ( PreviousMode && (a3 & 3) != 0 )
          {
            ExRaiseDatatypeMisalignment();
          }
          *(_QWORD *)&Src[0] = *(_QWORD *)a3;
          goto LABEL_20;
        case 3:
        case 4:
        case 5:
          if ( !PreviousMode )
          {
            goto LABEL_7;
          }
          v11 = 0x7FFFFFFF0000LL;
          if ( a3 < 0x7FFFFFFF0000LL )
          {
            v11 = a3;
          }
          v10 = *(_DWORD *)v11;
          break;
        default:
          __fastfail(0x25u);
      }
    }
    LODWORD(Src[0]) = v10;
LABEL_20:
    Object = 0LL;
    result = ObReferenceObjectByHandle(Handle, 4u, ExpWorkerFactoryObjectType, PreviousMode, &Object, 0LL);
    if ( result >= 0 )
    {
      if ( a2 != 8 )
      {
        Thread = 0;
        v14 = 0;
        v15 = (unsigned __int64 *)*((_QWORD *)Object + 2);
        Size_4.LockQueue.Lock = v15;
        Size_4.LockQueue.Next = 0LL;
        CurrentIrql = KeGetCurrentIrql();
        __writecr8(2uLL);
        if ( KiIrqlFlags )
        {
          KiRaiseIrqlProcessIrqlFlags(CurrentIrql, 2LL);
        }
        Size_4.OldIrql = CurrentIrql;
        if ( (BYTE6(PerfGlobalGroupMask) & 0x21) == 0 || PopHibernateInProgress )
        {
          if ( _InterlockedExchange64((volatile __int64 *)v15, (__int64)&Size_4) )
          {
            KxWaitForLockOwnerShip(&Size_4);
          }
        }
        else
        {
          KiAcquireQueuedSpinLockInstrumented(&Size_4, v15);
        }
        if ( a2 == 9 )
        {
          v17 = (char *)Object;
          switch ( LODWORD(Src[0]) )
          {
            case 2:
              v18 = *((_DWORD *)Object + 96);
              if ( v18 )
              {
                v19 = v18 - 1;
                *((_DWORD *)Object + 96) = v19;
                if ( !v19 )
                {
LABEL_31:
                  v14 = 1;
                  v20 = 1;
LABEL_35:
                  v21 = 0;
LABEL_36:
                  if ( v14 )
                  {
                    v25 = *((_DWORD *)v17 + 96);
                    if ( *(_BYTE *)(*((_QWORD *)v17 + 2) + 33LL) )
                    {
                      Thread = 128;
                    }
                    else
                    {
                      v26 = *((_DWORD *)v17 + 95);
                      if ( v25 >= v26 )
                      {
                        if ( !v21 )
                        {
                          Thread = -1073741527;
                        }
                      }
                      else
                      {
                        if ( *((_DWORD *)v17 + 100) || v25 + *((_DWORD *)v17 + 98) >= v26 )
                        {
                          goto LABEL_62;
                        }
                        if ( (*((_DWORD *)v17 + 102) & 0x200) != 0 )
                        {
                          ExpLeaveWorkerFactoryAwayMode(v17);
                        }
                        ++*((_DWORD *)v17 + 100);
                        KeReleaseInStackQueuedSpinLock(&Size_4);
                        v20 = 0;
                        Thread = ExpWorkerFactoryCreateThread(v17);
                        if ( Thread < 0 )
                        {
                          v20 = 1;
                          KeAcquireInStackQueuedSpinLock(*((PKSPIN_LOCK *)v17 + 2), &Size_4);
                          --*((_DWORD *)v17 + 100);
                          if ( v21 )
                          {
LABEL_62:
                            Thread = 0;
                          }
                        }
                      }
                    }
                  }
                  if ( v20 )
                  {
                    KeReleaseInStackQueuedSpinLock(&Size_4);
                  }
                  ObfDereferenceObjectWithTag(v17, 0x746C6644u);
                  return Thread;
                }
              }
              else
              {
                Thread = -1073741823;
              }
              break;
            case 0:
              v22 = *((_DWORD *)Object + 96);
              v20 = 1;
              if ( v22 == *((_DWORD *)Object + 97) )
              {
                Thread = -1073741823;
              }
              else
              {
                *((_DWORD *)Object + 96) = v22 + 1;
              }
              goto LABEL_35;
            case 1:
              goto LABEL_31;
            case 3:
              if ( *((_DWORD *)Object + 96) )
              {
                ExpRemoveCurrentThreadFromThreadHistory(Object);
                v33 = *((_DWORD *)v17 + 96) - 1;
                *((_DWORD *)v17 + 96) = v33;
                --*((_DWORD *)v17 + 97);
                v14 = v33 == 0;
              }
              else
              {
                Thread = -1073741823;
              }
              break;
            default:
LABEL_101:
              Thread = -1073741811;
              break;
          }
        }
        else
        {
          if ( a2 != 13 )
          {
            switch ( a2 )
            {
              case 2:
                v17 = (char *)Object;
                if ( *(_BYTE *)(*((_QWORD *)Object + 2) + 33LL) )
                {
                  goto LABEL_80;
                }
                v30 = *(_QWORD *)&Src[0];
                if ( *(__int64 *)&Src[0] >= 0 )
                {
                  goto LABEL_101;
                }
                if ( *(__int64 *)&Src[0] > -10000000 )
                {
                  v30 = -10000000LL;
                }
                if ( v30 < -6000000000LL )
                {
                  v30 = -6000000000LL;
                }
                *((_QWORD *)Object + 14) = v30;
                *((_QWORD *)&v37 + 1) = -1LL;
                KeSetTimer2(v17 + 424, v30, -v30, &v37);
                goto LABEL_34;
              case 3:
                v17 = (char *)Object;
                v23 = *((_DWORD *)Object + 101);
                if ( SLODWORD(Src[0]) >= 0 )
                {
                  v24 = v23 + LODWORD(Src[0]);
                  if ( v23 >= v23 + LODWORD(Src[0]) )
                  {
                    *((_DWORD *)Object + 101) = -1;
                    v24 = -1;
                  }
                  else
                  {
                    *((_DWORD *)Object + 101) = v24;
                  }
                }
                else if ( v23 > -LODWORD(Src[0]) )
                {
                  v24 = v23 + LODWORD(Src[0]);
                  *((_DWORD *)Object + 101) = v23 + LODWORD(Src[0]);
                }
                else
                {
                  *((_DWORD *)Object + 101) = 0;
                  v24 = 0;
                }
                if ( v23 )
                {
                  if ( !v24 && (*((_DWORD *)v17 + 102) & 0x200) != 0 )
                  {
                    ExpLeaveWorkerFactoryAwayMode(v17);
                  }
                }
                else if ( v24 && (unsigned __int8)ExpTryEnterWorkerFactoryAwayMode(v17) )
                {
                  ExpWorkerFactoryCheckCreate(v17, &Size_4);
                  v20 = 0;
                  goto LABEL_35;
                }
                goto LABEL_34;
              case 4:
                v17 = (char *)Object;
                if ( *(_BYTE *)(*((_QWORD *)Object + 2) + 33LL) )
                {
                  goto LABEL_80;
                }
                v29 = Src[0];
                v14 = LODWORD(Src[0]) > *((_DWORD *)Object + 94);
                *((_DWORD *)Object + 94) = Src[0];
                if ( *((_DWORD *)v17 + 95) < v29 )
                {
                  *((_DWORD *)v17 + 95) = v29;
                }
                if ( !v14 )
                {
                  v21 = 0;
                  goto LABEL_68;
                }
                v14 = 0;
                if ( (*((_DWORD *)v17 + 102) & 0x200) != 0 )
                {
                  ExpLeaveWorkerFactoryAwayMode(v17);
                }
                do
                {
                  if ( (unsigned int)(*((_DWORD *)v17 + 96) + *((_DWORD *)v17 + 98)) >= *((_DWORD *)v17 + 94) )
                  {
                    goto LABEL_34;
                  }
                  ++*((_DWORD *)v17 + 100);
                  KeReleaseInStackQueuedSpinLock(&Size_4);
                  Thread = ExpWorkerFactoryCreateThread(v17);
                  KeAcquireInStackQueuedSpinLock(*((PKSPIN_LOCK *)v17 + 2), &Size_4);
                }
                while ( Thread >= 0 );
                --*((_DWORD *)v17 + 100);
                goto LABEL_34;
              case 5:
                v17 = (char *)Object;
                v27 = *((_QWORD *)Object + 2);
                if ( *(_BYTE *)(v27 + 33) )
                {
LABEL_80:
                  Thread = 128;
                  goto LABEL_34;
                }
                v21 = 1;
                v28 = Src[0];
                if ( LODWORD(Src[0])
                  && (!*((_DWORD *)Object + 95) && *(_DWORD *)(v27 + 24) || *(int *)(*(_QWORD *)(v27 + 8) + 4LL) > 0) )
                {
                  v14 = 1;
                }
                *((_DWORD *)Object + 95) = Src[0];
                if ( v28 < *((_DWORD *)v17 + 94) )
                {
                  *((_DWORD *)v17 + 94) = v28;
                  v20 = 1;
                }
                else
                {
LABEL_68:
                  v20 = 1;
                }
                break;
              default:
                __fastfail(0x25u);
            }
            goto LABEL_36;
          }
          v17 = (char *)Object;
          *((_DWORD *)Object + 102) = *((_DWORD *)Object + 102) & 0xFFFFF7FF | ((Src[0] & 1) << 11);
        }
LABEL_34:
        v20 = 1;
        goto LABEL_35;
      }
      v31 = Object;
      v32 = Src[0];
      if ( !LODWORD(Src[0]) )
      {
        v32 = KeNumberProcessors_0;
        v31 = Object;
      }
      *(_DWORD *)(*(_QWORD *)(*((_QWORD *)Object + 2) + 8LL) + 44LL) = v32;
      ObfDereferenceObjectWithTag(v31, 0x746C6644u);
      return 0;
    }
  }
  else
  {
    switch ( a2 )
    {
      case 2:
        v9 = 8;
        goto LABEL_3;
      case 3:
      case 4:
      case 5:
      case 8:
      case 9:
      case 12:
      case 13:
      case 14:
        goto LABEL_2;
      case 6:
        result = -1073741822;
        break;
      case 10:
        v9 = 16;
        goto LABEL_3;
      case 15:
        if ( a4 >= 0x100 )
        {
          v9 = 256;
        }
        else
        {
          v9 = a4 + (a4 & 7);
        }
        goto LABEL_3;
      default:
        result = -1073741821;
        break;
    }
  }
  return result;
}
395 row(s)
