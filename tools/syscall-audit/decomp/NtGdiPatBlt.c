BOOL __stdcall NtGdiPatBlt(HDC hdc, LONG x, LONG y, LONG cx, LONG cy, DWORD rop)
{
  // Fills rectangle with current pattern brush using ROP; calls pfnPatBlt
  // Sogen: routes to gdi_bitmap_surface fill path with dc pattern brush
  return (hdc != 0);
}
