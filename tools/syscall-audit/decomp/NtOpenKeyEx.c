line
__int64 __fastcall NtOpenKeyEx(__int64 a1, __int64 a2, __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v13 = 0LL;
  HIDWORD(Parameter[0]) = 0;
  v12 = 0LL;
  CmpInitializeThreadInfo(&v12);
  Parameter[2] = v4;
  v16 = 0;
  v17 = 0LL;
  Parameter[0] = 0LL;
  v18 = 0LL;
  Parameter[1] = v5;
  Parameter[3] = a3;
  v15 = v6;
  LOBYTE(v18) = KeGetCurrentThread()->PreviousMode;
  v7 = KeExpandKernelStackAndCallout(CmOpenKeyCallout, Parameter, 0x4800uLL);
  v9 = (unsigned int)v7;
  if ( v7 >= 0 )
  {
    v9 = LODWORD(Parameter[0]);
  }
  CmpCleanupThreadInfo(&v12, v8, v9);
  return v10;
}
