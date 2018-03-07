// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Installer/InstallerError.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace BuildPatchServices
{
	class FFakeInstallerError
		: public IInstallerError
	{
	public:
		virtual bool HasError() const override
		{
			return bHasError;
		}

		virtual bool IsCancelled() const override
		{
			return bIsCancelled;
		}

		virtual bool CanRetry() const override
		{
			return bCanRetry;
		}

		virtual EBuildPatchInstallError GetErrorType() const override
		{
			return ErrorType;
		}

		virtual FString GetErrorCode() const override
		{
			return ErrorCode;
		}

		virtual FText GetErrorText() const override
		{
			return ErrorText;
		}

		virtual void SetError(EBuildPatchInstallError InErrorType, const TCHAR* InErrorCode, FText InErrorText) override
		{
			ErrorType = InErrorType;
			ErrorCode = InErrorCode;
			ErrorText = MoveTemp(InErrorText);
		}

		virtual int32 RegisterForErrors(FOnErrorDelegate Delegate)
		{
			int32 Handle = HandleCount++;
			Delegates.Add(Handle, Delegate);
			return Handle;
		}

		virtual void UnregisterForErrors(int32 Handle) override
		{
			Delegates.Remove(Handle);
		}

	public:
		bool bHasError;
		bool bIsCancelled;
		bool bCanRetry;
		EBuildPatchInstallError ErrorType;
		FString ErrorCode;
		FText ErrorText;
		TMap<int32, FOnErrorDelegate> Delegates;
		int32 HandleCount;
	};
}

#endif //WITH_DEV_AUTOMATION_TESTS
