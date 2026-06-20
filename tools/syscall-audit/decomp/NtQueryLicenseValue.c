__int64 __fastcall NtQueryLicenseValue(unsigned __int64 a1, _DWORD *a2, void *a3, unsigned int a4, _DWORD *a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]
|
  v32 = a4;
  v31 = a3;
  v30 = a2;
  v5 = a4;
  v28[0] = 0LL;
  v8 = 0;
  P = 0LL;
  v29 = 0;
  memset(Size, 0, sizeof(Size));
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( a1 && (v10 = a5) != 0LL && (a3 || !a4) )
  {
    if ( a4 > 0x800000 )
    {
      v8 = -1073741801;
    }
    else if ( PreviousMode )
    {
      v11 = 0x7FFFFFFF0000LL;
      v12 = 0x7FFFFFFF0000LL;
      if ( a1 < 0x7FFFFFFF0000LL )
      {
        v12 = a1;
      }
      v13 = *(_DWORD *)v12;
      LODWORD(v28[0]) = *(_DWORD *)v12;
      v14 = *(_QWORD *)(v12 + 8);
      *((_QWORD *)&v28[0] + 1) = v14;
      if ( v14 && (_WORD)v13 && (v13 & 1) == 0 )
      {
        if ( (v14 & 1) != 0 )
        {
          ExRaiseDatatypeMisalignment();
        }
        Pool2 = (void *)ExAllocatePool2(0x100uLL, (unsigned __int16)v13, 0x20534C53u);
        P = Pool2;
        if ( Pool2 )
        {
          v16 = Pool2;
          memmove(Pool2, *((const void **)&v28[0] + 1), LOWORD(v28[0]));
          *((_QWORD *)&v28[0] + 1) = v16;
          if ( a2 )
          {
            v17 = 0x7FFFFFFF0000LL;
            if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
            {
              v17 = (__int64)a2;
            }
            *(_DWORD *)v17 = *(_DWORD *)v17;
            v29 = *a2;
          }
          if ( !a3 || !(_DWORD)v5 )
          {
            goto LABEL_24;
          }
          v18 = (unsigned __int64)a3;
          v19 = (unsigned __int64)a3 + v5 - 1;
          if ( (unsigned __int64)a3 > v19 || v19 >= 0x7FFFFFFF0000LL )
          {
            ExRaiseAccessViolation();
          }
          v20 = (v19 & 0xFFFFFFFFFFFFF000uLL) + 4096;
          do
          {
            *(_BYTE *)v18 = *(_BYTE *)v18;
            v18 = (v18 & 0xFFFFFFFFFFFFF000uLL) + 4096;
          }
          while ( v18 != v20 );
          *(_QWORD *)&Size[1] = ExAllocatePool2(0x100uLL, v5, 0x20534C53u);
          if ( !*(_QWORD *)&Size[1] )
          {
            v8 = -1073741801;
          }
          else
          {
LABEL_24:
            if ( (unsigned __int64)v10 < 0x7FFFFFFF0000LL )
            {
              v11 = (__int64)v10;
            }
            *(_DWORD *)v11 = *(_DWORD *)v11;
            Size[0] = *v10;
          }
        }
        else
        {
          v8 = -1073741801;
        }
      }
      else
      {
        v8 = -1073741811;
      }
      if ( v8 >= 0 )
      {
        v21 = *(_QWORD *)(PsGetCurrentServerSiloGlobals() + 872);
        if ( qword_140FD7490 )
        {
          v22 = guard_dispatch_icall_no_overrides(v21, v28, &v29, *(_QWORD *)&Size[1], v5, Size);
        }
        else
        {
          v22 = SLQueryLicenseValueInternal(v21, v28, &v29, *(_QWORD *)&Size[1], v5, Size);
        }
        v23 = v22;
        v8 = v22;
        if ( a2 )
        {
          *a2 = v29;
        }
        v24 = Size[0];
        *v10 = Size[0];
        if ( v23 >= 0 && a3 )
        {
          if ( (unsigned int)v5 < v24 )
          {
            v8 = -1073741789;
          }
          else
          {
            memmove(a3, *(const void **)&Size[1], v24);
          }
        }
      }
    }
    else
    {
      v8 = ntoskrnl_27(a1, (_DWORD)a2, (_DWORD)a3, a4, (__int64)a5);
    }
  }
  else
  {
    v8 = -1073741811;
  }
  if ( P )
  {
    ExFreePoolWithTag(P, 0);
  }
  if ( *(_QWORD *)&Size[1] )
  {
    ExFreePoolWithTag(*(PVOID *)&Size[1], 0);
  }
  return (unsigned int)v8;
}
148 row(s)
