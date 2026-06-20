HBRUSH __stdcall NtGdiSelectBrushLocal(HDC hdc, HBRUSH hbr, HBRUSH *phbrOld)
{
  // Selects brush into DC, updating dc_attr and returning old brush
  // Sogen: updates DC brush field in GDI object table
  return hbr;
}
