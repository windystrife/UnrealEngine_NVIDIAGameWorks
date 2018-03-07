// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BootstrapPackagedGame.h"

#define IDI_EXEC_FILE 201
#define IDI_EXEC_ARGS 202

WCHAR* ReadResourceString(HMODULE ModuleHandle, LPCWSTR Name)
{
	WCHAR* Result = NULL;

	HRSRC ResourceHandle = FindResource(ModuleHandle, Name, RT_RCDATA);
	if(ResourceHandle != NULL)
	{
		HGLOBAL AllocHandle = LoadResource(ModuleHandle, ResourceHandle);
		if(AllocHandle != NULL)
		{
			WCHAR* Data = (WCHAR*)LockResource(AllocHandle);
			DWORD DataLen = SizeofResource(ModuleHandle, ResourceHandle) / sizeof(WCHAR);

			Result = new WCHAR[DataLen + 1];
			memcpy(Result, Data, DataLen * sizeof(WCHAR));
			Result[DataLen] = 0;
		}
	}

	return Result;
}

bool TryLoadDll(const WCHAR* ExecDirectory, const WCHAR* Name)
{
	// Try to load it from the system path
	if (LoadLibrary(Name) != nullptr)
	{
		return true;
	}

	// Try to load it from the application directory
	WCHAR AppLocalPath[MAX_PATH];
	PathCombine(AppLocalPath, ExecDirectory, Name);
	if (LoadLibrary(AppLocalPath) != nullptr)
	{
		return true;
	}

	// Otherwise fail
	return false;
}

int InstallMissingPrerequisites(const WCHAR* BaseDirectory, const WCHAR* ExecDirectory)
{
	// Look for missing prerequisites
	WCHAR MissingPrerequisites[1024] = { 0, };
	if(!TryLoadDll(ExecDirectory, L"MSVCP140.DLL") || !TryLoadDll(ExecDirectory, L"ucrtbase.dll"))
	{
		wcscat_s(MissingPrerequisites, TEXT("Microsoft Visual C++ 2015 Runtime\n"));
	}
	if(!TryLoadDll(ExecDirectory, L"XINPUT1_3.DLL"))
	{
		wcscat_s(MissingPrerequisites, TEXT("DirectX Runtime\n"));
	}

	// Check if there's anything missing
	if(MissingPrerequisites[0] != 0)
	{
		WCHAR MissingPrerequisitesMsg[1024];
		wsprintf(MissingPrerequisitesMsg, L"The following component(s) are required to run this program:\n\n%s", MissingPrerequisites);

		// If we don't have the installer, just notify the user and quit
		WCHAR PrereqInstaller[MAX_PATH];
#ifdef _M_X64
		PathCombine(PrereqInstaller, BaseDirectory, L"Engine\\Extras\\Redist\\en-us\\UE4PrereqSetup_x64.exe");
#else
		PathCombine(PrereqInstaller, BaseDirectory, L"Engine\\Extras\\Redist\\en-us\\UE4PrereqSetup_x86.exe");
#endif
		if(GetFileAttributes(PrereqInstaller) == INVALID_FILE_ATTRIBUTES)
		{
			MessageBox(NULL, MissingPrerequisitesMsg, NULL, MB_OK);
			return 9001;
		}

		// Otherwise ask them if they want to install them
		wcscat_s(MissingPrerequisitesMsg, L"\nWould you like to install them now?");
		if(MessageBox(NULL, MissingPrerequisitesMsg, NULL, MB_YESNO) == IDNO)
		{
			return 9002;
		}

		// Start the installer
		SHELLEXECUTEINFO ShellExecuteInfo;
		ZeroMemory(&ShellExecuteInfo, sizeof(ShellExecuteInfo));
		ShellExecuteInfo.cbSize = sizeof(ShellExecuteInfo);
		ShellExecuteInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
		ShellExecuteInfo.nShow = SW_SHOWNORMAL;
		ShellExecuteInfo.lpFile = PrereqInstaller;
		if(!ShellExecuteExW(&ShellExecuteInfo))
		{
			return 9003;
		}

		// Wait for the process to complete, then get its exit code
		DWORD ExitCode = 0;
		WaitForSingleObject(ShellExecuteInfo.hProcess, INFINITE);
		GetExitCodeProcess(ShellExecuteInfo.hProcess, &ExitCode);
		CloseHandle(ShellExecuteInfo.hProcess);
		if(ExitCode != 0)
		{
			return 9004;
		}
	}
	return 0;
}

