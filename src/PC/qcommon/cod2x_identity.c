#if defined(COD2_CODX) && COD2_CODX
#include "cod2x.h"
#include <stddef.h>
#include <string.h>
#if defined(COD2_X64) && COD2_X64
/* cl_hwid2 as CoD2x 1.4.6.8 derives it on Windows (mss32/hwid.cpp): nine machine
   values in a fixed text, hashed with the game's CL_BuildMD5. Windows reads the
   same WMI values, so a PC gets the value CoD2x sends from it. macOS and Linux fill
   each value from their closest source. Only the hash leaves the machine. */
#include "cdkey_hash.h"
#include <stdio.h>
#if defined(_WIN32) && defined(COD2_WINDOWS_NATIVE) && COD2_WINDOWS_NATIVE && !defined(DEDICATED)
#define COD2X_HWID_WMI 1
#define COBJMACROS
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <wbemidl.h>
#elif defined(__APPLE__)
#define COD2X_HWID_IOKIT 1
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <dlfcn.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#elif defined(__linux__)
#define COD2X_HWID_SYSFS 1
#include <dirent.h>
#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>
#endif
#if (COD2X_HWID_IOKIT || COD2X_HWID_SYSFS) && (defined(__i386__) || defined(__x86_64__))
#include <cpuid.h>
#endif

static void Cod2x_Hex(const unsigned char *bytes, char hex[33])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 16; ++i) {
        hex[2 * i] = digits[bytes[i] >> 4];
        hex[2 * i + 1] = digits[bytes[i] & 15];
    }
    hex[32] = '\0';
}

/* Windows 1.3 CL_BuildMD5 (CoD2MP_s.exe 0x4bc1d0): MD5 from a seeded state, then
   MD5 of its lowercase hex digest from a second seeded state. The first seed is the
   one Com_CDKeyHashBytes uses. */
static void Cod2x_BuildMD5(const char *data, unsigned int length, char hex[33])
{
    static const uint32_t seeds[2][4] = {
        { 0x6f1cd602u, 0x226c74beu, 0xb31c088du, 0x555a9639u },
        { 0x677d44b3u, 0xf137f963u, 0x9977ab6cu, 0x12214f0cu }
    };
    unsigned char digest[CC_MD5_DIGEST_LENGTH];
    char first[33];
    CC_MD5_CTX context;
    int round;
    for (round = 0; round < 2; ++round) {
        CC_MD5_Init(&context);
        context.A = seeds[round][0];
        context.B = seeds[round][1];
        context.C = seeds[round][2];
        context.D = seeds[round][3];
        if (round)
            CC_MD5_Update(&context, first, 32);
        else
            CC_MD5_Update(&context, data, length);
        CC_MD5_Final(digest, &context);
        Cod2x_Hex(digest, round ? hex : first);
    }
}

static int cod2x_shortHwid;
static unsigned int cod2x_hwidCode;
static char cod2x_hwidError[512];

const char *Cod2x_HwidError(unsigned int *code)
{
    *code = cod2x_hwidCode;
    return cod2x_hwidError;
}

/* CoD2x's text when 3 or more values are missing; the names are the ones its WMI
   read uses, on every platform. */
static void Cod2x_HwidMissing(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT], int failed, char *text,
                              size_t size)
{
    static const char *const names[COD2X_GPU_NAME] = {
        "Win32_BIOS.Manufacturer", "Win32_BIOS.SerialNumber", "Win32_Processor.Name",
        "Win32_Processor.ProcessorId", "Win32_BaseBoard.Manufacturer", "Win32_BaseBoard.SerialNumber",
        "Win32_ComputerSystemProduct.UUID", "Disc Model", "Disc Serial Number"};
    char list[512] = {0};
    int i, missing = failed;
    for (i = 0; i < COD2X_GPU_NAME; ++i) {
        if (!value[i][0]) {
            ++missing;
            strncat(list, names[i], sizeof(list) - strlen(list) - 1);
            strncat(list, " ", sizeof(list) - strlen(list) - 1);
        }
    }
    if (failed)
        strncat(list, "Video Controller ", sizeof(list) - strlen(list) - 1);
    snprintf(text, size, "Too many missing WMI properties (%d/%d). Missing: %s", missing, 7, list);
}

int Cod2x_HwidShortFromMachine(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT])
{
    /* cl_hwid: 32-bit FNV-1a over four of the values, top bit cleared, never 0. */
    char text[512];
    const unsigned char *p;
    unsigned int hash = 2166136261u;
    snprintf(text, sizeof(text), "%s %s %s %s", value[COD2X_BIOS_SERIAL], value[COD2X_CPU_ID],
             value[COD2X_BOARD_SERIAL], value[COD2X_PRODUCT_UUID]);
    for (p = (const unsigned char *)text; *p; ++p) {
        hash ^= *p;
        hash *= 16777619u;
    }
    hash &= 0x7fffffffu;
    return hash ? (int)hash : 1;
}

int Cod2x_ShortHwid(void)
{
    return cod2x_shortHwid;
}

static void Cod2x_HwidText(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT], char *text, size_t size)
{
    snprintf(text, size,
             "BIOS manufacturer: %s\nBIOS serial: %s\nCPU name: %s\nCPU ID: %s\nMotherboard manufacturer: %s\n"
             "Motherboard serial: %s\nDisk Model: %s\nDisk Serial: %s\nGPU Name: %s",
             value[COD2X_BIOS_MAKER], value[COD2X_BIOS_SERIAL], value[COD2X_CPU_NAME], value[COD2X_CPU_ID],
             value[COD2X_BOARD_MAKER], value[COD2X_BOARD_SERIAL], value[COD2X_DISK_MODEL],
             value[COD2X_DISK_SERIAL], value[COD2X_GPU_NAME]);
}

int Cod2x_HwidFromMachine(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT], int failed, char id[33])
{
    char text[1024];
    int i, missing = failed;
    id[0] = '\0';
    /* CoD2x refuses with 3 or more missing: an empty value up to the disk serial,
       or a failed GPU query. No GPU name doesn't count. */
    for (i = 0; i < COD2X_GPU_NAME; ++i)
        missing += !value[i][0];
    if (missing >= 3)
        return 0;
    Cod2x_HwidText(value, text, sizeof(text));
    Cod2x_BuildMD5(text, (unsigned int)strlen(text), id);
    cod2x_shortHwid = Cod2x_HwidShortFromMachine(value);
    return 1;
}

