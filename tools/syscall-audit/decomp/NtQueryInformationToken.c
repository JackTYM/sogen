NTSTATUS __stdcall NtQueryInformationToken(
        HANDLE TokenHandle,
        TOKEN_INFORMATION_CLASS TokenInformationClass,
        PVOID TokenInformation,
        ULONG TokenInformationLength,
        PULONG ReturnLength)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v8 = (int)TokenHandle;
  v9 = 0LL;
  Object = 0LL;
  v233 = 0;
  v245 = 0LL;
  v254 = 0LL;
  v234 = 0LL;
  SessionId = 0;
  v256 = 0LL;
  v251 = 0LL;
  v252 = 0LL;
  v253 = 0LL;
  LOBYTE(v230) = 0;
  v232 = 0LL;
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    ProbeForWrite(TokenInformation, TokenInformationLength, 4u);
    v11 = 0x7FFFFFFF0000LL;
    v12 = ReturnLength;
    if ( (unsigned __int64)ReturnLength < 0x7FFFFFFF0000LL )
    {
      v11 = (__int64)ReturnLength;
    }
    *(_DWORD *)v11 = *(_DWORD *)v11;
  }
  else
  {
    v12 = ReturnLength;
  }
  switch ( TokenInformationClass )
  {
    case -2:
      if ( !(unsigned int)Feature_2045735225__private_IsEnabledDeviceUsageNoInline() )
      {
        return -1073741821;
      }
      v242 = 0LL;
      v237 = 0LL;
      v238 = 0LL;
      DWORD1(v248) = 0;
      HIDWORD(v249) = 0;
      if ( !SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode) )
      {
        return -1073741821;
      }
      result = SepReferenceTokenByHandle(v8, 8, PreviousMode, v166, &Object, (__int64)&v230, (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 8;
      if ( TokenInformationLength != 8 )
      {
        goto LABEL_172;
      }
      if ( !(unsigned int)((__int64 (*)(void))Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline)() )
      {
        v18 = Object;
        goto LABEL_381;
      }
      LODWORD(Handle) = 0;
      RtlQueryElevationFlags(&Handle);
      v167 = Object;
      if ( ((unsigned __int8)Handle & 0x18) != 0x10
        || (unsigned int)SepGetTokenElevationType(Object) != 2
        || (v168 = v167[27], !*(_QWORD *)(v168 + 24)) )
      {
        v18 = v167;
        goto LABEL_381;
      }
      v169 = SepReferenceLogonSessionSilo(v168 + 24, *(_QWORD *)(v168 + 168), &v242);
      if ( v169 < 0 )
      {
        goto LABEL_255;
      }
      LODWORD(v248) = 48;
      *((_QWORD *)&v248 + 1) = 0LL;
      v170 = 0;
      if ( !PreviousMode )
      {
        v170 = 512;
      }
      DWORD2(v249) = v170;
      *(_QWORD *)&v249 = 0LL;
      v250 = 0LL;
      SeQuerySessionIdToken(v167, &SessionId);
      v171 = v242;
      v169 = SeSetSessionIdToken(*((PACCESS_TOKEN *)v242 + 7), SessionId);
      if ( v169 < 0 )
      {
        SepDeReferenceLogonSessionDirect(v171);
LABEL_255:
        ObfDereferenceObjectWithTag(v167, 0x74726853u);
        return v169;
      }
      v169 = SepDuplicateToken(v171[7], (unsigned int)&v248, 0, 1, 1, 0, 0, (__int64)&v238);
      SepDeReferenceLogonSessionDirect(v171);
      if ( v169 < 0 )
      {
        goto LABEL_255;
      }
      v172 = *((_QWORD *)v238 + 27);
      CurrentServerSilo = PsGetCurrentServerSilo();
      inserted = SepReferenceLogonSessionSilo(v172 + 16, CurrentServerSilo, &IntegritySA);
      if ( inserted < 0
        || (*((_DWORD *)v238 + 50) |= 0x10000000u,
            inserted = ObInsertObjectEx(v238, 0, 0LL, (__int64)&v237),
            inserted < 0) )
      {
        ObfDereferenceObject(v238);
        ObfDereferenceObjectWithTag(v167, 0x74726853u);
        return inserted;
      }
      else
      {
        *(_QWORD *)TokenInformation = v237;
        ObfDereferenceObjectWithTag(v167, 0x74726853u);
        return 0;
      }
    case 1:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        CurrentThread = KeGetCurrentThread();
        --CurrentThread->KernelApcDisable;
        v15 = Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v16 = 4 * *(unsigned __int8 *)(**((_QWORD **)v15 + 19) + 1LL) + 24;
        *v12 = v16;
        if ( TokenInformationLength < v16 )
        {
          goto LABEL_9;
        }
        RtlCopySidAndAttributesArray(
          1,
          *((_QWORD *)v15 + 19),
          v16,
          (int)TokenInformation,
          (char *)TokenInformation + 16,
          (__int64)&v234,
          (__int64)&v234);
        goto LABEL_15;
      }
      return result;
    case 2:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        v20 = KeGetCurrentThread();
        --v20->KernelApcDisable;
        v21 = (unsigned int *)Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v22 = v21[31];
        v23 = 16 * v22 - 8;
        if ( v22 > 1 )
        {
          v24 = *((_QWORD *)v21 + 19) + 16LL;
          v25 = v22 - 1;
          do
          {
            v23 += 4 * *(unsigned __int8 *)(*(_QWORD *)v24 + 1LL) + 8;
            v24 += 16LL;
            --v25;
          }
          while ( v25 );
        }
        *v12 = v23;
        if ( TokenInformationLength >= v23 )
        {
          *(_DWORD *)TokenInformation = v21[31] - 1;
          RtlCopySidAndAttributesArray(
            v21[31] - 1,
            *((_QWORD *)v21 + 19) + 16,
            v23,
            (_DWORD)TokenInformation + 8,
            (char *)TokenInformation + 16 * v21[31] - 8,
            (__int64)&v234,
            (__int64)&v234);
          goto LABEL_26;
        }
        goto LABEL_24;
      }
      return result;
    case 3:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        v31 = KeGetCurrentThread();
        --v31->KernelApcDisable;
        v21 = (unsigned int *)Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v32 = SepTokenPrivilegeCount(v21);
        if ( v32 <= 1 )
        {
          v33 = 16;
        }
        else
        {
          v33 = 12 * v32 + 4;
        }
        *v12 = v33;
        if ( TokenInformationLength >= v33 )
        {
          SepConvertTokenPrivileges(v21, TokenInformation);
          goto LABEL_26;
        }
        goto LABEL_24;
      }
      return result;
    case 4:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        v34 = KeGetCurrentThread();
        --v34->KernelApcDisable;
        v21 = (unsigned int *)Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v35 = 4 * *(unsigned __int8 *)(*(_QWORD *)(*((_QWORD *)v21 + 19) + 16LL * v21[36]) + 1LL) + 16;
        *v12 = v35;
        if ( TokenInformationLength < v35 )
        {
          goto LABEL_24;
        }
        *(_QWORD *)TokenInformation = (char *)TokenInformation + 8;
        RtlCopySid(v35 - 8, (char *)TokenInformation + 8, *(PSID *)(*((_QWORD *)v21 + 19) + 16LL * v21[36]));
        goto LABEL_26;
      }
      return result;
    case 5:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        v36 = KeGetCurrentThread();
        --v36->KernelApcDisable;
        v21 = (unsigned int *)Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v37 = 4 * *(unsigned __int8 *)(*((_QWORD *)v21 + 21) + 1LL) + 16;
        *v12 = v37;
        if ( TokenInformationLength < v37 )
        {
          goto LABEL_24;
        }
        *(_QWORD *)TokenInformation = (char *)TokenInformation + 8;
        RtlCopySid(v37 - 8, (char *)TokenInformation + 8, *((PSID *)v21 + 21));
        goto LABEL_26;
      }
      return result;
    case 6:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        v233 = 8;
        v38 = KeGetCurrentThread();
        --v38->KernelApcDisable;
        v21 = (unsigned int *)Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v39 = *((_QWORD *)v21 + 23);
        if ( v39 )
        {
          v40 = *(unsigned __int16 *)(v39 + 2) + 8;
        }
        else
        {
          v40 = v233;
        }
        *v12 = v40;
        if ( TokenInformationLength >= v40 )
        {
          v41 = (char *)TokenInformation + 8;
          if ( *((_QWORD *)v21 + 23) )
          {
            *(_QWORD *)TokenInformation = v41;
            memmove(v41, *((const void **)v21 + 23), *(unsigned __int16 *)(*((_QWORD *)v21 + 23) + 2LL));
          }
          else
          {
            *(_QWORD *)TokenInformation = 0LL;
          }
          goto LABEL_26;
        }
        goto LABEL_24;
      }
      return result;
    case 7:
      result = SepReferenceTokenByHandle(
                 v8,
                 16,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        *v12 = 16;
        if ( TokenInformationLength < 0x10 )
        {
          goto LABEL_11;
        }
        v18 = Object;
        *(_OWORD *)TokenInformation = *(_OWORD *)Object;
        goto LABEL_18;
      }
      return result;
    case 8:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        *v12 = 4;
        if ( TokenInformationLength < 4 )
        {
          goto LABEL_11;
        }
        v18 = Object;
        *(_DWORD *)TokenInformation = *((_DWORD *)Object + 48);
        goto LABEL_18;
      }
      return result;
    case 9:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        v18 = Object;
        if ( *((_DWORD *)Object + 48) != 2 )
        {
LABEL_381:
          ObfDereferenceObjectWithTag(v18, 0x74726853u);
          return -1073741821;
        }
        *v12 = 4;
        if ( TokenInformationLength < 4 )
        {
          goto LABEL_12;
        }
        *(_DWORD *)TokenInformation = v18[49];
        goto LABEL_18;
      }
      return result;
    case 10:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        *v12 = 56;
        if ( TokenInformationLength < 0x38 )
        {
          goto LABEL_11;
        }
        v43 = KeGetCurrentThread();
        --v43->KernelApcDisable;
        v21 = (unsigned int *)Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v246 = 0;
        *(_QWORD *)TokenInformation = *((_QWORD *)v21 + 2);
        *((_QWORD *)TokenInformation + 1) = *((_QWORD *)v21 + 3);
        *((_QWORD *)TokenInformation + 2) = *((_QWORD *)v21 + 5);
        *((_DWORD *)TokenInformation + 6) = v21[48];
        *((_DWORD *)TokenInformation + 7) = v21[49];
        *((_DWORD *)TokenInformation + 8) = v21[34];
        v44 = v21[34] - 4 * *(unsigned __int8 *)(*((_QWORD *)v21 + 21) + 1LL) - 8;
        v246 = v44;
        v45 = *((_QWORD *)v21 + 23);
        if ( v45 )
        {
          v44 -= *(unsigned __int16 *)(v45 + 2);
          v246 = v44;
        }
        *((_DWORD *)TokenInformation + 9) = v44;
        *((_DWORD *)TokenInformation + 10) = v21[31] - 1;
        *((_DWORD *)TokenInformation + 11) = SepTokenPrivilegeCount(v21);
        *((_QWORD *)TokenInformation + 6) = *((_QWORD *)v21 + 7);
        goto LABEL_26;
      }
      return result;
    case 11:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        v26 = KeGetCurrentThread();
        --v26->KernelApcDisable;
        v21 = (unsigned int *)Object;
        ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
        v27 = v21[32];
        v28 = 16 * v27 + 8;
        if ( v27 )
        {
          v29 = *((_QWORD *)v21 + 20);
          v30 = v27;
          do
          {
            v28 += 4 * *(unsigned __int8 *)(*(_QWORD *)v29 + 1LL) + 8;
            v29 += 16LL;
            --v30;
          }
          while ( v30 );
        }
        *v12 = v28;
        if ( TokenInformationLength >= v28 )
        {
          *(_DWORD *)TokenInformation = v21[32];
          RtlCopySidAndAttributesArray(
            v21[32],
            *((_QWORD *)v21 + 20),
            v28,
            (_DWORD)TokenInformation + 8,
            (char *)TokenInformation + 16 * v21[32] + 8,
            (__int64)&v234,
            (__int64)&v234);
          goto LABEL_26;
        }
        goto LABEL_24;
      }
      return result;
    case 12:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result >= 0 )
      {
        *v12 = 4;
        if ( TokenInformationLength < 4 )
        {
          goto LABEL_11;
        }
        v46 = Object;
        SeQuerySessionIdToken(Object, &SessionId);
        *(_DWORD *)TokenInformation = SessionId;
        *v12 = 4;
        goto LABEL_74;
      }
      return result;
    case 13:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v47 = 0;
      v48 = 0;
      v49 = KeGetCurrentThread();
      --v49->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      LODWORD(v237) = SepTokenPrivilegeCount(v21);
      LODWORD(v238) = 12 * (_DWORD)v237;
      v50 = v21[31];
      LODWORD(v244) = 16 * v50;
      v51 = 16 * v50;
      v52 = 0;
      if ( v50 < 2 )
      {
        goto LABEL_80;
      }
      v232 = *((_QWORD *)v21 + 19);
      v53 = (_QWORD *)v232;
      v54 = ((v50 - 2) >> 1) + 1;
      LODWORD(Handle) = 2 * v54;
      v55 = 2 * v54;
      do
      {
        v47 += (4 * *(unsigned __int8 *)(*v53 + 1LL) + 15) & 0xFFFFFFF8;
        v48 += (4 * *(unsigned __int8 *)(v53[2] + 1LL) + 15) & 0xFFFFFFF8;
        v53 += 4;
        --v54;
      }
      while ( v54 );
      v52 = (unsigned int)Handle;
      if ( (unsigned int)Handle < v50 )
      {
        v56 = v232;
      }
      else
      {
LABEL_80:
        if ( v52 >= v50 )
        {
LABEL_84:
          v57 = 0;
          v58 = 0;
          v59 = v47 + v48 + v51;
          v60 = v21[32];
          LODWORD(v243) = 16 * v60;
          v61 = 16 * v60;
          v62 = 0LL;
          if ( v60 < 2 )
          {
            goto LABEL_88;
          }
          v234 = *((_QWORD *)v21 + 20);
          v63 = (_QWORD *)v234;
          v64 = ((v60 - 2) >> 1) + 1;
          LODWORD(Handle) = 2 * v64;
          v232 = 2 * v64;
          do
          {
            v57 += (4 * *(unsigned __int8 *)(*v63 + 1LL) + 15) & 0xFFFFFFF8;
            v58 += (4 * *(unsigned __int8 *)(v63[2] + 1LL) + 15) & 0xFFFFFFF8;
            v63 += 4;
            --v64;
          }
          while ( v64 );
          v62 = (unsigned int)Handle;
          if ( (unsigned int)Handle < v60 )
          {
            v62 = v232;
            v65 = v234;
          }
          else
          {
LABEL_88:
            if ( (unsigned int)v62 >= v60 )
            {
LABEL_92:
              v66 = v57 + v58 + v61;
              v67 = (int)v238;
              v68 = v59 + v66 + (_DWORD)v238 + 56;
              *v12 = v68;
              if ( TokenInformationLength < v68 )
              {
                goto LABEL_24;
              }
              *((_QWORD *)TokenInformation + 6) = *((_QWORD *)v21 + 3);
              *((_DWORD *)TokenInformation + 1) = v59;
              *(_DWORD *)TokenInformation = v21[31];
              v69 = (char *)TokenInformation + 56;
              *((_QWORD *)TokenInformation + 1) = (char *)TokenInformation + 56;
              *((_DWORD *)TokenInformation + 5) = v66;
              *((_DWORD *)TokenInformation + 4) = v21[32];
              if ( v21[32] )
              {
                v70 = &v69[(v59 + 7) & 0xFFFFFFFFFFFFFFF8uLL];
                *((_QWORD *)TokenInformation + 3) = v70;
              }
              else
              {
                *((_QWORD *)TokenInformation + 3) = 0LL;
                v70 = v245;
              }
              *((_DWORD *)TokenInformation + 9) = v67;
              *((_DWORD *)TokenInformation + 8) = (_DWORD)v237;
              v71 = &v69[(unsigned int)v59 + (unsigned __int64)v66];
              *((_QWORD *)TokenInformation + 5) = v71;
              RtlCopySidAndAttributesArray(
                v21[31],
                *((_QWORD *)v21 + 19),
                v59 - (_DWORD)v244,
                (int)v69,
                &v69[(unsigned int)v244],
                (__int64)&v234,
                (__int64)&v234);
              if ( v70 )
              {
                RtlCopySidAndAttributesArray(
                  v21[32],
                  *((_QWORD *)v21 + 20),
                  v66 - (_DWORD)v243,
                  (int)v70,
                  &v70[(unsigned int)v243],
                  (__int64)&v234,
                  (__int64)&v234);
              }
              SepConvertTokenPrivilegesToLuidAndAttributes(v21, v71);
              goto LABEL_26;
            }
            v65 = *((_QWORD *)v21 + 20);
          }
          v61 = (_DWORD)v243 + ((4 * *(unsigned __int8 *)(*(_QWORD *)(v65 + 16 * v62) + 1LL) + 15) & 0xFFFFFFF8);
          goto LABEL_92;
        }
        v56 = *((_QWORD *)v21 + 19);
        v55 = v52;
      }
      v51 = (_DWORD)v244 + ((4 * *(unsigned __int8 *)(*(_QWORD *)(v56 + 16 * v55) + 1LL) + 15) & 0xFFFFFFF8);
      goto LABEL_84;
    case 15:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 4;
      if ( TokenInformationLength < 4 )
      {
        return -1073741789;
      }
      v18 = Object;
      *(_DWORD *)TokenInformation = (*((_DWORD *)Object + 50) >> 6) & 1;
      goto LABEL_18;
    case 16:
      if ( !SeSinglePrivilegeCheck(SeSecurityPrivilege, PreviousMode) )
      {
        return -1073741727;
      }
      result = SepReferenceTokenByHandle(v8, 8, PreviousMode, v127, &Object, (__int64)&v230, (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 31;
      if ( TokenInformationLength < 0x1F )
      {
        goto LABEL_11;
      }
      v128 = KeGetCurrentThread();
      --v128->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      *(_OWORD *)TokenInformation = *(_OWORD *)(v21 + 22);
      *((_QWORD *)TokenInformation + 2) = *((_QWORD *)v21 + 13);
      *((_DWORD *)TokenInformation + 6) = v21[28];
      *((_WORD *)TokenInformation + 14) = *((_WORD *)v21 + 58);
      *((_BYTE *)TokenInformation + 30) = *((_BYTE *)v21 + 118);
LABEL_26:
      v19 = (struct _ERESOURCE *)*((_QWORD *)v21 + 6);
      goto LABEL_16;
    case 17:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 8;
      if ( TokenInformationLength < 8 )
      {
        goto LABEL_11;
      }
      v18 = Object;
      *(_QWORD *)TokenInformation = *((_QWORD *)Object + 28);
      goto LABEL_18;
    case 18:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 4;
      if ( TokenInformationLength < 4 )
      {
        goto LABEL_11;
      }
      *(_DWORD *)TokenInformation = SepGetTokenElevationType(Object);
      ObfDereferenceObjectWithTag(v42, 0x74726853u);
      return 0;
    case 19:
      v237 = 0LL;
      v242 = 0LL;
      v243 = 0LL;
      v248 = 0LL;
      v249 = 0LL;
      *(_QWORD *)&v250 = 0LL;
      DWORD2(v250) = 0;
      v235 = 0;
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 8;
      if ( TokenInformationLength != 8 )
      {
LABEL_172:
        ObfDereferenceObjectWithTag(Object, 0x74726853u);
        return -1073741820;
      }
      if ( (unsigned int)((__int64 (*)(void))Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline)() )
      {
        LODWORD(Handle) = 0;
        RtlQueryElevationFlags(&Handle);
        v129 = ((unsigned __int8)Handle & 0x18) == 16;
        v235 = v129;
      }
      else
      {
        v129 = 0;
      }
      LOBYTE(v230) = SeSinglePrivilegeCheck(SeTcbPrivilege, PreviousMode);
      LODWORD(v238) = 2 - ((_BYTE)v230 != 0);
      IsEnabledDeviceUsageNoInline = Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline((unsigned __int8)-(char)v230);
      v131 = Object;
      if ( IsEnabledDeviceUsageNoInline )
      {
        if ( v129 && (unsigned int)SepGetTokenElevationType(Object) == 3 )
        {
          v133 = *((_QWORD *)v131 + 27);
          if ( !*(_QWORD *)(v133 + 16) )
          {
            Handle = 0LL;
            v134 = *(_QWORD *)(v133 + 8);
            v135 = -1073741822;
            if ( (unsigned int)Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline(v132)
              && ExGetExtensionTable(SepAuthExtensionHost) )
            {
              v135 = guard_dispatch_icall_no_overrides(v134, &Handle);
              ExReleaseExtensionTable(SepAuthExtensionHost);
            }
            if ( v135 < 0 )
            {
              ObfDereferenceObjectWithTag(v131, 0x74726853u);
              return v135;
            }
            v244 = 0LL;
            v136 = ObReferenceObjectByHandle(Handle, 8u, (POBJECT_TYPE)SeTokenObjectType, 0, &v244, 0LL);
            ObCloseHandle(Handle, 0);
            if ( v136 < 0 )
            {
              goto LABEL_194;
            }
            v137 = v244;
            v138 = SepReferenceLogonSessionSilo((char *)v244 + 24, *(_QWORD *)(*((_QWORD *)v131 + 27) + 168LL), &v237);
            ObfDereferenceObject(v137);
            if ( v138 < 0 )
            {
              ObfDereferenceObjectWithTag(v131, 0x74726853u);
              return v138;
            }
            SeQuerySessionIdToken(v131, &SessionId);
            v139 = v237;
            v136 = SeSetSessionIdToken(*((PACCESS_TOKEN *)v237 + 7), SessionId);
            if ( v136 < 0
              || (v139[2] = *(_QWORD *)(*((_QWORD *)v131 + 27) + 8LL),
                  v140 = *((_QWORD *)v131 + 27),
                  v141 = PsGetCurrentServerSilo(),
                  v136 = SepReferenceLogonSessionSilo(v140 + 8, v141, &IntegritySA),
                  v136 < 0) )
            {
              SepDeReferenceLogonSessionDirect(v139);
              goto LABEL_194;
            }
            goto LABEL_197;
          }
        }
        v136 = SepReferenceLogonSessionSilo(
                 *((_QWORD *)v131 + 27) + 16LL,
                 *(_QWORD *)(*((_QWORD *)v131 + 27) + 168LL),
                 &v237);
        if ( v136 < 0 )
        {
          goto LABEL_194;
        }
      }
      else
      {
        v136 = SepReferenceLogonSessionSilo(
                 *((_QWORD *)Object + 27) + 16LL,
                 *(_QWORD *)(*((_QWORD *)Object + 27) + 168LL),
                 &v237);
        if ( v136 < 0 )
        {
          goto LABEL_194;
        }
      }
      v139 = v237;
LABEL_197:
      v143 = 0;
      if ( (unsigned int)Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline(v142) && v235 && !(_BYTE)v230 )
      {
        v144 = 0;
        v145 = *(_QWORD *)(*((_QWORD *)v131 + 27) + 56LL);
        if ( v145 )
        {
          v146 = KeGetCurrentThread();
          --v146->KernelApcDisable;
          ExAcquireResourceSharedLite(*(PERESOURCE *)(v145 + 48), 1u);
          LODWORD(v234) = *(_DWORD *)(v145 + 200);
          ExReleaseResourceLite(*(PERESOURCE *)(v145 + 48));
          KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
          v131 = Object;
          v139 = v237;
          v144 = v234;
        }
        v143 = 0;
        if ( (v144 & 0x8000000) != 0 )
        {
          LODWORD(Handle) = 0;
          memset(&SubjectContext, 0, sizeof(SubjectContext));
          SeCaptureSubjectContext(&SubjectContext);
          PrimaryToken = (PSID **)SubjectContext.PrimaryToken;
          v148 = *(_QWORD *)(*((_QWORD *)SubjectContext.PrimaryToken + 27) + 56LL);
          if ( v148 )
          {
            v149 = KeGetCurrentThread();
            --v149->KernelApcDisable;
            ExAcquireResourceSharedLite(*(PERESOURCE *)(v148 + 48), 1u);
            LODWORD(Handle) = *(_DWORD *)(v148 + 200);
            ExReleaseResourceLite(*(PERESOURCE *)(v148 + 48));
            KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
            v131 = Object;
            v139 = v237;
            PrimaryToken = (PSID **)SubjectContext.PrimaryToken;
          }
          v143 = 0;
          if ( ((unsigned int)Handle & 0x8000000) == 0
            || !RtlEqualSid(**((PSID **)v131 + 19), *PrimaryToken[19])
            || (IntegritySA = 0LL, SeQueryTokenIntegrity(PrimaryToken, &IntegritySA), IntegritySA.Attributes != 96)
            || (Sid = IntegritySA.Sid, *RtlSubAuthorityCountSid(IntegritySA.Sid) != 1)
            || *RtlSubAuthoritySid(Sid, 0) != 12288 )
          {
LABEL_232:
            v164 = 1;
            SeReleaseSubjectContext(&SubjectContext);
            goto LABEL_235;
          }
          Process = (unsigned __int64)KeGetCurrentThread()->ApcState.Process;
          v153 = *(_QWORD **)(Process + 848);
          IntegritySA.Sid = v153;
          v154 = 0;
          v155 = L"02";
          v156 = 192LL;
          while ( 1 )
          {
            v157 = *(_QWORD *)(PsGetCurrentServerSiloGlobals(Process, v151, v153, v156) + 1200);
            v158 = *v155;
            Process = *(unsigned __int16 *)v153;
            if ( (unsigned __int16)Process >= (unsigned __int16)v158 )
            {
              v159 = (unsigned __int16 *)*((_QWORD *)v155 + 1);
              v151 = *v155;
              v160 = &v159[v158 >> 1];
              if ( v159 >= v160 )
              {
LABEL_233:
                v164 = 1;
                v143 = 1;
                v131 = Object;
                v139 = v237;
                SeReleaseSubjectContext(&SubjectContext);
                goto LABEL_235;
              }
              Process = (Process - v151) >> 1;
              v161 = v153[1] + 2 * Process - (_QWORD)v159;
              while ( 1 )
              {
                v162 = *v159;
                if ( (unsigned int)v162 >= 0x61 )
                {
                  if ( (unsigned int)v162 > 0x7A )
                  {
                    if ( v157 && (unsigned __int16)v162 >= (unsigned __int16)v156 )
                    {
                      v151 = v162 & 0xF;
                      Process = (unsigned int)v151
                              + *(unsigned __int16 *)(v157
                                                    + 2LL
                                                    * (((unsigned __int8)v162 >> 4)
                                                     + (unsigned int)*(unsigned __int16 *)(v157 + 2 * (v162 >> 8))));
                      LOWORD(v162) = *(_WORD *)(v157 + 2 * Process) + v162;
                    }
                  }
                  else
                  {
                    LOWORD(v162) = v162 - 32;
                  }
                }
                v163 = *(unsigned __int16 *)((char *)v159 + v161);
                if ( (unsigned int)v163 >= 0x61 )
                {
                  if ( (unsigned int)v163 > 0x7A )
                  {
                    if ( v157 && (unsigned __int16)v163 >= 0xC0u )
                    {
                      v151 = v163 & 0xF;
                      Process = (unsigned int)v151
                              + *(unsigned __int16 *)(v157
                                                    + 2LL
                                                    * (((unsigned __int8)v163 >> 4)
                                                     + (unsigned int)*(unsigned __int16 *)(v157 + 2 * (v163 >> 8))));
                      LOWORD(v163) = *(_WORD *)(v157 + 2 * Process) + v163;
                    }
                  }
                  else
                  {
                    LOWORD(v163) = v163 - 32;
                  }
                }
                if ( (_WORD)v162 != (_WORD)v163 )
                {
                  break;
                }
                if ( ++v159 >= v160 )
                {
                  goto LABEL_233;
                }
                LOWORD(v156) = 192;
              }
              v153 = IntegritySA.Sid;
              v156 = 192LL;
            }
            ++v154;
            v155 += 8;
            if ( v154 )
            {
              v143 = 0;
              v131 = Object;
              v139 = v237;
              goto LABEL_232;
            }
          }
        }
      }
      v164 = 1;
LABEL_235:
      LODWORD(v248) = 48;
      *((_QWORD *)&v248 + 1) = 0LL;
      v165 = 0;
      if ( !PreviousMode )
      {
        v165 = 512;
      }
      DWORD2(v249) = v165;
      *(_QWORD *)&v249 = 0LL;
      v250 = 0LL;
      if ( (unsigned int)Feature_ShadowAdmin__private_IsEnabledDeviceUsageNoInline(512LL) )
      {
        v164 = v143 + 1;
      }
      v136 = SepDuplicateToken(v139[7], (unsigned int)&v248, 0, (_DWORD)v238, v164, 0, 0, (__int64)&v243);
      SepDeReferenceLogonSessionDirect(v139);
      if ( v136 >= 0 )
      {
        v136 = ObInsertObjectEx(v243, 0, 0LL, (__int64)&v242);
        if ( v136 >= 0 )
        {
          *(_QWORD *)TokenInformation = v242;
          ObfDereferenceObjectWithTag(v131, 0x74726853u);
          return 0;
        }
      }
LABEL_194:
      ObfDereferenceObjectWithTag(v131, 0x74726853u);
      return v136;
    case 20:
      *v12 = 4;
      if ( TokenInformationLength != 4 )
      {
        return -1073741820;
      }
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v175 = KeGetCurrentThread();
      --v175->KernelApcDisable;
      v176 = (PERESOURCE *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      TokenElevation = SepGetTokenElevation(v176);
      ExReleaseResourceLite(v176[6]);
      KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
      *(_DWORD *)TokenInformation = TokenElevation;
      goto LABEL_17;
    case 21:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 1;
      if ( !TokenInformationLength )
      {
        goto LABEL_11;
      }
      v18 = Object;
      *(_BYTE *)TokenInformation = (*((_DWORD *)Object + 50) & 0x810) != 0;
      goto LABEL_18;
    case 22:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v72 = KeGetCurrentThread();
      --v72->KernelApcDisable;
      v15 = Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v73 = 0;
      v74 = 1LL;
      v75 = 37LL;
      do
      {
        v76 = v73 + 1;
        if ( (v74 & *((_QWORD *)v15 + 8)) == 0 )
        {
          v76 = v73;
        }
        v73 = v76;
        v74 = __ROL8__(v74, 1);
        --v75;
      }
      while ( v75 );
      v77 = 12 * v76 + 16;
      v78 = v77 - 12;
      if ( !v73 )
      {
        v78 = v77;
      }
      LODWORD(v234) = v78;
      v79 = 0;
      v80 = 0;
      v81 = *((_DWORD *)v15 + 31);
      v82 = 16 * v81;
      LODWORD(Handle) = 16 * v81;
      v241 = *((_DWORD *)v15 + 32);
      LODWORD(v244) = 16 * v241;
      LODWORD(v242) = *((_DWORD *)v15 + 200);
      LODWORD(v243) = 16 * (_DWORD)v242;
      LODWORD(v245) = 16 * v81;
      v83 = 0;
      if ( v81 < 2 )
      {
        goto LABEL_111;
      }
      v237 = (HANDLE)*((_QWORD *)v15 + 19);
      v84 = v237;
      v85 = ((v81 - 2) >> 1) + 1;
      LODWORD(v238) = 2 * v85;
      v86 = 2 * v85;
      do
      {
        v79 += (4 * *(unsigned __int8 *)(*v84 + 1LL) + 15) & 0xFFFFFFF8;
        v80 += (4 * *(unsigned __int8 *)(v84[2] + 1LL) + 15) & 0xFFFFFFF8;
        v84 += 4;
        --v85;
      }
      while ( v85 );
      v83 = (unsigned int)v238;
      if ( (unsigned int)v238 < v81 )
      {
        v87 = v237;
        goto LABEL_114;
      }
      v82 = (unsigned int)v245;
LABEL_111:
      if ( v83 < v81 )
      {
        v87 = (_QWORD *)*((_QWORD *)v15 + 19);
        v86 = v83;
LABEL_114:
        v82 = (_DWORD)Handle + ((4 * *(unsigned __int8 *)(v87[2 * v86] + 1LL) + 15) & 0xFFFFFFF8);
      }
      v88 = 0;
      v89 = 0;
      LODWORD(v245) = v79 + v80 + v82;
      v90 = (unsigned int)v244;
      v233 = (int)v244;
      v91 = 0;
      v92 = v241;
      if ( v241 < 2 )
      {
        goto LABEL_120;
      }
      v238 = (PVOID)*((_QWORD *)v15 + 20);
      v93 = v238;
      v94 = ((v241 - 2) >> 1) + 1;
      v95 = v94;
      v91 = 2 * v94;
      v96 = 2LL * v94;
      do
      {
        v88 += (4 * *(unsigned __int8 *)(*v93 + 1LL) + 15) & 0xFFFFFFF8;
        v89 += (4 * *(unsigned __int8 *)(v93[2] + 1LL) + 15) & 0xFFFFFFF8;
        v93 += 4;
        --v95;
      }
      while ( v95 );
      v92 = v241;
      if ( v91 < v241 )
      {
        v97 = v238;
        goto LABEL_123;
      }
      v90 = v233;
LABEL_120:
      if ( v91 < v92 )
      {
        v97 = (_QWORD *)*((_QWORD *)v15 + 20);
        v96 = v91;
LABEL_123:
        v90 = (_DWORD)v244 + ((4 * *(unsigned __int8 *)(v97[2 * v96] + 1LL) + 15) & 0xFFFFFFF8);
      }
      v98 = 0;
      v99 = 0;
      v233 = v88 + v89 + v90;
      v100 = (unsigned int)v243;
      SessionId = (unsigned int)v243;
      v101 = 0;
      v102 = (unsigned int)v242;
      if ( (unsigned int)v242 < 2 )
      {
        goto LABEL_129;
      }
      v238 = (PVOID)*((_QWORD *)v15 + 99);
      v103 = v238;
      v104 = ((unsigned int)((_DWORD)v242 - 2) >> 1) + 1;
      v105 = v104;
      v101 = 2 * v104;
      v106 = 2LL * v104;
      do
      {
        v98 += (4 * *(unsigned __int8 *)(*v103 + 1LL) + 15) & 0xFFFFFFF8;
        v99 += (4 * *(unsigned __int8 *)(v103[2] + 1LL) + 15) & 0xFFFFFFF8;
        v103 += 4;
        --v105;
      }
      while ( v105 );
      v102 = (unsigned int)v242;
      if ( v101 < (unsigned int)v242 )
      {
        v107 = v238;
        goto LABEL_132;
      }
      v100 = SessionId;
LABEL_129:
      if ( v101 < v102 )
      {
        v107 = (_QWORD *)*((_QWORD *)v15 + 99);
        v106 = v101;
LABEL_132:
        v100 = (_DWORD)v243 + ((4 * *(unsigned __int8 *)(v107[2 * v106] + 1LL) + 15) & 0xFFFFFFF8);
      }
      SessionId = v98 + v99 + v100;
      LODWORD(v238) = 0;
      v108 = *((_QWORD *)v15 + 98);
      if ( v108 )
      {
        LODWORD(v238) = (4 * *(unsigned __int8 *)(v108 + 1) + 15) & 0xFFFFFFF8;
      }
      v109 = v230;
      v110 = v232;
      if ( !(_BYTE)v230 )
      {
        v110 = *((_QWORD *)v15 + 138);
      }
      LODWORD(v237) = 0;
      if ( v110 )
      {
        LODWORD(v237) = (4 * *(unsigned __int8 *)(v110 + 1) + 15) & 0xFFFFFFF8;
      }
      v111 = (unsigned int *)*((_QWORD *)v15 + 97);
      v112 = 0LL;
      LODWORD(v242) = 0;
      if ( !v111 )
      {
        goto LABEL_157;
      }
      v113 = 112LL * *v111;
      if ( v113 > 0xFFFFFFFF )
      {
        LODWORD(v114) = -1073741675;
        goto LABEL_154;
      }
      v115 = v113 + 48;
      v116 = -1;
      if ( v115 >= 0x30 )
      {
        v116 = v115;
      }
      LODWORD(v114) = v115 < 0x30 ? 0xC0000095 : 0;
      if ( v115 < 0x30 )
      {
        goto LABEL_154;
      }
      IntegritySA.Sid = v111 + 2;
      v117 = (void **)*((_QWORD *)v111 + 1);
      if ( v117 == (void **)(v111 + 2) )
      {
LABEL_152:
        LODWORD(v112) = v116;
LABEL_154:
        v109 = v230;
        if ( (int)v114 >= 0 )
        {
          LODWORD(v9) = v112;
        }
        goto LABEL_157;
      }
      while ( 1 )
      {
        v118 = (v116 + 1) & 0xFFFFFFFE;
        if ( v118 < v116 )
        {
          break;
        }
        v119 = v118 + *((unsigned __int16 *)v117 + 16);
        v120 = -1;
        if ( v119 >= v118 )
        {
          v120 = v118 + *((unsigned __int16 *)v117 + 16);
        }
        v241 = v120;
        v114 = v119 < v118 ? 0xC0000095 : 0;
        if ( v119 < v118
          || (LODWORD(v114) = AuthzBasepGetInternalSecurityAttributeValueCopyoutBufferSize(v117, &v241, v114, v112),
              (int)v114 < 0) )
        {
          LODWORD(v112) = (_DWORD)v242;
          goto LABEL_154;
        }
        v117 = (void **)*v117;
        v116 = v241;
        if ( v117 == IntegritySA.Sid )
        {
          goto LABEL_152;
        }
      }
      v109 = v230;
LABEL_157:
      v121 = (int)v237;
      v122 = SessionId;
      v123 = (int)v238;
      v124 = v233;
      v125 = (int)v245;
      v126 = v233 + (_DWORD)v238 + SessionId + (_DWORD)v237 + v9 + v234 + (_DWORD)v245 + 904;
      *v12 = v126;
      if ( TokenInformationLength < v126 )
      {
LABEL_9:
        v17 = (struct _ERESOURCE *)*((_QWORD *)v15 + 6);
LABEL_10:
        ExReleaseResourceLite(v17);
        KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
LABEL_11:
        v18 = Object;
LABEL_12:
        ObfDereferenceObjectWithTag(v18, 0x74726853u);
        return -1073741789;
      }
      else
      {
        SepCopyTokenAccessInformation(
          (_DWORD)v15,
          (_DWORD)TokenInformation,
          TokenInformationLength,
          v73,
          v125,
          (_DWORD)Handle,
          v124,
          (_DWORD)v244,
          v123,
          v122,
          (_DWORD)v243,
          v121,
          v9,
          v109,
          v232);
LABEL_15:
        v19 = (struct _ERESOURCE *)*((_QWORD *)v15 + 6);
LABEL_16:
        ExReleaseResourceLite(v19);
        KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
LABEL_17:
        v18 = Object;
LABEL_18:
        ObfDereferenceObjectWithTag(v18, 0x74726853u);
        return 0;
      }
    case 23:
    case 24:
    case 26:
    case 29:
    case 40:
    case 42:
    case 47:
    case 48:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 4;
      if ( TokenInformationLength < 4 )
      {
        goto LABEL_11;
      }
      v46 = Object;
      switch ( TokenInformationClass )
      {
        case TokenVirtualizationAllowed:
          v178 = *((_DWORD *)Object + 50) >> 9;
          break;
        case TokenVirtualizationEnabled:
          v178 = *((_DWORD *)Object + 50) >> 10;
          break;
        case TokenUIAccess:
          v178 = *((_DWORD *)Object + 50) >> 12;
          break;
        case TokenIsRestricted:
          LOBYTE(v9) = (*((_DWORD *)Object + 50) & 0x18) != 0;
          goto LABEL_287;
        case TokenPrivateNameSpace:
          LOWORD(v178) = *((_WORD *)Object + 101);
          break;
        case MaxTokenInfoClass:
          LODWORD(v9) = (unsigned __int8)SepIsSandboxedToken(Object, PreviousMode);
          goto LABEL_287;
        case TokenAppContainerNumber|TokenAuditPolicy:
          LOBYTE(HandleInformation) = 0;
          LOBYTE(v227) = 1;
          LODWORD(v9) = (unsigned __int8)SepSidInTokenSidHash(
                                           (char *)Object + 808,
                                           0LL,
                                           SeAppSiloSid,
                                           0LL,
                                           v227,
                                           HandleInformation);
          goto LABEL_287;
        default:
          v178 = *((_DWORD *)Object + 50) >> 14;
          break;
      }
      LODWORD(v9) = v178 & 1;
LABEL_287:
      *(_DWORD *)TokenInformation = v9;
LABEL_74:
      ObfDereferenceObjectWithTag(v46, 0x74726853u);
      return 0;
    case 25:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v190 = KeGetCurrentThread();
      --v190->KernelApcDisable;
      v15 = Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      SepCopyTokenIntegrity(v15, &v254);
      v191 = v254;
      v192 = 4 * *(unsigned __int8 *)(v254 + 1) + 24;
      *v12 = v192;
      if ( TokenInformationLength < v192 )
      {
        goto LABEL_9;
      }
      RtlCopySidAndAttributesArray(
        1,
        (int)&v254,
        4 * *(unsigned __int8 *)(v191 + 1) + 8,
        (int)TokenInformation,
        (char *)TokenInformation + 16,
        (__int64)&v234,
        (__int64)&v234);
      goto LABEL_15;
    case 27:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 4;
      if ( TokenInformationLength < 4 )
      {
        goto LABEL_11;
      }
      MandatoryPolicyToken = SeQueryMandatoryPolicyToken(Object, TokenInformation);
      SecurityAttributesToken = MandatoryPolicyToken;
      ObfDereferenceObjectWithTag(v194, 0x74726853u);
      return MandatoryPolicyToken;
    case 28:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v195 = KeGetCurrentThread();
      --v195->KernelApcDisable;
      v15 = Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v196 = 0LL;
      v197 = *((_DWORD *)v15 + 31);
      if ( !v197 )
      {
        goto LABEL_321;
      }
      v198 = *((_QWORD *)v15 + 19);
      while ( (*(_DWORD *)(v198 + 16LL * (unsigned int)v196 + 8) & 0xC0000000) != 0xC0000000 )
      {
        v196 = (unsigned int)(v196 + 1);
        if ( (unsigned int)v196 >= v197 )
        {
          goto LABEL_321;
        }
      }
      v9 = *(_QWORD *)(v198 + 16LL * (unsigned int)v196);
LABEL_321:
      if ( !v9 )
      {
        ExReleaseResourceLite(*((PERESOURCE *)v15 + 6));
        KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
        ObfDereferenceObjectWithTag(Object, 0x74726853u);
        return -1073741275;
      }
      v199 = 4 * *(unsigned __int8 *)(*(_QWORD *)(*((_QWORD *)v15 + 19) + 16LL * (unsigned int)v196) + 1LL) + 32;
      *v12 = v199;
      if ( TokenInformationLength < v199 )
      {
        goto LABEL_9;
      }
      *(_DWORD *)TokenInformation = 1;
      RtlCopySidAndAttributesArray(
        1,
        *((_DWORD *)v15 + 38) + 16 * v196,
        4 * *(unsigned __int8 *)(*(_QWORD *)(*((_QWORD *)v15 + 19) + 16 * v196) + 1LL) + 8,
        (_DWORD)TokenInformation + 8,
        (char *)TokenInformation + 24,
        (__int64)&v234,
        (__int64)&v234);
      goto LABEL_15;
    case 30:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v185 = KeGetCurrentThread();
      --v185->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v186 = v21[200];
      v187 = 16 * v186 + 8;
      if ( v186 )
      {
        v188 = *((_QWORD *)v21 + 99);
        v189 = v186;
        do
        {
          v187 += 4 * *(unsigned __int8 *)(*(_QWORD *)v188 + 1LL) + 8;
          v188 += 16LL;
          --v189;
        }
        while ( v189 );
      }
      *v12 = v187;
      if ( TokenInformationLength < v187 )
      {
        goto LABEL_24;
      }
      *(_DWORD *)TokenInformation = v21[200];
      RtlCopySidAndAttributesArray(
        v21[200],
        *((_QWORD *)v21 + 99),
        v187,
        (_DWORD)TokenInformation + 8,
        (char *)TokenInformation + 16 * v21[200] + 8,
        (__int64)&v234,
        (__int64)&v234);
      goto LABEL_26;
    case 31:
      v182 = 8;
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v183 = KeGetCurrentThread();
      --v183->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v184 = (unsigned __int8 *)*((_QWORD *)v21 + 98);
      if ( v184 )
      {
        v182 = 4 * v184[1] + 16;
      }
      *v12 = v182;
      if ( TokenInformationLength < v182 )
      {
        goto LABEL_24;
      }
      if ( v184 )
      {
        v184 = (unsigned __int8 *)TokenInformation + 8;
      }
      *(_QWORD *)TokenInformation = v184;
      if ( v184 )
      {
        RtlCopySid(v182 - 8, v184, *((PSID *)v21 + 98));
      }
      goto LABEL_26;
    case 32:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      *v12 = 4;
      if ( TokenInformationLength < 4 )
      {
        goto LABEL_11;
      }
      v179 = KeGetCurrentThread();
      --v179->KernelApcDisable;
      v180 = Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v181 = *((_QWORD *)v180 + 135);
      if ( v181 )
      {
        LODWORD(v9) = *(_DWORD *)(v181 + 40);
      }
      ExReleaseResourceLite(*((PERESOURCE *)v180 + 6));
      KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
      *(_DWORD *)TokenInformation = v9;
      goto LABEL_17;
    case 33:
    case 34:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v200 = KeGetCurrentThread();
      --v200->KernelApcDisable;
      v201 = Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v202 = *((_QWORD *)v201 + 137);
      if ( v202
        && ((v203 = *(__int128 **)(v202 + 576)) != 0LL || TokenInformationClass != TokenUserClaimAttributes)
        && ((v204 = *(__int128 **)(v202 + 584)) != 0LL || TokenInformationClass != TokenDeviceClaimAttributes) )
      {
        if ( TokenInformationClass == TokenUserClaimAttributes )
        {
          v204 = v203;
        }
        AuthzBasepQueryClaimAttributesToken(v204, 0LL, 0LL, &v233);
        v205 = v233;
      }
      else
      {
        v205 = 16;
        *(_QWORD *)&v252 = (char *)&v251 + 8;
        *((_QWORD *)&v251 + 1) = (char *)&v251 + 8;
        DWORD2(v252) = 0;
        *((_QWORD *)&v253 + 1) = &v253;
        *(_QWORD *)&v253 = &v253;
        v204 = &v251;
      }
      *v12 = v205;
      if ( TokenInformationLength < v205 )
      {
        v17 = (struct _ERESOURCE *)*((_QWORD *)v201 + 6);
        goto LABEL_10;
      }
      SecurityAttributesToken = AuthzBasepQueryClaimAttributesToken(
                                  v204,
                                  TokenInformation,
                                  TokenInformationLength,
                                  &v233);
      v206 = (struct _ERESOURCE *)*((_QWORD *)v201 + 6);
LABEL_338:
      ExReleaseResourceLite(v206);
      KiLeaveCriticalRegionUnsafe(KeGetCurrentThread());
LABEL_339:
      ObfDereferenceObjectWithTag(Object, 0x74726853u);
      return SecurityAttributesToken;
    case 37:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v207 = KeGetCurrentThread();
      --v207->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v208 = *((_QWORD *)v21 + 137);
      if ( v208 && (v209 = *(_DWORD *)v208) != 0 )
      {
        v210 = 16 * v209 + 8;
      }
      else
      {
        v209 = 0;
        v210 = 24;
      }
      if ( v209 )
      {
        v211 = *(_QWORD *)(v208 + 8);
        v212 = v209;
        do
        {
          v210 += 4 * *(unsigned __int8 *)(*(_QWORD *)v211 + 1LL) + 8;
          v211 += 16LL;
          --v212;
        }
        while ( v212 );
      }
      *v12 = v210;
      if ( TokenInformationLength < v210 )
      {
        goto LABEL_24;
      }
      *(_OWORD *)TokenInformation = 0LL;
      *((_QWORD *)TokenInformation + 2) = 0LL;
      *(_DWORD *)TokenInformation = v209;
      if ( v209 )
      {
        RtlCopySidAndAttributesArray(
          v209,
          *(_QWORD *)(*((_QWORD *)v21 + 137) + 8LL),
          v210,
          (_DWORD)TokenInformation + 8,
          (char *)TokenInformation + 16 * v209 + 8,
          (__int64)&v234,
          (__int64)&v234);
      }
      goto LABEL_26;
    case 39:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v213 = KeGetCurrentThread();
      --v213->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v214 = *((_QWORD *)v21 + 97);
      if ( v214 )
      {
        AuthzBasepQuerySecurityAttributesToken(v214, 0LL, 0LL, 0LL, 0, &v233);
        v215 = v233;
      }
      else
      {
        v215 = 16;
      }
      *v12 = v215;
      if ( TokenInformationLength < v215 )
      {
        goto LABEL_24;
      }
      SecurityAttributesToken = AuthzBasepQuerySecurityAttributesToken(
                                  *((_QWORD *)v21 + 97),
                                  0LL,
                                  0LL,
                                  TokenInformation,
                                  TokenInformationLength,
                                  &v233);
      v206 = (struct _ERESOURCE *)*((_QWORD *)v21 + 6);
      goto LABEL_338;
    case 41:
      v216 = 8;
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v217 = KeGetCurrentThread();
      --v217->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v218 = v230;
      v219 = (void *)v232;
      v220 = (unsigned __int8 *)v232;
      if ( !(_BYTE)v230 )
      {
        v220 = (unsigned __int8 *)*((_QWORD *)v21 + 138);
      }
      if ( v220 )
      {
        v216 = 4 * v220[1] + 16;
      }
      *v12 = v216;
      if ( TokenInformationLength < v216 )
      {
LABEL_24:
        v17 = (struct _ERESOURCE *)*((_QWORD *)v21 + 6);
        goto LABEL_10;
      }
      if ( v220 )
      {
        v220 = (unsigned __int8 *)TokenInformation + 8;
      }
      *(_QWORD *)TokenInformation = v220;
      if ( v220 )
      {
        if ( !v218 )
        {
          v219 = (void *)*((_QWORD *)v21 + 138);
        }
        RtlCopySid(v216 - 8, v220, v219);
      }
      goto LABEL_370;
    case 43:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v221 = KeGetCurrentThread();
      --v221->KernelApcDisable;
      v222 = (PERESOURCE *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      LODWORD(Size) = TokenInformationLength;
      SecurityAttributesToken = SepInternalQuerySecurityAttributesTokenEx(
                                  (int)v222,
                                  v223,
                                  0,
                                  0,
                                  1,
                                  TokenInformation,
                                  Size,
                                  (__int64)v12);
      ExReleaseResourceLite(v222[6]);
      KeLeaveCriticalRegionThread(KeGetCurrentThread());
      goto LABEL_339;
    case 44:
      result = SepReferenceTokenByHandle(
                 v8,
                 8,
                 PreviousMode,
                 TokenInformationLength,
                 &Object,
                 (__int64)&v230,
                 (__int64)&v232);
      if ( result < 0 )
      {
        return result;
      }
      v224 = KeGetCurrentThread();
      --v224->KernelApcDisable;
      v21 = (unsigned int *)Object;
      ExAcquireResourceSharedLite(*((PERESOURCE *)Object + 6), 1u);
      v225 = 16;
      v226 = *((_QWORD *)v21 + 144);
      if ( v226 )
      {
        v225 = *(unsigned __int16 *)(v226 + 42) + 16;
      }
      *v12 = v225;
      if ( TokenInformationLength < v225 )
      {
        ExReleaseResourceLite(*((PERESOURCE *)v21 + 6));
        KeLeaveCriticalRegionThread(KeGetCurrentThread());
        goto LABEL_11;
      }
      if ( *((_QWORD *)v21 + 144) )
      {
        *((_BYTE *)TokenInformation + 8) = 1;
        *(_QWORD *)TokenInformation = (char *)TokenInformation + 16;
        memmove(
          (char *)TokenInformation + 16,
          *(const void **)(*((_QWORD *)v21 + 144) + 48LL),
          *(unsigned __int16 *)(*((_QWORD *)v21 + 144) + 42LL));
      }
      else
      {
        *((_BYTE *)TokenInformation + 8) = 0;
        *(_QWORD *)TokenInformation = 0LL;
      }
LABEL_370:
      ExReleaseResourceLite(*((PERESOURCE *)v21 + 6));
      KeLeaveCriticalRegionThread(KeGetCurrentThread());
      goto LABEL_17;
    default:
      return -1073741821;
  }
}
