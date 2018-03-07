// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformProcess.h"
#include "HAL/PlatformMisc.h"
#include "Misc/AssertionMacros.h"
#include "Logging/LogMacros.h"
#include "HAL/PlatformAffinity.h"
#include "HAL/UnrealMemory.h"
#include "Templates/UnrealTemplate.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Containers/StringConv.h"
#include "Containers/UnrealString.h"
#include "Misc/SingleThreadEvent.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Internationalization/Internationalization.h"
#include "CoreGlobals.h"
#include "Stats/Stats.h"
#include "Misc/CoreStats.h"
#include "Runtime/Core/Resources/Windows/ModuleVersionResource.h"
#include "Windows/WindowsHWrapper.h"

#include "Windows/AllowWindowsPlatformTypes.h"
	#include <shellapi.h>
	#include <ShlObj.h>
	#include <LM.h>
	#include <Psapi.h>
	#include <TlHelp32.h>

	namespace ProcessConstants
	{
		uint32 WIN_STD_INPUT_HANDLE = STD_INPUT_HANDLE;
		uint32 WIN_STD_OUTPUT_HANDLE = STD_OUTPUT_HANDLE;
		uint32 WIN_ATTACH_PARENT_PROCESS = ATTACH_PARENT_PROCESS;		
		uint32 WIN_STILL_ACTIVE = STILL_ACTIVE;
	}

#include "Windows/HideWindowsPlatformTypes.h"
#include "Windows/WindowsPlatformMisc.h"

#pragma comment(lib, "psapi.lib")

static bool ReadLibraryImportsFromMemory(const IMAGE_DOS_HEADER *Header, TArray<FString> &ImportNames);
static const void *MapRvaToPointer(const IMAGE_DOS_HEADER *Header, const IMAGE_NT_HEADERS *NtHeader, size_t Rva);


// static variables
TArray<FString> FWindowsPlatformProcess::DllDirectoryStack;
TArray<FString> FWindowsPlatformProcess::DllDirectories;


void FWindowsPlatformProcess::AddDllDirectory(const TCHAR* Directory)
{
	FString NormalizedDirectory = FPaths::ConvertRelativePathToFull(Directory);
	FPaths::NormalizeDirectoryName(NormalizedDirectory);
	FPaths::MakePlatformFilename(NormalizedDirectory);
	DllDirectories.AddUnique(NormalizedDirectory);
}

void* FWindowsPlatformProcess::GetDllHandle( const TCHAR* FileName )
{
	check(FileName);

	// Combine the explicit DLL search directories with the contents of the directory stack 
	TArray<FString> SearchPaths;
	SearchPaths.Add(FPlatformProcess::GetModulesDirectory());
	if(DllDirectoryStack.Num() > 0)
	{
		SearchPaths.Add(DllDirectoryStack.Top());
	}
	for(int32 Idx = 0; Idx < DllDirectories.Num(); Idx++)
	{
		SearchPaths.Add(DllDirectories[Idx]);
	}

	// Load the DLL, avoiding windows dialog boxes if missing
	DWORD ErrorMode = 0;
	if(!FParse::Param(::GetCommandLineW(), TEXT("dllerrors")))
	{
		ErrorMode |= SEM_NOOPENFILEERRORBOX;
		if(FParse::Param(::GetCommandLineW(), TEXT("unattended")))
		{
			ErrorMode |= SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX;
		}
	}

	DWORD PrevErrorMode = 0;
	BOOL bHavePrevErrorMode = ::SetThreadErrorMode(ErrorMode, &PrevErrorMode);

	// Load the DLL, avoiding windows dialog boxes if missing
	void* Handle = LoadLibraryWithSearchPaths(FileName, SearchPaths);
	
	if(bHavePrevErrorMode)
	{
		::SetThreadErrorMode(PrevErrorMode, NULL);
	}

	return Handle;
}

void FWindowsPlatformProcess::FreeDllHandle( void* DllHandle )
{
	// It is okay to call FreeLibrary on 0
	::FreeLibrary((HMODULE)DllHandle);
}

FString FWindowsPlatformProcess::GenerateApplicationPath( const FString& AppName, EBuildConfigurations::Type BuildConfiguration)
{
	FString PlatformName = GetBinariesSubdirectory();
	FString ExecutablePath = FString::Printf(TEXT("..\\..\\..\\Engine\\Binaries\\%s\\%s"), *PlatformName, *AppName);

	if (BuildConfiguration != EBuildConfigurations::Development && BuildConfiguration != EBuildConfigurations::DebugGame)
	{
		ExecutablePath += FString::Printf(TEXT("-%s-%s"), *PlatformName, EBuildConfigurations::ToString(BuildConfiguration));
	}

	ExecutablePath += TEXT(".exe");

	return ExecutablePath;
}

void* FWindowsPlatformProcess::GetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	check(DllHandle);
	check(ProcName);
	return (void*)::GetProcAddress( (HMODULE)DllHandle, TCHAR_TO_ANSI(ProcName) );
}

int32 FWindowsPlatformProcess::GetDllApiVersion( const TCHAR* Filename )
{
	int32 Result = -1;

	// Retrieves the embedded API version from a DLL
	check(Filename);
	HMODULE hModule = LoadLibraryEx(Filename, NULL, LOAD_LIBRARY_AS_DATAFILE);
	if(hModule != NULL)
	{
		HRSRC hResInfo = FindResource(hModule, MAKEINTRESOURCE(ID_MODULE_API_VERSION_RESOURCE), RT_RCDATA);
		if(hResInfo != NULL)
		{
			HGLOBAL hResGlobal = LoadResource(hModule, hResInfo);
			if(hResGlobal != NULL)
			{
				void *pResData = LockResource(hResGlobal);
				if(pResData != NULL)
				{
					::DWORD Length = SizeofResource(hModule, hResInfo);
					if(Length > 0)
					{
						char *Str = (char*)pResData;
						if(Str[Length - 1] == 0)
						{
							char *End = Str;
							uint64 Value = FCStringAnsi::Strtoui64(Str, &End, 10);
							if(*End == 0 && Value <= INT_MAX)
							{
								Result = (int32)Value;
							}
						}
					}
					UnlockResource(pResData);
				}
			}
		}
		FreeLibrary(hModule);
	}

	return Result;
}

void FWindowsPlatformProcess::PushDllDirectory(const TCHAR* Directory)
{
	// set the directory in windows
	::SetDllDirectory(Directory);
	// remember it
	DllDirectoryStack.Push(Directory);
}

void FWindowsPlatformProcess::PopDllDirectory(const TCHAR* Directory)
{
	// don't allow too many pops (indicates bad code that should be fixed, but won't kill anything, so using ensure)
	ensureMsgf(DllDirectoryStack.Num() > 0, TEXT("Tried to PopDllDirectory too many times"));
	// verify we are popping the top
	checkf(DllDirectoryStack.Top() == Directory, TEXT("There was a PushDllDirectory/PopDllDirectory mismatch (Popped %s, which didn't match %s)"), *DllDirectoryStack.Top(), Directory);
	// pop it off
	DllDirectoryStack.Pop();

	// and now set the new DllDirectory to the old value
	if (DllDirectoryStack.Num() > 0)
	{
		::SetDllDirectory(*DllDirectoryStack.Top());
	}
	else
	{
		::SetDllDirectory(TEXT(""));
	}
}

