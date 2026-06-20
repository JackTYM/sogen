NTSTATUS __stdcall NtCreateSection(                             | 
        PHANDLE SectionHandle,                                  | 
        ACCESS_MASK DesiredAccess,                              | 
        POBJECT_ATTRIBUTES ObjectAttributes,                    | 
        PLARGE_INTEGER MaximumSize,                             | 
        ULONG SectionPageProtection,                            | 
        ULONG AllocationAttributes,                             | 
        HANDLE FileHandle)                                      | 
{                                                               | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND] | 
                                                                | 
  v7 = AllocationAttributes;                                    | 
  v8 = 0;                                                       | 
  Address = 0LL;                                                | 
  v12 = 0LL;                                                    | 
  if ( (AllocationAttributes & 0x7F) != 0 )                     | 
  {                                                             | 
    v7 = AllocationAttributes & 0xFFFFFF80;                     | 
    *(_QWORD *)&v12 = 2LL;                                      | 
    *((_QWORD *)&v12 + 1) = (AllocationAttributes & 0x7F) - 1;  | 
    Address = &v12;                                             | 
    v8 = 1;                                                     | 
  }                                                             | 
  LODWORD(ullMultiplicand) = v8;                                | 
  return MiCreateSectionCommon(                                 | 
           (int)SectionHandle,                                  | 
           DesiredAccess,                                       | 
           (int)ObjectAttributes,                               | 
           (int)MaximumSize,                                    | 
           SectionPageProtection,                               | 
           v7,                                                  | 
           (__int64)FileHandle,                                 | 
           Address,                                             | 
           ullMultiplicand,                                     | 
           1,                                                   | 
           KeGetCurrentThread()->PreviousMode);                 | 
}                                                               | 
