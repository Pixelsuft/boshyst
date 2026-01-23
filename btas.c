#include <windows.h>
#include <stdio.h>

int main() {
    const char* childExe = "I Wanna Be The Boshy.exe";
    const char* dllPath = "boshyst.dll";

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    if (!CreateProcessA(childExe, NULL, NULL, NULL, FALSE, CREATE_SUSPENDED, NULL, NULL, &si, &pi)) {
        printf("Failed to create process (%d)\n", GetLastError());
        return 1;
    }

    size_t pathLen = strlen(dllPath) + 1;
    LPVOID remoteBuf = VirtualAllocEx(pi.hProcess, NULL, pathLen, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	
	if (!remoteBuf) {
        printf("VirtualAllocEx failed");
        return 1;
    }
	
	WriteProcessMemory(pi.hProcess, remoteBuf, dllPath, pathLen, NULL);

	LPVOID loadLibAddr = (LPVOID)GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
	HANDLE hThread = CreateRemoteThread(pi.hProcess, NULL, 0, (LPTHREAD_START_ROUTINE)loadLibAddr, remoteBuf, 0, NULL);
	
	if (!hThread) {
        printf("Failed to create a remote thread (%d)\n", GetLastError());
        return 1;
    }
	WaitForSingleObject(hThread, INFINITE);
	CloseHandle(hThread);
	VirtualFreeEx(pi.hProcess, remoteBuf, pathLen, 0x00004000);

    ResumeThread(pi.hThread);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 0;
}
