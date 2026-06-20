__int64 __fastcall NtUserValidateTimerCallback(__int64 a1)                                                             | 
{                                                                                                                      | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                        | 
                                                                                                                       | 
  v2 = 1LL;                                                                                                            | 
  EnterLeaveCritShared::EnterLeaveCritShared(&v9, 1LL);                                                                | 
  CurrentThreadNonPaged = (__int64 *)W32GetCurrentThreadNonPaged();                                                    | 
  if ( CurrentThreadNonPaged )                                                                                         | 
  {                                                                                                                    | 
    v4 = *CurrentThreadNonPaged;                                                                                       | 
  }                                                                                                                    | 
  else                                                                                                                 | 
  {                                                                                                                    | 
    v4 = 0LL;                                                                                                          | 
  }                                                                                                                    | 
  if ( *(_DWORD *)(v4 + 672) > 0x501u                                                                                  | 
    || (*(_DWORD *)(v4 + 688) & 0x1000000) == 0                                                                        | 
    || (_InterlockedCompareExchange((volatile signed __int32 *)(v4 + 528), 0, 0) & 0xC) != 0                           | 
    || (v8 = *(HANDLE *)(W32GetUserSessionState() + 63680), PsGetProcessId(**(PEPROCESS **)(v4 + 464)) == v8) )        | 
  {                                                                                                                    | 
    v5 = (_QWORD **)(W32GetUserSessionState() + 57680);                                                                | 
    for ( i = *v5; i != v5; i = (_QWORD *)*i )                                                                         | 
    {                                                                                                                  | 
      if ( *(_QWORD *)(*(i - 6) + 464LL) == *(_QWORD *)(v4 + 464) && (*(_DWORD *)(i - 3) & 6) == 0 && *(i - 5) == a1 ) | 
      {                                                                                                                | 
        goto LABEL_13;                                                                                                 | 
      }                                                                                                                | 
    }                                                                                                                  | 
    v2 = 0LL;                                                                                                          | 
  }                                                                                                                    | 
LABEL_13:                                                                                                              | 
  UserSessionSwitchLeaveCrit();                                                                                        | 
  return v2;                                                                                                           | 
}                                                                                                                      | 
34 row(s)
