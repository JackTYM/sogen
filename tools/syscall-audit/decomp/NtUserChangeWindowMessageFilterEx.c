__int64 __fastcall NtUserChangeWindowMessageFilterEx(__int64 a1, unsigned int a2, unsigned int a3, void *a4)  | 
{                                                                                                             | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                               | 
                                                                                                              | 
  Src = 0LL;                                                                                                  | 
  v8 = EnterCrit(0LL, 0LL);                                                                                   | 
  v9 = ValidateReceivingHwnd(a1, 1LL);                                                                        | 
  v10 = (struct tagWND *)v9;                                                                                  | 
  if ( v9 )                                                                                                   | 
  {                                                                                                           | 
    Win32HMThreadLockAlways<tagHOOK>::Win32HMThreadLockAlways<tagHOOK>(BugCheckParameter3, v8, v9);           | 
    if ( *(_QWORD *)(v8 + 464) != *(_QWORD *)(*((_QWORD *)v10 + 2) + 464LL) )                                 | 
    {                                                                                                         | 
      v12 = 5LL;                                                                                              | 
LABEL_22:                                                                                                     | 
      v11 = 0;                                                                                                | 
      UserSetLastError(v12);                                                                                  | 
      goto LABEL_23;                                                                                          | 
    }                                                                                                         | 
    v13 = *((_QWORD *)v10 + 5);                                                                               | 
    if ( *(char *)(v13 + 20) < 0 || *(char *)(v13 + 19) < 0 )                                                 | 
    {                                                                                                         | 
      v12 = 1400LL;                                                                                           | 
      goto LABEL_22;                                                                                          | 
    }                                                                                                         | 
    if ( a3 <= 2 )                                                                                            | 
    {                                                                                                         | 
      if ( a3 )                                                                                               | 
      {                                                                                                       | 
        if ( a2 )                                                                                             | 
        {                                                                                                     | 
          goto LABEL_12;                                                                                      | 
        }                                                                                                     | 
      }                                                                                                       | 
      else if ( !a2 )                                                                                         | 
      {                                                                                                       | 
LABEL_12:                                                                                                     | 
        if ( a4 )                                                                                             | 
        {                                                                                                     | 
          v16[0] = 0LL;                                                                                       | 
          RtlCopyFromUser(v16, a4, 8uLL);                                                                     | 
          Src = v16[0];                                                                                       | 
          if ( LODWORD(v16[0]) != 8 )                                                                         | 
          {                                                                                                   | 
            UserSetLastError(87LL);                                                                           | 
            v11 = 0;                                                                                          | 
LABEL_23:                                                                                                     | 
            Win32HMThreadLockAlways<tagWND>::~Win32HMThreadLockAlways<tagWND>((ULONG_PTR)BugCheckParameter3); | 
            goto LABEL_24;                                                                                    | 
          }                                                                                                   | 
        }                                                                                                     | 
        else                                                                                                  | 
        {                                                                                                     | 
          LODWORD(Src) = 8;                                                                                   | 
        }                                                                                                     | 
        v11 = ChangeWindowMessageFilterEx(v10, a2);                                                           | 
        if ( v11 && a4 )                                                                                      | 
        {                                                                                                     | 
          RtlCopyToUser(a4, &Src, 8uLL);                                                                      | 
        }                                                                                                     | 
        goto LABEL_23;                                                                                        | 
      }                                                                                                       | 
    }                                                                                                         | 
    v12 = 87LL;                                                                                               | 
    goto LABEL_22;                                                                                            | 
  }                                                                                                           | 
  v11 = 0;                                                                                                    | 
LABEL_24:                                                                                                     | 
  UserSessionSwitchLeaveCrit();                                                                               | 
  return v11;                                                                                                 | 
}                                                                                                             | 
71 row(s)