static void LaunchWebURL( const FString& URLParams, FString* Error )
{
	UE_LOG(LogWindows, Log, TEXT("LaunchURL %s"), *URLParams);

	FString BrowserOpenCommand;

	// First lookup the program Id for the default browser.
	FString ProgId;
	if (FWindowsPlatformMisc::QueryRegKey(HKEY_CURRENT_USER, TEXT("Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice"), TEXT("Progid"), ProgId))
	{
		// If we found it, then lookup it's open shell command in the classes registry.
		FString BrowserRegPath = ProgId + TEXT("\\shell\\open\\command");
		FWindowsPlatformMisc::QueryRegKey(HKEY_CLASSES_ROOT, *BrowserRegPath, NULL, BrowserOpenCommand);
	}

	// If we failed to find a default browser using the newer location, revert to using shell open command for the HTTP file association.
	if (BrowserOpenCommand.IsEmpty())
	{
		FWindowsPlatformMisc::QueryRegKey(HKEY_CLASSES_ROOT, TEXT("http\\shell\\open\\command"), NULL, BrowserOpenCommand);
	}

	// If we have successfully looked up the correct shell command, then we can create a new process using that command
	// we do this instead of shell execute due to security concerns.  By starting the browser directly we avoid most issues.
	if (!BrowserOpenCommand.IsEmpty())
	{
		FString ExePath, ExeArgs;

		// If everything has gone to plan, the shell command should be something like this:
		// "C:\Program Files (x86)\Mozilla Firefox\firefox.exe" -osint -url "%1"
		// We need to extract out the executable portion, and the arguments portion and expand any %1's with the URL,
		// then start the browser process.

		// Extract the exe and any arguments to the executable.
		const int32 FirstQuote = BrowserOpenCommand.Find(TEXT("\""));
		if (FirstQuote != INDEX_NONE)
		{
			const int32 SecondQuote = BrowserOpenCommand.Find(TEXT("\""), ESearchCase::IgnoreCase, ESearchDir::FromStart, FirstQuote + 1);
			if (SecondQuote != INDEX_NONE)
			{
				ExePath = BrowserOpenCommand.Mid(FirstQuote + 1, (SecondQuote - 1) - FirstQuote);
				ExeArgs = BrowserOpenCommand.Mid(SecondQuote + 1);
			}
		}

		// If anything failed to parse right, don't continue down this path, just use shell execute.
		if (!ExePath.IsEmpty())
		{
			if (ExeArgs.ReplaceInline(TEXT("%1"), *URLParams) == 0)
			{
				// If we fail to detect the placement token we append the URL to the arguments.
				// This is for robustness, and to fix a known error case when using Internet Explorer 8. 
				ExeArgs.Append(TEXT(" \"") + URLParams + TEXT("\""));
			}

			// Now that we have the shell open command to use, run the shell command in the open process with any and all parameters.
			if (FPlatformProcess::CreateProc(*ExePath, *ExeArgs, true, false, false, NULL, 0, NULL, NULL).IsValid())
			{
				// Success!
				return;
			}
			else
			{
				if (Error)
				{
					*Error = NSLOCTEXT("Core", "UrlFailed", "Failed launching URL").ToString();
				}
			}
		}
	}

	// If all else fails just do a shell execute and let windows sort it out.  But only do it if it's an
	// HTTP or HTTPS address.  A malicious address could be problematic if just passed directly to shell execute.
	if (URLParams.StartsWith(TEXT("http://")) || URLParams.StartsWith(TEXT("https://")))
	{
		const HINSTANCE Code = ::ShellExecuteW(NULL, TEXT("open"), *URLParams, NULL, NULL, SW_SHOWNORMAL);
		if (Error)
		{
			*Error = ((PTRINT)Code <= 32) ? NSLOCTEXT("Core", "UrlFailed", "Failed launching URL").ToString() : TEXT("");
		}
	}
}

static void LaunchDefaultHandlerForURL( const TCHAR* URL, FString* Error )
{
	// ShellExecute will open the default handler for a URL
	const HINSTANCE Code = ::ShellExecuteW(NULL, TEXT("open"), URL, NULL, NULL, SW_SHOWNORMAL);
	if (Error)
	{
		*Error = ((PTRINT)Code <= 32) ? NSLOCTEXT("Core", "UrlFailed", "Failed launching URL").ToString() : TEXT("");
	}
}

bool FWindowsPlatformProcess::CanLaunchURL(const TCHAR* URL)
{
	return URL != nullptr;
}

void FWindowsPlatformProcess::LaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	check(URL);

	// Initialize the error to empty string.
	if (Error)
	{
		*Error = TEXT("");
	}

	// Use the default handler if we have a URI scheme name that doesn't look like a Windows path, and is not http: or https:
	FString SchemeName;
	if(FParse::SchemeNameFromURI(URL, SchemeName) && SchemeName.Len() > 1 && SchemeName != TEXT("http") && SchemeName != TEXT("https"))
	{
		LaunchDefaultHandlerForURL(URL, Error);
	}
	else
	{
		FString URLParams = FString::Printf(TEXT("%s %s"), URL, Parms ? Parms : TEXT("")).TrimEnd();
		LaunchWebURL( URLParams, Error );
	}

}

FProcHandle FWindowsPlatformProcess::CreateProc( const TCHAR* URL, const TCHAR* Parms, bool bLaunchDetached, bool bLaunchHidden, bool bLaunchReallyHidden, uint32* OutProcessID, int32 PriorityModifier, const TCHAR* OptionalWorkingDirectory, void* PipeWriteChild, void * PipeReadChild)
{
	//UE_LOG(LogWindows, Log,  TEXT("CreateProc %s %s"), URL, Parms );

	// initialize process attributes
	SECURITY_ATTRIBUTES Attr;
	Attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	Attr.lpSecurityDescriptor = NULL;
	Attr.bInheritHandle = true;

	// initialize process creation flags
	uint32 CreateFlags = NORMAL_PRIORITY_CLASS;
	if (PriorityModifier < 0)
	{
		CreateFlags = (PriorityModifier == -1) ? BELOW_NORMAL_PRIORITY_CLASS : IDLE_PRIORITY_CLASS;
	}
	else if (PriorityModifier > 0)
	{
		CreateFlags = (PriorityModifier == 1) ? ABOVE_NORMAL_PRIORITY_CLASS : HIGH_PRIORITY_CLASS;
	}

	if (bLaunchDetached)
	{
		CreateFlags |= DETACHED_PROCESS;
	}

	// initialize window flags
	uint32 dwFlags = 0;
	uint16 ShowWindowFlags = SW_HIDE;
	if (bLaunchReallyHidden)
	{
		dwFlags = STARTF_USESHOWWINDOW;
	}
	else if (bLaunchHidden)
	{
		dwFlags = STARTF_USESHOWWINDOW;
		ShowWindowFlags = SW_SHOWMINNOACTIVE;
	}

	if (PipeWriteChild != nullptr || PipeReadChild != nullptr)
	{
		dwFlags |= STARTF_USESTDHANDLES;
	}

	// initialize startup info
	STARTUPINFO StartupInfo = {
		sizeof(STARTUPINFO),
		NULL, NULL, NULL,
		(::DWORD)CW_USEDEFAULT,
		(::DWORD)CW_USEDEFAULT,
		(::DWORD)CW_USEDEFAULT,
		(::DWORD)CW_USEDEFAULT,
		(::DWORD)0, (::DWORD)0, (::DWORD)0,
		(::DWORD)dwFlags,
		ShowWindowFlags,
		0, NULL,
		HANDLE(PipeReadChild),
		HANDLE(PipeWriteChild),
		HANDLE(PipeWriteChild)
	};

	// create the child process
	FString CommandLine = FString::Printf(TEXT("\"%s\" %s"), URL, Parms);
	PROCESS_INFORMATION ProcInfo;

	if (!CreateProcess(NULL, CommandLine.GetCharArray().GetData(), &Attr, &Attr, true, (::DWORD)CreateFlags, NULL, OptionalWorkingDirectory, &StartupInfo, &ProcInfo))
	{
		DWORD ErrorCode = GetLastError();

		TCHAR ErrorMessage[512];
		FWindowsPlatformMisc::GetSystemErrorMessage(ErrorMessage, 512, ErrorCode);

		UE_LOG(LogWindows, Warning, TEXT("CreateProc failed: %s (0x%08x)"), ErrorMessage, ErrorCode);
		if (ErrorCode == ERROR_NOT_ENOUGH_MEMORY || ErrorCode == ERROR_OUTOFMEMORY)
		{
			// These errors are common enough that we want some available memory information
			FPlatformMemoryStats Stats = FPlatformMemory::GetStats();
			UE_LOG(LogWindows, Warning, TEXT("Mem used: %.2f MB, OS Free %.2f MB"), Stats.UsedPhysical / 1048576.0f, Stats.AvailablePhysical / 1048576.0f);
		}
		UE_LOG(LogWindows, Warning, TEXT("URL: %s %s"), URL, Parms);
		if (OutProcessID != nullptr)
		{
			*OutProcessID = 0;
		}

		return FProcHandle();
	}

	if (OutProcessID != nullptr)
	{
		*OutProcessID = ProcInfo.dwProcessId;
	}

	::CloseHandle( ProcInfo.hThread );

	return FProcHandle(ProcInfo.hProcess);
}

