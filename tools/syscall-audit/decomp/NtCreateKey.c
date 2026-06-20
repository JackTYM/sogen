line
__int64 __fastcall NtCreateKey(__int64 a1, __int64 a2, __int64 a3, __int64 a4, __int64 a5, int a6, __int64 a7)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v20 = 0;
  v23 = 0;
  v26 = 0;
  v16 = 0LL;
  v17 = 0LL;
  HIDWORD(Parameter[0]) = 0;
  CmpInitializeThreadInfo(&v16);
  Parameter[0] = 0LL;
  v20 = 0;
  v23 = 0;
  v26 = 0;
  v28 = 0LL;
  v24 = a5;
  v25 = a6;
  v27 = a7;
  Parameter[1] = v8;
  v19 = v9;
  v21 = a3;
  v22 = v10;
  v11 = KeExpandKernelStackAndCallout(CmCreateKeyCallout, Parameter, 0x4800uLL);
  v13 = (unsigned int)v11;
  if ( v11 >= 0 )
  {
    v13 = LODWORD(Parameter[0]);
  }
  CmpCleanupThreadInfo(&v16, v12, v13);
  return v14;
}
