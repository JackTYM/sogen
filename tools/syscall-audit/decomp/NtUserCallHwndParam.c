UINT_PTR __stdcall NtUserCallHwndParam(HWND hwnd, ULONG_PTR dwParam, DWORD dwCode)
{
  // Dispatches one of ~60 window management subcommands (dwCode index) with hwnd+param
  // NtUserCallHwndParam routes to xxxCallHwndParam dispatch table
  return 0;
}
