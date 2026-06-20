__int64 __fastcall NtConnectPort(__int64 a1, __int64 a2, __int64 a3, __int64 a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  return NtSecureConnectPort(a1, a2, a3, a4, 0LL);
}
6 row(s)
