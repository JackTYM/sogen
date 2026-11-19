// NSI layout probe for a CI runner (Windows Server 2022): hooks nsi.dll's NtDeviceIoControlFile import in this process,
// drives the iphlpapi/cryptnet paths that read the network store, and logs every request with the full key/rw/
// dynamic/static records it returned, plus the documented API structures for correlation. Read-only.
// usage: nsiprobe.exe <logfile>
#define _WIN32_WINNT 0x0A00
#include <winsock2.h>
#include <ws2ipdef.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winternl.h>
#include <iphlpapi.h>
#include <netioapi.h>
#include <wincrypt.h>
#include <stdio.h>

typedef NTSTATUS(NTAPI* ioctl_fn)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, ULONG, PVOID, ULONG, PVOID, ULONG);
static ioctl_fn real_ioctl;
static FILE* out;
static int seq;

static void hex(const char* label, const void* p, size_t n)
{
    fprintf(out, "    %s (%zu):", label, n);
    const unsigned char* b = (const unsigned char*)p;
    for (size_t i = 0; i < n; i++) fprintf(out, "%s%02x", i % 32 == 0 ? "\n      " : "", b[i]);
    fprintf(out, "\n");
}

static NTSTATUS NTAPI hook(HANDLE h, HANDLE ev, PVOID apc, PVOID ctx, PIO_STATUS_BLOCK iosb, ULONG code, PVOID in, ULONG inlen, PVOID o,
                           ULONG olen)
{
    int id = ++seq;
    unsigned char before[256] = {0};
    memcpy(before, in, inlen < sizeof(before) ? inlen : sizeof(before));
    NTSTATUS s = real_ioctl(h, ev, apc, ctx, iosb, code, in, inlen, o, olen);
    fprintf(out, "#%d IOCTL 0x%lX in=%lu out=%lu same=%d status=0x%08lX info=%llu\n", id, code, inlen, olen, in == o, (unsigned long)s,
            iosb ? (unsigned long long)iosb->Information : 0ull);
    hex("request before", before, inlen < sizeof(before) ? inlen : sizeof(before));
    hex("request after", in, inlen < 256 ? inlen : 256);
    const unsigned char* b = (const unsigned char*)in;
    // Module id: pointer at 0x10 (NPI_MODULEID).
    if (inlen >= 0x18)
    {
        const unsigned char* m = *(const unsigned char* const*)(b + 0x10);
        if (m && !IsBadReadPtr(m, 24)) hex("module", m, 24);
    }
    // Every (pointer, size) pair in the request: dump what the pointer refers to after the call. For an enumeration the
    // count sits at 0x68; dump count * size bytes then.
    ULONG64 count = 1;
    if (code == 0x12001B && inlen >= 0x70) memcpy(&count, b + 0x68, 8);
    if (count > 64) count = 64;
    for (ULONG off = 0x18; off + 12 <= inlen; off += 8)
    {
        ULONG64 p;
        DWORD size;
        memcpy(&p, b + off, 8);
        memcpy(&size, b + off + 8, 4);
        if (p < 0x10000 || p > 0x7fffffffffffull || size == 0 || size > 0x4000) continue;
        size_t n = (size_t)size * (code == 0x12001B ? (size_t)count : 1);
        if (IsBadReadPtr((void*)p, n)) continue;
        char label[64];
        sprintf(label, "buffer @0x%lx size %lu x %llu", off, size, (unsigned long long)(code == 0x12001B ? count : 1));
        hex(label, (void*)p, n);
    }
    fflush(out);
    return s;
}

static void patch_iat(HMODULE mod)
{
    if (!mod) return;
    PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)mod;
    PIMAGE_NT_HEADERS nth = (PIMAGE_NT_HEADERS)((BYTE*)mod + dos->e_lfanew);
    IMAGE_DATA_DIRECTORY dir = nth->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    for (PIMAGE_IMPORT_DESCRIPTOR imp = (PIMAGE_IMPORT_DESCRIPTOR)((BYTE*)mod + dir.VirtualAddress); imp->Name; imp++)
        for (PIMAGE_THUNK_DATA t = (PIMAGE_THUNK_DATA)((BYTE*)mod + imp->FirstThunk); t->u1.Function; t++)
            if ((void*)t->u1.Function == (void*)real_ioctl)
            {
                DWORD old;
                VirtualProtect(&t->u1.Function, sizeof(void*), PAGE_READWRITE, &old);
                t->u1.Function = (ULONG_PTR)hook;
                VirtualProtect(&t->u1.Function, sizeof(void*), old, &old);
                fprintf(out, "patched %s import in module %p\n", (char*)mod + imp->Name, (void*)mod);
            }
}

