BOOL __stdcall NtGdiMoveToEx(HDC hdc, LONG x, LONG y, LPPOINT pptOut)
{
  // Updates current position in DC_ATTR; calls PFNTABLE::pfnMoveToEx for device path
  // Sogen: reads dc_attr from GDI handle table and updates ptlCurrent field
  return (hdc != 0);
}
