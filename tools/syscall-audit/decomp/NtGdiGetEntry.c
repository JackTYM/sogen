__int64 __fastcall NtGdiGetEntry(unsigned int a1, unsigned __int64 a2)                | 
{                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                       | 
                                                                                      | 
  SessionState = W32GetSessionState();                                                | 
  EntryFromIndex = GdiHandleManager::GetEntryFromIndex(                               | 
                     *(GdiHandleManager **)(*(_QWORD *)(SessionState + 88) + 5672LL), | 
                     a1);                                                             | 
  if ( !EntryFromIndex )                                                              | 
  {                                                                                   | 
    return 3221225473LL;                                                              | 
  }                                                                                   | 
  if ( a2 + 24 > MmUserProbeAddress || a2 + 24 <= a2 )                                | 
  {                                                                                   | 
    *(_BYTE *)MmUserProbeAddress = 0;                                                 | 
  }                                                                                   | 
  *(_OWORD *)a2 = *(_OWORD *)EntryFromIndex;                                          | 
  *(_QWORD *)(a2 + 16) = *((_QWORD *)EntryFromIndex + 2);                             | 
  return 0LL;                                                                         | 
}                                                                                     | 
20 row(s)