#if COD2X_HWID_WMI || COD2X_HWID_IOKIT
/* Like _wcsnicmp in the C locale: ASCII letters only. */
static int Cod2x_SameText(const char *text, const char *other, size_t length)
{
    for (; length; --length, ++text, ++other) {
        char a = *text >= 'a' && *text <= 'z' ? *text - ('a' - 'A') : *text;
        char b = *other >= 'a' && *other <= 'z' ? *other - ('a' - 'A') : *other;
        if (a != b)
            return 0;
        if (!a)
            break;
    }
    return 1;
}

/* CoD2x's split of adapter names into discrete and integrated. */
static int Cod2x_IntegratedGpu(const char *name)
{
    static const char *const discrete[] = {
        "NVIDIA", "GEFORCE", "RTX", "GTX", "QUADRO", "FIREPRO", "RADEON PRO", "RADEON RX", "RADEON R9",
        "RADEON R8", "RADEON R7 ", "INTEL ARC"
    };
    static const char *const integrated[] = {
        "INTEL(R) HD GRAPHICS", "INTEL(R) UHD GRAPHICS", "INTEL(R) IRIS GRAPHICS", "INTEL(R) IRIS PLUS GRAPHICS",
        "INTEL(R) IRIS XE GRAPHICS", "INTEL(R) GRAPHICS", "INTEL(R) GMA", "AMD RADEON(TM) GRAPHICS",
        "AMD RADEON(TM) VEGA", "AMD RADEON VEGA", "AMD RADEON RX VEGA", "AMD RADEON R2 GRAPHICS",
        "AMD RADEON R3 GRAPHICS", "AMD RADEON R4 GRAPHICS", "AMD RADEON R5 GRAPHICS", "AMD RADEON R6 GRAPHICS",
        "AMD RADEON R7 GRAPHICS", "AMD RADEON R8 GRAPHICS"
    };
    static const char basic[] = "Microsoft Basic Display Adapter";
    const char *at;
    size_t i;
    if (!*name)
        return 0;
    for (i = 0; i < sizeof(discrete) / sizeof(discrete[0]); ++i)
        for (at = name; *at; ++at)
            if (Cod2x_SameText(at, discrete[i], strlen(discrete[i])))
                return 0;
    for (i = 0; i < sizeof(integrated) / sizeof(integrated[0]); ++i)
        if (Cod2x_SameText(name, integrated[i], strlen(integrated[i])))
            return 1;
    return Cod2x_SameText(name, basic, sizeof(basic));
}
#endif

#if (COD2X_HWID_IOKIT || COD2X_HWID_SYSFS) && (defined(__i386__) || defined(__x86_64__))
/* Win32_Processor.ProcessorId: CPUID leaf 1, EDX then EAX. */
static void Cod2x_ProcessorId(char out[COD2X_MACHINE_TEXT])
{
    unsigned int a, b, c, d;
    out[0] = '\0';
    if (__get_cpuid(1, &a, &b, &c, &d))
        snprintf(out, COD2X_MACHINE_TEXT, "%08X%08X", d, a);
}
#endif

#if COD2X_HWID_WMI
#include "win32/win_cod2x.h"
extern void Com_Printf(const char *, ...);

/* CoD2x waits at most 3 x 3000 ms for each enumerator step. The whole read gets a
   limit of its own, so a stuck WMI connection can't stop startup either. */
#define COD2X_WMI_STEP_MS 3000
#define COD2X_WMI_STEP_TRIES 3
#define COD2X_WMI_TOTAL_MS 30000

typedef struct {
    volatile LONG refs;
    int ok, failed;
    HRESULT code;
    char id[33];
    char error[512];
    char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT];
} Cod2xMachine;

/* Each value is converted into 128 bytes, as in CoD2x. A longer value makes the
   conversion fail there and leaves the buffer unterminated; here it is cut at 127. */
static void Cod2x_WmiText(const VARIANT *value, char out[COD2X_MACHINE_TEXT])
{
    out[0] = '\0';
    if (value->vt == VT_BSTR && value->bstrVal) {
        WideCharToMultiByte(CP_ACP, 0, value->bstrVal, -1, out, COD2X_MACHINE_TEXT, NULL, NULL);
        out[COD2X_MACHINE_TEXT - 1] = '\0';
    }
}

