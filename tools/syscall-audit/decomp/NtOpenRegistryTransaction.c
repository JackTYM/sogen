line
__int64 __fastcall NtOpenRegistryTransaction(HANDLE *a1, int a2, int a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v13 = 0LL;
  v14 = 0LL;
  Handle = 0LL;
  CmpInitializeThreadInfo(&v13);
  v6 = CmpAcquireShutdownRundown();
  if ( v6 )
  {
    PreviousMode = KeGetCurrentThread()->PreviousMode;
    if ( PreviousMode == 1 )
    {
      v10 = 0x7FFFFFFF0000LL;
      if ( (unsigned __int64)a1 < 0x7FFFFFFF0000LL )
      {
        v10 = (__int64)a1;
      }
      *(_QWORD *)v10 = 0LL;
    }
    else
    {
      *a1 = 0LL;
    }
    LOBYTE(v7) = PreviousMode;
    v8 = ObOpenObjectByName(a3, (_DWORD)CmRegistryTransactionType, v7, 0, a2, 0LL, (__int64)&Handle);
    if ( v8 >= 0 )
    {
      *a1 = Handle;
      Handle = 0LL;
      v8 = 0;
    }
  }
  else
  {
    v8 = -1073741431;
  }
  if ( Handle )
  {
    NtClose(Handle);
  }
  if ( v6 )
  {
    CmpReleaseShutdownRundown();
  }
  CmpCleanupThreadInfo(&v13);
  return (unsigned int)v8;
}
