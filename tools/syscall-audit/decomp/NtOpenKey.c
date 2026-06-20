line
__int64 __fastcall NtOpenKey(__int64 a1, __int64 a2, __int64 a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  return NtOpenKeyEx(a1, a2, a3, 0LL);
}