FProcHandle FWindowsPlatformProcess::OpenProcess(uint32 ProcessID)
{
	return FProcHandle(::OpenProcess(PROCESS_ALL_ACCESS, 0, ProcessID));
}

bool FWindowsPlatformProcess::IsProcRunning( FProcHandle & ProcessHandle )
{
	bool bApplicationRunning = true;
	uint32 WaitResult = ::WaitForSingleObject(ProcessHandle.Get(), 0);
	if (WaitResult != WAIT_TIMEOUT)
	{
		bApplicationRunning = false;
	}
	return bApplicationRunning;
}

void FWindowsPlatformProcess::WaitForProc( FProcHandle & ProcessHandle )
{
	::WaitForSingleObject(ProcessHandle.Get(), INFINITE);
}

void FWindowsPlatformProcess::CloseProc(FProcHandle & ProcessHandle)
{
	if (ProcessHandle.IsValid())
	{
		::CloseHandle(ProcessHandle.Get());
		ProcessHandle.Reset();
	}
}

void FWindowsPlatformProcess::TerminateProc( FProcHandle & ProcessHandle, bool KillTree )
{
	if (KillTree)
	{
		HANDLE SnapShot = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

		if (SnapShot != INVALID_HANDLE_VALUE)
		{
			::DWORD ProcessId = ::GetProcessId(ProcessHandle.Get());

			PROCESSENTRY32 Entry;
			Entry.dwSize = sizeof(PROCESSENTRY32);

			if (::Process32First(SnapShot, &Entry))
			{
				do
				{
					if (Entry.th32ParentProcessID == ProcessId)
					{
						HANDLE ChildProcHandle = ::OpenProcess(PROCESS_ALL_ACCESS, 0, Entry.th32ProcessID);

						if (ChildProcHandle)
						{
							FProcHandle ChildHandle(ChildProcHandle);
							TerminateProc(ChildHandle, KillTree);
//							::TerminateProcess(ChildProcHandle, 1);
						}
					}
				}
				while(::Process32Next(SnapShot, &Entry));
			}
		}
	}

	TerminateProcess(ProcessHandle.Get(),0);
}

uint32 FWindowsPlatformProcess::GetCurrentProcessId()
{
	return ::GetCurrentProcessId();
}

void FWindowsPlatformProcess::SetThreadAffinityMask( uint64 AffinityMask )
{
	if( AffinityMask != FPlatformAffinity::GetNoAffinityMask() )
	{
		::SetThreadAffinityMask( ::GetCurrentThread(), (DWORD_PTR)AffinityMask );
	}
}

bool FWindowsPlatformProcess::GetProcReturnCode( FProcHandle & ProcHandle, int32* ReturnCode )
{
	return ::GetExitCodeProcess( ProcHandle.Get(), (::DWORD *)ReturnCode ) && *((uint32*)ReturnCode) != ProcessConstants::WIN_STILL_ACTIVE;
}

bool FWindowsPlatformProcess::GetApplicationMemoryUsage(uint32 ProcessId, SIZE_T* OutMemoryUsage)
{
	bool bSuccess = false;
	HANDLE ProcessHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, false, ProcessId);

	if (ProcessHandle != NULL)
	{
		PROCESS_MEMORY_COUNTERS_EX MemoryInfo;

		if (GetProcessMemoryInfo(ProcessHandle, (PROCESS_MEMORY_COUNTERS*)&MemoryInfo, sizeof(MemoryInfo)))
		{
			*OutMemoryUsage = MemoryInfo.PrivateUsage;
			bSuccess = true;
		}

		::CloseHandle(ProcessHandle);
	}

	return bSuccess;
}

bool FWindowsPlatformProcess::IsApplicationRunning( uint32 ProcessId )
{
	bool bApplicationRunning = true;
	HANDLE ProcessHandle = ::OpenProcess(SYNCHRONIZE, false, ProcessId);
	if (ProcessHandle == NULL)
	{
		bApplicationRunning = false;
	}
	else
	{
		uint32 WaitResult = WaitForSingleObject(ProcessHandle, 0);
		if (WaitResult != WAIT_TIMEOUT)
		{
			bApplicationRunning = false;
		}
		::CloseHandle(ProcessHandle);
	}
	return bApplicationRunning;
}

bool FWindowsPlatformProcess::IsApplicationRunning( const TCHAR* ProcName )
{
	// append the extension

	FString ProcNameWithExtension = ProcName;
	if( ProcNameWithExtension.Find( TEXT(".exe"), ESearchCase::IgnoreCase, ESearchDir::FromEnd ) == INDEX_NONE )
	{
		ProcNameWithExtension += TEXT(".exe");
	}

	HANDLE SnapShot = ::CreateToolhelp32Snapshot( TH32CS_SNAPPROCESS, 0 );
	if( SnapShot != INVALID_HANDLE_VALUE )
	{
		PROCESSENTRY32 Entry;
		Entry.dwSize = sizeof( PROCESSENTRY32 );

		if( ::Process32First( SnapShot, &Entry ) )
		{
			do
			{
				if( FCString::Stricmp( *ProcNameWithExtension, Entry.szExeFile ) == 0 )
				{
					::CloseHandle( SnapShot );
					return true;
				}
			} while( ::Process32Next( SnapShot, &Entry ) );
		}
	}

	::CloseHandle( SnapShot );
	return false;
}

FString FWindowsPlatformProcess::GetApplicationName( uint32 ProcessId )
{
	FString Output = TEXT("");
	HANDLE ProcessHandle = ::OpenProcess(PROCESS_QUERY_INFORMATION, false, ProcessId);
	if (ProcessHandle != NULL)
	{
		const int32 ProcessNameBufferSize = 4096;
		TCHAR ProcessNameBuffer[ProcessNameBufferSize];
		
		int32 InOutSize = ProcessNameBufferSize;
		static_assert(sizeof(::DWORD) == sizeof(int32), "DWORD size doesn't match int32. Is it the future or the past?");

#if WINVER == 0x0502
		GetProcessImageFileName(ProcessHandle, ProcessNameBuffer, InOutSize);
#else
		QueryFullProcessImageName(ProcessHandle, 0, ProcessNameBuffer, (PDWORD)(&InOutSize));
#endif

		Output = ProcessNameBuffer;
	}

	return Output;
}

void FWindowsPlatformProcess::ReadFromPipes(FString* OutStrings[], HANDLE InPipes[], int32 PipeCount)
{
	for (int32 PipeIndex = 0; PipeIndex < PipeCount; ++PipeIndex)
	{
		if (InPipes[PipeIndex] && OutStrings[PipeIndex])
		{
			*OutStrings[PipeIndex] += ReadPipe(InPipes[PipeIndex]);
		}
	}
}

/**
 * Executes a process, returning the return code, stdout, and stderr. This
 * call blocks until the process has returned.
 */