static HRESULT Cod2x_WmiBlanket(IUnknown *proxy)
{
    return CoSetProxyBlanket(proxy, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL,
                             RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
}

static HRESULT Cod2x_WmiQuery(IWbemServices *services, const wchar_t *text, IEnumWbemClassObject **rows)
{
    BSTR language = SysAllocString(L"WQL"), query = SysAllocString(text);
    HRESULT hr = E_OUTOFMEMORY;
    *rows = NULL;
    if (language && query)
        hr = IWbemServices_ExecQuery(services, language, query,
                                     WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, rows);
    SysFreeString(language);
    SysFreeString(query);
    if (SUCCEEDED(hr) && !*rows)
        hr = E_FAIL;
    if (SUCCEEDED(hr))
        hr = Cod2x_WmiBlanket((IUnknown *)*rows);
    if (FAILED(hr) && *rows) {
        IEnumWbemClassObject_Release(*rows);
        *rows = NULL;
    }
    return hr;
}

/* S_OK with a row, WBEM_S_FALSE at the end, or a failure. */
static HRESULT Cod2x_WmiNext(IEnumWbemClassObject *rows, IWbemClassObject **row)
{
    HRESULT hr = WBEM_S_TIMEDOUT;
    ULONG count = 0;
    int tries;
    *row = NULL;
    for (tries = 0; tries < COD2X_WMI_STEP_TRIES && hr == WBEM_S_TIMEDOUT; ++tries)
        hr = IEnumWbemClassObject_Next(rows, COD2X_WMI_STEP_MS, 1, row, &count);
    if (hr == WBEM_S_TIMEDOUT)
        hr = HRESULT_FROM_WIN32(WAIT_TIMEOUT);
    if (SUCCEEDED(hr))
        hr = count && *row ? S_OK : WBEM_S_FALSE;
    if (hr != S_OK && *row) {
        IWbemClassObject_Release(*row);
        *row = NULL;
    }
    return hr;
}

/* The first row of a query, or E_FAIL if there is none. */
static HRESULT Cod2x_WmiFirst(IWbemServices *services, const wchar_t *text, IWbemClassObject **row)
{
    IEnumWbemClassObject *rows;
    HRESULT hr = Cod2x_WmiQuery(services, text, &rows);
    *row = NULL;
    if (FAILED(hr))
        return hr;
    hr = Cod2x_WmiNext(rows, row);
    IEnumWbemClassObject_Release(rows);
    return hr == WBEM_S_FALSE ? E_FAIL : hr;
}

static HRESULT Cod2x_WmiGet(IWbemClassObject *row, const wchar_t *name, char out[COD2X_MACHINE_TEXT])
{
    VARIANT value;
    HRESULT hr;
    VariantInit(&value);
    hr = IWbemClassObject_Get(row, name, 0, &value, NULL, NULL);
    out[0] = '\0';
    if (SUCCEEDED(hr))
        Cod2x_WmiText(&value, out);
    VariantClear(&value);
    return hr;
}

/* "<prefix><DeviceID of row><suffix>" */
static HRESULT Cod2x_WmiNextDevice(IWbemServices *services, IWbemClassObject **row, const wchar_t *prefix,
                                   const wchar_t *suffix)
{
    wchar_t text[256];
    VARIANT id;
    HRESULT hr;
    VariantInit(&id);
    hr = IWbemClassObject_Get(*row, L"DeviceID", 0, &id, NULL, NULL);
    IWbemClassObject_Release(*row);
    *row = NULL;
    if (SUCCEEDED(hr) && (id.vt != VT_BSTR || !id.bstrVal ||
                          wcslen(prefix) + wcslen(id.bstrVal) + wcslen(suffix) >= sizeof(text) / sizeof(text[0])))
        hr = E_FAIL;
    if (SUCCEEDED(hr)) {
        wcscpy(text, prefix);
        wcscat(text, id.bstrVal);
        wcscat(text, suffix);
        hr = Cod2x_WmiFirst(services, text, row);
    }
    VariantClear(&id);
    return hr;
}

/* The physical disk that holds C:. */
static HRESULT Cod2x_WmiSystemDisk(IWbemServices *services, char model[COD2X_MACHINE_TEXT],
                                   char serial[COD2X_MACHINE_TEXT])
{
    IWbemClassObject *row;
    DWORD volume;
    HRESULT hr = Cod2x_WmiFirst(services, L"SELECT * FROM Win32_LogicalDisk WHERE DeviceID='C:'", &row);
    if (SUCCEEDED(hr))
        hr = Cod2x_WmiNextDevice(services, &row, L"ASSOCIATORS OF {Win32_LogicalDisk.DeviceID='",
                                 L"'} WHERE AssocClass=Win32_LogicalDiskToPartition");
    if (SUCCEEDED(hr))
        hr = Cod2x_WmiNextDevice(services, &row, L"ASSOCIATORS OF {Win32_DiskPartition.DeviceID='",
                                 L"'} WHERE AssocClass=Win32_DiskDriveToDiskPartition");
    if (FAILED(hr))
        return hr;
    hr = Cod2x_WmiGet(row, L"Model", model);
    if (SUCCEEDED(hr))
        hr = Cod2x_WmiGet(row, L"SerialNumber", serial);
    IWbemClassObject_Release(row);
    if (FAILED(hr))
        return hr;
    /* No disk serial (as under Wine): the C: volume serial instead. */
    if (!serial[0]) {
        if (GetVolumeInformationA("C:\\", NULL, 0, &volume, NULL, NULL, NULL, 0))
            snprintf(serial, COD2X_MACHINE_TEXT, "%lu", (unsigned long)volume);
        else
            snprintf(serial, COD2X_MACHINE_TEXT, "UnknownSerial");
    }
    return S_OK;
}

static int Cod2x_WmiIntegratedGpu(const wchar_t *name)
{
    char ascii[256];
    size_t i;
    /* Non-ASCII characters never match CoD2x's ASCII names, whatever their case. */
    for (i = 0; name[i] && i < sizeof(ascii) - 1; ++i)
        ascii[i] = name[i] < 128 ? (char)name[i] : 0x7f;
    ascii[i] = '\0';
    return Cod2x_IntegratedGpu(ascii);
}

/* The first discrete PCI adapter with more than 100 MiB, else no name. CoD2x keeps
   a fallback name too, but its loop only ends at WBEM_S_FALSE, which it returns
   before the fallback is used. */
static HRESULT Cod2x_WmiGpu(IWbemServices *services, char out[COD2X_MACHINE_TEXT])
{
    IEnumWbemClassObject *rows;
    IWbemClassObject *row;
    HRESULT hr = Cod2x_WmiQuery(services, L"SELECT Name, AdapterRAM, PNPDeviceID FROM Win32_VideoController", &rows);
    out[0] = '\0';
    if (FAILED(hr))
        return hr;
    while ((hr = Cod2x_WmiNext(rows, &row)) == S_OK) {
        VARIANT name, memory, device;
        int found;
        VariantInit(&name);
        VariantInit(&memory);
        VariantInit(&device);
        IWbemClassObject_Get(row, L"Name", 0, &name, NULL, NULL);
        IWbemClassObject_Get(row, L"AdapterRAM", 0, &memory, NULL, NULL);
        IWbemClassObject_Get(row, L"PNPDeviceID", 0, &device, NULL, NULL);
        found = name.vt == VT_BSTR && name.bstrVal && (memory.vt == VT_I4 || memory.vt == VT_UI4) &&
                memory.uintVal > 100u * 1024u * 1024u && device.vt == VT_BSTR && device.bstrVal &&
                wcsstr(device.bstrVal, L"PCI") && !Cod2x_WmiIntegratedGpu(name.bstrVal);
        if (found)
            Cod2x_WmiText(&name, out);
        VariantClear(&name);
        VariantClear(&memory);
        VariantClear(&device);
        IWbemClassObject_Release(row);
        if (found)
            break;
    }
    IEnumWbemClassObject_Release(rows);
    return hr;
}

static int Cod2x_WmiMachine(Cod2xMachine *machine)
{
    static const struct {
        const wchar_t *query, *name;
        const char *label;
    } reads[] = {
        { L"SELECT * FROM Win32_BIOS", L"Manufacturer", "Win32_BIOS.Manufacturer" },
        { L"SELECT * FROM Win32_BIOS", L"SerialNumber", "Win32_BIOS.SerialNumber" },
        { L"SELECT * FROM Win32_Processor", L"Name", "Win32_Processor.Name" },
        { L"SELECT * FROM Win32_Processor", L"ProcessorId", "Win32_Processor.ProcessorId" },
        { L"SELECT * FROM Win32_BaseBoard", L"Manufacturer", "Win32_BaseBoard.Manufacturer" },
        { L"SELECT * FROM Win32_BaseBoard", L"SerialNumber", "Win32_BaseBoard.SerialNumber" },
        { L"SELECT * FROM Win32_ComputerSystemProduct", L"UUID", "Win32_ComputerSystemProduct.UUID" }
    };
    IWbemLocator *locator = NULL;
    IWbemServices *services = NULL;
    IWbemClassObject *row;
    BSTR space;
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    int i, ok = 0;
    if (FAILED(hr)) {
        machine->code = hr;
        snprintf(machine->error, sizeof(machine->error), "COM failed (0x%08lX)", (unsigned long)hr);
        return 0;
    }
    hr = CoCreateInstance(&CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER, &IID_IWbemLocator, (void **)&locator);
    if (SUCCEEDED(hr)) {
        space = SysAllocString(L"ROOT\\CIMV2");
        hr = space ? IWbemLocator_ConnectServer(locator, space, NULL, NULL, NULL, 0, NULL, NULL, &services)
                   : E_OUTOFMEMORY;
        SysFreeString(space);
    }
    if (SUCCEEDED(hr))
        hr = Cod2x_WmiBlanket((IUnknown *)services);
    if (FAILED(hr)) {
        machine->code = hr;
        snprintf(machine->error, sizeof(machine->error), "WMI unavailable (0x%08lX)", (unsigned long)hr);
        goto done;
    }
    for (i = 0; i < (int)(sizeof(reads) / sizeof(reads[0])); ++i) {
        hr = Cod2x_WmiFirst(services, reads[i].query, &row);
        if (SUCCEEDED(hr)) {
            hr = Cod2x_WmiGet(row, reads[i].name, machine->value[i]);
            IWbemClassObject_Release(row);
        }
        if (FAILED(hr)) {
            machine->code = hr;
            if (hr == HRESULT_FROM_WIN32(WAIT_TIMEOUT))
                snprintf(machine->error, sizeof(machine->error),
                         "Timeout while reading WMI property '%s'. Please check your system's WMI service.",
                         reads[i].label);
            else
                snprintf(machine->error, sizeof(machine->error), "Failed to read WMI property '%s' (Error: 0x%08X).",
                         reads[i].label, (unsigned int)hr);
            goto done;
        }
    }
    hr = Cod2x_WmiSystemDisk(services, machine->value[COD2X_DISK_MODEL], machine->value[COD2X_DISK_SERIAL]);
    if (FAILED(hr)) {
        machine->code = hr;
        snprintf(machine->error, sizeof(machine->error), "WMI system disk failed (0x%08lX)", (unsigned long)hr);
        goto done;
    }
    machine->failed = FAILED(Cod2x_WmiGpu(services, machine->value[COD2X_GPU_NAME]));
    ok = 1;
done:
    if (services)
        IWbemServices_Release(services);
    if (locator)
        IWbemLocator_Release(locator);
    CoUninitialize();
    return ok;
}

static void Cod2x_ReleaseMachine(Cod2xMachine *machine)
{
    if (!InterlockedDecrement(&machine->refs)) {
        SecureZeroMemory(machine, sizeof(*machine));
        free(machine);
    }
}

/* Its own thread, so the caller's COM apartment doesn't matter. */
static DWORD WINAPI Cod2x_MachineThread(LPVOID argument)
{
    Cod2xMachine *machine = argument;
    if (Cod2x_WmiMachine(machine)) {
        machine->ok = Cod2x_HwidFromMachine(machine->value, machine->failed, machine->id);
        if (!machine->ok) {
            machine->code = E_FAIL;
            Cod2x_HwidMissing(machine->value, machine->failed, machine->error, sizeof(machine->error));
        }
    }
    Cod2x_ReleaseMachine(machine);
    return 0;
}

static int Cod2x_Base64Encode(const unsigned char *data, size_t length, char *out, size_t capacity)
{
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i, used = 0;
    if ((length + 2) / 3 * 4 + 1 > capacity)
        return -1;
    for (i = 0; i < length; i += 3) {
        unsigned int bits = (unsigned int)data[i] << 16;
        if (i + 1 < length)
            bits |= (unsigned int)data[i + 1] << 8;
        if (i + 2 < length)
            bits |= data[i + 2];
        out[used++] = digits[bits >> 18 & 63];
        out[used++] = digits[bits >> 12 & 63];
        out[used++] = i + 1 < length ? digits[bits >> 6 & 63] : '=';
        out[used++] = i + 2 < length ? digits[bits & 63] : '=';
    }
    out[used] = '\0';
    return (int)used;
}

static int Cod2x_Base64Decode(const char *in, unsigned char *out, size_t capacity)
{
    static const char digits[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned int bits = 0;
    int pending = 0;
    size_t used = 0;
    for (; *in && *in != '='; ++in) {
        const char *digit = strchr(digits, *in);
        if (!digit)
            return -1;
        bits = bits << 6 | (unsigned int)(digit - digits);
        pending += 6;
        if (pending >= 8) {
            pending -= 8;
            if (used + 1 >= capacity)
                return -1;
            out[used++] = (unsigned char)(bits >> pending);
            bits &= (1u << pending) - 1;
        }
    }
    out[used] = '\0';
    return (int)used;
}

/* The next non-empty line, as strtok splits them. */
static char *Cod2x_NextLine(char **cursor)
{
    char *line = *cursor, *end;
    while (*line == '\n')
        ++line;
    if (!*line)
        return NULL;
    end = strchr(line, '\n');
    if (end)
        *end++ = '\0';
    else
        end = line + strlen(line);
    *cursor = end;
    return line;
}

static void Cod2x_HwidFatal(const char *message)
{
    Win_Cod2xFatal(message);
}

/* The values CoD2x keeps beside the game's own (mss32/hwid.cpp): this machine's
   cl_hwid2, the text it came from, the previous one and what changed, and a random
   REGID. They stay in the registry; nothing here sends them. */
static void Cod2x_HwidRegistry(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT], const char *id)
{
    char text[1024], stored[64] = {0}, source[2048] = {0}, encoded[2048], diff[1024] = {0};
    unsigned char decoded[2048];
    const char *previous;
    DWORD type = 0, size, count;
    /* 1.4.4.4 changed how the value is made; an older install starts over. */
    if (Win_Cod2xVersionChanged(&previous) && (!previous[0] || Win_Cod2xCompareVersions(previous, "1.4.4.4") < 0)) {
        Win_Cod2xRegDelete("HWID");
        Win_Cod2xRegDelete("HWID_OLD");
        Win_Cod2xRegDelete("HWID_SOURCE");
        Win_Cod2xRegDelete("HWID_DIFF");
        Win_Cod2xRegDelete("HWID_DIFF_COUNT");
    }
    Cod2x_HwidText(value, text, sizeof(text));
    size = sizeof(stored) - 1;
    if (Win_Cod2xRegRead("HWID", &type, stored, &size) && type == REG_SZ && strcmp(stored, id)) {
        if (!Win_Cod2xRegWrite("HWID_OLD", REG_SZ, stored, (DWORD)strlen(stored) + 1))
            Cod2x_HwidFatal("Failed to save old HWID to registry.");
        count = 1;
        size = sizeof(count);
        if (Win_Cod2xRegRead("HWID_DIFF_COUNT", &type, &count, &size) && type == REG_DWORD)
            ++count;
        else
            count = 1;
        if (!Win_Cod2xRegWrite("HWID_DIFF_COUNT", REG_DWORD, &count, sizeof(count)))
            Cod2x_HwidFatal("Failed to update HWID_DIFF_COUNT in registry.");
        size = sizeof(source) - 1;
        if (Win_Cod2xRegRead("HWID_SOURCE", &type, source, &size) && type == REG_SZ) {
            char oldLines[2048], newLines[1024], *oldCursor = oldLines, *newCursor = newLines, *oldLine, *newLine;
            if (Cod2x_Base64Decode(source, decoded, sizeof(decoded)) < 0)
                Cod2x_HwidFatal("Failed to decode HWID source from Base64.");
            if (!strcmp((const char *)decoded, text))
                Cod2x_HwidFatal("HWID source has not changed, but HWID hash is different. This should not happen.");
            /* Each line that changed, before and after. */
            snprintf(oldLines, sizeof(oldLines), "%s", (const char *)decoded);
            snprintf(newLines, sizeof(newLines), "%s", text);
            oldLine = Cod2x_NextLine(&oldCursor);
            newLine = Cod2x_NextLine(&newCursor);
            while (oldLine && newLine) {
                if (strcmp(oldLine, newLine)) {
                    char pair[256];
                    snprintf(pair, sizeof(pair), "%s\n%s\n", oldLine, newLine);
                    strncat(diff, pair, sizeof(diff) - strlen(diff) - 1);
                }
                oldLine = Cod2x_NextLine(&oldCursor);
                newLine = Cod2x_NextLine(&newCursor);
            }
            if (diff[0] && !Win_Cod2xRegWrite("HWID_DIFF", REG_SZ, diff, (DWORD)strlen(diff) + 1))
                Cod2x_HwidFatal("Failed to save HWID change difference to registry.");
        }
    }
    if (!Win_Cod2xRegWrite("HWID", REG_SZ, id, (DWORD)strlen(id) + 1))
        Cod2x_HwidFatal("Failed to save HWID to registry.");
    if (Cod2x_Base64Encode((const unsigned char *)text, strlen(text), encoded, sizeof(encoded)) < 0)
        Cod2x_HwidFatal("Failed to encode HWID source to Base64.");
    if (!Win_Cod2xRegWrite("HWID_SOURCE", REG_SZ, encoded, (DWORD)strlen(encoded) + 1))
        Cod2x_HwidFatal("Failed to save HWID source to registry.");
    /* REGID tells copies of one disk image apart. */
    size = sizeof(stored) - 1;
    memset(stored, 0, sizeof(stored));
    if (!Win_Cod2xRegRead("REGID", &type, stored, &size) || type != REG_SZ || !stored[0]) {
        char seed[64], regid[33];
        snprintf(seed, sizeof(seed), "%u_%u", (unsigned int)GetTickCount(), (unsigned int)rand());
        Cod2x_BuildMD5(seed, (unsigned int)strlen(seed), regid);
        Win_Cod2xRegWrite("REGID", REG_SZ, regid, (DWORD)strlen(regid) + 1);
    }
}

static int Cod2x_MachineHwid(char id[33])
{
    Cod2xMachine *machine = calloc(1, sizeof(*machine));
    HANDLE thread;
    int ok = 0;
    if (!machine)
        return 0;
    machine->refs = 2;
    thread = CreateThread(NULL, 0, Cod2x_MachineThread, machine, 0, NULL);
    if (!thread) {
        free(machine);
        return 0;
    }
    if (WaitForSingleObject(thread, COD2X_WMI_TOTAL_MS) != WAIT_OBJECT_0) {
        cod2x_hwidCode = (unsigned int)HRESULT_FROM_WIN32(WAIT_TIMEOUT);
        snprintf(cod2x_hwidError, sizeof(cod2x_hwidError),
                 "Timeout while reading WMI properties. Please check your system's WMI service.");
    } else if (machine->ok) {
        memcpy(id, machine->id, 33);
        Cod2x_HwidRegistry(machine->value, id);
        ok = 1;
    } else {
        cod2x_hwidCode = (unsigned int)machine->code;
        snprintf(cod2x_hwidError, sizeof(cod2x_hwidError), "%s", machine->error);
    }
    CloseHandle(thread);
    Cod2x_ReleaseMachine(machine);
    return ok;
}
#elif COD2X_HWID_IOKIT
/* The engine exports stubs under some IOKit and CoreFoundation names, so every
   call goes to the installed SDK images explicitly. */
typedef struct {
    void *iokit, *core;
    CFMutableDictionaryRef (*matching)(const char *);
    CFMutableDictionaryRef (*bsdMatching)(mach_port_t, uint32_t, const char *);
    io_service_t (*first)(mach_port_t, CFDictionaryRef);
    kern_return_t (*all)(mach_port_t, CFDictionaryRef, io_iterator_t *);
    io_object_t (*next)(io_iterator_t);
    kern_return_t (*releaseObject)(io_object_t);
    CFTypeRef (*property)(io_registry_entry_t, CFStringRef, CFAllocatorRef, IOOptionBits);
    CFTypeRef (*search)(io_registry_entry_t, const char *, CFStringRef, CFAllocatorRef, IOOptionBits);
    CFTypeID (*typeOf)(CFTypeRef);
    CFTypeID (*stringType)(void);
    CFTypeID (*dataType)(void);
    CFTypeID (*dictionaryType)(void);
    Boolean (*stringBytes)(CFStringRef, char *, CFIndex, CFStringEncoding);
    CFIndex (*dataLength)(CFDataRef);
    const UInt8 *(*dataBytes)(CFDataRef);
    const void *(*lookup)(CFDictionaryRef, const void *);
    void (*releaseValue)(CFTypeRef);
} Cod2xSDK;

static int Cod2x_OpenSDK(Cod2xSDK *sdk)
{
    memset(sdk, 0, sizeof(*sdk));
    sdk->iokit = dlopen("/System/Library/Frameworks/IOKit.framework/IOKit", RTLD_LAZY | RTLD_LOCAL);
    sdk->core = dlopen("/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation", RTLD_LAZY | RTLD_LOCAL);
    if (!sdk->iokit || !sdk->core)
        return 0;
#define COD2X_SDK(image, field, name) ((*(void **)&sdk->field = dlsym(sdk->image, name)) != NULL)
    return COD2X_SDK(iokit, matching, "IOServiceMatching") && COD2X_SDK(iokit, bsdMatching, "IOBSDNameMatching") &&
           COD2X_SDK(iokit, first, "IOServiceGetMatchingService") &&
           COD2X_SDK(iokit, all, "IOServiceGetMatchingServices") && COD2X_SDK(iokit, next, "IOIteratorNext") &&
           COD2X_SDK(iokit, releaseObject, "IOObjectRelease") &&
           COD2X_SDK(iokit, property, "IORegistryEntryCreateCFProperty") &&
           COD2X_SDK(iokit, search, "IORegistryEntrySearchCFProperty") && COD2X_SDK(core, typeOf, "CFGetTypeID") &&
           COD2X_SDK(core, stringType, "CFStringGetTypeID") && COD2X_SDK(core, dataType, "CFDataGetTypeID") &&
           COD2X_SDK(core, dictionaryType, "CFDictionaryGetTypeID") &&
           COD2X_SDK(core, stringBytes, "CFStringGetCString") && COD2X_SDK(core, dataLength, "CFDataGetLength") &&
           COD2X_SDK(core, dataBytes, "CFDataGetBytePtr") && COD2X_SDK(core, lookup, "CFDictionaryGetValue") &&
           COD2X_SDK(core, releaseValue, "CFRelease");
#undef COD2X_SDK
}

static void Cod2x_CloseSDK(Cod2xSDK *sdk)
{
    if (sdk->core)
        dlclose(sdk->core);
    if (sdk->iokit)
        dlclose(sdk->iokit);
}

/* A string, or data up to its first NUL; anything else is empty. */
static void Cod2x_CFText(const Cod2xSDK *sdk, CFTypeRef value, char out[COD2X_MACHINE_TEXT])
{
    CFIndex length;
    out[0] = '\0';
    if (!value)
        return;
    if (sdk->typeOf(value) == sdk->stringType()) {
        if (!sdk->stringBytes((CFStringRef)value, out, COD2X_MACHINE_TEXT, kCFStringEncodingUTF8))
            out[0] = '\0';
    } else if (sdk->typeOf(value) == sdk->dataType()) {
        length = sdk->dataLength((CFDataRef)value);
        if (length > COD2X_MACHINE_TEXT - 1)
            length = COD2X_MACHINE_TEXT - 1;
        memcpy(out, sdk->dataBytes((CFDataRef)value), (size_t)length);
        out[length] = '\0';
    }
}

static void Cod2x_IOText(const Cod2xSDK *sdk, io_registry_entry_t entry, CFStringRef name, int parents,
                         char out[COD2X_MACHINE_TEXT])
{
    CFTypeRef value = parents ? sdk->search(entry, kIOServicePlane, name, NULL,
                                            kIORegistryIterateRecursively | kIORegistryIterateParents)
                              : sdk->property(entry, name, NULL, 0);
    Cod2x_CFText(sdk, value, out);
    if (value)
        sdk->releaseValue(value);
}

/* Model and serial from the Device Characteristics of the disk under the boot volume. */
static void Cod2x_IOSystemDisk(const Cod2xSDK *sdk, char model[COD2X_MACHINE_TEXT], char serial[COD2X_MACHINE_TEXT])
{
    struct statfs root;
    const char *name;
    io_service_t media;
    CFTypeRef disk;
    if (statfs("/", &root))
        return;
    name = strncmp(root.f_mntfromname, "/dev/", 5) ? root.f_mntfromname : root.f_mntfromname + 5;
    media = sdk->first(MACH_PORT_NULL, sdk->bsdMatching(MACH_PORT_NULL, 0, name));
    if (!media)
        return;
    disk = sdk->search(media, kIOServicePlane, CFSTR("Device Characteristics"), NULL,
                       kIORegistryIterateRecursively | kIORegistryIterateParents);
    if (disk && sdk->typeOf(disk) == sdk->dictionaryType()) {
        Cod2x_CFText(sdk, sdk->lookup((CFDictionaryRef)disk, CFSTR("Product Name")), model);
        Cod2x_CFText(sdk, sdk->lookup((CFDictionaryRef)disk, CFSTR("Serial Number")), serial);
    }
    if (disk)
        sdk->releaseValue(disk);
    sdk->releaseObject(media);
}

/* The first accelerator whose model CoD2x wouldn't call integrated. */
static void Cod2x_IOGpu(const Cod2xSDK *sdk, char out[COD2X_MACHINE_TEXT])
{
    io_iterator_t list;
    io_object_t gpu;
    char name[COD2X_MACHINE_TEXT];
    out[0] = '\0';
    if (sdk->all(MACH_PORT_NULL, sdk->matching("IOAccelerator"), &list) != KERN_SUCCESS)
        return;
    while ((gpu = sdk->next(list))) {
        if (!out[0]) {
            Cod2x_IOText(sdk, gpu, CFSTR("model"), 1, name);
            if (name[0] && !Cod2x_IntegratedGpu(name))
                memcpy(out, name, sizeof(name));
        }
        sdk->releaseObject(gpu);
    }
    sdk->releaseObject(list);
}

static int Cod2x_ReadMachine(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT])
{
    Cod2xSDK sdk;
    io_service_t platform;
    size_t size;
    int ok = Cod2x_OpenSDK(&sdk);
    if (ok) {
        platform = sdk.first(MACH_PORT_NULL, sdk.matching("IOPlatformExpertDevice"));
        if (platform) {
            /* Apple makes both the firmware and the logic board. */
            Cod2x_IOText(&sdk, platform, CFSTR("manufacturer"), 0, value[COD2X_BIOS_MAKER]);
            memcpy(value[COD2X_BOARD_MAKER], value[COD2X_BIOS_MAKER], COD2X_MACHINE_TEXT);
            Cod2x_IOText(&sdk, platform, CFSTR("IOPlatformSerialNumber"), 0, value[COD2X_BIOS_SERIAL]);
            Cod2x_IOText(&sdk, platform, CFSTR("mlb-serial-number"), 0, value[COD2X_BOARD_SERIAL]);
            Cod2x_IOText(&sdk, platform, CFSTR("IOPlatformUUID"), 0, value[COD2X_PRODUCT_UUID]);
            sdk.releaseObject(platform);
        }
        Cod2x_IOSystemDisk(&sdk, value[COD2X_DISK_MODEL], value[COD2X_DISK_SERIAL]);
        Cod2x_IOGpu(&sdk, value[COD2X_GPU_NAME]);
    }
    Cod2x_CloseSDK(&sdk);
    size = COD2X_MACHINE_TEXT;
    if (sysctlbyname("machdep.cpu.brand_string", value[COD2X_CPU_NAME], &size, NULL, 0))
        value[COD2X_CPU_NAME][0] = '\0';
#if defined(__i386__) || defined(__x86_64__)
    Cod2x_ProcessorId(value[COD2X_CPU_ID]);
#else
    {
        /* No CPUID on arm64 Macs: the CPU family and subfamily, in its format. */
        uint32_t family = 0, subfamily = 0;
        size_t length = sizeof(family);
        if (!sysctlbyname("hw.cpufamily", &family, &length, NULL, 0) && family) {
            length = sizeof(subfamily);
            if (sysctlbyname("hw.cpusubfamily", &subfamily, &length, NULL, 0))
                subfamily = 0;
            snprintf(value[COD2X_CPU_ID], COD2X_MACHINE_TEXT, "%08X%08X", family, subfamily);
        }
    }
#endif
    /* The booted system volume is a snapshot that every update replaces, so there is
       no stable stand-in for CoD2x's volume serial. */
    if (!value[COD2X_DISK_SERIAL][0])
        snprintf(value[COD2X_DISK_SERIAL], COD2X_MACHINE_TEXT, "UnknownSerial");
    return ok;
}
#elif COD2X_HWID_SYSFS
/* The first line, without its line break. Unreadable is empty. */
static void Cod2x_FileText(const char *path, char out[COD2X_MACHINE_TEXT])
{
    FILE *file = fopen(path, "r");
    size_t length;
    out[0] = '\0';
    if (!file)
        return;
    if (!fgets(out, COD2X_MACHINE_TEXT, file))
        out[0] = '\0';
    fclose(file);
    length = strcspn(out, "\r\n");
    out[length] = '\0';
}