int main(int argc, char** argv)
{
    out = fopen(argc > 1 ? argv[1] : "nsiprobe.txt", "w");
    if (!out) out = stdout;
    OSVERSIONINFOW v = {sizeof(v)};
    ((LONG(WINAPI*)(OSVERSIONINFOW*))GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion"))(&v);
    fprintf(out, "Windows %lu.%lu.%lu\n", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
    real_ioctl = (ioctl_fn)GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtDeviceIoControlFile");
    LoadLibraryA("iphlpapi.dll");
    patch_iat(LoadLibraryA("nsi.dll"));

    fprintf(out, "== GetIfTable2\n");
    PMIB_IF_TABLE2 t = NULL;
    fprintf(out, "GetIfTable2 -> %lu\n", GetIfTable2(&t));
    for (ULONG i = 0; t && i < t->NumEntries; i++)
    {
        fprintf(out, "  row %lu luid=%llx index=%lu type=%lu oper=%d\n", i, (unsigned long long)t->Table[i].InterfaceLuid.Value,
                t->Table[i].InterfaceIndex, t->Table[i].Type, t->Table[i].OperStatus);
        hex("MIB_IF_ROW2", &t->Table[i], sizeof(MIB_IF_ROW2));
    }
    fprintf(out, "== GetIpInterfaceTable(AF_INET)\n");
    PMIB_IPINTERFACE_TABLE it = NULL;
    fprintf(out, "-> %lu\n", GetIpInterfaceTable(AF_INET, &it));
    for (ULONG i = 0; it && i < it->NumEntries; i++) hex("MIB_IPINTERFACE_ROW", &it->Table[i], sizeof(MIB_IPINTERFACE_ROW));
    fprintf(out, "== GetUnicastIpAddressTable(AF_INET)\n");
    PMIB_UNICASTIPADDRESS_TABLE ut = NULL;
    fprintf(out, "-> %lu\n", GetUnicastIpAddressTable(AF_INET, &ut));
    for (ULONG i = 0; ut && i < ut->NumEntries; i++) hex("MIB_UNICASTIPADDRESS_ROW", &ut->Table[i], sizeof(MIB_UNICASTIPADDRESS_ROW));
    fprintf(out, "== GetIpForwardTable2(AF_INET)\n");
    PMIB_IPFORWARD_TABLE2 ft = NULL;
    fprintf(out, "-> %lu\n", GetIpForwardTable2(AF_INET, &ft));
    for (ULONG i = 0; ft && i < ft->NumEntries; i++) hex("MIB_IPFORWARD_ROW2", &ft->Table[i], sizeof(MIB_IPFORWARD_ROW2));
    fprintf(out, "== GetAdaptersInfo\n");
    ULONG len = 0;
    GetAdaptersInfo(NULL, &len);
    PIP_ADAPTER_INFO ai = (PIP_ADAPTER_INFO)malloc(len);
    fprintf(out, "-> %lu (len %lu)\n", GetAdaptersInfo(ai, &len), len);
    for (PIP_ADAPTER_INFO a = ai; a; a = a->Next) hex("IP_ADAPTER_INFO", a, sizeof(*a));
    fprintf(out, "== GetAdaptersAddresses\n");
    len = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS, NULL, NULL, &len);
    PIP_ADAPTER_ADDRESSES aa = (PIP_ADAPTER_ADDRESSES)malloc(len);
    fprintf(out, "-> %lu (len %lu)\n", GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS, NULL, aa, &len), len);
    for (PIP_ADAPTER_ADDRESSES a = aa; a; a = a->Next)
        fprintf(out, "  adapter %s ifindex=%lu type=%lu oper=%d mtu=%lu\n", a->AdapterName, a->IfIndex, a->IfType, a->OperStatus, a->Mtu);
    fprintf(out, "== GetNetworkParams\n");
    len = 0;
    GetNetworkParams(NULL, &len);
    PFIXED_INFO fi = (PFIXED_INFO)malloc(len);
    fprintf(out, "-> %lu\n", GetNetworkParams(fi, &len));
    fprintf(out, "== cryptnet CryptRetrieveObjectByUrl (connectivity check)\n");
    patch_iat(LoadLibraryA("cryptnet.dll"));
    void* obj = NULL;
    BOOL ok = CryptRetrieveObjectByUrlW(L"http://crl3.digicert.com/DigiCertGlobalRootCA.crl", CONTEXT_OID_CRL, CRYPT_WIRE_ONLY_RETRIEVAL, 15000,
                                        &obj, NULL, NULL, NULL, NULL);
    fprintf(out, "-> %d err=%lu\n", ok, ok ? 0 : GetLastError());
    fprintf(out, "== done, %d NSI requests\n", seq);
    fclose(out);
    return 0;
}
