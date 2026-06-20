__int64 __fastcall NtAcceptConnectPort(int a1, __int64 a2, void *a3, char a4, void *a5, __int64 a6)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v7 = AlpcpAcceptConnectPort(a1, 0, 0, 0, 0LL, a2, a3, 0LL, a4, a5, a6, 1);
  KeLeaveCriticalRegion();
  if ( v7 == -1073740029 )
  {
    return (unsigned int)-1073741813;
  }
  return v7;
}
14 row(s)