static void Cod2x_CpuName(char out[COD2X_MACHINE_TEXT])
{
    FILE *file = fopen("/proc/cpuinfo", "r");
    char line[512], *value;
    out[0] = '\0';
    if (!file)
        return;
    while (fgets(line, sizeof(line), file)) {
        if (strncmp(line, "model name", 10) || !(value = strchr(line, ':')))
            continue;
        value += strspn(value + 1, " \t") + 1;
        value[strcspn(value, "\r\n")] = '\0';
        snprintf(out, COD2X_MACHINE_TEXT, "%s", value);
        break;
    }
    fclose(file);
}

/* The block device mounted at /, from its mount source or else from / itself. */
static int Cod2x_RootDevice(dev_t *device)
{
    FILE *mounts = fopen("/proc/self/mounts", "r");
    char source[PATH_MAX], target[PATH_MAX];
    struct stat status;
    int found = 0;
    if (mounts) {
        while (fscanf(mounts, "%4095s %4095s%*[^\n]", source, target) == 2)
            if (!strcmp(target, "/") && !stat(source, &status) && S_ISBLK(status.st_mode)) {
                *device = status.st_rdev;
                found = 1;
            }
        fclose(mounts);
    }
    if (!found && !stat("/", &status)) {
        *device = status.st_dev;
        found = 1;
    }
    return found;
}

