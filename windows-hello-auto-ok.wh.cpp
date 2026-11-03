// ==WindhawkMod==
// @id              windows-hello-auto-ok
// @name            Windows Hello Auto OK
// @description     Language-independent confirmation of ready Windows Hello biometric dialogs across applications.
// @version         0.1.0
// @author          Contributors
// @include         windhawk.exe
// @architecture    x86
// @architecture    x86-64
// @compilerOptions -lole32 -loleaut32 -luuid -ladvapi32 -lshell32 -luiautomationcore
// @license         MIT
// ==/WindhawkMod==

// ==WindhawkModReadme==
/*
# Windows Hello Auto OK

Confirms the ready OK button in standard Windows Hello biometric dialogs,
regardless of the requesting application or interface language.

Detection uses AutomationId, control types, visibility and enabled state.
It never matches captions, translated messages, account names or websites.
Only a foreground window owned by the native Windows CredentialUIBroker is
considered. Visible credential input fields prevent automatic confirmation.

The mod runs in an isolated Windhawk tool process. Windows requires elevated
access to credential UI, so enabling the mod may display a UAC consent prompt.
Declining that prompt leaves automatic confirmation inactive; toggle the mod
off and on to retry. No scheduled tasks or services are created by this mod.

This is an experimental release. Its readiness check describes the standard
Windows Hello UI structure, not an independent biometric-success API. Windows
still performs authentication. Custom application dialogs and future changes
to the system UI may require an update.
*/
// ==/WindhawkModReadme==

// ==WindhawkModSettings==
/*
- Enabled: true
  $name: Enable automatic confirmation
- DelayMs: 350
  $name: Confirmation delay in milliseconds
  $description: A stable ready state is required for 150 to 3000 milliseconds.
*/
// ==/WindhawkModSettings==

#include <windows.h>
#include <uiautomation.h>
#include <wrl/client.h>
#include <shellapi.h>
#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

using Microsoft::WRL::ComPtr;

namespace HelloAutoOk {
struct Node {
    std::wstring automationId;
    CONTROLTYPEID type = 0;
    bool enabled = false;
    bool visible = true;
    bool password = false;
#ifndef HELLO_AUTO_OK_TEST
    ComPtr<IUIAutomationElement> element;
#endif
};

bool IsConfirmation(const Node& node) {
    return node.visible && node.enabled && node.type == UIA_ButtonControlTypeId &&
        node.automationId == L"OkButton";
}

bool IsReady(const std::vector<Node>& nodes) {
    int confirmations = 0;
    int confirmationIds = 0;
    int biometricFeedback = 0;
    int credentialLists = 0;
    for (const auto& node : nodes) {
        if (!node.visible) continue;
        if (node.enabled && (node.password || node.type == UIA_EditControlTypeId)) return false;
        if (node.automationId == L"OkButton") ++confirmationIds;
        if (IsConfirmation(node)) ++confirmations;
        if (node.automationId == L"BioFeedbackText" && node.type == UIA_TextControlTypeId)
            ++biometricFeedback;
        if (node.automationId == L"CredentialsListV2" && node.type == UIA_ListControlTypeId)
            ++credentialLists;
    }
    return confirmations == 1 && confirmationIds == 1 &&
        biometricFeedback == 1 && credentialLists == 1;
}

#ifndef HELLO_AUTO_OK_TEST
HANDLE stopEvent = nullptr;
HANDLE worker = nullptr;
std::atomic<bool> enabled{true};
std::atomic<DWORD> delayMs{350};
std::wstring nativeBroker;
std::wstring wowBroker;

bool Elevated() {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation{};
    DWORD size = 0;
    bool result = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size) &&
                  elevation.TokenIsElevated;
    CloseHandle(token);
    return result;
}

void SetState(PCWSTR state) {
    Wh_SetStringValue(L"runtime-state", state);
    Wh_Log(L"%s", state);
}

bool IsBroker(HWND window) {
    if (!window || !IsWindowVisible(window)) return false;
    DWORD pid = 0;
    GetWindowThreadProcessId(window, &pid);
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process) return false;
    WCHAR path[32768];
    DWORD length = ARRAYSIZE(path);
    bool result = QueryFullProcessImageNameW(process, 0, path, &length) &&
        (_wcsicmp(path, nativeBroker.c_str()) == 0 || _wcsicmp(path, wowBroker.c_str()) == 0);
    CloseHandle(process);
    return result;
}

