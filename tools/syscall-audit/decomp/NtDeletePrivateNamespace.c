NTSTATUS __fastcall NtDeletePrivateNamespace(void *a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v5 = 0LL;
  Object = 0LL;
  result = ObReferenceObjectByHandle(
             a1,
             0x10000u,
             ObpDirectoryObjectType,
             KeGetCurrentThread()->PreviousMode,
             &Object,
             &v5);
  if ( result >= 0 )
  {
    v2 = Object;
    if ( *((_QWORD *)Object + 40) )
    {
      v3 = ObpVerifyCreatorAccessCheck((char *)Object + 392);
      if ( v3 >= 0 )
      {
        v3 = ObpRemoveNamespaceFromTable(v2);
      }
    }
    else
    {
      v3 = -1073741816;
    }
    ObfDereferenceObject(v2);
    return v3;
  }
  return result;
}
33 row(s)