/* Model and serial of the disk under /: from a partition to its disk, and through
   device-mapper or RAID to the first disk below. */
static void Cod2x_SysSystemDisk(char model[COD2X_MACHINE_TEXT], char serial[COD2X_MACHINE_TEXT])
{
    char path[PATH_MAX], probe[PATH_MAX + 320];
    unsigned char page[4 + COD2X_MACHINE_TEXT];
    struct statvfs volume;
    dev_t device;
    FILE *file;
    size_t length;
    int depth;
    if (Cod2x_RootDevice(&device)) {
        snprintf(probe, sizeof(probe), "/sys/dev/block/%u:%u", major(device), minor(device));
        if (realpath(probe, path)) {
            for (depth = 0; depth < 8; ++depth) {
                DIR *below;
                struct dirent *entry;
                int moved = 0;
                snprintf(probe, sizeof(probe), "%s/partition", path);
                if (!access(probe, F_OK) && strrchr(path, '/')) {
                    *strrchr(path, '/') = '\0';
                    continue;
                }
                snprintf(probe, sizeof(probe), "%s/slaves", path);
                if ((below = opendir(probe))) {
                    while ((entry = readdir(below)))
                        if (entry->d_name[0] != '.') {
                            snprintf(probe, sizeof(probe), "%s/slaves/%s", path, entry->d_name);
                            moved = realpath(probe, path) != NULL;
                            break;
                        }
                    closedir(below);
                }
                if (!moved)
                    break;
            }
            snprintf(probe, sizeof(probe), "%s/device/model", path);
            Cod2x_FileText(probe, model);
            snprintf(probe, sizeof(probe), "%s/device/serial", path);
            Cod2x_FileText(probe, serial);
            /* SCSI and SATA disks: the unit serial number page. */
            snprintf(probe, sizeof(probe), "%s/device/vpd_pg80", path);
            if (!serial[0] && (file = fopen(probe, "rb"))) {
                length = fread(page, 1, sizeof(page), file);
                fclose(file);
                if (length > 4 && page[1] == 0x80) {
                    size_t size = (size_t)page[2] << 8 | page[3];
                    if (size > length - 4)
                        size = length - 4;
                    if (size > COD2X_MACHINE_TEXT - 1)
                        size = COD2X_MACHINE_TEXT - 1;
                    memcpy(serial, page + 4, size);
                    serial[size] = '\0';
                }
            }
        }
    }
    /* CoD2x's fallback is the 32-bit serial of the system volume. */
    if (!serial[0]) {
        if (!statvfs("/", &volume) && (unsigned long)(uint32_t)volume.f_fsid)
            snprintf(serial, COD2X_MACHINE_TEXT, "%lu", (unsigned long)(uint32_t)volume.f_fsid);
        else
            snprintf(serial, COD2X_MACHINE_TEXT, "UnknownSerial");
    }
}

