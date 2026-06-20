__int64 __fastcall NtUserSetWindowCompositionAttribute(__int64 a1, void *a2)                                            | 
{                                                                                                                       | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                         | 
                                                                                                                        | 
  v3 = -1073741811;                                                                                                     | 
  v28 = -1073741811;                                                                                                    | 
  v31 = 0LL;                                                                                                            | 
  v32 = 0LL;                                                                                                            | 
  v26 = 0LL;                                                                                                            | 
  v27 = 0LL;                                                                                                            | 
  v29 = 0LL;                                                                                                            | 
  Length = 0LL;                                                                                                         | 
  RtlCopyFromUser(&v29, a2, 0x18uLL);                                                                                   | 
  v31 = v29;                                                                                                            | 
  v32 = Length;                                                                                                         | 
  v4 = (unsigned int)Length;                                                                                            | 
  CurrentProcessWow64Process = PsGetCurrentProcessWow64Process();                                                       | 
  v6 = (volatile void *)_mm_srli_si128(v29, 8).m128i_u64[0];                                                            | 
  ProbeForRead(v6, (unsigned int)v4, CurrentProcessWow64Process != 0 ? 1 : 4);                                          | 
  v7 = v29.m128i_i32[0];                                                                                                | 
  if ( (unsigned int)(v29.m128i_i32[0] - 1) <= 0x24 )                                                                   | 
  {                                                                                                                     | 
    v8 = qword_1403538D8[2 * v29.m128i_i32[0]];                                                                         | 
    if ( v4 >= v8 )                                                                                                     | 
    {                                                                                                                   | 
      memmove(&v26, (const void *)v6, v8);                                                                              | 
      v9 = 0LL;                                                                                                         | 
      v3 = 0;                                                                                                           | 
      v28 = 0;                                                                                                          | 
      goto LABEL_6;                                                                                                     | 
    }                                                                                                                   | 
    v3 = -1073741789;                                                                                                   | 
    v28 = -1073741789;                                                                                                  | 
  }                                                                                                                     | 
  v9 = 0LL;                                                                                                             | 
LABEL_6:                                                                                                                | 
  if ( v3 < 0 )                                                                                                         | 
  {                                                                                                                     | 
LABEL_112:                                                                                                              | 
    v24 = RtlNtStatusToDosError(v3);                                                                                    | 
    UserSetLastError(v24);                                                                                              | 
    v23 = v3 < 0;                                                                                                       | 
    goto LABEL_113;                                                                                                     | 
  }                                                                                                                     | 
  v10 = 1;                                                                                                              | 
  EnterCrit(1LL);                                                                                                       | 
  v11 = ValidateHwndIAMComponetUIAware(a1);                                                                             | 
  v12 = v11;                                                                                                            | 
  if ( !v11 )                                                                                                           | 
  {                                                                                                                     | 
    goto LABEL_110;                                                                                                     | 
  }                                                                                                                     | 
  if ( v7 != 17                                                                                                         | 
    || !(unsigned int)IsTopLevelOrLayeredChildWindow(v11)                                                               | 
    || *(_DWORD *)(*(_QWORD *)(v12 + 40) + 236LL) == 15 )                                                               | 
  {                                                                                                                     | 
    if ( v7 == 36 && (unsigned int)IsTopLevelOrLayeredChildWindow(v12) )                                                | 
    {                                                                                                                   | 
      updated = SetWindowCompositionAlpha(                                                                              | 
                  (struct tagWND *)v12,                                                                                 | 
                  (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                           | 
LABEL_109:                                                                                                              | 
      v3 = updated;                                                                                                     | 
      goto LABEL_111;                                                                                                   | 
    }                                                                                                                   | 
    if ( (unsigned int)IsTopLevelWindow(v12) || v7 == 20 && (unsigned int)IsTopLevelOrLayeredChildWindow(v12) )         | 
    {                                                                                                                   | 
      if ( v7 > 19 )                                                                                                    | 
      {                                                                                                                 | 
        if ( v7 > 29 )                                                                                                  | 
        {                                                                                                               | 
          switch ( v7 )                                                                                                 | 
          {                                                                                                             | 
            case 30:                                                                                                    | 
              updated = SetWindowCompositionSystemBackdropType(                                                         | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            case 31:                                                                                                    | 
              updated = _DwmAsyncSetTaggedWindowRect((const struct tagWND *)v12, &v26);                                 | 
              goto LABEL_109;                                                                                           | 
            case 32:                                                                                                    | 
              updated = _DwmAsyncClearTaggedWindowRect((const struct tagWND *)v12);                                     | 
              goto LABEL_109;                                                                                           | 
            case 33:                                                                                                    | 
              updated = SetWindowCompositionRemoteAppPolicy(                                                            | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
          }                                                                                                             | 
          if ( v7 != 34 )                                                                                               | 
          {                                                                                                             | 
            if ( v7 == 35 )                                                                                             | 
            {                                                                                                           | 
              v3 = 0;                                                                                                   | 
              if ( v26.left )                                                                                           | 
              {                                                                                                         | 
                if ( (v26.left & 0xFF000000) != 0xFF000000 )                                                            | 
                {                                                                                                       | 
LABEL_25:                                                                                                               | 
                  v3 = -1073741811;                                                                                     | 
                  goto LABEL_111;                                                                                       | 
                }                                                                                                       | 
                *(_DWORD *)(v12 + 180) = v26.left & 0xFFFFFF;                                                           | 
              }                                                                                                         | 
              else                                                                                                      | 
              {                                                                                                         | 
                *(_DWORD *)(v12 + 180) = -1;                                                                            | 
              }                                                                                                         | 
              goto LABEL_111;                                                                                           | 
            }                                                                                                           | 
            if ( v7 == 37 )                                                                                             | 
            {                                                                                                           | 
              updated = DwmSetBorderMargins(                                                                            | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            }                                                                                                           | 
          }                                                                                                             | 
          goto LABEL_98;                                                                                                | 
        }                                                                                                               | 
        if ( v7 == 29 )                                                                                                 | 
        {                                                                                                               | 
          updated = SetWindowDisableMoveSizeFeedback(                                                                   | 
                      (struct tagWND *)v12,                                                                             | 
                      (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                       | 
          goto LABEL_109;                                                                                               | 
        }                                                                                                               | 
        if ( v7 != 20 )                                                                                                 | 
        {                                                                                                               | 
          switch ( v7 )                                                                                                 | 
          {                                                                                                             | 
            case 22:                                                                                                    | 
              updated = SetWindowCompositionVisualOwner(                                                                | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            case 23:                                                                                                    | 
              v18 = PtiCurrent();                                                                                       | 
              if ( (unsigned int)IAMThreadAccessGranted(v18) )                                                          | 
              {                                                                                                         | 
                if ( (v26.left != 0) != *(_BYTE *)(*(_QWORD *)(v12 + 40) + 233LL) >> 7 )                                | 
                {                                                                                                       | 
                  SetOrClrWF(v26.left != 0, v12, 55680LL, 1LL);                                                         | 
                }                                                                                                       | 
                v3 = 0;                                                                                                 | 
              }                                                                                                         | 
              else                                                                                                      | 
              {                                                                                                         | 
                v3 = -1073741790;                                                                                       | 
              }                                                                                                         | 
              goto LABEL_111;                                                                                           | 
            case 24:                                                                                                    | 
              updated = SetWindowCompositionExcludedFromDDA(                                                            | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            case 25:                                                                                                    | 
              updated = SetWindowCompositionPassiveUpdateMode(                                                          | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            case 26:                                                                                                    | 
              updated = SetWindowCompositionDarkModeColors(                                                             | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            case 27:                                                                                                    | 
              updated = SetWindowCompositionCornerStyle(                                                                | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            case 28:                                                                                                    | 
              updated = SetWindowCompositionPartColor(                                                                  | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
          }                                                                                                             | 
          goto LABEL_98;                                                                                                | 
        }                                                                                                               | 
        SetWindowCompositionFreezeSWR((struct tagWND *)v12, (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26); | 
      }                                                                                                                 | 
      else                                                                                                              | 
      {                                                                                                                 | 
        if ( v7 == 19 )                                                                                                 | 
        {                                                                                                               | 
          updated = SetWindowCompositionAccentPolicy(                                                                   | 
                      (struct tagWND *)v12,                                                                             | 
                      (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                       | 
          goto LABEL_109;                                                                                               | 
        }                                                                                                               | 
        if ( v7 > 9 )                                                                                                   | 
        {                                                                                                               | 
          if ( v7 == 10 )                                                                                               | 
          {                                                                                                             | 
            updated = _DwmAsyncSetCompositionAttribute(v12, 10LL, &v26);                                                | 
            goto LABEL_109;                                                                                             | 
          }                                                                                                             | 
          if ( v7 == 11 )                                                                                               | 
          {                                                                                                             | 
            updated = SetWindowCompositionNCRenderingExiled(                                                            | 
                        (struct tagWND *)v12,                                                                           | 
                        (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                     | 
            goto LABEL_109;                                                                                             | 
          }                                                                                                             | 
          if ( v7 != 12 )                                                                                               | 
          {                                                                                                             | 
            if ( v7 == 13 )                                                                                             | 
            {                                                                                                           | 
              updated = SetWindowCompositionExcludeFromLivePreview(                                                     | 
                          (struct tagWND *)v12,                                                                         | 
                          (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                   | 
              goto LABEL_109;                                                                                           | 
            }                                                                                                           | 
            if ( v7 != 14 )                                                                                             | 
            {                                                                                                           | 
              if ( v7 == 15 )                                                                                           | 
              {                                                                                                         | 
                updated = SetWindowCompositionForceActiveWindowAppearance(                                              | 
                            (struct tagWND *)v12,                                                                       | 
                            (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                 | 
                goto LABEL_109;                                                                                         | 
              }                                                                                                         | 
              if ( v7 == 16 )                                                                                           | 
              {                                                                                                         | 
                updated = SetWindowCompositionDisallowPeek(                                                             | 
                            (struct tagWND *)v12,                                                                       | 
                            (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                 | 
                goto LABEL_109;                                                                                         | 
              }                                                                                                         | 
            }                                                                                                           | 
          }                                                                                                             | 
          goto LABEL_98;                                                                                                | 
        }                                                                                                               | 
        if ( v7 != 9 )                                                                                                  | 
        {                                                                                                               | 
          if ( v7 != 1 )                                                                                                | 
          {                                                                                                             | 
            switch ( v7 )                                                                                               | 
            {                                                                                                           | 
              case 2:                                                                                                   | 
                updated = SetWindowCompositionNCRenderPolicy(                                                           | 
                            (struct tagWND *)v12,                                                                       | 
                            (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                 | 
                goto LABEL_109;                                                                                         | 
              case 3:                                                                                                   | 
                updated = SetWindowCompositionTransitions(                                                              | 
                            (struct tagWND *)v12,                                                                       | 
                            (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                 | 
                goto LABEL_109;                                                                                         | 
              case 4:                                                                                                   | 
                updated = SetWindowCompositionAllowNCPaint(                                                             | 
                            (struct tagWND *)v12,                                                                       | 
                            (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                 | 
                goto LABEL_109;                                                                                         | 
            }                                                                                                           | 
            if ( v7 != 5 )                                                                                              | 
            {                                                                                                           | 
              if ( v7 == 6 )                                                                                            | 
              {                                                                                                         | 
                updated = SetWindowCompositionNonClientRTLLayout(                                                       | 
                            (struct tagWND *)v12,                                                                       | 
                            (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                 | 
                goto LABEL_109;                                                                                         | 
              }                                                                                                         | 
              if ( v7 == 7 )                                                                                            | 
              {                                                                                                         | 
                updated = SetWindowCompositionForceIconicSWR(                                                           | 
                            (struct tagWND *)v12,                                                                       | 
                            (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                 | 
                goto LABEL_109;                                                                                         | 
              }                                                                                                         | 
            }                                                                                                           | 
          }                                                                                                             | 
LABEL_98:                                                                                                               | 
          if ( *(unsigned int **)&WPP_GLOBAL_Control == &WPP_GLOBAL_Control                                             | 
            || (*(_DWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 44LL) & 1) == 0                                            | 
            || *(_BYTE *)(*(_QWORD *)&WPP_GLOBAL_Control + 41LL) < 3u )                                                 | 
          {                                                                                                             | 
            v10 = 0;                                                                                                    | 
          }                                                                                                             | 
          v19 = *(_QWORD *)&WPP_RECORDER_INITIALIZED != (_QWORD)&WPP_RECORDER_INITIALIZED;                              | 
          if ( v10 || *(unsigned int **)&WPP_RECORDER_INITIALIZED != &WPP_RECORDER_INITIALIZED )                        | 
          {                                                                                                             | 
            UserSessionState = W32GetUserSessionState();                                                                | 
            LOBYTE(v21) = v19;                                                                                          | 
            LOBYTE(v22) = v10;                                                                                          | 
            WPP_RECORDER_AND_TRACE_SF_sdq(                                                                              | 
              *(_QWORD *)(*(_QWORD *)&WPP_GLOBAL_Control + 24LL),                                                       | 
              v22,                                                                                                      | 
              v21,                                                                                                      | 
              *(_QWORD *)(UserSessionState + 69512));                                                                   | 
          }                                                                                                             | 
          goto LABEL_25;                                                                                                | 
        }                                                                                                               | 
      }                                                                                                                 | 
      updated = SetWindowCompositionHasIconicBitmap(                                                                    | 
                  (struct tagWND *)v12,                                                                                 | 
                  (const struct tagWINDOWCOMPOSITIONATTRIBDATA_UNION *)&v26);                                           | 
      goto LABEL_109;                                                                                                   | 
    }                                                                                                                   | 
LABEL_110:                                                                                                              | 
    v3 = -1073741816;                                                                                                   | 
    goto LABEL_111;                                                                                                     | 
  }                                                                                                                     | 
  v13 = 0;                                                                                                              | 
  v14 = PtiCurrent();                                                                                                   | 
  if ( (unsigned int)IAMThreadAccessGranted(v14) )                                                                      | 
  {                                                                                                                     | 
    v13 = 2;                                                                                                            | 
  }                                                                                                                     | 
  else                                                                                                                  | 
  {                                                                                                                     | 
    v15 = *((_QWORD *)v14 + 58);                                                                                        | 
    if ( *(_QWORD *)(*(_QWORD *)(v12 + 16) + 464LL) == v15                                                              | 
      && ((*(_BYTE *)(v15 + 800) & 0x30) != 0x10 || !(unsigned int)IsTopLevelWindow(v12))                               | 
      || (unsigned int)CoreWindowProp::IsComponent((const struct tagWND *)v12)                                          | 
      && (Host = CoreWindowProp::GetHost((const struct tagWND *)v12)) != 0LL                                            | 
      && *(_QWORD *)(*((_QWORD *)Host + 2) + 464LL) == *((_QWORD *)v14 + 58) )                                          | 
    {                                                                                                                   | 
      v13 = 1;                                                                                                          | 
    }                                                                                                                   | 
    else                                                                                                                | 
    {                                                                                                                   | 
      v3 = -1073741790;                                                                                                 | 
    }                                                                                                                   | 
  }                                                                                                                     | 
  if ( v3 >= 0 )                                                                                                        | 
  {                                                                                                                     | 
    if ( !v26.left && *(_QWORD *)(v12 + 120) && ((unsigned int)_GetWindowCloakState() & v13) != 0 )                     | 
    {                                                                                                                   | 
      goto LABEL_25;                                                                                                    | 
    }                                                                                                                   | 
    updated = zzzUpdateWindowCompositionCloak(v12, v26.left == 0, v13);                                                 | 
    goto LABEL_109;                                                                                                     | 
  }                                                                                                                     | 
LABEL_111:                                                                                                              | 
  UserSessionSwitchLeaveCrit();                                                                                         | 
  v23 = v3 < 0;                                                                                                         | 
  if ( v3 < 0 )                                                                                                         | 
  {                                                                                                                     | 
    goto LABEL_112;                                                                                                     | 
  }                                                                                                                     | 
LABEL_113:                                                                                                              | 
  LOBYTE(v9) = !v23;                                                                                                    | 
  return v9;                                                                                                            | 
}                                                                                                                       | 
347 row(s)
