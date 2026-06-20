__int64 __fastcall NtListenPort(HANDLE Handle, __int64 a2)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  do
  {
    result = NtReplyWaitReceivePortEx(Handle, 0LL);
  }
  while ( !(_DWORD)result && (*(_WORD *)(a2 + 4) & 0x7FFF) != 0xA );
  return result;
}
11 row(s)