/* sysfs has no adapter names, so the PCI vendor:device ID stands in. The first
   adapter not made by Intel counts as discrete; with none, no name, as in CoD2x. */
static void Cod2x_SysGpu(char out[COD2X_MACHINE_TEXT])
{
    char path[64], line[COD2X_MACHINE_TEXT];
    FILE *file;
    int card;
    out[0] = '\0';
    for (card = 0; card < 16 && !out[0]; ++card) {
        snprintf(path, sizeof(path), "/sys/class/drm/card%d/device/uevent", card);
        if (!(file = fopen(path, "r")))
            continue;
        while (fgets(line, sizeof(line), file))
            if (!strncmp(line, "PCI_ID=", 7) && strncmp(line + 7, "8086:", 5)) {
                line[strcspn(line, "\r\n")] = '\0';
                snprintf(out, COD2X_MACHINE_TEXT, "%s", line + 7);
                break;
            }
        fclose(file);
    }
}

static int Cod2x_ReadMachine(char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT])
{
    /* The serials and the UUID need root on most systems; unreadable counts as empty. */
    Cod2x_FileText("/sys/class/dmi/id/bios_vendor", value[COD2X_BIOS_MAKER]);
    Cod2x_FileText("/sys/class/dmi/id/product_serial", value[COD2X_BIOS_SERIAL]);
    Cod2x_CpuName(value[COD2X_CPU_NAME]);
#if defined(__i386__) || defined(__x86_64__)
    Cod2x_ProcessorId(value[COD2X_CPU_ID]);
#endif
    Cod2x_FileText("/sys/class/dmi/id/board_vendor", value[COD2X_BOARD_MAKER]);
    Cod2x_FileText("/sys/class/dmi/id/board_serial", value[COD2X_BOARD_SERIAL]);
    Cod2x_FileText("/sys/class/dmi/id/product_uuid", value[COD2X_PRODUCT_UUID]);
    if (!value[COD2X_PRODUCT_UUID][0])
        Cod2x_FileText("/etc/machine-id", value[COD2X_PRODUCT_UUID]);
    Cod2x_SysSystemDisk(value[COD2X_DISK_MODEL], value[COD2X_DISK_SERIAL]);
    Cod2x_SysGpu(value[COD2X_GPU_NAME]);
    return 1;
}
#endif