bool FWindowsPlatformProcess::ExecProcess( const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr )
{
	PROCESS_INFORMATION ProcInfo;
	SECURITY_ATTRIBUTES Attr;

	FString CommandLine = FString::Printf(TEXT("%s %s"), URL, Params);

	Attr.nLength = sizeof(SECURITY_ATTRIBUTES);
	Attr.lpSecurityDescriptor = NULL;
	Attr.bInheritHandle = true;

	uint32 CreateFlags = NORMAL_PRIORITY_CLASS;
	CreateFlags |= DETACHED_PROCESS;

	uint32 dwFlags = STARTF_USESHOWWINDOW;
	uint16 ShowWindowFlags = SW_SHOWMINNOACTIVE;

	const int32 MaxPipeCount = 2;
	HANDLE ReadablePipes[MaxPipeCount] = {0};
	HANDLE WritablePipes[MaxPipeCount] = {0};
	const bool bRedirectOutput = OutStdOut != NULL || OutStdErr != NULL;

	if (bRedirectOutput)
	{
		dwFlags |= STARTF_USESTDHANDLES;
		for (int32 PipeIndex = 0; PipeIndex < MaxPipeCount; ++PipeIndex)
		{
			verify(::CreatePipe(&ReadablePipes[PipeIndex], &WritablePipes[PipeIndex], &Attr, 0));
			verify(::SetHandleInformation(ReadablePipes[PipeIndex], /*dwMask=*/ HANDLE_FLAG_INHERIT, /*dwFlags=*/ 0));
		}
	}

	bool bSuccess = false;
	STARTUPINFO StartupInfo = { sizeof(STARTUPINFO), NULL, NULL, NULL,
		(::DWORD)CW_USEDEFAULT, (::DWORD)CW_USEDEFAULT, (::DWORD)CW_USEDEFAULT, (::DWORD)CW_USEDEFAULT,
		(::DWORD)0, (::DWORD)0, (::DWORD)0, (::DWORD)dwFlags, ShowWindowFlags, 0, NULL,
		::GetStdHandle((::DWORD)ProcessConstants::WIN_STD_INPUT_HANDLE), WritablePipes[0], WritablePipes[1] };
	if (CreateProcess(NULL, CommandLine.GetCharArray().GetData(), &Attr, &Attr, true, CreateFlags,
		NULL, NULL, &StartupInfo, &ProcInfo))
	{
		if (bRedirectOutput)
		{
			FString* OutStrings[MaxPipeCount] = { OutStdOut, OutStdErr };
			FProcHandle ProcHandle(ProcInfo.hProcess);
			do 
			{
				ReadFromPipes(OutStrings, ReadablePipes, MaxPipeCount);
				FPlatformProcess::Sleep(0);
			} while (IsProcRunning(ProcHandle));
			ReadFromPipes(OutStrings, ReadablePipes, MaxPipeCount);
		}
		else
		{
			::WaitForSingleObject(ProcInfo.hProcess, INFINITE);
		}		
		if (OutReturnCode)
		{
			verify(::GetExitCodeProcess(ProcInfo.hProcess, (::DWORD*)OutReturnCode));
		}
		::CloseHandle(ProcInfo.hProcess);
		::CloseHandle(ProcInfo.hThread);
		bSuccess = true;
	}
	else
	{
		// if CreateProcess failed, we should return a useful error code, which GetLastError will have
		if (OutReturnCode)
		{
			*OutReturnCode = GetLastError();
		}
		if (bRedirectOutput)
		{
			for (int32 PipeIndex = 0; PipeIndex < MaxPipeCount; ++PipeIndex)
			{
				verify(::CloseHandle(WritablePipes[PipeIndex]));
			}
		}
	}

	if (bRedirectOutput)
	{
		for (int32 PipeIndex = 0; PipeIndex < MaxPipeCount; ++PipeIndex)
		{
			verify(::CloseHandle(ReadablePipes[PipeIndex]));
		}
	}

	return bSuccess;
}

bool FWindowsPlatformProcess::ExecElevatedProcess(const TCHAR* URL, const TCHAR* Params, int32* OutReturnCode)
{
	SHELLEXECUTEINFO ShellExecuteInfo;
	ZeroMemory(&ShellExecuteInfo, sizeof(ShellExecuteInfo));
	ShellExecuteInfo.cbSize = sizeof(ShellExecuteInfo);
	ShellExecuteInfo.fMask = SEE_MASK_UNICODE | SEE_MASK_NOCLOSEPROCESS;
	ShellExecuteInfo.lpFile = URL;
	ShellExecuteInfo.lpVerb = TEXT("runas");
	ShellExecuteInfo.nShow = SW_SHOW;
	ShellExecuteInfo.lpParameters = Params;

	bool bSuccess = false;
	if (ShellExecuteEx(&ShellExecuteInfo))
	{
		::WaitForSingleObject(ShellExecuteInfo.hProcess, INFINITE);
		if (OutReturnCode != NULL)
		{
			verify(::GetExitCodeProcess(ShellExecuteInfo.hProcess, (::DWORD*)OutReturnCode));
		}
		verify(::CloseHandle(ShellExecuteInfo.hProcess));
		bSuccess = true;
	}
	return bSuccess;
}

void FWindowsPlatformProcess::CleanFileCache()
{
	bool bShouldCleanShaderWorkingDirectory = true;
#if !(UE_BUILD_SHIPPING && WITH_EDITOR)
	// Only clean the shader working directory if we are the first instance, to avoid deleting files in use by other instances
	//@todo - check if any other instances are running right now
	bShouldCleanShaderWorkingDirectory = GIsFirstInstance;
#endif

	if (bShouldCleanShaderWorkingDirectory && !FParse::Param( FCommandLine::Get(), TEXT("Multiprocess")))
	{
		// get shader path, and convert it to the userdirectory
		for (const auto& SHaderSourceDirectoryEntry : FPlatformProcess::AllShaderSourceDirectoryMappings())
		{
			FString ShaderDir = FString(FPlatformProcess::BaseDir()) / SHaderSourceDirectoryEntry.Value;
			FString UserShaderDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*ShaderDir);
			FPaths::CollapseRelativeDirectories(ShaderDir);

			// make sure we don't delete from the source directory
			if (ShaderDir != UserShaderDir)
			{
				IFileManager::Get().DeleteDirectory(*UserShaderDir, false, true);
			}
		}

		FPlatformProcess::CleanShaderWorkingDir();
	}
}

const TCHAR* FWindowsPlatformProcess::BaseDir()
{
	static TCHAR Result[512]=TEXT("");
	if( !Result[0] )
	{
		// Normally the BaseDir is determined from the path of the running process module, 
		// but for debugging, particularly client or server, it can be useful to point the
		// code at an existing cooked directory. If using -BaseFromWorkingDir set the
		// workingdir in your debugger to the <path>/Project/Binaries/Win64 of your cooked
		// data
		// Too early to use the FCommand line interface
		FString BaseArg;
		FParse::Value(::GetCommandLineW(), TEXT("-basedir="), BaseArg);

		if (BaseArg.Len())
		{
			BaseArg = BaseArg.Replace(TEXT("\\"), TEXT("/"));
			BaseArg += TEXT('/');
			FCString::Strcpy(Result, *BaseArg);
		}
		else if (FCString::Stristr(::GetCommandLineW(), TEXT("-BaseFromWorkingDir")))
		{
			::GetCurrentDirectory(512, Result);

			FString TempResult(Result);
			TempResult = TempResult.Replace(TEXT("\\"), TEXT("/"));
			TempResult += TEXT('/');
			FCString::Strcpy(Result, *TempResult);
		}
		else
		{
			// Get the directory containing the current module if possible, or use the directory containing the executable if not
			HMODULE hCurrentModule;
			if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&BaseDir, &hCurrentModule) == 0)
			{
				hCurrentModule = hInstance;
			}
			GetModuleFileName(hCurrentModule, Result, ARRAY_COUNT(Result));
			FString TempResult(Result);
			TempResult = TempResult.Replace(TEXT("\\"), TEXT("/"));
			FCString::Strcpy(Result, *TempResult);
			int32 StringLength = FCString::Strlen(Result);
			if (StringLength > 0)
			{
				--StringLength;
				for (; StringLength > 0; StringLength--)
				{
					if (Result[StringLength - 1] == TEXT('/') || Result[StringLength - 1] == TEXT('\\'))
					{
						break;
					}
				}
			}
			Result[StringLength] = 0;

			FString CollapseResult(Result);
			FPaths::CollapseRelativeDirectories(CollapseResult);
			FCString::Strcpy(Result, *CollapseResult);
		}
	}
	return Result;
}

