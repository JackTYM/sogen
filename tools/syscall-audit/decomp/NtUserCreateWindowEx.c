__int64 __fastcall NtUserCreateWindowEx(                                                                              | 
        unsigned int a1,                                                                                              | 
        ULONG64 a2,                                                                                                   | 
        __m128i *a3,                                                                                                  | 
        __m128i *a4,                                                                                                  | 
        int a5,                                                                                                       | 
        int a6,                                                                                                       | 
        int a7,                                                                                                       | 
        int a8,                                                                                                       | 
        int a9,                                                                                                       | 
        __int64 a10,                                                                                                  | 
        __int64 a11,                                                                                                  | 
        __int64 a12,                                                                                                  | 
        __int64 a13,                                                                                                  | 
        int a14,                                                                                                      | 
        int a15,                                                                                                      | 
        int a16,                                                                                                      | 
        __int64 a17)                                                                                                  | 
{                                                                                                                     | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                       | 
                                                                                                                      | 
  v17 = a4;                                                                                                           | 
  v18 = a3;                                                                                                           | 
  v19 = a2;                                                                                                           | 
  *(_OWORD *)Size = 0LL;                                                                                              | 
  v52 = 0LL;                                                                                                          | 
  v53 = 0LL;                                                                                                          | 
  EnterCrit(0LL, 0LL);                                                                                                | 
  Win32HMOptionalThreadLockAlways<tagMENU>::Win32HMOptionalThreadLockAlways<tagMENU>(BugCheckParameter3);             | 
  tagTLBLOCK::_unnamed_type_list_::_unnamed_type_list_((tagTLBLOCK::_unnamed_type_list_ *)BugCheckParameter2);        | 
  SmartObjStackRef<tagMENU>::SmartObjStackRef<tagMENU>(v50);                                                          | 
  v21 = 0LL;                                                                                                          | 
  if ( a10 == -3 )                                                                                                    | 
  {                                                                                                                   | 
    v49 = 0LL;                                                                                                        | 
    v22 = PtiCurrent();                                                                                               | 
    if ( v22 )                                                                                                        | 
    {                                                                                                                 | 
      v23 = *((_QWORD *)v22 + 62);                                                                                    | 
      if ( v23 )                                                                                                      | 
      {                                                                                                               | 
        v49 = *(struct tagWND **)(v23 + 112);                                                                         | 
      }                                                                                                               | 
    }                                                                                                                 | 
    if ( (a16 & 1) != 0 )                                                                                             | 
    {                                                                                                                 | 
      goto LABEL_86;                                                                                                  | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  else if ( a10 )                                                                                                     | 
  {                                                                                                                   | 
    v49 = (struct tagWND *)ValidateHwnd();                                                                            | 
    if ( !v49 )                                                                                                       | 
    {                                                                                                                 | 
      goto LABEL_75;                                                                                                  | 
    }                                                                                                                 | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v49 = 0LL;                                                                                                        | 
  }                                                                                                                   | 
  if ( (a16 & 0xFFFFFFF8) != 0 )                                                                                      | 
  {                                                                                                                   | 
LABEL_86:                                                                                                             | 
    UserSetLastError(87LL);                                                                                           | 
    goto LABEL_75;                                                                                                    | 
  }                                                                                                                   | 
  v24 = PtiCurrent();                                                                                                 | 
  v56[0] = (ULONG_PTR)v24;                                                                                            | 
  if ( (a5 & 0xC0000000) == 0x40000000 || !a11 )                                                                      | 
  {                                                                                                                   | 
    v25 = SmartObjStackRef<tagMENU>::SmartObjStackRef<tagMENU>(v58, a11);                                             | 
    v51 = *(_QWORD *)(v25 + 16);                                                                                      | 
    v26 = **(_QWORD **)v25;                                                                                           | 
    if ( v26 != *(_QWORD *)v50[0] )                                                                                   | 
    {                                                                                                                 | 
      SmartObjStackRefBase<tagMENU>::DecrementCountAndTryFree(v50);                                                   | 
      if ( v26 )                                                                                                      | 
      {                                                                                                               | 
        v50[0] = *(_QWORD *)(v26 + 152);                                                                              | 
        ++*(_DWORD *)(v50[0] + 8);                                                                                    | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        v50[0] = gSmartObjNullRef;                                                                                    | 
      }                                                                                                               | 
    }                                                                                                                 | 
    SmartObjStackRef<tagMENU>::~SmartObjStackRef<tagMENU>(v58);                                                       | 
    v17 = a4;                                                                                                         | 
    v18 = a3;                                                                                                         | 
    v19 = a2;                                                                                                         | 
  }                                                                                                                   | 
  else                                                                                                                | 
  {                                                                                                                   | 
    v46 = ValidateHmenu(a11, 1LL);                                                                                    | 
    v51 = 0LL;                                                                                                        | 
    SmartObjStackRefBase<tagMENU>::operator=(v50, v46);                                                               | 
    if ( (unsigned __int8)SmartObjStackRef<tagMENU>::operator==(v50) )                                                | 
    {                                                                                                                 | 
      goto LABEL_75;                                                                                                  | 
    }                                                                                                                 | 
    v47 = v51;                                                                                                        | 
    if ( !v51 )                                                                                                       | 
    {                                                                                                                 | 
      v47 = *(_QWORD *)v50[0];                                                                                        | 
      v17 = a4;                                                                                                       | 
      v18 = a3;                                                                                                       | 
      v19 = a2;                                                                                                       | 
    }                                                                                                                 | 
    Win32HM_LockIntoThread<0>(v24, v47, BugCheckParameter3);                                                          | 
  }                                                                                                                   | 
  AppCompatFlags2 = GetAppCompatFlags2(1024LL);                                                                       | 
  v28 = a1 & 0x800777FF;                                                                                              | 
  if ( (AppCompatFlags2 & 0x800) == 0 )                                                                               | 
  {                                                                                                                   | 
    v28 = a1;                                                                                                         | 
  }                                                                                                                   | 
  v29 = v28 & 0x8A7F77FF;                                                                                             | 
  if ( (v28 & 0x8A7F77FF) == v28 )                                                                                    | 
  {                                                                                                                   | 
    v29 = v28;                                                                                                        | 
  }                                                                                                                   | 
  v62 = v29;                                                                                                          | 
  v30 = (__m128i **)MmUserProbeAddress;                                                                               | 
  if ( (v19 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                                           | 
  {                                                                                                                   | 
    if ( v19 >= MmUserProbeAddress )                                                                                  | 
    {                                                                                                                 | 
      v19 = MmUserProbeAddress;                                                                                       | 
    }                                                                                                                 | 
    *(_OWORD *)Size = *(_OWORD *)v19;                                                                                 | 
    v37 = _mm_srli_si128(*(__m128i *)Size, 8).m128i_u64[0];                                                           | 
    if ( v37 )                                                                                                        | 
    {                                                                                                                 | 
      if ( LODWORD(Size[0]) > (HIDWORD(Size[0]) & 0x7FFFFFFFu) )                                                      | 
      {                                                                                                               | 
        ExRaiseAccessViolation();                                                                                     | 
      }                                                                                                               | 
      if ( (Size[0] & 0x8000000000000000uLL) != 0LL )                                                                 | 
      {                                                                                                               | 
        v42 = v37 + LODWORD(Size[0]) + 1LL;                                                                           | 
        if ( v42 < v37 || v42 > MmUserProbeAddress )                                                                  | 
        {                                                                                                             | 
          ExRaiseAccessViolation();                                                                                   | 
        }                                                                                                             | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        if ( (v37 & 1) != 0 )                                                                                         | 
        {                                                                                                             | 
          ExRaiseDatatypeMisalignment();                                                                              | 
        }                                                                                                             | 
        v38 = v37 + LODWORD(Size[0]) + 2LL;                                                                           | 
        if ( v38 < v37 || v38 > MmUserProbeAddress )                                                                  | 
        {                                                                                                             | 
          ExRaiseAccessViolation();                                                                                   | 
        }                                                                                                             | 
      }                                                                                                               | 
      v39 = Win32AllocPoolWithQuotaZInit(LODWORD(Size[0]) + 2LL, 2020897621LL);                                       | 
      Size[1] = v39;                                                                                                  | 
      if ( !v39 )                                                                                                     | 
      {                                                                                                               | 
        ExRaiseStatus(-1073741801);                                                                                   | 
      }                                                                                                               | 
      if ( v55 != (void (*)(void *))-1LL )                                                                            | 
      {                                                                                                               | 
        BugCheckParameter4 = PtiCurrent();                                                                            | 
        KeBugCheckEx(0x164u, 0x12uLL, (ULONG_PTR)BugCheckParameter2, v39, (ULONG_PTR)BugCheckParameter4);             | 
      }                                                                                                               | 
      v40 = PtiCurrent();                                                                                             | 
      BugCheckParameter2[0] = *((_QWORD *)v40 + 48);                                                                  | 
      *((_QWORD *)v40 + 48) = BugCheckParameter2;                                                                     | 
      BugCheckParameter2[1] = v39;                                                                                    | 
      v55 = Win32FreePool;                                                                                            | 
      memmove((void *)Size[1], (const void *)v37, LODWORD(Size[0]));                                                  | 
      *(_WORD *)(Size[1] + 2 * ((unsigned __int64)LODWORD(Size[0]) >> 1)) = 0;                                        | 
      HIDWORD(Size[0]) = HIDWORD(Size[0]) & 0x80000000 | (LODWORD(Size[0]) + 2) & 0x7FFFFFFF;                         | 
      v30 = (__m128i **)MmUserProbeAddress;                                                                           | 
      v29 = v62;                                                                                                      | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      LODWORD(Size[0]) = 0;                                                                                           | 
    }                                                                                                                 | 
    v19 = (ULONG64)Size;                                                                                              | 
  }                                                                                                                   | 
  if ( ((unsigned __int64)v18 & 0xFFFFFFFFFFFF0000uLL) != 0 )                                                         | 
  {                                                                                                                   | 
    if ( v18 >= *v30 )                                                                                                | 
    {                                                                                                                 | 
      v18 = *v30;                                                                                                     | 
    }                                                                                                                 | 
    v52 = *v18;                                                                                                       | 
    v33 = _mm_srli_si128(v52, 8).m128i_u64[0];                                                                        | 
    if ( v33 )                                                                                                        | 
    {                                                                                                                 | 
      if ( v52.m128i_i32[0] > (v52.m128i_i32[1] & 0x7FFFFFFFu) )                                                      | 
      {                                                                                                               | 
        ExRaiseAccessViolation();                                                                                     | 
      }                                                                                                               | 
      if ( v52.m128i_i32[1] < 0 )                                                                                     | 
      {                                                                                                               | 
        v36 = v33 + v52.m128i_u32[0] + 1LL;                                                                           | 
        if ( v36 < v33 || v36 > (unsigned __int64)*v30 )                                                              | 
        {                                                                                                             | 
          ExRaiseAccessViolation();                                                                                   | 
        }                                                                                                             | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        if ( (v33 & 1) != 0 )                                                                                         | 
        {                                                                                                             | 
          ExRaiseDatatypeMisalignment();                                                                              | 
        }                                                                                                             | 
        v34 = v33 + v52.m128i_u32[0] + 2LL;                                                                           | 
        if ( v34 < v33 || v34 > (unsigned __int64)*v30 )                                                              | 
        {                                                                                                             | 
          ExRaiseAccessViolation();                                                                                   | 
        }                                                                                                             | 
      }                                                                                                               | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      v52.m128i_i32[0] = 0;                                                                                           | 
    }                                                                                                                 | 
    v18 = &v52;                                                                                                       | 
  }                                                                                                                   | 
  if ( v17 )                                                                                                          | 
  {                                                                                                                   | 
    if ( v17 >= *v30 )                                                                                                | 
    {                                                                                                                 | 
      v17 = *v30;                                                                                                     | 
    }                                                                                                                 | 
    v53 = *v17;                                                                                                       | 
    v31 = _mm_srli_si128(v53, 8).m128i_u64[0];                                                                        | 
    if ( v31 )                                                                                                        | 
    {                                                                                                                 | 
      if ( v53.m128i_i32[0] > (v53.m128i_i32[1] & 0x7FFFFFFFu) )                                                      | 
      {                                                                                                               | 
        ExRaiseAccessViolation();                                                                                     | 
      }                                                                                                               | 
      if ( v53.m128i_i32[1] < 0 )                                                                                     | 
      {                                                                                                               | 
        v35 = v31 + v53.m128i_u32[0] + 1LL;                                                                           | 
        if ( v35 < v31 || v35 > (unsigned __int64)*v30 )                                                              | 
        {                                                                                                             | 
          ExRaiseAccessViolation();                                                                                   | 
        }                                                                                                             | 
      }                                                                                                               | 
      else                                                                                                            | 
      {                                                                                                               | 
        if ( (v31 & 1) != 0 )                                                                                         | 
        {                                                                                                             | 
          ExRaiseDatatypeMisalignment();                                                                              | 
        }                                                                                                             | 
        v32 = v31 + v53.m128i_u32[0] + 2LL;                                                                           | 
        if ( v32 < v31 || v32 > (unsigned __int64)*v30 )                                                              | 
        {                                                                                                             | 
          ExRaiseAccessViolation();                                                                                   | 
        }                                                                                                             | 
      }                                                                                                               | 
    }                                                                                                                 | 
    else                                                                                                              | 
    {                                                                                                                 | 
      v53.m128i_i32[0] = 0;                                                                                           | 
    }                                                                                                                 | 
    v17 = &v53;                                                                                                       | 
  }                                                                                                                   | 
  v43 = v49;                                                                                                          | 
  Win32HMOptionalThreadLock<tagWND>::Win32HMOptionalThreadLock<tagWND>(v56, v56[0], v49);                             | 
  Window = (__int64 *)xxxCreateWindowEx(                                                                              | 
                        v29,                                                                                          | 
                        v19,                                                                                          | 
                        (int)v18,                                                                                     | 
                        (int)v17,                                                                                     | 
                        a5,                                                                                           | 
                        a6,                                                                                           | 
                        a7,                                                                                           | 
                        a8,                                                                                           | 
                        a9,                                                                                           | 
                        v43,                                                                                          | 
                        (__int64)v50,                                                                                 | 
                        a12,                                                                                          | 
                        a13,                                                                                          | 
                        a14,                                                                                          | 
                        a15,                                                                                          | 
                        a16,                                                                                          | 
                        a17);                                                                                         | 
  if ( Window )                                                                                                       | 
  {                                                                                                                   | 
    v21 = *Window;                                                                                                    | 
  }                                                                                                                   | 
  Win32HMThreadLock<tagCURSOR>::~Win32HMThreadLock<tagCURSOR>((ULONG_PTR)v56);                                        | 
LABEL_75:                                                                                                             | 
  SmartObjStackRef<tagMENU>::~SmartObjStackRef<tagMENU>(v50);                                                         | 
  Win32RawLockedNtObject<tagDESKTOP>::~Win32RawLockedNtObject<tagDESKTOP>(BugCheckParameter2);                        | 
  Win32HMOptionalThreadLockAlways<tagMENU>::~Win32HMOptionalThreadLockAlways<tagMENU>((ULONG_PTR)BugCheckParameter3); | 
  UserSessionSwitchLeaveCrit();                                                                                       | 
  return v21;                                                                                                         | 
}                                                                                                                     | 
300 row(s)
