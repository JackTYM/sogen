__int64 __fastcall NtQuerySystemInformation(unsigned int a1, __int64 a2, int a3, __int64 a4)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  p_PrimaryGroupThread = 0LL;
  v6 = a4;
  PrimaryGroupThread = 0;
  v8 = a1;
  switch ( a1 )
  {
    case 8u:
    case 0x17u:
    case 0x2Au:
    case 0x3Du:
    case 0x53u:
    case 0x64u:
    case 0x6Cu:
    case 0x8Du:
      PrimaryGroupThread = KeQueryPrimaryGroupThread(KeGetCurrentThread());
      goto LABEL_3;
    case 0x49u:
LABEL_3:
      p_PrimaryGroupThread = &PrimaryGroupThread;
      v9 = 2LL;
      return ExpQuerySystemInformation(v8, p_PrimaryGroupThread, v9, a2, a3, v6);
    case 0x6Bu:
    case 0x79u:
    case 0xB4u:
    case 0xD2u:
    case 0xD3u:
    case 0xDEu:
    case 0xE7u:
    case 0xEEu:
    case 0xEFu:
    case 0xF0u:
    case 0xFEu:
      return 3221225475LL;
    default:
      v9 = 0LL;
      return ExpQuerySystemInformation(v8, p_PrimaryGroupThread, v9, a2, a3, v6);
  }
}
