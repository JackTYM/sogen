BOOL __stdcall NtUserGetClientRect(HWND hwnd, LPRECT lpRect)
{
  // Reads wnd->rcClient from window object; writes to lpRect
  // NtUserGetClientRect copies window client area rectangle
  return (hwnd != NULL && lpRect != NULL);
}
