line
__int64 __fastcall NtCreateKeyTransacted(
        __int64 a1,
        int a2,
        __int64 a3,
        int a4,
        __int64 a5,
        int a6,
        HANDLE Handle,
        __int64 a8)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v21 = 0LL;
  Parameter[1] = 0;
  v20 = 0LL;
  memset_0(Parameter, 0, 0x44uLL);
  CmpInitializeThreadInfo(&v20);
  if ( !(unsigned __int8)CmpAcquireShutdownRundown() )
  {
    v12 = -1073741431;
    goto LABEL_13;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  Object = 0LL;
  v14 = ObReferenceObjectByHandle(Handle, 4u, CmRegistryTransactionType, PreviousMode, &Object, 0LL);
  v15 = (unsigned __int64)Object;
  v12 = v14;
  if ( v14 == -1073741788 )
  {
    v16 = KeGetCurrentThread()->PreviousMode;
    Object = 0LL;
    v17 = ObReferenceObjectByHandle(Handle, 4u, (POBJECT_TYPE)TmTransactionObjectType, v16, &Object, 0LL);
    v15 = (unsigned __int64)Object;
    v12 = v17;
    if ( v17 >= 0 )
    {
LABEL_8:
      v25 = 0;
      v28 = 0;
      v31 = 0;
      v29 = a5;
      v30 = a6;
      v32 = a8;
      v23 = a1;
      v24 = a2;
      v26 = a3;
      v27 = a4;
      v33 = v15;
      v12 = KeExpandKernelStackAndCallout(CmCreateKeyCallout, Parameter, 0x4800uLL);
      if ( v12 >= 0 )
      {
        v12 = Parameter[0];
      }
    }
  }
  else if ( v14 >= 0 )
  {
    v15 = (unsigned __int64)Object | 1;
    goto LABEL_8;
  }
  if ( v15 )
  {
    CmpTransDereferenceTransaction(v15);
  }
  CmpReleaseShutdownRundown();
LABEL_13:
  CmpCleanupThreadInfo(&v20);
  return (unsigned int)v12;
}
