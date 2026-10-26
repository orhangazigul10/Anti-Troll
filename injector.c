/* antitroll.dll dosyasini Among Us.exe icine yukler (LoadLibrary). */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <wchar.h>

static DWORD find_pid(const wchar_t *name) {
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    DWORD pid = 0;
    if (snap == INVALID_HANDLE_VALUE) return 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

static int is_loaded(DWORD pid, const wchar_t *modname) {
    MODULEENTRY32W me;
    me.dwSize = sizeof(me);
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    int found = 0;
    if (snap == INVALID_HANDLE_VALUE) return 0;
    if (Module32FirstW(snap, &me)) {
        do {
            if (_wcsicmp(me.szModule, modname) == 0) { found = 1; break; }
        } while (Module32NextW(snap, &me));
    }
    CloseHandle(snap);
    return found;
}

int main(void) {
    const wchar_t *game = L"Among Us.exe";
    wchar_t dll[MAX_PATH];
    GetModuleFileNameW(NULL, dll, MAX_PATH);
    wchar_t *slash = wcsrchr(dll, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscat(dll, L"antitroll.dll");

    if (GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES) {
        printf("HATA: antitroll.dll bulunamadi. Once build.bat calistir.\n");
        return 1;
    }
    DWORD pid = find_pid(game);
    if (!pid) { printf("HATA: Among Us.exe calismiyor.\n"); return 1; }

    if (is_loaded(pid, L"antitroll.dll")) {
        printf("antitroll.dll zaten yuklu. Yeni surum icin oyunu kapatip tekrar ac.\n");
        return 0;
    }

    HANDLE proc = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                              PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, pid);
    if (!proc) { printf("HATA: surece erisilemedi (kod %lu). Yonetici olarak dene.\n", GetLastError()); return 1; }

    SIZE_T bytes = (wcslen(dll) + 1) * sizeof(wchar_t);
    void *remote = VirtualAllocEx(proc, NULL, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!remote || !WriteProcessMemory(proc, remote, dll, bytes, NULL)) {
        printf("HATA: bellek yazilamadi (kod %lu).\n", GetLastError());
        return 1;
    }
    LPTHREAD_START_ROUTINE load = (LPTHREAD_START_ROUTINE)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
    HANDLE th = CreateRemoteThread(proc, NULL, 0, load, remote, 0, NULL);
    if (!th) { printf("HATA: thread olusturulamadi (kod %lu).\n", GetLastError()); return 1; }
    WaitForSingleObject(th, 10000);
    DWORD ret = 0;
    GetExitCodeThread(th, &ret);
    VirtualFreeEx(proc, remote, 0, MEM_RELEASE);
    CloseHandle(th);
    CloseHandle(proc);
    printf(ret ? "Yuklendi.\n" : "HATA: LoadLibrary basarisiz.\n");
    return ret ? 0 : 1;
}
