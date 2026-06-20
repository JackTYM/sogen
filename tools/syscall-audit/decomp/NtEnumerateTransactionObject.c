// attributes: thunk                                                                                                      | 
NTSTATUS __stdcall NtEnumerateTransactionObject(                                                                          | 
        HANDLE RootObjectHandle,                                                                                          | 
        KTMOBJECT_TYPE QueryType,                                                                                         | 
        PKTMOBJECT_CURSOR ObjectCursor,                                                                                   | 
        ULONG ObjectCursorLength,                                                                                         | 
        PULONG ReturnLength)                                                                                              | 
{                                                                                                                         | 
  // [COLLAPSED LOCAL DECLARATIONS. PRESS NUMPAD "+" TO EXPAND]                                                           | 
                                                                                                                          | 
  return __imp_NtEnumerateTransactionObject(RootObjectHandle, QueryType, ObjectCursor, ObjectCursorLength, ReturnLength); | 
}                                                                                                                         | 
12 row(s)
