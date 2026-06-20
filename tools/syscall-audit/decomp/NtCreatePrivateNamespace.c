__int64 __fastcall NtCreatePrivateNamespace(__int64 *a1, __int64 a2, int a3, void *a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  Object = 0LL;
  v34 = 0LL;
  P = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    v31 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )
    {
      v31 = (__int64)a1;
    }
    *(_QWORD *)v31 = *(_QWORD *)v31;
  }
  result = ObpCaptureBoundaryDescriptor(a4);
  if ( (int)result >= 0 )
  {
    v8 = P;
    v9 = ObpVerifyCreatorAccessCheck((char *)P + 48);
    if ( v9 >= 0 )
    {
      v11 = (unsigned int)(*((_DWORD *)P + 6) + 392);
      if ( v11 < *((_QWORD *)P + 3) )
      {
        v9 = -1073741811;
      }
      else
      {
        p_Object = &Object;
        LOBYTE(v10) = PreviousMode;
        LOBYTE(p_Object) = PreviousMode;
        v13 = ObCreateObjectEx((_DWORD)p_Object, (_DWORD)ObpDirectoryObjectType, a3, v10);
        if ( v13 >= 0 )
        {
          v14 = Object;
          memset_0(Object, 0, (unsigned int)v11);
          v15 = ((unsigned __int64)v14 + 351) & 0xFFFFFFFFFFFFFFF8uLL;
          *(_QWORD *)(v15 + 8) = v15;
          *(_QWORD *)v15 = v15;
          v16 = v8[3];
          *(_QWORD *)(v15 + 24) = v16;
          *(_QWORD *)(v15 + 16) = 0LL;
          *(_BYTE *)(v15 + 40) = *((_BYTE *)v8 + 40);
          memmove((void *)(v15 + 48), v8 + 6, v16);
          ExFreePoolWithTag(v8, 0x534E624Fu);
          v14[37] = 0LL;
          *((_DWORD *)v14 + 85) = -1;
          *((_DWORD *)v14 + 84) = 1;
          if ( (*((_BYTE *)v14 - 22) & 2) != 0 )
          {
            v17 = (__int64)v14 - ObpInfoMaskToOffset[*((_BYTE *)v14 - 22) & 3] - 48;
          }
          else
          {
            v17 = 0LL;
          }
          if ( v17 )
          {
            v18 = -1073741773;
          }
          else
          {
            v18 = ObpRegisterPrivateNamespace(((unsigned __int64)v14 + 351) & 0xFFFFFFFFFFFFFFF8uLL);
            if ( v18 >= 0 )
            {
              PsReferenceSiloContext(v14);
              inserted = ObInsertObjectEx(v14, 0, 0LL, (__int64)&v34);
              CurrentServerSiloGlobals = PsGetCurrentServerSiloGlobals();
              CurrentThread = KeGetCurrentThread();
              --CurrentThread->KernelApcDisable;
              v22 = (signed __int64 *)(CurrentServerSiloGlobals + 720);
              v23 = KeAbPreAcquire(CurrentServerSiloGlobals + 720, 0LL, 0LL);
              v24 = v23;
              if ( _interlockedbittestandset64((volatile signed __int32 *)(CurrentServerSiloGlobals + 720), 0LL) )
              {
                ExfAcquirePushLockExclusiveEx(v22, v23, v22);
              }
              if ( v24 )
              {
                *(_BYTE *)(v24 + 10) = 1;
              }
              if ( inserted < 0 || (v14[42] & 2) != 0 )
              {
                v25 = *(_QWORD *)v15;
                v26 = *(_QWORD **)(v15 + 8);
                if ( *(_QWORD *)(*(_QWORD *)v15 + 8LL) != v15 || *v26 != v15 )
                {
                  __fastfail(3u);
                }
                *v26 = v25;
                *(_QWORD *)(v25 + 8) = v26;
                --*(_DWORD *)(CurrentServerSiloGlobals + 728);
                ObfDereferenceObject(Object);
              }
              else
              {
                v27 = Object;
                *(_QWORD *)(v15 + 16) = Object;
                v27[40] = v15;
              }
              _m_prefetchw(v22);
              v28 = *v22;
              v29 = *v22 - 16;
              if ( (*v22 & 0xFFFFFFFFFFFFFFF0uLL) <= 0x10 )
              {
                v29 = 0LL;
              }
              if ( (v28 & 2) != 0 || (v30 = *v22, v30 != _InterlockedCompareExchange64(v22, v29, v28)) )
              {
                ExfReleasePushLock(v22, v29);
              }
              KeAbPostRelease((ULONG_PTR)v22);
              KeLeaveCriticalRegion();
              if ( inserted >= 0 )
              {
                *a1 = v34;
              }
              return (unsigned int)inserted;
            }
          }
          ObfDereferenceObject(v14);
          return (unsigned int)v18;
        }
        v9 = v13;
      }
    }
    ExFreePoolWithTag(v8, 0x534E624Fu);
    return (unsigned int)v9;
  }
  return result;
}
134 row(s)
