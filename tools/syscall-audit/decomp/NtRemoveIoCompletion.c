NTSTATUS __fastcall NtRemoveIoCompletion(HANDLE Handle, _QWORD *a2, _QWORD *a3, _OWORD *a4, unsigned __int64 a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v23 = 0LL;
  v21 = 0LL;
  v22 = 0LL;
  v17 = 0;
  v19 = 0LL;
  v18 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    v9 = 0x7FFFFFFF0000LL;
    v10 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )
    {
      v10 = (__int64)a3;
    }
    *(_QWORD *)v10 = *(_QWORD *)v10;
    v11 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
    {
      v11 = (__int64)a2;
    }
    *(_QWORD *)v11 = *(_QWORD *)v11;
    v12 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )
    {
      v12 = (__int64)a4;
    }
    *(_DWORD *)v12 = *(_DWORD *)v12;
    if ( a5 )
    {
      v18 = &v19;
      if ( a5 < 0x7FFFFFFF0000LL )
      {
        v9 = a5;
      }
      v19 = *(_QWORD *)v9;
    }
    v13 = (unsigned __int64)v18;
  }
  else
  {
    v13 = (unsigned __int64)v18;
    if ( a5 )
    {
      v13 = a5;
    }
  }
  Object = 0LL;
  result = ObReferenceObjectByHandle(Handle, 2u, IoCompletionObjectType, PreviousMode, &Object, 0LL);
  if ( result >= 0 )
  {
    LOBYTE(HandleInformation) = PreviousMode;
    v15 = IoRemoveIoCompletion(Object, &v21, &v23, 1LL, &v17, HandleInformation, v13, 0);
    ObfDereferenceObject(Object);
    if ( !v15 )
    {
      *a2 = v21;
      *a3 = *((_QWORD *)&v21 + 1);
      *a4 = v22;
    }
    return v15;
  }
  return result;
}
68 row(s)
