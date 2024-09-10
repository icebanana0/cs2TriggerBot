#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <thread>
#include <random>
#include "offsets.hpp"
#include "client.dll.hpp"

DWORD GetProcessID(const std::wstring& processName) {
    std::wcout << L"[*] Searching for process: " << processName << std::endl;

    PROCESSENTRY32 pe32 = { sizeof(PROCESSENTRY32) };
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        std::wcout << L"[!] Failed to create process snapshot." << std::endl;
        return 0;
    }

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            if (!_wcsicmp(pe32.szExeFile, processName.c_str())) {
                std::wcout << L"[*] Found process: " << pe32.szExeFile << L" with PID: " << pe32.th32ProcessID << std::endl;
                CloseHandle(hProcessSnap);
                return pe32.th32ProcessID;
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }

    std::wcout << L"[!] Process not found." << std::endl;
    CloseHandle(hProcessSnap);
    return 0;
}

DWORD_PTR GetModuleBaseAddress(DWORD processID, const std::wstring& moduleName) {
    std::wcout << L"[*] Searching for module: " << moduleName << std::endl;

    MODULEENTRY32 me32 = { sizeof(MODULEENTRY32) };
    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, processID);
    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        std::wcout << L"[!] Failed to create module snapshot." << std::endl;
        return 0;
    }

    if (Module32First(hModuleSnap, &me32)) {
        do {
            if (!_wcsicmp(me32.szModule, moduleName.c_str())) {
                std::wcout << L"[*] Found module: " << me32.szModule << L" with base address: "
                    << reinterpret_cast<void*>(me32.modBaseAddr) << std::endl;
                CloseHandle(hModuleSnap);
                return reinterpret_cast<DWORD_PTR>(me32.modBaseAddr);
            }
        } while (Module32Next(hModuleSnap, &me32));
    }

    std::wcout << L"[!] Module not found." << std::endl;
    CloseHandle(hModuleSnap);
    return 0;
}

template <typename T> T Read(HANDLE hProcess, uintptr_t address) {
    T ret;
    ReadProcessMemory(hProcess, (LPCVOID)address, &ret, sizeof(T), nullptr);
    return ret;
}

template <typename T> bool Write(HANDLE hProcess, uintptr_t address, T value) {
    return WriteProcessMemory(hProcess, (LPVOID)address, &value, sizeof(T), nullptr);
}

void MouseClick() {
    INPUT input = { 0 };
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    SendInput(1, &input, sizeof(INPUT));

    Sleep(10);  // Short delay between press and release

    input.mi.dwFlags = MOUSEEVENTF_LEFTUP;
    SendInput(1, &input, sizeof(INPUT));
}

float GetRandomDelay(float start, float stop) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(start, stop);
    return dis(gen);
}

bool IsKeyPressed(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

void TriggerBot(HANDLE hProcess, uintptr_t client, float start, float stop) {
    uintptr_t player = Read<uintptr_t>(hProcess, client + offsets::dwLocalPlayerPawn);
    int entityId = Read<int>(hProcess, player + offsets::m_iIDEntIndex);

    if (entityId > 0) {
        uintptr_t entList = Read<uintptr_t>(hProcess, client + offsets::dwEntityList);
        uintptr_t entEntry = Read<uintptr_t>(hProcess, entList + 0x8 * (entityId >> 9) + 0x10);
        uintptr_t entity = Read<uintptr_t>(hProcess, entEntry + 120 * (entityId & 0x1FF));
        int entityTeam = Read<int>(hProcess, entity + offsets::m_iTeamNum);
        int playerTeam = Read<int>(hProcess, player + offsets::m_iTeamNum);

        bool isHuman = (entityTeam == 2 || entityTeam == 3);
        if (entityTeam != playerTeam && isHuman) {
            float clickDelay = GetRandomDelay(start, stop);
            MouseClick();
            Sleep(static_cast<DWORD>(clickDelay * 1000));
        }
    }
    Sleep(30);  // 0.03 seconds delay
}

int main() {
    std::wcout << L"[*] Starting trigger bot..." << std::endl;
    DWORD processID = GetProcessID(L"cs2.exe");
    if (!processID) {
        std::wcout << L"[!] Could not find process ID." << std::endl;
        return -1;
    }

    HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processID);
    if (!hProcess) {
        std::wcout << L"[!] Failed to open process." << std::endl;
        return -1;
    }

    uintptr_t client = GetModuleBaseAddress(processID, L"client.dll");
    if (!client) {
        std::wcout << L"[!] Could not find module base address." << std::endl;
        CloseHandle(hProcess);
        return -1;
    }

    bool isRageMode = false;
    bool isToggled = false;

    std::wcout << L"[*] Triggerbot active. Press INSERT to toggle Rage Mode. Press END to exit." << std::endl;

    while (true) {
        if (IsKeyPressed(VK_INSERT)) {
            if (!isToggled) {
                isRageMode = !isRageMode;
                isToggled = true;
                Beep(2500, 100);
                if (isRageMode) {
                    Beep(2500, 100);
                }
                std::wcout << (isRageMode ? L"[*] Rage Mode ON" : L"[*] Rage Mode OFF") << std::endl;
            }
        }
        else {
            isToggled = false;
        }

        if (isRageMode) {
            TriggerBot(hProcess, client, 0.015f, 0.03f);
        }
        else if (IsKeyPressed(VK_SHIFT) || IsKeyPressed(VK_MENU)) {  // SHIFT or ALT
            TriggerBot(hProcess, client, 0.035f, 0.06f);
        }

        if (IsKeyPressed(VK_END)) {
            break;
        }

        Sleep(10);  // Small delay to reduce CPU usage
    }

    CloseHandle(hProcess);
    std::wcout << L"[*] Triggerbot stopped." << std::endl;
    return 0;
}