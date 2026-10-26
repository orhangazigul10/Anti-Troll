/* Among Us anti-troll (host-only)
   Mac basladiktan ilk N saniye acil toplanti butonunu engeller.
   Ceset raporuna dokunmaz. Meeting sonrasi cooldown oyunun kendi ayari olarak kalir.
   Hook: PlayerControl.ReportDeadBody(target) -- target == NULL ise buton basisi.
   IL2CPP fonksiyonlari calisma aninda isimle cozulur, sabit offset yok. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include "MinHook.h"

typedef void*       (*fn_domain_get)(void);
typedef void**      (*fn_domain_get_assemblies)(void *domain, size_t *size);
typedef void*       (*fn_assembly_get_image)(void *assembly);
typedef const char* (*fn_image_get_name)(void *image);
typedef void*       (*fn_class_from_name)(void *image, const char *ns, const char *name);
typedef void*       (*fn_class_get_method_from_name)(void *klass, const char *name, int argc);
typedef void*       (*fn_class_get_field_from_name)(void *klass, const char *name);
typedef void        (*fn_field_static_get_value)(void *field, void *value);
typedef void*       (*fn_thread_attach)(void *domain);

typedef void (*ReportDeadBody_t)(void *self, void *target, void *method_info);

static HINSTANCE g_self;
static FILE *g_log;
static ReportDeadBody_t g_orig_report;
static volatile ULONGLONG g_match_start;   /* 0 = mac yok */
static int g_block_ms = 45000;

static void *g_domain, *g_ship_field;
static fn_thread_attach p_thread_attach;
static fn_field_static_get_value p_static_get;

static void logf_(const char *fmt, ...) {
    if (!g_log) return;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);
    fflush(g_log);
}

static void hk_report(void *self, void *target, void *mi) {
    ULONGLONG start = g_match_start;
    if (target == NULL && start != 0) {
        ULONGLONG elapsed = GetTickCount64() - start;
        if (elapsed < (ULONGLONG)g_block_ms) {
            logf_("buton engellendi (maca %llu ms)\n", (unsigned long long)elapsed);
            return;
        }
    }
    g_orig_report(self, target, mi);
}

/* ShipStatus.Instance degisimini izleyerek yeni mac / mac sonu anlar. */
static DWORD WINAPI poll_thread(LPVOID unused) {
    (void)unused;
    p_thread_attach(g_domain);
    void *last = NULL;
    for (;;) {
        void *inst = NULL;
        p_static_get(g_ship_field, &inst);
        if (inst != last) {
            last = inst;
            if (inst) {
                g_match_start = GetTickCount64();
                logf_("yeni mac algilandi, %d sn buton kilidi basladi\n", g_block_ms / 1000);
            } else {
                g_match_start = 0;
                logf_("mac bitti\n");
            }
        }
        Sleep(250);
    }
    return 0;
}

#define GETFN(var, type, name) \
    type var = (type)(void *)GetProcAddress(ga, name); \
    if (!var) { logf_("HATA: %s bulunamadi.\n", name); return 1; }

static DWORD WINAPI worker(LPVOID unused) {
    (void)unused;
    wchar_t dir[MAX_PATH], logp[MAX_PATH], ini[MAX_PATH];
    GetModuleFileNameW(g_self, dir, MAX_PATH);
    wchar_t *slash = wcsrchr(dir, L'\\');
    if (slash) *(slash + 1) = 0;
    wcscpy(logp, dir); wcscat(logp, L"antitroll_log.txt");
    wcscpy(ini, dir);  wcscat(ini, L"antitroll.ini");
    g_log = _wfopen(logp, L"w");

    int secs = (int)GetPrivateProfileIntW(L"antitroll", L"first_round_seconds", 45, ini);
    if (secs < 0) secs = 0;
    g_block_ms = secs * 1000;
    logf_("antitroll: basladi (kilit suresi %d sn)\n", secs);

    HMODULE ga;
    while (!(ga = GetModuleHandleA("GameAssembly.dll"))) Sleep(500);

    GETFN(domain_get,   fn_domain_get,              "il2cpp_domain_get")
    GETFN(get_asms,     fn_domain_get_assemblies,   "il2cpp_domain_get_assemblies")
    GETFN(asm_image,    fn_assembly_get_image,      "il2cpp_assembly_get_image")
    GETFN(image_name,   fn_image_get_name,          "il2cpp_image_get_name")
    GETFN(class_by_name, fn_class_from_name,        "il2cpp_class_from_name")
    GETFN(method_by_name, fn_class_get_method_from_name, "il2cpp_class_get_method_from_name")
    GETFN(field_by_name, fn_class_get_field_from_name, "il2cpp_class_get_field_from_name")
    GETFN(static_get,   fn_field_static_get_value,  "il2cpp_field_static_get_value")
    GETFN(thread_attach, fn_thread_attach,          "il2cpp_thread_attach")

    void *image = NULL;
    for (int tries = 0; tries < 240 && !image; tries++) {
        g_domain = domain_get();
        if (g_domain) {
            size_t n = 0;
            void **asms = get_asms(g_domain, &n);
            for (size_t i = 0; asms && i < n; i++) {
                void *img = asm_image(asms[i]);
                const char *nm = img ? image_name(img) : NULL;
                if (nm && strstr(nm, "Assembly-CSharp")) { image = img; break; }
            }
        }
        if (!image) Sleep(500);
    }
    if (!image) { logf_("HATA: Assembly-CSharp bulunamadi.\n"); return 1; }

    Sleep(3000);
    thread_attach(g_domain);

    void *pc = class_by_name(image, "", "PlayerControl");
    void *ship = class_by_name(image, "", "ShipStatus");
    if (!pc || !ship) { logf_("HATA: PlayerControl/ShipStatus sinifi bulunamadi.\n"); return 1; }

    void *mi = method_by_name(pc, "ReportDeadBody", 1);
    if (!mi || !*(void **)mi) { logf_("HATA: ReportDeadBody bulunamadi.\n"); return 1; }
    void *target = *(void **)mi;   /* MethodInfo.methodPointer */

    g_ship_field = field_by_name(ship, "Instance");
    if (!g_ship_field) { logf_("HATA: ShipStatus.Instance alani bulunamadi.\n"); return 1; }
    p_thread_attach = thread_attach;
    p_static_get = static_get;

    if (MH_Initialize() != MH_OK) { logf_("HATA: MH_Initialize\n"); return 1; }
    if (MH_CreateHook(target, (LPVOID)hk_report, (LPVOID *)&g_orig_report) != MH_OK) {
        logf_("HATA: MH_CreateHook\n"); return 1;
    }
    if (MH_EnableHook(target) != MH_OK) { logf_("HATA: MH_EnableHook\n"); return 1; }

    CreateThread(NULL, 0, poll_thread, NULL, 0, NULL);
    logf_("hazir: ReportDeadBody hooklandi (rva=0x%llX)\n",
          (unsigned long long)((uintptr_t)target - (uintptr_t)ga));
    return 0;
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        g_self = h;
        DisableThreadLibraryCalls(h);
        CreateThread(NULL, 0, worker, NULL, 0, NULL);
    }
    return TRUE;
}
