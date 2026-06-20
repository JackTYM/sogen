__int64 __fastcall NtUserChangeDisplaySettings(int a1, int a2, int a3, __int64 a4)                           | 
{                                                                                                            | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                              | 
                                                                                                             | 
  v21[0] = 0;                                                                                                | 
  CDisplayScenarioContextScope::ContextScopeConstructor((CDisplayScenarioContextScope *)v21, 0LL, 0x1Au, 0); | 
  v8 = v22;                                                                                                  | 
  UserSessionState = W32GetUserSessionState();                                                               | 
  v10 = UserCritInternal::_anonymous_namespace_::EnterCritInternal(                                          | 
          UserSessionState,                                                                                  | 
          0LL,                                                                                               | 
          0LL,                                                                                               | 
          _lambda_63b61c2369133a205197eda5bd671ee7_::_lambda_invoker_cdecl_);                                | 
  *(_QWORD *)(UserSessionState + 16) = v10;                                                                  | 
  if ( v10 )                                                                                                 | 
  {                                                                                                          | 
    DestroySharedUserCritDeferredUnlockList(UserSessionState + 19832);                                       | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19888);                                     | 
    DestroyDeferredUnlockObjectAssignmentList(UserSessionState + 19872);                                     | 
  }                                                                                                          | 
  if ( *(_DWORD *)(W32GetUserGdiSessionState() + 32) )                                                       | 
  {                                                                                                          | 
    v12 = PtiCurrent();                                                                                      | 
    IsEnabledDeviceUsageNoInline = Feature_UIPIModernization__private_IsEnabledDeviceUsageNoInline();        | 
    v15 = (UIPrivilegeIsolation *)(*((_QWORD *)v12 + 58) + 856LL);                                           | 
    if ( IsEnabledDeviceUsageNoInline )                                                                      | 
    {                                                                                                        | 
      v16 = UIPrivilegeIsolation::MeetsUIPIThreshold(v15, (const struct tagUIPI_INFO *)0x2000, v14);         | 
    }                                                                                                        | 
    else                                                                                                     | 
    {                                                                                                        | 
      v20[1] = -1;                                                                                           | 
      v20[0] = 0x2000;                                                                                       | 
      v20[2] = 0;                                                                                            | 
      v16 = UIPrivilegeIsolation::CheckAccessEx(v15, v20, 0LL);                                              | 
    }                                                                                                        | 
    v17 = v16;                                                                                               | 
    if ( (unsigned __int8)tagPROCESSINFO::CanPerformUIOperation(*((_QWORD *)v12 + 58), 16LL) && v17 )        | 
    {                                                                                                        | 
      if ( (a3 & 0x8000000) != 0 )                                                                           | 
      {                                                                                                      | 
        v18 = 87LL;                                                                                          | 
LABEL_14:                                                                                                    | 
        v11 = -1;                                                                                            | 
        UserSetLastError(v18);                                                                               | 
        goto LABEL_16;                                                                                       | 
      }                                                                                                      | 
    }                                                                                                        | 
    else if ( a3 != 34 )                                                                                     | 
    {                                                                                                        | 
      v18 = 5LL;                                                                                             | 
      goto LABEL_14;                                                                                         | 
    }                                                                                                        | 
    DrvLogDiagDisplayChange(0LL, 8LL);                                                                       | 
    v11 = xxxUserChangeDisplaySettings(a1, a2, 0, a3, a4, UserMode, v8);                                     | 
    goto LABEL_16;                                                                                           | 
  }                                                                                                          | 
  v11 = -1;                                                                                                  | 
LABEL_16:                                                                                                    | 
  UserSessionSwitchLeaveCritWithNonPaged();                                                                  | 
  CDisplayScenarioContextScope::~CDisplayScenarioContextScope((CDisplayScenarioContextScope *)v21);          | 
  return v11;                                                                                                | 
}                                                                                                            | 
63 row(s)
