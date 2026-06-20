__int64 __fastcall NtAlpcAcceptConnectPort(
        int a1,
        int a2,
        int a3,
        int a4,
        void *a5,
        __int64 a6,
        void *a7,
        __int64 a8,
        char a9)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  CurrentThread = KeGetCurrentThread();
  --CurrentThread->KernelApcDisable;
  v10 = AlpcpAcceptConnectPort(a1, a3 & 0xC0000000, a2, a4, a5, a6, a7, a8, a9, 0LL, 0LL, 0);
  KeLeaveCriticalRegion();
  return v10;
}
19 row(s)
