__int64 __fastcall NtAcquireCrossVmMutant(void *a1, unsigned __int64 a2)                                | 
{                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                         | 
                                                                                                        | 
  v14 = 0LL;                                                                                            | 
  if ( !ExCrossVmMutantObjectType )                                                                     | 
  {                                                                                                     | 
    return 3221225508LL;                                                                                | 
  }                                                                                                     | 
  PreviousMode = KeGetCurrentThread()->PreviousMode;                                                    | 
  v4 = (__int64 *)a2;                                                                                   | 
  if ( a2 && PreviousMode )                                                                             | 
  {                                                                                                     | 
    v5 = 0x7FFFFFFF0000LL;                                                                              | 
    if ( a2 < 0x7FFFFFFF0000LL )                                                                        | 
    {                                                                                                   | 
      v5 = a2;                                                                                          | 
    }                                                                                                   | 
    v14 = *(_QWORD *)v5;                                                                                | 
    v4 = &v14;                                                                                          | 
  }                                                                                                     | 
  Object = 0LL;                                                                                         | 
  v7 = ObReferenceObjectByHandle(a1, 0x100000u, ExCrossVmMutantObjectType, PreviousMode, &Object, 0LL); | 
  if ( v7 >= 0 )                                                                                        | 
  {                                                                                                     | 
    v12[0] = 0LL;                                                                                       | 
    ExtensionHostForCrossVmObject = ExpGetExtensionHostForCrossVmObject(v6, v12);                       | 
    if ( ExtensionHostForCrossVmObject )                                                                | 
    {                                                                                                   | 
      LOBYTE(v9) = PreviousMode;                                                                        | 
      v11 = v4;                                                                                         | 
      v10 = Object;                                                                                     | 
      v7 = guard_dispatch_icall_no_overrides(Object, v11, v9);                                          | 
    }                                                                                                   | 
    else                                                                                                | 
    {                                                                                                   | 
      v7 = -1073741822;                                                                                 | 
      v10 = Object;                                                                                     | 
    }                                                                                                   | 
    if ( ExtensionHostForCrossVmObject )                                                                | 
    {                                                                                                   | 
      ExReleaseExtensionTable(v12[0]);                                                                  | 
    }                                                                                                   | 
    ObfDereferenceObject(v10);                                                                          | 
  }                                                                                                     | 
  return (unsigned int)v7;                                                                              | 
}                                                                                                       | 
47 row(s)