bool Read(IUIAutomation* automation, HWND window, std::vector<Node>* result) {
    result->clear();
    if (!IsBroker(window)) return false;
    ComPtr<IUIAutomationElement> root;
    ComPtr<IUIAutomationCondition> condition;
    ComPtr<IUIAutomationElementArray> elements;
    if (FAILED(automation->ElementFromHandle(window, &root)) ||
        FAILED(automation->CreateTrueCondition(&condition)) ||
        FAILED(root->FindAll(TreeScope_Descendants, condition.Get(), &elements))) return false;
    int count = 0;
    if (FAILED(elements->get_Length(&count)) || count > 160) return false;
    for (int i = 0; i < count; ++i) {
        ComPtr<IUIAutomationElement> element;
        BOOL offscreen = TRUE, isEnabled = FALSE, password = TRUE;
        CONTROLTYPEID type = 0;
        if (FAILED(elements->GetElement(i, &element)) ||
            FAILED(element->get_CurrentIsOffscreen(&offscreen))) return false;
        if (offscreen) continue;
        if (FAILED(element->get_CurrentIsEnabled(&isEnabled)) ||
            FAILED(element->get_CurrentIsPassword(&password)) ||
            FAILED(element->get_CurrentControlType(&type))) return false;
        Node node;
        node.enabled = isEnabled;
        node.password = password;
        node.type = type;
        // Credential values and localized Name/Text properties are never queried.
        if (!password && type != UIA_EditControlTypeId) {
            BSTR id = nullptr;
            HRESULT hr = element->get_CurrentAutomationId(&id);
            if (FAILED(hr)) { SysFreeString(id); return false; }
            if (id) node.automationId.assign(id, SysStringLen(id));
            SysFreeString(id);
        }
        node.element = element;
        result->push_back(std::move(node));
    }
    return true;
}

void LoadSettings() {
    enabled.store(Wh_GetIntSetting(L"Enabled") != 0);
    delayMs.store(static_cast<DWORD>(std::clamp(Wh_GetIntSetting(L"DelayMs"), 150, 3000)));
}

DWORD WINAPI Watch(void*) {
    HRESULT apartment = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(apartment)) { SetState(L"com-initialization-failed"); return 1; }
    {
        ComPtr<IUIAutomation> automation;
        if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS(&automation)))) {
            SetState(L"uia-initialization-failed");
        } else {
            HWND candidate = nullptr, handled = nullptr;
            ULONGLONG readySince = 0;
            SetState(L"watching");
            while (WaitForSingleObject(stopEvent, 150) == WAIT_TIMEOUT) {
                try {
                    if (!enabled.load()) { candidate = nullptr; continue; }
                    HWND window = GetForegroundWindow();
                    if (!IsBroker(window)) { candidate = nullptr; handled = nullptr; continue; }
                    if (window == handled) continue;
                    std::vector<Node> nodes;
                    if (!Read(automation.Get(), window, &nodes) || !IsReady(nodes)) {
                        candidate = nullptr; continue;
                    }
                    if (candidate != window) {
                        candidate = window; readySince = GetTickCount64(); continue;
                    }
                    if (GetTickCount64() - readySince < delayMs.load()) continue;
                    std::vector<Node> current;
                    if (GetForegroundWindow() != window || !enabled.load() ||
                        !Read(automation.Get(), window, &current) || !IsReady(current)) {
                        candidate = nullptr; continue;
                    }
                    auto button = std::find_if(current.begin(), current.end(), IsConfirmation);
                    ComPtr<IUIAutomationInvokePattern> invoke;
                    handled = window;
                    candidate = nullptr;
                    if (FAILED(button->element->GetCurrentPatternAs(UIA_InvokePatternId,
                                                                    IID_PPV_ARGS(&invoke)))) {
                        SetState(L"invoke-unavailable"); continue;
                    }
                    if (GetForegroundWindow() != window || !enabled.load()) continue;
                    SetState(L"invoking-confirmation");
                    if (FAILED(invoke->Invoke())) { SetState(L"invoke-failed"); continue; }
                    if (WaitForSingleObject(stopEvent, 900) != WAIT_TIMEOUT) break;
                    if (!IsWindow(window) || !IsWindowVisible(window)) {
                        Wh_SetIntValue(L"confirmed-count", Wh_GetIntValue(L"confirmed-count", 0) + 1);
                        SetState(L"confirmed");
                    } else SetState(L"confirmation-result-unknown");
                } catch (...) { candidate = nullptr; SetState(L"snapshot-error"); }
            }
        }
    }
    CoUninitialize();
    SetState(L"stopped");
    return 0;
}
#endif
}  // namespace HelloAutoOk

