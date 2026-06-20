__int64 __fastcall NtSecureConnectPort(
        HANDLE *a1,
        unsigned __int64 a2,
        unsigned __int64 a3,
        unsigned __int64 a4,
        PVOID Src,
        unsigned __int64 a6,
        _DWORD *a7,
        volatile void *a8,
        unsigned int *a9)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v39 = a2;
  v41 = a1;
  v42 = a7;
  v43 = a9;
  Address = a8;
  Object = 0LL;
  memset_0(v51, 0, 0x40uLL);
  Handle = 0LL;
  P = 0LL;
  v40 = 0LL;
  memset(v49, 0, sizeof(v49));
  v50 = 0LL;
  v32 = 0;
  v53 = 0LL;
  v54 = 0;
  BugCheckParameter2 = 0LL;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v47 = 0LL;
  v48 = 0LL;
  v44 = 0LL;
  v45 = 0LL;
  v46 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( !PreviousMode )
  {
    if ( a9 )
    {
      v32 = *a9;
    }
    if ( a4 )
    {
      v44 = *(_OWORD *)a4;
      v45 = *(_OWORD *)(a4 + 16);
      v46 = *(_OWORD *)(a4 + 32);
    }
    if ( a3 )
    {
      v53 = *(_QWORD *)a3;
      v54 = *(_DWORD *)(a3 + 8);
    }
    P = Src;
    v20 = 0;
LABEL_39:
    v30 = (unsigned __int64)&v53 & -(__int64)(a3 != 0);
    v22 = P;
    ClientPort = AlpcpCreateClientPort(
                   (unsigned int)&Handle,
                   (unsigned int)&Object,
                   0,
                   v39,
                   0LL,
                   0LL,
                   0LL,
                   0LL,
                   (__int64)P,
                   v30,
                   1);
    if ( v22 != Src && (unsigned __int8)v20 <= 1u )
    {
      ExFreePoolWithTag(v22, 0);
    }
    if ( ClientPort >= 0 )
    {
      LOWORD(v49[0]) = v32;
      v39 = (unsigned __int64)&v44 & -(__int64)(a4 != 0);
      v23 = Object;
      ClientPort = AlpcpFormatConnectionRequest(
                     (unsigned int)&v40,
                     0,
                     (_DWORD)Object,
                     (_DWORD)Address,
                     (__int64)v49,
                     0LL,
                     v39,
                     (__int64)&BugCheckParameter2,
                     1,
                     v20);
      if ( ClientPort >= 0 )
      {
        v24 = v40;
        v25 = *(_DWORD *)(v40 + 264);
        if ( AlpcpLogEnabled )
        {
          AlpcpLogConnectRequest(v40);
        }
        v51[0] = v23;
        v51[1] = v24;
        v52 = 0x20000;
        v26 = AlpcpDispatchConnectionRequest(v51);
        ClientPort = v26;
        if ( v26 < 0 )
        {
          if ( AlpcpLogEnabled )
          {
            AlpcpLogConnectFail(v25, (unsigned int)v26);
          }
          AlpcpUnlockMessage(v24);
        }
        else
        {
          v27 = AlpcpReceiveLegacyConnectionReply(
                  (unsigned int)v51,
                  (_DWORD)Address,
                  (unsigned int)&v32,
                  BugCheckParameter2,
                  v39,
                  (unsigned __int64)&v47 & -(__int64)(a6 != 0));
          ClientPort = v27;
          if ( v27 )
          {
            if ( AlpcpLogEnabled )
            {
              AlpcpLogConnectFail(v25, v27);
            }
          }
          else
          {
            if ( AlpcpLogEnabled )
            {
              AlpcpLogConnectSuccess(v25);
            }
            *v41 = Handle;
            if ( a4 )
            {
              *(_OWORD *)a4 = v44;
              *(_OWORD *)(a4 + 16) = v45;
              *(_OWORD *)(a4 + 32) = v46;
            }
            if ( a6 )
            {
              *(_OWORD *)a6 = v47;
              *(_QWORD *)(a6 + 16) = v48;
            }
            if ( v43 )
            {
              *v43 = v32;
            }
            if ( v42 )
            {
              *v42 = v23[68];
            }
          }
        }
      }
      ObfDereferenceObject(v23);
      if ( BugCheckParameter2 )
      {
        AlpcpDereferenceBlobEx(BugCheckParameter2);
      }
      if ( ClientPort )
      {
        NtClose(Handle);
      }
    }
    goto LABEL_60;
  }
  v12 = 0x7FFFFFFF0000LL;
  v13 = 0x7FFFFFFF0000LL;
  if ( (unsigned __int64)v41 < 0x7FFFFFFF0000LL )
  {
    v13 = (__int64)v41;
  }
  *(_QWORD *)v13 = *(_QWORD *)v13;
  if ( a9 )
  {
    v14 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a9 < 0x7FFFFFFF0000LL )
    {
      v14 = (__int64)a9;
    }
    v32 = *(_DWORD *)v14;
    ProbeForWrite(Address, v32, 1u);
  }
  if ( a4 )
  {
    v15 = 0x7FFFFFFF0000LL;
    if ( a4 < 0x7FFFFFFF0000LL )
    {
      v15 = a4;
    }
    RtlCopyVolatileMemory(&v44, (const void *)v15, 0x30uLL);
    if ( (_DWORD)v44 != 48 )
    {
      ClientPort = -1073741811;
      v33 = -1073741811;
      goto LABEL_60;
    }
    if ( (a4 & 3) != 0 )
    {
      goto LABEL_13;
    }
    v16 = 0x7FFFFFFF0000LL;
    if ( a4 < 0x7FFFFFFF0000LL )
    {
      v16 = a4;
    }
    *(_BYTE *)v16 = *(_BYTE *)v16;
    *(_BYTE *)(v16 + 47) = *(_BYTE *)(v16 + 47);
  }
  if ( a6 )
  {
    v17 = 0x7FFFFFFF0000LL;
    if ( a6 < 0x7FFFFFFF0000LL )
    {
      v17 = a6;
    }
    if ( *(_DWORD *)v17 != 24 )
    {
      ClientPort = -1073741811;
      v33 = -1073741811;
      goto LABEL_60;
    }
    if ( (a6 & 3) != 0 )
    {
LABEL_13:
      ExRaiseDatatypeMisalignment();
    }
    v18 = 0x7FFFFFFF0000LL;
    if ( a6 < 0x7FFFFFFF0000LL )
    {
      v18 = a6;
    }
    *(_BYTE *)v18 = *(_BYTE *)v18;
    *(_BYTE *)(v18 + 23) = *(_BYTE *)(v18 + 23);
  }
  if ( v42 )
  {
    v19 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)v42 < 0x7FFFFFFF0000LL )
    {
      v19 = (__int64)v42;
    }
    *(_DWORD *)v19 = *(_DWORD *)v19;
  }
  if ( a3 )
  {
    if ( a3 < 0x7FFFFFFF0000LL )
    {
      v12 = a3;
    }
    RtlCopyVolatileMemory(&v53, (const void *)v12, 0xCuLL);
  }
  P = Src;
  v20 = PreviousMode;
  if ( !Src )
  {
    goto LABEL_39;
  }
  ClientPort = SeCaptureSid(Src, v29, 1, (__int64)&P);
  v33 = ClientPort;
  if ( ClientPort >= 0 )
  {
    goto LABEL_39;
  }
LABEL_60:
  KeLeaveCriticalRegion();
  return (unsigned int)ClientPort;
}
272 row(s)
