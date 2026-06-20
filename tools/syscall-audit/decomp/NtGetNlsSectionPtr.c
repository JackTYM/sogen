NTSTATUS __fastcall NtGetNlsSectionPtr(unsigned int a1, unsigned int a2, PVOID *a3, _QWORD *a4, _QWORD *a5)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  *(&ObjectAttributes.Length + 1) = 0;
  *(&ObjectAttributes.Attributes + 1) = 0;
  v28 = 0LL;
  FileHandle = 0LL;
  SectionHandle = 0LL;
  IoStatusBlock = 0LL;
  v21 = 0LL;
  v22 = 0LL;
  if ( !a4 && !a3 )
  {
    return -1073741811;
  }
  PreviousMode = KeGetCurrentThread()->PreviousMode;
  if ( PreviousMode )
  {
    v10 = 0x7FFFFFFF0000LL;
    if ( a4 )
    {
      v11 = 0x7FFFFFFF0000LL;
      if ( (unsigned __int64)a4 < 0x7FFFFFFF0000LL )
      {
        v11 = (__int64)a4;
      }
      *(_QWORD *)v11 = *(_QWORD *)v11;
    }
    if ( a5 )
    {
      v12 = 0x7FFFFFFF0000LL;
      if ( (unsigned __int64)a5 < 0x7FFFFFFF0000LL )
      {
        v12 = (__int64)a5;
      }
      *(_QWORD *)v12 = *(_QWORD *)v12;
    }
    if ( a3 )
    {
      if ( (unsigned __int64)a3 < 0x7FFFFFFF0000LL )
      {
        v10 = (__int64)a3;
      }
      *(_QWORD *)v10 = *(_QWORD *)v10;
    }
    if ( a3 )
    {
      return -1073741583;
    }
  }
  result = RtlpInitNlsSectionName(a1, a2, v44);
  if ( result >= 0 )
  {
    ObjectAttributes.Length = 48;
    ObjectAttributes.RootDirectory = 0LL;
    ObjectAttributes.Attributes = 720;
    ObjectAttributes.ObjectName = (PUNICODE_STRING)&v28;
    *(_OWORD *)&ObjectAttributes.SecurityDescriptor = 0LL;
    if ( ((a1 - 11) & 0xFFFFFFFC) != 0 || a1 == 13 )
    {
      v14 = -1073741823;
      goto LABEL_20;
    }
    v14 = ZwOpenSection(&SectionHandle, 4u, &ObjectAttributes);
    if ( v14 >= 0 )
    {
LABEL_20:
      if ( v14 < 0 )
      {
        return v14;
      }
      Object = 0LL;
      v14 = ObReferenceObjectByHandle(SectionHandle, 0xF001Fu, MmSectionObjectType, 0, &Object, 0LL);
      ZwClose(SectionHandle);
      if ( v14 < 0 )
      {
        return v14;
      }
      if ( a4 )
      {
        v26 = 0LL;
        if ( !PreviousMode )
        {
          v24 = 0LL;
          v22 = 0LL;
          v18 = Object;
          v19 = MiSectionControlArea(Object);
          MiCheckPurgeAndUpMapCount(v19);
          LODWORD(v24) = v24 & 0xFFFF0000;
          v17 = v18[6] - v24;
          v14 = MiInsertInSystemSpace(
                  *((_DWORD *)v18 + 12) - (int)v24,
                  (_DWORD)v18,
                  (unsigned int)&v24,
                  0,
                  0LL,
                  (__int64)&v22);
          if ( v14 < 0 )
          {
            MiDereferenceControlArea(v19);
          }
          else
          {
            v21 = v22;
            v14 = 0;
          }
          goto LABEL_30;
        }
        Process = KeGetCurrentThread()->ApcState.Process;
        MiSectionControlArea(Object);
        memset_0(v31, 0, 0x80uLL);
        v42 = KeGetCurrentThread()->ApcState.Process;
        v41 = Process;
        v38 = KeGetCurrentThread()->PreviousMode;
        v37 = 0;
        v35 = 0x400000;
        v36 = 2;
        v40 = 0;
        if ( v21 )
        {
          v39 |= 1u;
        }
        v34 = 0LL;
        v16 = *(_QWORD *)&Process[1].PrimaryGroup - 1LL;
        if ( v16 >= 0x7FFFFFFEFFFFLL )
        {
          v16 = 0x7FFFFFFEFFFFLL;
        }
        v32 = v16;
        v33 = 0x10000LL;
        v14 = MiMapViewOfSection((_DWORD)Object, (unsigned int)v31, (unsigned int)&v21, 0, (__int64)&v26, 1, 1);
        if ( v14 >= 0 )
        {
          v17 = v34;
LABEL_30:
          if ( !a3 )
          {
            ObfDereferenceObject(Object);
          }
          if ( v14 >= 0 )
          {
            if ( a4 )
            {
              *a4 = v21;
            }
            if ( a5 )
            {
              *a5 = v17;
            }
            if ( a3 )
            {
              *a3 = Object;
            }
          }
          return v14;
        }
        if ( v21 )
        {
          ++dword_140E301EC;
        }
        else
        {
          ++dword_140E301E8;
        }
      }
      v17 = v22;
      goto LABEL_30;
    }
    *(&v30.Length + 1) = 0;
    *(&v30.Attributes + 1) = 0;
    v29 = 0LL;
    result = RtlpInitNlsFileName(a1, a2, v45);
    if ( result >= 0 )
    {
      v30.Length = 48;
      v30.RootDirectory = 0LL;
      v30.Attributes = 576;
      v30.ObjectName = (PUNICODE_STRING)&v29;
      *(_OWORD *)&v30.SecurityDescriptor = 0LL;
      result = ZwOpenFile(&FileHandle, 0x100000u, &v30, &IoStatusBlock, 1u, 0);
      if ( result >= 0 )
      {
        v14 = ZwCreateSection(&SectionHandle, 4u, &ObjectAttributes, 0LL, 2u, 0x8000000u, FileHandle);
        ZwClose(FileHandle);
        goto LABEL_20;
      }
    }
  }
  return result;
}
