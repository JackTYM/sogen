__int64 __fastcall NtAssociateWaitCompletionPacket(
        void *a1,
        void *a2,
        void *a3,
        __int64 a4,
        __int64 a5,
        int a6,
        __int64 a7,
        _BYTE *a8)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v8 = 0;
  v9 = 0;
  v10 = 0LL;
  v11 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v12 = ObReferenceObjectByHandle(a1, 1u, IopWaitCompletionPacketObjectType, PreviousMode, &Object, 0LL);
  v13 = (KSPIN_LOCK *)Object;
  if ( v12 >= 0 )
  {
    v25 = 0LL;
    v12 = ObReferenceObjectByHandle(a2, 2u, IoCompletionObjectType, PreviousMode, &v25, 0LL);
    v10 = v25;
    if ( v12 >= 0 )
    {
      v24 = 0LL;
      v12 = ObReferenceObjectByHandle(a3, 0x100000u, 0LL, PreviousMode, &v24, 0LL);
      v11 = v24;
      v23 = v12;
      if ( v12 >= 0 )
      {
        AssociatedWaitObject = (_BYTE *)ObGetAssociatedWaitObject(v24);
        v26 = AssociatedWaitObject;
        if ( AssociatedWaitObject && (((*AssociatedWaitObject & 0x7F) - 2) & 0xFD) != 0 )
        {
          v9 = 1;
          SpinLock = v13 + 12;
          v8 = KeAcquireSpinLockRaiseToDpc(v13 + 12);
          if ( *((_BYTE *)v13 + 104) )
          {
            v12 = -1073741585;
          }
          else
          {
            IsEnabledDeviceUsageNoInline = Feature_1806129466__private_IsEnabledDeviceUsageNoInline();
            v16 = Object;
            if ( IsEnabledDeviceUsageNoInline && *((_BYTE *)Object + 105) )
            {
              v12 = -1073700861;
            }
            else
            {
              *((_BYTE *)Object + 104) = 1;
              v16[6] = a4;
              v16[7] = a5;
              *((_DWORD *)v16 + 18) = a6;
              v16[8] = a7;
              v16[10] = v24;
              v16[11] = v25;
              v17 = KeRegisterObjectNotification(v26);
              KeReleaseSpinLock(SpinLock, v8);
              v9 = 0;
              if ( a8 )
              {
                if ( PreviousMode )
                {
                  v18 = 0x7FFFFFFF0000LL;
                  if ( (unsigned __int64)a8 < 0x7FFFFFFF0000LL )
                  {
                    v18 = (__int64)a8;
                  }
                  *(_BYTE *)v18 = *(_BYTE *)v18;
                }
                *a8 = v17;
              }
              v10 = 0LL;
              v11 = 0LL;
              v13 = 0LL;
            }
          }
        }
        else
        {
          v12 = -1073741583;
        }
      }
    }
  }
  if ( v9 )
  {
    v19 = (volatile signed __int64 *)(v13 + 12);
    if ( (BYTE6(PerfGlobalGroupMask) & 1) == 0 || PopHibernateInProgress )
    {
      _InterlockedAnd64(v19, 0LL);
    }
    else
    {
      KiReleaseSpinLockInstrumented(v19, retaddr);
    }
    if ( KiIrqlFlags )
    {
      KiLowerIrqlProcessIrqlFlags(KeGetCurrentIrql(), v8);
    }
    __writecr8(v8);
  }
  if ( v10 )
  {
    ObfDereferenceObjectWithTag(v10, 0x746C6644u);
  }
  if ( v11 )
  {
    ObfDereferenceObjectWithTag(v11, 0x746C6644u);
  }
  if ( v13 )
  {
    ObfDereferenceObjectWithTag(v13, 0x746C6644u);
  }
  return (unsigned int)v12;
}
