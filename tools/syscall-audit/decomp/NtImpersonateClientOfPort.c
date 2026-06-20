__int64 __fastcall NtImpersonateClientOfPort(void *a1)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  result = NtAlpcImpersonateClientOfPort(a1);
  if ( (_DWORD)result == -1073740030 )
  {
    return 3221226015LL;
  }
  return result;
}
11 row(s)