const TCHAR* FWindowsPlatformProcess::UserDir()
{
	static FString WindowsUserDir;
	if( !WindowsUserDir.Len() )
	{
		TCHAR UserPath[MAX_PATH];
		// get the My Documents directory
		HRESULT Ret = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, UserPath);

		// make the base user dir path
		WindowsUserDir = FString(UserPath).Replace(TEXT("\\"), TEXT("/")) + TEXT("/");
	}
	return *WindowsUserDir;
}

const TCHAR* FWindowsPlatformProcess::UserTempDir()
{
	static FString WindowsUserTempDir;
	if( !WindowsUserTempDir.Len() )
	{
		TCHAR TempPath[MAX_PATH];
		ZeroMemory(TempPath, sizeof(TCHAR) * MAX_PATH);

		::GetTempPath(MAX_PATH, TempPath);

		// Always expand the temp path in case windows returns short directory names.
		TCHAR FullTempPath[MAX_PATH];
		ZeroMemory(FullTempPath, sizeof(TCHAR) * MAX_PATH);
		::GetLongPathName(TempPath, FullTempPath, MAX_PATH);

		WindowsUserTempDir = FString(FullTempPath).Replace(TEXT("\\"), TEXT("/"));
	}
	return *WindowsUserTempDir;
}

const TCHAR* FWindowsPlatformProcess::UserSettingsDir()
{
	static FString WindowsUserSettingsDir;
	if (!WindowsUserSettingsDir.Len())
	{
		TCHAR UserPath[MAX_PATH];
		// get the My Documents directory
		HRESULT Ret = SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, SHGFP_TYPE_CURRENT, UserPath);

		// make the base user dir path
		WindowsUserSettingsDir = FString(UserPath).Replace(TEXT("\\"), TEXT("/")) + TEXT("/");
	}
	return *WindowsUserSettingsDir;
}

const TCHAR* FWindowsPlatformProcess::ApplicationSettingsDir()
{
	static FString WindowsApplicationSettingsDir;
	if( !WindowsApplicationSettingsDir.Len() )
	{
		TCHAR ApplictionSettingsPath[MAX_PATH];
		// get the My Documents directory
		HRESULT Ret = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, ApplictionSettingsPath);

		// make the base user dir path
		// @todo rocket this folder should be based on your company name, not just be hard coded to /Epic/
		WindowsApplicationSettingsDir = FString(ApplictionSettingsPath).Replace(TEXT("\\"), TEXT("/")) + TEXT("/Epic/");
	}
	return *WindowsApplicationSettingsDir;
}

const TCHAR* FWindowsPlatformProcess::ComputerName()
{
	static TCHAR Result[256]=TEXT("");
	if( !Result[0] )
	{
		uint32 Size=ARRAY_COUNT(Result);
		GetComputerName( Result, (::DWORD*)&Size );
	}
	return Result;
}

const TCHAR* FWindowsPlatformProcess::UserName(bool bOnlyAlphaNumeric/* = true*/)
{
	static TCHAR Result[256]=TEXT("");
	static TCHAR ResultAlpha[256]=TEXT("");
	if( bOnlyAlphaNumeric )
	{
		if( !ResultAlpha[0] )
		{
			uint32 Size=ARRAY_COUNT(ResultAlpha);
			GetUserName( ResultAlpha, (::DWORD*)&Size );
			TCHAR *c, *d;
			for( c=ResultAlpha, d=ResultAlpha; *c!=0; c++ )
				if( FChar::IsAlnum(*c) )
					*d++ = *c;
			*d++ = 0;
		}
		return ResultAlpha;
	}
	else
	{
		if( !Result[0] )
		{
			uint32 Size=ARRAY_COUNT(Result);
			GetUserName( Result, (::DWORD*)&Size );
		}
		return Result;
	}
}

void FWindowsPlatformProcess::SetCurrentWorkingDirectoryToBaseDir()
{
	FPlatformMisc::CacheLaunchDir();
	verify(SetCurrentDirectoryW(BaseDir()));
}

/** Get the current working directory (only really makes sense on desktop platforms) */
FString FWindowsPlatformProcess::GetCurrentWorkingDirectory()
{
	// get the current working directory (uncached)
	TCHAR CurrentDirectory[MAX_PATH];
	GetCurrentDirectoryW(MAX_PATH, CurrentDirectory);
	return CurrentDirectory;
}

const FString FWindowsPlatformProcess::ShaderWorkingDir()
{
	return (FString(FPlatformProcess::UserTempDir()) / TEXT("UnrealShaderWorkingDir/"));
}


const TCHAR* FWindowsPlatformProcess::ExecutableName(bool bRemoveExtension)
{
	static TCHAR Result[512]=TEXT("");
	static TCHAR ResultWithExt[512]=TEXT("");
	if( !Result[0] )
	{
		// Get complete path for the executable
		if ( GetModuleFileName( hInstance, Result, ARRAY_COUNT(Result) ) != 0 )
		{
			// Remove all of the path information by finding the base filename
			FString FileName = Result;
			FString FileNameWithExt = Result;
			FCString::Strncpy( Result, *( FPaths::GetBaseFilename(FileName) ), ARRAY_COUNT(Result) );
			FCString::Strncpy( ResultWithExt, *( FPaths::GetCleanFilename(FileNameWithExt) ), ARRAY_COUNT(ResultWithExt) );
		}
		// If the call failed, zero out the memory to be safe
		else
		{
			FMemory::Memzero( Result, sizeof( Result ) );
			FMemory::Memzero( ResultWithExt, sizeof( ResultWithExt ) );
		}
	}

	return (bRemoveExtension ? Result : ResultWithExt);
}

const TCHAR* FWindowsPlatformProcess::GetModuleExtension()
{
	return TEXT("dll");
}

const TCHAR* FWindowsPlatformProcess::GetBinariesSubdirectory()
{
	if (PLATFORM_64BITS)
	{
		return TEXT("Win64");
	}
	return TEXT("Win32");
}

const FString FWindowsPlatformProcess::GetModulesDirectory()
{
	static FString Result;
	if(Result.Len() == 0)
	{
		// Get the handle to the current module
		HMODULE hCurrentModule;
		if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCWSTR)&GetModulesDirectory, &hCurrentModule) == 0)
		{
			hCurrentModule = hInstance;
		}

		// Get the directory for it
		TCHAR Buffer[MAX_PATH] = TEXT("");
		GetModuleFileName(hCurrentModule, Buffer, ARRAY_COUNT(Buffer));
		*FCString::Strrchr(Buffer, '\\') = 0;

		// Normalize the resulting path
		Result = Buffer;
		FPaths::MakeStandardFilename(Result);
	}
	return Result;
}

void FWindowsPlatformProcess::LaunchFileInDefaultExternalApplication( const TCHAR* FileName, const TCHAR* Parms /*= NULL*/, ELaunchVerb::Type Verb /*= ELaunchVerb::Open*/ )
{
	const TCHAR* VerbString = Verb == ELaunchVerb::Edit ? TEXT("edit") : TEXT("open");

	// First attempt to open the file in its default application
	UE_LOG(LogWindows, Log,  TEXT("LaunchFileInExternalEditor %s %s"), FileName, Parms ? Parms : TEXT("") );
	HINSTANCE Code = ::ShellExecuteW( NULL, VerbString, FileName, Parms ? Parms : TEXT(""), TEXT(""), SW_SHOWNORMAL );
	
	UE_LOG(LogWindows, Log,  TEXT("Launch application code for %s %s: %d"), FileName, Parms ? Parms : TEXT(""), (PTRINT)Code );

	// If opening the file in the default application failed, check to see if it's because the file's extension does not have
	// a default application associated with it. If so, prompt the user with the Windows "Open With..." dialog to allow them to specify
	// an application to use.
	if ( (PTRINT)Code == SE_ERR_NOASSOC || (PTRINT)Code == SE_ERR_ASSOCINCOMPLETE )
	{
		::ShellExecuteW( NULL, VerbString, TEXT("RUNDLL32.EXE"), *FString::Printf( TEXT("shell32.dll,OpenAs_RunDLL %s"), FileName ), TEXT(""), SW_SHOWNORMAL );
	}
}