int SpawnTarget(WCHAR* CmdLine)
{
	STARTUPINFO StartupInfo;
	ZeroMemory(&StartupInfo, sizeof(StartupInfo));
	StartupInfo.cb = sizeof(StartupInfo);

	PROCESS_INFORMATION ProcessInfo;
	ZeroMemory(&ProcessInfo, sizeof(ProcessInfo));

	if(!CreateProcess(NULL, CmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &StartupInfo, &ProcessInfo))
	{
		DWORD ErrorCode = GetLastError();

		WCHAR* Buffer = new WCHAR[wcslen(CmdLine) + 50];
		wsprintf(Buffer, L"Couldn't start:\n%s\nCreateProcess() returned %x.", CmdLine, ErrorCode);
		MessageBoxW(NULL, Buffer, NULL, MB_OK);
		delete[] Buffer;

		return 9005;
	}

	WaitForSingleObject(ProcessInfo.hProcess, INFINITE);
	DWORD ExitCode = 9006;
	GetExitCodeProcess(ProcessInfo.hProcess, &ExitCode);

	CloseHandle(ProcessInfo.hThread);
	CloseHandle(ProcessInfo.hProcess);
	return (int)ExitCode;
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, TCHAR* CmdLine, int ShowCmd)
{
	(void)hPrevInstance;
	(void)ShowCmd;

	// Get the current module filename
	WCHAR CurrentModuleFile[MAX_PATH];
	GetModuleFileNameW(hInstance, CurrentModuleFile, sizeof(CurrentModuleFile));

	// Get the base directory from the current module filename
	WCHAR BaseDirectory[MAX_PATH];
	PathCanonicalize(BaseDirectory, CurrentModuleFile);
	PathRemoveFileSpec(BaseDirectory);

	// Get the executable to run
	WCHAR* ExecFile = ReadResourceString(hInstance, MAKEINTRESOURCE(IDI_EXEC_FILE));
	if(ExecFile == NULL)
	{
		MessageBoxW(NULL, L"This program is used for packaged games and is not meant to be run directly.", NULL, MB_OK);
		return 9000;
	}

	// Get the directory containing the target to be executed
	WCHAR* TempExecDirectory = new WCHAR[wcslen(BaseDirectory) + wcslen(ExecFile) + 20];
	wsprintf(TempExecDirectory, L"%s\\%s", BaseDirectory, ExecFile);
	WCHAR ExecDirectory[MAX_PATH];
	PathCanonicalize(ExecDirectory, TempExecDirectory);
	delete[] TempExecDirectory;
	PathRemoveFileSpec(ExecDirectory);

	// Create a full command line for the program to run
	WCHAR* BaseArgs = ReadResourceString(hInstance, MAKEINTRESOURCE(IDI_EXEC_ARGS));
	WCHAR* ChildCmdLine = new WCHAR[wcslen(BaseDirectory) + wcslen(ExecFile) + wcslen(BaseArgs) + wcslen(CmdLine) + 20];
	wsprintf(ChildCmdLine, L"\"%s\\%s\" %s %s", BaseDirectory, ExecFile, BaseArgs, CmdLine);
	delete[] BaseArgs;
	delete[] ExecFile;

	// Install the prerequisites
	int ExitCode = InstallMissingPrerequisites(BaseDirectory, ExecDirectory);
	if(ExitCode != 0)
	{
		delete[] ChildCmdLine;
		return ExitCode;
	}

	// Spawn the target executable
	ExitCode = SpawnTarget(ChildCmdLine);
	if(ExitCode != 0)
	{
		delete[] ChildCmdLine;
		return ExitCode;
	}

	delete[] ChildCmdLine;
	return ExitCode;
}
