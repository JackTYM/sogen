__int64 __fastcall NtAlpcQueryInformationMessage(
        HANDLE Handle,
        __int64 a2,
        int a3,
        unsigned __int64 a4,
        unsigned int a5,
        unsigned __int64 a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  BugCheckParameter2 = 0LL;
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v31 = KeGetCurrentThread();
  v11 = v31->PreviousMode;
  if ( v11 )
  {
    v12 = a2 & 3;
    if ( (a2 & 3) != 0 )
    {
      goto LABEL_3;
    }
    v19 = 0x7FFFFFFF0000LL;
  }
  else
  {
    v12 = a2 & 3;
    v19 = 0x7FFFFFFF0000LL;
  }
  if ( (*(_WORD *)(a2 + 4) & 0x1000) != 0 )
  {
    v13 = *(_DWORD *)(a2 + 16);
    v27 = v13;
    v14 = *(_DWORD *)(a2 + 20);
  }
  else
  {
    if ( v11 && v12 )
    {
      goto LABEL_3;
    }
    v13 = *(_DWORD *)(a2 + 24);
    v27 = v13;
    v14 = *(_DWORD *)(a2 + 32);
  }
  v28 = v14;
  if ( !PreviousMode )
  {
    v18 = a6;
    goto LABEL_27;
  }
  if ( a5 )
  {
    v15 = a4;
    if ( (a4 & 3) == 0 )
    {
      v16 = a4 + a5 - 1LL;
      if ( a4 > v16 || v16 >= 0x7FFFFFFF0000LL )
      {
        ExRaiseAccessViolation();
      }
      v17 = (v16 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      do
      {
        *(_BYTE *)v15 = *(_BYTE *)v15;
        v15 = (v15 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      }
      while ( v15 != v17 );
      v13 = v27;
      goto LABEL_13;
    }
LABEL_3:
    ExRaiseDatatypeMisalignment();
  }
LABEL_13:
  v18 = a6;
  if ( a6 )
  {
    if ( a6 < 0x7FFFFFFF0000LL )
    {
      v19 = a6;
    }
    *(_DWORD *)v19 = *(_DWORD *)v19;
    v13 = v27;
  }
LABEL_27:
  if ( !v13 )
  {
    v20 = -1073741811;
    goto LABEL_39;
  }
  Object = 0LL;
  v20 = ObReferenceObjectByHandle(Handle, 0x20000u, AlpcPortObjectType, PreviousMode, &Object, 0LL);
  if ( v20 >= 0 )
  {
    v20 = AlpcpLookupMessage((_DWORD)Object, v13, v28, v21, (__int64)&BugCheckParameter2);
    if ( v20 < 0 )
    {
LABEL_38:
      ObfDereferenceObject(Object);
      goto LABEL_39;
    }
    v22 = BugCheckParameter2;
    if ( !*(_QWORD *)(BugCheckParameter2 + 24) )
    {
      v20 = -1073740029;
      goto LABEL_37;
    }
    if ( a3 )
    {
      v23 = a3 - 1;
      if ( !v23 )
      {
        TokenModifiedIdMessage = AlpcpQueryTokenModifiedIdMessage((_DWORD)Object, BugCheckParameter2, a4, a5, v18);
        goto LABEL_36;
      }
      v24 = v23 - 1;
      if ( v24 )
      {
        if ( v24 == 1 )
        {
          TokenModifiedIdMessage = AlpcpQueryHandleInformationMessage((_DWORD)Object, BugCheckParameter2, a4, a5, v18);
LABEL_36:
          v20 = TokenModifiedIdMessage;
LABEL_37:
          AlpcpUnlockMessage(v22);
          goto LABEL_38;
        }
      }
      else if ( !a4 && !a5 && !v18 )
      {
        v20 = (*(_DWORD *)(BugCheckParameter2 + 40) & 7) != 4 ? 0x103 : 0;
        goto LABEL_37;
      }
      v20 = -1073741811;
      goto LABEL_37;
    }
    TokenModifiedIdMessage = AlpcpQuerySidMessage((_DWORD)Object, BugCheckParameter2, a4, a5, v18);
    goto LABEL_36;
  }
LABEL_39:
  KeLeaveCriticalRegionThread(KeGetCurrentThread());
  return (unsigned int)v20;
}
145 row(s)
