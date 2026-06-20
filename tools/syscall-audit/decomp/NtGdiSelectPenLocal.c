HPEN __stdcall NtGdiSelectPenLocal(HDC hdc, HPEN hpen, HPEN *phpOld)
{
  // Selects pen into DC, updating dc_attr.hpen field
  // Sogen: updates DC pen field in GDI object table
  return hpen;
}
