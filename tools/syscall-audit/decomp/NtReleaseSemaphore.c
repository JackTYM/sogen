__int64 __fastcall NtReleaseSemaphore(HANDLE Handle, int a2, _DWORD *a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( a3 && PreviousMode )
  {
    v9 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )
    {
      v9 = (__int64)a3;
    }
    *(_DWORD *)v9 = *(_DWORD *)v9;
  }
  if ( a2 <= 0 )
  {
    return 3221225485LL;
  }
  SystemArgument1 = 0LL;
  v6 = ObReferenceObjectByHandle(Handle, 2u, (POBJECT_TYPE)ExSemaphoreObjectType, PreviousMode, &SystemArgument1, 0LL);
  if ( v6 >= 0 )
  {
    LODWORD(v10) = 0;
    v7 = SystemArgument1;
    v6 = KeReleaseSemaphoreEx(SystemArgument1, 0, (__int64)&v10);
    LODWORD(SystemArgument1) = v6;
    ObfDereferenceObject(v7);
    if ( v6 >= 0 )
    {
      if ( a3 )
      {
        *a3 = v10;
      }
    }
  }
  return (unsigned int)v6;
}