void FWindowsPlatformProcess::ExploreFolder( const TCHAR* FilePath )
{
	if (IFileManager::Get().DirectoryExists( FilePath ))
	{
		// Explore the folder
		::ShellExecuteW( NULL, TEXT("explore"), FilePath, NULL, NULL, SW_SHOWNORMAL );
	}
	else
	{
		// Explore the file
		FString NativeFilePath = FString(FilePath).Replace(TEXT("/"), TEXT("\\"));
		FString Parameters = FString::Printf( TEXT("/select,%s"), *NativeFilePath);
		::ShellExecuteW( NULL, TEXT("open"), TEXT("explorer.exe"), *Parameters, NULL, SW_SHOWNORMAL );
	}
}

/**
 * Resolves UNC path to local (full) path if possible.
 *
 * @param	InUNCPath		UNC path to resolve
 *
 * @param	OutPath		Resolved local path
 *
 * @return true if the path was resolved, false otherwise
 */
bool FWindowsPlatformProcess::ResolveNetworkPath( FString InUNCPath, FString& OutPath )
{
	// Get local machine name first and check if this UNC path points to local share
	// (if it's not UNC path it will also fail this check)
	uint32 ComputerNameSize = MAX_COMPUTERNAME_LENGTH;
	TCHAR ComputerName[MAX_COMPUTERNAME_LENGTH + 3] = { '\\', '\\', '\0', };

	if ( GetComputerName( ComputerName + 2, (::DWORD*)&ComputerNameSize ) )
	{
		// Check if the filename is pointing to local shared folder
		if ( InUNCPath.StartsWith( ComputerName ) )
		{
			// Get the share name (it's the first folder after the computer name)
			int32 ComputerNameLen = FCString::Strlen( ComputerName );
			int32 ShareNameLen = InUNCPath.Find( TEXT( "\\" ), ESearchCase::CaseSensitive, ESearchDir::FromStart, ComputerNameLen + 1 ) - ComputerNameLen - 1;
			FString ShareName = InUNCPath.Mid( ComputerNameLen + 1, ShareNameLen );

			// NetShareGetInfo doesn't accept const TCHAR* as the share name so copy to temp array
			SHARE_INFO_2* BufPtr = NULL;
			::NET_API_STATUS res;
			TCHAR ShareNamePtr[MAX_PATH];
			FCString::Strcpy(ShareNamePtr, ShareName.Len() + 1, *ShareName);

			// Call the NetShareGetInfo function, specifying level 2
			if ( ( res = NetShareGetInfo( NULL, ShareNamePtr, 2, (LPBYTE*)&BufPtr ) ) == ERROR_SUCCESS )
			{
				// Construct the local path
				OutPath = FString( BufPtr->shi2_path ) + InUNCPath.Mid( ComputerNameLen + 1 + ShareNameLen );

				// Free the buffer allocated by NetShareGetInfo
				NetApiBufferFree(BufPtr);
				
				return true;
			}
		}
	}

	// InUNCPath is not an UNC path or it's not pointing to local folder or something went wrong in NetShareGetInfo (insufficient privileges?)
	return false;
}

void FWindowsPlatformProcess::Sleep( float Seconds )
{
	SCOPE_CYCLE_COUNTER( STAT_Sleep );
	FThreadIdleStats::FScopeIdle Scope;
	SleepNoStats( Seconds );
}

void FWindowsPlatformProcess::SleepNoStats(float Seconds)
{
	uint32 Milliseconds = (uint32)(Seconds * 1000.0);
	if (Milliseconds == 0)
	{
		::SwitchToThread();
	}
	::Sleep(Milliseconds);
}

void FWindowsPlatformProcess::SleepInfinite()
{
	check(FPlatformProcess::SupportsMultithreading());
	::Sleep(INFINITE);
}

#include "WindowsEvent.h"

FEvent* FWindowsPlatformProcess::CreateSynchEvent(bool bIsManualReset)
{
	// Allocate the new object
	FEvent* Event = NULL;	
	if (FPlatformProcess::SupportsMultithreading())
	{
		Event = new FEventWin();
	}
	else
	{
		// Fake event object.
		Event = new FSingleThreadEvent();
	}
	// If the internal create fails, delete the instance and return NULL
	if (!Event->Create(bIsManualReset))
	{
		delete Event;
		Event = NULL;
	}
	return Event;
}

#include "Windows/AllowWindowsPlatformTypes.h"

bool FEventWin::Wait(uint32 WaitTime, const bool bIgnoreThreadIdleStats /*= false*/)
{
	WaitForStats();

	SCOPE_CYCLE_COUNTER( STAT_EventWait );
	check( Event );

	FThreadIdleStats::FScopeIdle Scope( bIgnoreThreadIdleStats );
	return (WaitForSingleObject( Event, WaitTime ) == WAIT_OBJECT_0);
}

void FEventWin::Trigger()
{
	TriggerForStats();
	check( Event );
	SetEvent( Event );
}

void FEventWin::Reset()
{
	ResetForStats();
	check( Event );
	ResetEvent( Event );
}

#include "Windows/HideWindowsPlatformTypes.h"

#include "WindowsRunnableThread.h"

FRunnableThread* FWindowsPlatformProcess::CreateRunnableThread()
{
	return new FRunnableThreadWin();
}

void FWindowsPlatformProcess::ClosePipe( void* ReadPipe, void* WritePipe )
{
	if ( ReadPipe != NULL && ReadPipe != INVALID_HANDLE_VALUE )
	{
		::CloseHandle(ReadPipe);
	}
	if ( WritePipe != NULL && WritePipe != INVALID_HANDLE_VALUE )
	{
		::CloseHandle(WritePipe);
	}
}

bool FWindowsPlatformProcess::CreatePipe( void*& ReadPipe, void*& WritePipe )
{
	SECURITY_ATTRIBUTES Attr = { sizeof(SECURITY_ATTRIBUTES), NULL, true };
	
	if (!::CreatePipe(&ReadPipe, &WritePipe, &Attr, 0))
	{
		return false;
	}

	if (!::SetHandleInformation(ReadPipe, HANDLE_FLAG_INHERIT, 0))
	{
		return false;
	}

	return true;
}

FString FWindowsPlatformProcess::ReadPipe( void* ReadPipe )
{
	FString Output;

	uint32 BytesAvailable = 0;
	if (::PeekNamedPipe(ReadPipe, NULL, 0, NULL, (::DWORD*)&BytesAvailable, NULL) && (BytesAvailable > 0))
	{
		UTF8CHAR* Buffer = new UTF8CHAR[BytesAvailable + 1];
		uint32 BytesRead = 0;
		if (::ReadFile(ReadPipe, Buffer, BytesAvailable, (::DWORD*)&BytesRead, NULL))
		{
			if (BytesRead > 0)
			{
				Buffer[BytesRead] = '\0';
				Output += FUTF8ToTCHAR((const ANSICHAR*)Buffer).Get();
			}
		}
		delete [] Buffer;
	}

	return Output;
}