#if COD2X_HWID_IOKIT || COD2X_HWID_SYSFS
static int Cod2x_MachineHwid(char id[33])
{
    char value[COD2X_MACHINE_VALUES][COD2X_MACHINE_TEXT];
    int ok;
    memset(value, 0, sizeof(value));
    ok = Cod2x_ReadMachine(value) && Cod2x_HwidFromMachine(value, 0, id);
    if (!ok) {
        cod2x_hwidCode = 0x80004005u; /* E_FAIL, as CoD2x reports it */
        Cod2x_HwidMissing(value, 0, cod2x_hwidError, sizeof(cod2x_hwidError));
    }
    memset(value, 0, sizeof(value));
    return ok;
}
#elif !COD2X_HWID_WMI
static int Cod2x_MachineHwid(char id[33])
{
    (void)id;
    return 0;
}
#endif
#endif
#if defined(__APPLE__)
#include "cdkey_hash.h"
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#if !defined(COD2_X64) || !COD2_X64

static void Cod2x_Hex(const unsigned char *bytes, char hex[33])
{
    static const char digits[] = "0123456789abcdef";
    size_t i;
    for (i = 0; i < 16; ++i) {
        hex[2 * i] = digits[bytes[i] >> 4];
        hex[2 * i + 1] = digits[bytes[i] & 15];
    }
    hex[32] = '\0';
}
#endif
#endif