#ifndef HELLO_AUTO_OK_TEST
BOOL WhTool_ModInit() {
    using namespace HelloAutoOk;
    if (!Elevated()) { SetState(L"elevation-required"); return FALSE; }
    WCHAR windows[MAX_PATH];
    UINT length = GetSystemWindowsDirectoryW(windows, ARRAYSIZE(windows));
    if (!length || length >= ARRAYSIZE(windows)) return FALSE;
    // GetSystemDirectory would be redirected when the tool host is 32-bit.
    nativeBroker = std::wstring(windows) + L"\\System32\\CredentialUIBroker.exe";
    wowBroker = std::wstring(windows) + L"\\SysWOW64\\CredentialUIBroker.exe";
    LoadSettings();
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent) return FALSE;
    worker = CreateThread(nullptr, 0, Watch, nullptr, 0, nullptr);
    if (!worker) { CloseHandle(stopEvent); stopEvent = nullptr; return FALSE; }
    return TRUE;
}

void WhTool_ModSettingsChanged() { HelloAutoOk::LoadSettings(); }

void WhTool_ModUninit() {
    using namespace HelloAutoOk;
    if (stopEvent) SetEvent(stopEvent);
    if (worker) {
        // Only this dedicated host is terminated if a UIA provider becomes stuck.
        if (WaitForSingleObject(worker, 5000) != WAIT_OBJECT_0) ExitProcess(0);
        CloseHandle(worker); worker = nullptr;
    }
    if (stopEvent) { CloseHandle(stopEvent); stopEvent = nullptr; }
}

// Adapted from the official Windhawk 1.7.3 tool-mod pattern:
// https://github.com/ramensoftware/windhawk/wiki/Mods-as-tools:-Running-mods-in-a-dedicated-process
// The launcher explicitly requests UAC elevation for its isolated tool host.
bool g_isToolModProcessLauncher;
HANDLE g_toolModProcessMutex;

void WINAPI EntryPoint_Hook() { ExitThread(0); }

BOOL Wh_ModInit() {
    DWORD sessionId;
    if (ProcessIdToSessionId(GetCurrentProcessId(), &sessionId) && sessionId == 0) return FALSE;
    bool excluded = false, toolProcess = false, ownToolProcess = false;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return FALSE;
    for (int i = 1; i < argc; ++i) {
        for (const auto* flag : {L"-service", L"-service-start", L"-service-stop", L"-exit",
                                 L"-restart", L"-safe-mode", L"-check-for-updates",
                                 L"-app-settings-changed", L"-run-ui"}) {
            if (wcscmp(argv[i], flag) == 0) excluded = true;
        }
    }
    for (int i = 1; i < argc - 1; ++i) {
        if (wcscmp(argv[i], L"-tool-mod") == 0) {
            toolProcess = true;
            ownToolProcess = wcscmp(argv[i + 1], WH_MOD_ID) == 0;
            break;
        }
    }
    LocalFree(argv);
    if (excluded) return FALSE;
    if (ownToolProcess) {
        g_toolModProcessMutex = CreateMutexW(nullptr, TRUE, L"windhawk-tool-mod_" WH_MOD_ID);
        if (!g_toolModProcessMutex || GetLastError() == ERROR_ALREADY_EXISTS) ExitProcess(1);
        if (!WhTool_ModInit()) ExitProcess(1);
        auto* base = reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
        auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
        auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
        Wh_SetFunctionHook(base + nt->OptionalHeader.AddressOfEntryPoint,
                           reinterpret_cast<void*>(EntryPoint_Hook), nullptr);
        return TRUE;
    }
    if (toolProcess) return FALSE;
    g_isToolModProcessLauncher = true;
    return TRUE;
}

void Wh_ModAfterInit() {
    if (!g_isToolModProcessLauncher) return;
    HANDLE existing = OpenMutexW(SYNCHRONIZE, FALSE, L"windhawk-tool-mod_" WH_MOD_ID);
    if (existing) { CloseHandle(existing); return; }
    if (GetLastError() == ERROR_ACCESS_DENIED) return;
    WCHAR path[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, path, ARRAYSIZE(path));
    if (!length || length >= ARRAYSIZE(path)) return;
    const std::wstring arguments = L"-tool-mod \"" WH_MOD_ID L"\"";
    HRESULT apartment = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    SHELLEXECUTEINFOW info{sizeof(info)};
    info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    info.lpVerb = HelloAutoOk::Elevated() ? nullptr : L"runas";
    info.lpFile = path;
    info.lpParameters = arguments.c_str();
    info.nShow = SW_HIDE;
    if (!ShellExecuteExW(&info)) {
        Wh_Log(L"Tool host not started; error %lu", GetLastError());
    } else if (info.hProcess) CloseHandle(info.hProcess);
    if (SUCCEEDED(apartment)) CoUninitialize();
}

void Wh_ModSettingsChanged() {
    if (!g_isToolModProcessLauncher) WhTool_ModSettingsChanged();
}

void Wh_ModUninit() {
    if (g_isToolModProcessLauncher) return;
    WhTool_ModUninit();
    ExitProcess(0);
}
#endif