bool FWindowsPlatformProcess::ReadPipeToArray(void* ReadPipe, TArray<uint8> & Output)
{
	uint32 BytesAvailable = 0;
	if (::PeekNamedPipe(ReadPipe, NULL, 0, NULL, (::DWORD*)&BytesAvailable, NULL) && (BytesAvailable > 0))
	{
		Output.SetNumUninitialized(BytesAvailable);
		uint32 BytesRead = 0;
		if (::ReadFile(ReadPipe, Output.GetData(), BytesAvailable, (::DWORD*)&BytesRead, NULL))
		{
			if (BytesRead < BytesAvailable)
			{
				Output.SetNum(BytesRead);
			}

			return true;
		}
		else
		{
			Output.Empty();
		}
	}

	return false;
}

bool FWindowsPlatformProcess::WritePipe(void* WritePipe, const FString& Message, FString* OutWritten)
{
	// If there is not a message or WritePipe is null
	if (Message.Len() == 0 || WritePipe == nullptr)
	{
		return false;
	}

	// Convert input to UTF8CHAR
	uint32 BytesAvailable = Message.Len();
	UTF8CHAR * Buffer = new UTF8CHAR[BytesAvailable + 1];
	for (uint32 i = 0; i < BytesAvailable; i++)
	{
		Buffer[i] = Message[i];
	}
	Buffer[BytesAvailable] = '\n';

	// Write to pipe
	uint32 BytesWritten = 0;
	bool bIsWritten = !!WriteFile(WritePipe, Buffer, BytesAvailable + 1, (::DWORD*)&BytesWritten, nullptr);

	// Get written message
	if (OutWritten)
	{
		Buffer[BytesWritten] = '\0';
		*OutWritten = FUTF8ToTCHAR((const ANSICHAR*)Buffer).Get();
	}

	return bIsWritten;
}

#include "Windows/AllowWindowsPlatformTypes.h"

FWindowsPlatformProcess::FWindowsSemaphore::FWindowsSemaphore(const FString & InName, HANDLE InSemaphore)
	:	FSemaphore(InName)
	,	Semaphore(InSemaphore)
{
}

FWindowsPlatformProcess::FWindowsSemaphore::~FWindowsSemaphore()
{
	// actual cleanup should be done in DeleteInterprocessSynchObject() since it can return errors
}

void FWindowsPlatformProcess::FWindowsSemaphore::Lock()
{
	check(Semaphore);
	DWORD WaitResult = WaitForSingleObject(Semaphore, INFINITE);
	if (WaitResult != WAIT_OBJECT_0)
	{
		DWORD ErrNo = GetLastError();
		UE_LOG(LogHAL, Warning, TEXT("WaitForSingleObject(,INFINITE) for semaphore '%s' failed with return code 0x%08x and LastError = %d"),
			GetName(),
			WaitResult,
			ErrNo);
	}
}

bool FWindowsPlatformProcess::FWindowsSemaphore::TryLock(uint64 NanosecondsToWait)
{
	check(Semaphore);
	DWORD MillisecondsToWait = NanosecondsToWait / 1000000ULL;
	DWORD WaitResult = WaitForSingleObject(Semaphore, MillisecondsToWait);
	if (WaitResult != WAIT_OBJECT_0 && WaitResult != WAIT_TIMEOUT)	// timeout is not a warning
	{
		DWORD ErrNo = GetLastError();
		UE_LOG(LogHAL, Warning, TEXT("WaitForSingleObject(,INFINITE) for semaphore '%s' failed with return code 0x%08x and LastError = %d"),
			GetName(),
			WaitResult,
			ErrNo);
	}

	return WaitResult == WAIT_OBJECT_0;
}

void FWindowsPlatformProcess::FWindowsSemaphore::Unlock()
{
	check(Semaphore);
	if (!ReleaseSemaphore(Semaphore, 1, NULL))
	{
		DWORD ErrNo = GetLastError();
		UE_LOG(LogHAL, Warning, TEXT("ReleaseSemaphore(,ReleaseCount=1,) for semaphore '%s' failed with LastError = %d"),
			GetName(),
			ErrNo);
	}
}

FWindowsPlatformProcess::FSemaphore * FWindowsPlatformProcess::NewInterprocessSynchObject(const FString & Name, bool bCreate, uint32 MaxLocks)
{
	HANDLE Semaphore = NULL;
	
	if (bCreate)
	{
		Semaphore = CreateSemaphore(NULL, MaxLocks, MaxLocks, *Name);
		if (NULL == Semaphore)
		{
			DWORD ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("CreateSemaphore(Attrs=NULL, InitialValue=%d, MaxValue=%d, Name='%s') failed with LastError = %d"),
				MaxLocks, MaxLocks,
				*Name,
				ErrNo);
			return NULL;
		}
	}
	else
	{
		DWORD AccessRights = SYNCHRONIZE | SEMAPHORE_MODIFY_STATE;
		Semaphore = OpenSemaphore(AccessRights, false, *Name);
		if (NULL == Semaphore)
		{
			DWORD ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("OpenSemaphore(AccessRights=0x%08x, bInherit=false, Name='%s') failed with LastError = %d"),
				AccessRights,
				*Name,
				ErrNo);
			return NULL;
		}
	}
	check(Semaphore);

	return new FWindowsSemaphore(Name, Semaphore);
}

bool FWindowsPlatformProcess::DeleteInterprocessSynchObject(FSemaphore * Object)
{
	if (NULL == Object)
	{
		return false;
	}

	FWindowsSemaphore * WinSem = static_cast< FWindowsSemaphore * >(Object);
	check( WinSem );

	HANDLE Semaphore = WinSem->GetSemaphore();
	bool bSucceeded = false;
	if (Semaphore)
	{
		bSucceeded = (CloseHandle(Semaphore) == TRUE);
		if (!bSucceeded)
		{
			DWORD ErrNo = GetLastError();
			UE_LOG(LogHAL, Warning, TEXT("CloseHandle() for semaphore '%s' failed with LastError = %d"),
				Object->GetName(),
				ErrNo);
		}
	}

	// delete anyways
	delete WinSem;

	return bSucceeded;
}

bool FWindowsPlatformProcess::Daemonize()
{
	// TODO: implement
	return true;
}

void *FWindowsPlatformProcess::LoadLibraryWithSearchPaths(const FString& FileName, const TArray<FString>& SearchPaths)
{
	// Make sure the initial module exists. If we can't find it from the path we're given, it's probably a system dll.
	FString FullFileName = FileName;
	if (FPaths::FileExists(*FullFileName))
	{
		// Convert it to a full path, since LoadLibrary will try to resolve it against the executable directory (which may not be the same as the working dir)
		FullFileName = FPaths::ConvertRelativePathToFull(FullFileName);

		// Create a list of files which we've already checked for imports. Don't add the initial file to this list to improve the resolution of dependencies for direct circular dependencies of this
		// module; by allowing the module to be visited twice, any mutually depended on DLLs will be visited first.
		TArray<FString> VisitedImportNames;

		// Find a list of all the DLLs that need to be loaded
		TArray<FString> ImportFileNames;
		ResolveImportsRecursive(*FullFileName, SearchPaths, ImportFileNames, VisitedImportNames);

		// Load all the missing dependencies first
		for (int32 Idx = 0; Idx < ImportFileNames.Num(); Idx++)
		{
			if (GetModuleHandle(*ImportFileNames[Idx]) == nullptr)
			{
				if(LoadLibrary(*ImportFileNames[Idx]))
				{
					UE_LOG(LogWindows, Verbose, TEXT("Preloaded '%s'"), *ImportFileNames[Idx]);
				}
				else
				{
					UE_LOG(LogWindows, Log, TEXT("Failed to preload '%s' (GetLastError=%d)"), *ImportFileNames[Idx], GetLastError());
					LogImportDiagnostics(ImportFileNames[Idx], SearchPaths);
				}
			}
		}
	}

	// Try to load the actual library
	void* Handle = LoadLibrary(*FullFileName);
	if(Handle)
	{
		UE_LOG(LogWindows, Verbose, TEXT("Loaded %s"), *FullFileName);
	}
	else
	{
		UE_LOG(LogWindows, Log, TEXT("Failed to load '%s' (GetLastError=%d)"), *FileName, ::GetLastError());
		if(IFileManager::Get().FileExists(*FileName))
		{
			LogImportDiagnostics(FileName, SearchPaths);
		}
		else
		{
			UE_LOG(LogWindows, Log, TEXT("File '%s' does not exist"), *FileName);
		}
	}
	return Handle;
}

