__int64 __fastcall NtQueryDefaultLocale(__int64 a1, int *a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v2 = a1;
  v3 = 0;
  if ( KeGetCurrentThread()->PreviousMode )
  {
    a1 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
    {
      a1 = (__int64)a2;
    }
    *(_DWORD *)a1 = *(_DWORD *)a1;
  }
  if ( v2 )
  {
    SessionLocaleId = PsGetSessionLocaleId(a1, a2, 0LL);
  }
  else
  {
    SessionLocaleId = PsDefaultSystemLocaleId;
  }
  *a2 = SessionLocaleId;
  return v3;
}
