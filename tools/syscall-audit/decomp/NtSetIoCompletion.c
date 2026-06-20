NTSTATUS __fastcall NtSetIoCompletion(void *a1, int a2, int a3, int a4, __int64 a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  Object = 0LL;
  result = ObReferenceObjectByHandle(a1, 2u, IoCompletionObjectType, KeGetCurrentThread()->PreviousMode, &Object, 0LL);
  if ( result >= 0 )
  {
    v9 = IoSetIoCompletionEx((_DWORD)Object, a2, a3, a4, a5, 1, 0LL);
    ObfDereferenceObject(Object);
    return v9;
  }
  return result;
}
14 row(s)
