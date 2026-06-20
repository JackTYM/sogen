__int64 __fastcall NtQuerySecurityAttributesToken(
        int a1,
        __int64 a2,
        unsigned int a3,
        void *a4,
        size_t a5,
        unsigned __int64 a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  P = 0LL;
  v9 = 0;
  Object = 0LL;
  v29 = 0LL;
  v25[0] = 0;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  v25[1] = PreviousMode;
  if ( (_DWORD)a5 )
  {
    if ( a4 )
    {
      goto LABEL_3;
    }
LABEL_32:
    SecurityAttributesToken = -1073741811;
    v26 = -1073741811;
LABEL_33:
    v21 = (PERESOURCE *)Object;
    goto LABEL_23;
  }
  if ( a4 )
  {
    goto LABEL_32;
  }
LABEL_3:
  if ( !PreviousMode )
  {
    v14 = a6;
    goto LABEL_20;
  }
  if ( (_DWORD)a5 )
  {
    v11 = (unsigned __int64)a4;
    if ( ((unsigned __int8)a4 & 3) != 0 )
    {
      goto LABEL_18;
    }
    v12 = (unsigned __int64)a4 + (unsigned int)a5 - 1;
    if ( (unsigned __int64)a4 > v12 || v12 >= 0x7FFFFFFF0000LL )
    {
LABEL_16:
      ExRaiseAccessViolation();
    }
    v13 = (v12 & 0xFFFFFFFFFFFFF000uLL) + 4096;
    do
    {
      *(_BYTE *)v11 = *(_BYTE *)v11;
      v11 = (v11 & 0xFFFFFFFFFFFFF000uLL) + 4096;
    }
    while ( v11 != v13 );
  }
  v14 = a6;
  v15 = a6;
  if ( (a6 & 3) != 0 )
  {
LABEL_18:
    ExRaiseDatatypeMisalignment();
  }
  v16 = a6 + 3;
  if ( a6 >= a6 + 3 || v16 >= 0x7FFFFFFF0000LL )
  {
    goto LABEL_16;
  }
  v17 = (v16 & 0xFFFFFFFFFFFFF000uLL) + 4096;
  do
  {
    *(_BYTE *)v15 = *(_BYTE *)v15;
    v15 = (v15 & 0xFFFFFFFFFFFFF000uLL) + 4096;
  }
  while ( v15 != v17 );
LABEL_20:
  SecurityAttributesToken = SepCaptureUnicodeStringArray(a2, a3, PreviousMode, &P);
  v26 = SecurityAttributesToken;
  if ( SecurityAttributesToken < 0 )
  {
    goto LABEL_33;
  }
  SecurityAttributesToken = SepReferenceTokenByHandle(a1, 8, PreviousMode, v19, &Object, (__int64)v25, (__int64)&v29);
  v26 = SecurityAttributesToken;
  if ( SecurityAttributesToken < 0 )
  {
    goto LABEL_33;
  }
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v21 = (PERESOURCE *)Object;
  ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
  v9 = 1;
  LODWORD(Size) = a5;
  SecurityAttributesToken = SepInternalQuerySecurityAttributesTokenEx((int)v21, v22, (int)P, a3, 0, a4, Size, v14);
  v26 = SecurityAttributesToken;
LABEL_23:
  if ( PreviousMode == 1 && P )
  {
    ExFreePoolWithTag(P, 0);
  }
  if ( v9 )
  {
    ExReleaseResourceLite(v21[6]);
    KeLeaveCriticalRegionThread(KeGetCurrentThread());
    SecurityAttributesToken = v26;
    v21 = (PERESOURCE *)Object;
  }
  if ( v21 )
  {
    ObfDereferenceObjectWithTag(v21, 0x74726853u);
  }
  return (unsigned int)SecurityAttributesToken;
}
