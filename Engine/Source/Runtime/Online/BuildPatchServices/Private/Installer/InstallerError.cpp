// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/InstallerError.h"
#include "Misc/ScopeLock.h"
#include "IBuildInstaller.h"
#include "BuildPatchServicesPrivate.h"
#include "ThreadSafeCounter.h"

namespace BuildPatchServices
{
	FText GetStandardErrorText(EBuildPatchInstallError ErrorType)
	{
		// The generic texts, for when not specified by the system reporting the error.
		static const FText NoError(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_NoError", "The operation was successful."));
		static const FText DownloadError(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_DownloadError", "Could not download patch data. Please try again later."));
		static const FText FileConstructionFail(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_FileConstructionFail", "A file corruption has occurred. Please try again."));
		static const FText MoveFileToInstall(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_MoveFileToInstall", "A file access error has occurred. Please check your running processes."));
		static const FText BuildVerifyFail(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_BuildCorrupt", "The installation is corrupt. Please contact support."));
		static const FText ApplicationClosing(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_ApplicationClosing", "The application is closing."));
		static const FText ApplicationError(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_ApplicationError", "Patching service could not start. Please contact support."));
		static const FText UserCanceled(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_UserCanceled", "User cancelled."));
		static const FText PrerequisiteError(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_PrerequisiteError", "The necessary prerequisites have failed to install. Please contact support."));
		static const FText InitializationError(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_InitializationError", "The installer failed to initialize. Please contact support."));
		static const FText PathLengthExceeded(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_PathLengthExceeded", "Maximum path length exceeded. Please specify a shorter install location."));
		static const FText OutOfDiskSpace(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_OutOfDiskSpace", "Not enough disk space available. Please free up some disk space and try again."));
		static const FText InvalidOrMax(NSLOCTEXT("BuildPatchInstallError", "BuildPatchInstallShortError_InvalidOrMax", "An unknown error ocurred. Please contact support."));

		switch (ErrorType)
		{
			case EBuildPatchInstallError::NoError: return NoError;
			case EBuildPatchInstallError::DownloadError: return DownloadError;
			case EBuildPatchInstallError::FileConstructionFail: return FileConstructionFail;
			case EBuildPatchInstallError::MoveFileToInstall: return MoveFileToInstall;
			case EBuildPatchInstallError::BuildVerifyFail: return BuildVerifyFail;
			case EBuildPatchInstallError::ApplicationClosing: return ApplicationClosing;
			case EBuildPatchInstallError::ApplicationError: return ApplicationError;
			case EBuildPatchInstallError::UserCanceled: return UserCanceled;
			case EBuildPatchInstallError::PrerequisiteError: return PrerequisiteError;
			case EBuildPatchInstallError::InitializationError: return InitializationError;
			case EBuildPatchInstallError::PathLengthExceeded: return PathLengthExceeded;
			case EBuildPatchInstallError::OutOfDiskSpace: return OutOfDiskSpace;
			default: return InvalidOrMax;
		}
	}

	FText GetDiskSpaceMessage(const FString& Location, uint64 RequiredBytes, uint64 AvailableBytes, const FNumberFormattingOptions* FormatOptions)
	{
		static const FText OutOfDiskSpace(NSLOCTEXT("BuildPatchInstallError", "InstallDirectoryDiskSpace", "There is not enough space at {Location}\n{RequiredBytes} is required.\n{AvailableBytes} is available.\nYou need an additional {SpaceAdditional} to perform the installation."));
		static const FNumberFormattingOptions DefaultOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(2)
			.SetMaximumFractionalDigits(2);
		FormatOptions = FormatOptions == nullptr ? &DefaultOptions : FormatOptions;
		FFormatNamedArguments Arguments;
		Arguments.Emplace(TEXT("Location"), FText::FromString(Location));
		Arguments.Emplace(TEXT("RequiredBytes"), FText::AsMemory(RequiredBytes, FormatOptions));
		Arguments.Emplace(TEXT("AvailableBytes"), FText::AsMemory(AvailableBytes, FormatOptions));
		Arguments.Emplace(TEXT("SpaceAdditional"), FText::AsMemory(RequiredBytes - AvailableBytes, FormatOptions));
		return FText::Format(OutOfDiskSpace, Arguments);
	}

	class FInstallerError
		: public IInstallerError
	{
	public:
		FInstallerError();
		~FInstallerError();

		// IInstallerError interface begin.
		virtual bool HasError() const override;
		virtual bool IsCancelled() const override;
		virtual bool CanRetry() const override;
		virtual EBuildPatchInstallError GetErrorType() const override;
		virtual FString GetErrorCode() const override;
		virtual FText GetErrorText() const override;
		virtual void SetError(EBuildPatchInstallError InErrorType, const TCHAR* InErrorCode, FText InErrorText) override;
		virtual int32 RegisterForErrors(FOnErrorDelegate Delegate) override;
		virtual void UnregisterForErrors(int32 Handle) override;
		// IInstallerError interface end.

	private:
		mutable FCriticalSection ThreadLockCs;
		EBuildPatchInstallError ErrorType;
		FString ErrorCode;
		FText ErrorText;
		FThreadSafeCounter DelegateCounter;
		TMap<int32, FOnErrorDelegate> RegisteredDelegates;
	};

	FInstallerError::FInstallerError()
		: ThreadLockCs()
		, ErrorType(EBuildPatchInstallError::NoError)
		, ErrorCode(InstallErrorPrefixes::ErrorTypeStrings[static_cast<int32>(ErrorType)])
		, ErrorText(GetStandardErrorText(ErrorType))
		, DelegateCounter(0)
		, RegisteredDelegates()
	{
	}

	FInstallerError::~FInstallerError()
	{
	}

	bool FInstallerError::HasError() const
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		return ErrorType != EBuildPatchInstallError::NoError;
	}

	bool FInstallerError::IsCancelled() const
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		return ErrorType == EBuildPatchInstallError::UserCanceled
			|| ErrorType == EBuildPatchInstallError::ApplicationClosing;
	}

	bool FInstallerError::CanRetry() const
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		return ErrorType != EBuildPatchInstallError::DownloadError
			&& ErrorType != EBuildPatchInstallError::MoveFileToInstall
			&& ErrorType != EBuildPatchInstallError::InitializationError
			&& ErrorType != EBuildPatchInstallError::PathLengthExceeded
			&& ErrorType != EBuildPatchInstallError::OutOfDiskSpace;
	}

	EBuildPatchInstallError FInstallerError::GetErrorType() const
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		return ErrorType;
	}

	FString FInstallerError::GetErrorCode() const
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		return ErrorCode;
	}

	FText FInstallerError::GetErrorText() const
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		return ErrorText;
	}

	void FInstallerError::SetError(EBuildPatchInstallError InErrorType, const TCHAR* InErrorCode, FText InErrorText)
	{
		// Only accept the first error
		TArray<FOnErrorDelegate> DelegatesToCall;
		ThreadLockCs.Lock();
		if (!HasError())
		{
			ErrorType = InErrorType;
			ErrorCode = InstallErrorPrefixes::ErrorTypeStrings[static_cast<int32>(InErrorType)];
			ErrorCode += InErrorCode;
			ErrorText = InErrorText.IsEmpty() ? GetStandardErrorText(ErrorType) : MoveTemp(InErrorText);
			RegisteredDelegates.GenerateValueArray(DelegatesToCall);
			if (IsCancelled())
			{
				UE_LOG(LogBuildPatchServices, Log, TEXT("%s %s"), *EnumToString(ErrorType), *ErrorCode);
			}
			else
			{
				UE_LOG(LogBuildPatchServices, Error, TEXT("%s %s"), *EnumToString(ErrorType), *ErrorCode);
			}
		}
		ThreadLockCs.Unlock();
		for (const FOnErrorDelegate& DelegateToCall : DelegatesToCall)
		{
			DelegateToCall();
		}
	}

	int32 FInstallerError::RegisterForErrors(FOnErrorDelegate Delegate)
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		int32 Handle = DelegateCounter.Increment();
		RegisteredDelegates.Add(Handle, Delegate);
		return Handle;
	}

	void FInstallerError::UnregisterForErrors(int32 Handle)
	{
		FScopeLock ScopeLock(&ThreadLockCs);
		RegisteredDelegates.Remove(Handle);
	}

	IInstallerError* FInstallerErrorFactory::Create()
	{
		return new FInstallerError();
	}

	const FString& EnumToString(const EBuildPatchInstallError& InErrorType)
	{
		// Const enum strings, special case no error.
		static const FString NoError("SUCCESS");
		static const FString DownloadError("EBuildPatchInstallError::DownloadError");
		static const FString FileConstructionFail("EBuildPatchInstallError::FileConstructionFail");
		static const FString MoveFileToInstall("EBuildPatchInstallError::MoveFileToInstall");
		static const FString BuildVerifyFail("EBuildPatchInstallError::BuildVerifyFail");
		static const FString ApplicationClosing("EBuildPatchInstallError::ApplicationClosing");
		static const FString ApplicationError("EBuildPatchInstallError::ApplicationError");
		static const FString UserCanceled("EBuildPatchInstallError::UserCanceled");
		static const FString PrerequisiteError("EBuildPatchInstallError::PrerequisiteError");
		static const FString InitializationError("EBuildPatchInstallError::InitializationError");
		static const FString PathLengthExceeded("EBuildPatchInstallError::PathLengthExceeded");
		static const FString OutOfDiskSpace("EBuildPatchInstallError::OutOfDiskSpace");
		static const FString InvalidOrMax("EBuildPatchInstallError::InvalidOrMax");

		switch (InErrorType)
		{
			case EBuildPatchInstallError::NoError: return NoError;
			case EBuildPatchInstallError::DownloadError: return DownloadError;
			case EBuildPatchInstallError::FileConstructionFail: return FileConstructionFail;
			case EBuildPatchInstallError::MoveFileToInstall: return MoveFileToInstall;
			case EBuildPatchInstallError::BuildVerifyFail: return BuildVerifyFail;
			case EBuildPatchInstallError::ApplicationClosing: return ApplicationClosing;
			case EBuildPatchInstallError::ApplicationError: return ApplicationError;
			case EBuildPatchInstallError::UserCanceled: return UserCanceled;
			case EBuildPatchInstallError::PrerequisiteError: return PrerequisiteError;
			case EBuildPatchInstallError::InitializationError: return InitializationError;
			case EBuildPatchInstallError::PathLengthExceeded: return PathLengthExceeded;
			case EBuildPatchInstallError::OutOfDiskSpace: return OutOfDiskSpace;
			default: return InvalidOrMax;
		}
	}

}
