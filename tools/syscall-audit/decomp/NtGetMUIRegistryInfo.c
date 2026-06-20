__int64 __fastcall NtGetMUIRegistryInfo(int a1, _DWORD *a2, volatile void *a3)
{
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]

  v18[1] = 0;
  v22 = 0;
  memset(&Event, 0, sizeof(Event));
  Handle = 0LL;
  v13 = 0LL;
  v14 = 0LL;
  v15 = 0LL;
  v24 = 0;
  if ( !KeGetCurrentThread()->PreviousMode || (_DWORD)InitSafeBootMode )
  {
    goto LABEL_25;
  }
  if ( a2 )
  {
    v5 = 0x7FFFFFFF0000LL;
    v6 = 0x7FFFFFFF0000LL;
    if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
    {
      v6 = (__int64)a2;
    }
    Length = *(_DWORD *)v6;
    if ( *(_DWORD *)v6 )
    {
      if ( !a3 )
      {
        goto LABEL_8;
      }
      goto LABEL_10;
    }
  }
  else
  {
    if ( (a1 & 0xA) == 0 )
    {
      goto LABEL_8;
    }
    Length = 0;
    v5 = 0x7FFFFFFF0000LL;
  }
  if ( a3 )
  {
LABEL_8:
    v7 = -1073741811;
    goto LABEL_26;
  }
LABEL_10:
  v8 = 1;
  if ( a1 )
  {
    v8 = a1;
  }
  if ( (v8 & 0xFFFFFFF4) != 0 )
  {
    goto LABEL_8;
  }
  if ( MUIRegistryLock || (v7 = MUIInitializeResourceLock(&MUIRegistryLock), (v7 & 0xC0000000) != 0xC0000000) )
  {
    CurrentThread = KeGetCurrentThread();
    --CurrentThread->KernelApcDisable;
    ExAcquireResourceExclusiveLite(MUIRegistryLock, 1u);
    v24 = 1;
    if ( MUIRegistryInfo != (PVOID)-1LL )
    {
      if ( (v8 & 1) != 0 )
      {
        v10 = 0;
        if ( MUIRegistryInfo )
        {
          goto LABEL_17;
        }
        KeInitializeEvent(&Event, SynchronizationEvent, 0);
        *(_QWORD *)&v13 = &Event;
        *((_QWORD *)&v13 + 1) = 0LL;
        *(_QWORD *)&v14 = 0LL;
        *((_QWORD *)&v14 + 1) = (unsigned __int16)PsInstallUILanguageId;
        *(_QWORD *)&v15 = (unsigned __int16)PsMachineUILanguageId;
        DWORD2(v15) = -1073741823;
        v18[0] = 48;
        v19 = 0LL;
        v21 = 512;
        v20 = 0LL;
        v23 = 0LL;
        v7 = PsCreateSystemThreadEx(&Handle, 0x1FFFFFLL, v18, 0LL, 0LL, MUIRegistrySystemRoutine, &v13, 0LL, 0LL);
        if ( v7 >= 0 )
        {
          ZwClose(Handle);
          v7 = KeWaitForSingleObject(&Event, Executive, 0, 0, 0LL);
          if ( v7 >= 0 )
          {
            v7 = DWORD2(v15);
            if ( SDWORD2(v15) < 0 )
            {
              goto LABEL_35;
            }
            MUIRegistryInfo = (PVOID)*((_QWORD *)&v13 + 1);
            LODWORD(MUIRegistryInfoSize) = v14;
            if ( !DWORD1(v14) )
            {
              MUIBugCheck(32770LL);
            }
            if ( !HIDWORD(v14) )
            {
              if ( PsUILanguageComitted )
              {
                MUIBugCheck(32769LL);
              }
              MigrateOOBELanguageToInstallationLanguage();
            }
            if ( !DWORD1(v15) )
            {
              PsMachineUILanguageId = PsInstallUILanguageId;
            }
          }
        }
        if ( v7 >= 0 )
        {
LABEL_17:
          if ( Length )
          {
            if ( Length < (unsigned int)MUIRegistryInfoSize )
            {
              v7 = -1073741789;
              goto LABEL_19;
            }
            v10 = 1;
          }
          v7 = 0;
LABEL_19:
          if ( (unsigned __int64)a2 < 0x7FFFFFFF0000LL )
          {
            v5 = (__int64)a2;
          }
          *(_DWORD *)v5 = *(_DWORD *)v5;
          *a2 = MUIRegistryInfoSize;
          if ( v10 )
          {
            ProbeForWrite(a3, Length, 1u);
            memset_0((void *)a3, 0, Length);
            memmove((void *)a3, MUIRegistryInfo, (unsigned int)MUIRegistryInfoSize);
          }
          goto LABEL_26;
        }
LABEL_35:
        MUIRegistryInfo = (PVOID)-1LL;
        LODWORD(MUIRegistryInfoSize) = 0;
        goto LABEL_26;
      }
      if ( (v8 & 2) != 0 )
      {
        if ( MUIRegistryInfo )
        {
          ExFreePoolWithTag(MUIRegistryInfo, 0);
          MUIRegistryInfo = 0LL;
          LODWORD(MUIRegistryInfoSize) = 0;
          if ( (v8 & 8) != 0 )
          {
            ++*(_DWORD *)(MmWriteableSharedUserData + 932);
          }
        }
      }
      else
      {
        if ( (v8 & 8) == 0 )
        {
          goto LABEL_8;
        }
        ++*(_DWORD *)(MmWriteableSharedUserData + 932);
        if ( MUIRegistryInfo )
        {
          *((_DWORD *)MUIRegistryInfo + 3) = MEMORY[0xFFFFF780000003A4];
        }
      }
LABEL_57:
      v7 = 0;
      goto LABEL_26;
    }
    if ( (v8 & 2) != 0 )
    {
      MUIRegistryInfo = 0LL;
      LODWORD(MUIRegistryInfoSize) = 0;
      goto LABEL_57;
    }
LABEL_25:
    v7 = -1073741823;
  }
LABEL_26:
  if ( v24 )
  {
    ExReleaseResourceLite(MUIRegistryLock);
    KeLeaveCriticalRegion();
  }
  return (unsigned int)v7;
}
