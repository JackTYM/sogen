__int64 __fastcall NtRemoveIoCompletionEx(
        HANDLE Handle,
        unsigned __int64 a2,
        unsigned int a3,
        _DWORD *a4,
        unsigned __int64 a5,
        char a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v7 = a3;
  v10 = a5;
  memset(P, 0, sizeof(P));
  v25 = 0LL;
  v23 = 0;
  if ( a3 - 1 > 0x7FFFFFE )
  {
    return 3221225485LL;
  }
  v24 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    if ( 32LL * a3 )
    {
      v15 = a2;
      if ( (a2 & 7) != 0 )
      {
        ExRaiseDatatypeMisalignment();
      }
      v16 = 32LL * a3 + a2 - 1;
      if ( a2 > v16 || (v17 = 0x7FFFFFFF0000LL, v16 >= 0x7FFFFFFF0000LL) )
      {
        ExRaiseAccessViolation();
      }
      v18 = (v16 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      do
      {
        *(_BYTE *)v15 = *(_BYTE *)v15;
        v15 = (v15 & 0xFFFFFFFFFFFFF000uLL) + 4096;
      }
      while ( v15 != v18 );
    }
    else
    {
      v17 = 0x7FFFFFFF0000LL;
    }
    v19 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )
    {
      v19 = (__int64)a4;
    }
    *(_DWORD *)v19 = *(_DWORD *)v19;
    if ( a5 )
    {
      v24 = &v25;
      if ( a5 < 0x7FFFFFFF0000LL )
      {
        v17 = a5;
      }
      v25 = *(_QWORD *)v17;
    }
    v10 = (unsigned __int64)v24;
  }
  else if ( !a5 )
  {
    v10 = (unsigned __int64)v24;
  }
  if ( (unsigned int)v7 > 0x10 )
  {
    Pool2 = ExAllocatePool2(0x40uLL, 8 * v7, 0x6F436F49u);
    v12 = P;
    if ( Pool2 )
    {
      v12 = (_OWORD *)Pool2;
    }
    v21 = 16;
    if ( Pool2 )
    {
      v21 = v7;
    }
    LODWORD(v7) = v21;
  }
  else
  {
    v12 = P;
  }
  Object = 0LL;
  v13 = ObReferenceObjectByHandle(Handle, 2u, IoCompletionObjectType, PreviousMode, &Object, 0LL);
  if ( v13 >= 0 )
  {
    LOBYTE(HandleInformation) = PreviousMode;
    v13 = IoRemoveIoCompletion(Object, a2, v12, (unsigned int)v7, &v23, HandleInformation, v10, a6);
    ObfDereferenceObject(Object);
  }
  if ( v12 != P )
  {
    ExFreePoolWithTag(v12, 0);
  }
  if ( v13 >= 0 )
  {
    *a4 = v23;
  }
  return (unsigned int)v13;
}
105 row(s)