void FWindowsPlatformProcess::ResolveImportsRecursive(const FString& FileName, const TArray<FString>& SearchPaths, TArray<FString>& ImportFileNames, TArray<FString>& VisitedImportNames)
{
	// Read the imports for this library
	TArray<FString> ImportNames;
	if(ReadLibraryImports(*FileName, ImportNames))
	{
		// Find all the imports that haven't already been resolved
		for(int Idx = 0; Idx < ImportNames.Num(); Idx++)
		{
			const FString &ImportName = ImportNames[Idx];
			if(!VisitedImportNames.Contains(ImportName))
			{
				// Prevent checking this import again
				VisitedImportNames.Add(ImportName);

				// Try to resolve this import
				FString ImportFileName;
				if(ResolveImport(*ImportName, SearchPaths, ImportFileName))
				{
					ResolveImportsRecursive(ImportFileName, SearchPaths, ImportFileNames, VisitedImportNames);
					ImportFileNames.Add(ImportFileName);
				}
			}
		}
	}
}

bool FWindowsPlatformProcess::ResolveImport(const FString& Name, const TArray<FString>& SearchPaths, FString& OutFileName)
{
	// Look for the named DLL on any of the search paths
	for(int Idx = 0; Idx < SearchPaths.Num(); Idx++)
	{
		FString FileName = SearchPaths[Idx] / Name;
		if(FPaths::FileExists(FileName))
		{
			OutFileName = FPaths::ConvertRelativePathToFull(FileName);
			return true;
		}
	}
	return false;
}

bool FWindowsPlatformProcess::ReadLibraryImports(const TCHAR* FileName, TArray<FString>& ImportNames)
{
	bool bResult = false;

	// Open the DLL using a file mapping, so we don't need to map any more than is necessary
	HANDLE NewFileHandle = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if(NewFileHandle != INVALID_HANDLE_VALUE)
	{
		HANDLE NewFileMappingHandle = CreateFileMapping(NewFileHandle, NULL, PAGE_READONLY, 0, 0, NULL);
		if(NewFileMappingHandle != NULL)
		{
			void* NewData = MapViewOfFile(NewFileMappingHandle, FILE_MAP_READ, 0, 0, 0);
			if(NewData != NULL)
			{
				const IMAGE_DOS_HEADER* Header = (const IMAGE_DOS_HEADER*)NewData;
				bResult = ReadLibraryImportsFromMemory(Header, ImportNames);
				UnmapViewOfFile(NewData);
			}
			CloseHandle(NewFileMappingHandle);
		}
		CloseHandle(NewFileHandle);
	}

	return bResult;
}

bool ReadLibraryImportsFromMemory(const IMAGE_DOS_HEADER *Header, TArray<FString> &ImportNames)
{
	bool bResult = false;
	if(Header->e_magic == IMAGE_DOS_SIGNATURE)
	{
		IMAGE_NT_HEADERS *NtHeader = (IMAGE_NT_HEADERS*)((BYTE*)Header + Header->e_lfanew);
		if(NtHeader->Signature == IMAGE_NT_SIGNATURE)
		{
			// Find the import directory header
			IMAGE_DATA_DIRECTORY *ImportDirectoryEntry = &NtHeader->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];

			// Enumerate the imports
			IMAGE_IMPORT_DESCRIPTOR *ImportDescriptors = (IMAGE_IMPORT_DESCRIPTOR*)MapRvaToPointer(Header, NtHeader, ImportDirectoryEntry->VirtualAddress);
			for(size_t ImportIdx = 0; ImportIdx * sizeof(IMAGE_IMPORT_DESCRIPTOR) < ImportDirectoryEntry->Size; ImportIdx++)
			{
				IMAGE_IMPORT_DESCRIPTOR *ImportDescriptor = ImportDescriptors + ImportIdx;
				if(ImportDescriptor->Name != 0)
				{
					const char *ImportName = (const char*)MapRvaToPointer(Header, NtHeader, ImportDescriptor->Name);
					ImportNames.Add(ImportName);
				}
			}

			bResult = true;
		}
	}
	return bResult;
}

const void *MapRvaToPointer(const IMAGE_DOS_HEADER *Header, const IMAGE_NT_HEADERS *NtHeader, size_t Rva)
{
	const IMAGE_SECTION_HEADER *SectionHeaders = (const IMAGE_SECTION_HEADER*)(NtHeader + 1);
	for(size_t SectionIdx = 0; SectionIdx < NtHeader->FileHeader.NumberOfSections; SectionIdx++)
	{
		const IMAGE_SECTION_HEADER *SectionHeader = SectionHeaders + SectionIdx;
		if(Rva >= SectionHeader->VirtualAddress && Rva < SectionHeader->VirtualAddress + SectionHeader->SizeOfRawData)
		{
			return (const BYTE*)Header + SectionHeader->PointerToRawData + (Rva - SectionHeader->VirtualAddress);
		}
	}
	return NULL;
}

void FWindowsPlatformProcess::LogImportDiagnostics(const FString& FileName, const TArray<FString>& SearchPaths)
{
	TArray<FString> ImportNames;
	if(ReadLibraryImports(*FileName, ImportNames))
	{
		bool bIncludeSearchPaths = false;
		for(const FString& ImportName : ImportNames)
		{
			if(GetModuleHandle(*ImportName) == nullptr)
			{
				UE_LOG(LogWindows, Log, TEXT("  Missing import: %s"), *ImportName);
				bIncludeSearchPaths = true;
			}
		}
		if(bIncludeSearchPaths)
		{
			for (const FString& SearchPath : SearchPaths)
			{
				UE_LOG(LogWindows, Log, TEXT("  Looked in: %s"), *SearchPath);
			}
		}
	}
}

FWindowsPlatformProcess::FProcEnumerator::FProcEnumerator()
{
	SnapshotHandle = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	CurrentEntry = new PROCESSENTRY32();
	CurrentEntry->dwSize = 0;
}

FWindowsPlatformProcess::FProcEnumerator::~FProcEnumerator()
{
	::CloseHandle(SnapshotHandle);
	delete CurrentEntry;
}

FWindowsPlatformProcess::FProcEnumInfo::FProcEnumInfo(const PROCESSENTRY32& InInfo)
	: Info(new PROCESSENTRY32(InInfo))
{

}

FWindowsPlatformProcess::FProcEnumInfo::~FProcEnumInfo()
{
	delete Info;
}

bool FWindowsPlatformProcess::FProcEnumerator::MoveNext()
{
	if (CurrentEntry->dwSize == 0)
	{
		CurrentEntry->dwSize = sizeof(PROCESSENTRY32);

		return ::Process32First(SnapshotHandle, CurrentEntry) == TRUE;
	}

	return ::Process32Next(SnapshotHandle, CurrentEntry) == TRUE;
}

FWindowsPlatformProcess::FProcEnumInfo FWindowsPlatformProcess::FProcEnumerator::GetCurrent() const
{
	return FProcEnumInfo(*CurrentEntry);
}

uint32 FWindowsPlatformProcess::FProcEnumInfo::GetPID() const
{
	return Info->th32ProcessID;
}

uint32 FWindowsPlatformProcess::FProcEnumInfo::GetParentPID() const
{
	return Info->th32ParentProcessID;
}

FString FWindowsPlatformProcess::FProcEnumInfo::GetName() const
{
	return Info->szExeFile;
}

FString FWindowsPlatformProcess::FProcEnumInfo::GetFullPath() const
{
	return GetApplicationName(GetPID());
}

#include "Windows/HideWindowsPlatformTypes.h"
