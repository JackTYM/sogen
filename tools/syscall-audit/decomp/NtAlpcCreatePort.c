__int64 __fastcall NtAlpcCreatePort(__int64 a1, __int64 a2, __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  v7 = 0;
  v6 = 0;
  --CurrentThread->KernelApcDisable;
  ConnectionPort = AlpcpCreateConnectionPort(a1, a2, a3, 0LL, v6, v7);
  KeLeaveCriticalRegion();
  return ConnectionPort;
}
12 row(s)