#if !defined(COD2_X64) || !COD2_X64
int Cod2x_HwidFromUUID(const char *uuid, char id[33])
{
    id[0] = '\0';
#if defined(__APPLE__)
    /* Domain-separated SHA-256, truncated to 128 bits. No raw UUID is sent.
       CoD2x src/shared/server.cpp:428 requires a 32-character identity. */
    char input[] = "opencod2:cod2x:hwid2:v1:00000000-0000-0000-0000-000000000000";
    const size_t prefix = sizeof("opencod2:cod2x:hwid2:v1:") - 1;
    unsigned char digest[CC_SHA256_DIGEST_LENGTH];
    size_t i;
    int nonzero = 0;
    if (!uuid || strlen(uuid) != 36)
        return 0;
    for (i = 0; i < 36; ++i) {
        char c = uuid[i];
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (c != '-')
                return 0;
        } else {
            if (c >= 'A' && c <= 'F')
                c += 'a' - 'A';
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
                return 0;
            nonzero |= c != '0';
        }
        input[prefix + i] = c;
    }
    if (!nonzero)
        return 0;
    CC_SHA256(input, (CC_LONG)(sizeof(input) - 1), digest);
    Cod2x_Hex(digest, id);
    return 1;
#else
    (void)uuid;
    return 0;
#endif
}
#endif

int Cod2x_ReadMachineHwid(char id[33])
{
    id[0] = '\0';
#if defined(COD2_X64) && COD2_X64
    return Cod2x_MachineHwid(id);
#elif defined(__APPLE__)
    io_service_t platform;
    CFTypeRef value;
    char uuid[64];
    int ok = 0;
    platform = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (!platform)
        return 0;
    value = IORegistryEntryCreateCFProperty(platform, CFSTR("IOPlatformUUID"), kCFAllocatorDefault, 0);
    IOObjectRelease(platform);
    if (value) {
        if (CFGetTypeID(value) == CFStringGetTypeID() &&
            CFStringGetCString((CFStringRef)value, uuid, sizeof(uuid), kCFStringEncodingASCII))
            ok = Cod2x_HwidFromUUID(uuid, id);
        CFRelease(value);
    }
    return ok;
#else
    return 0;
#endif
}

int Cod2x_CDKeyHash(const char *key, char hash[33])
{
    hash[0] = '\0';
#if defined(__APPLE__)
    char normalized[33];
    size_t i, n = 0;
    if (!key)
        return 0;
    for (i = 0; key[i] && i < 32; ++i) {
        unsigned char c = (unsigned char)key[i];
        if (c >= 'a' && c <= 'z')
            c -= 'a' - 'A';
        if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            normalized[n++] = (char)c;
    }
    if (!n)
        return 0;
    Com_CDKeyHashBytes(normalized, (unsigned int)n, hash);
    return 1;
#else
    (void)key;
    return 0;
#endif
}
#endif
