line
__int64 __fastcall NtOpenKeyTransactedEx(int a1, int a2, int a3, int a4, HANDLE Handle)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v17 = 0LL;
  v18 = 0LL;
  CmpInitializeThreadInfo(&v17);
  if ( !(unsigned __int8)CmpAcquireShutdownRundown() )
  {
    v12 = -1073741431;
    goto LABEL_8;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v10 = ObReferenceObjectByHandle(Handle, 4u, CmRegistryTransactionType, PreviousMode, &Object, 0LL);
  v11 = (__int64)Object;
  v12 = v10;
  if ( v10 != -1073741788 )
  {
    if ( v10 < 0 )
    {
      goto LABEL_5;
    }
    v11 = (unsigned __int64)Object | 1;
    goto LABEL_4;
  }
  v13 = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v14 = ObReferenceObjectByHandle(Handle, 4u, (POBJECT_TYPE)TmTransactionObjectType, v13, &Object, 0LL);
  v11 = (__int64)Object;
  v12 = v14;
  if ( v14 >= 0 )
  {
LABEL_4:
    v12 = CmOpenKey(a1, a2, a3, a4, v11, KeGetCurrentThread()->PreviousMode);
  }
LABEL_5:
  if ( v11 )
  {
    CmpTransDereferenceTransaction(v11);
  }
  CmpReleaseShutdownRundown();
LABEL_8:
  CmpCleanupThreadInfo(&v17);
  return v12;
}
