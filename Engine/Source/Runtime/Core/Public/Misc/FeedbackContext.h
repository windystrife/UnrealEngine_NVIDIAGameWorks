// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/OutputDevice.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Internationalization/Text.h"
#include "Misc/ScopeLock.h"
#include "Misc/SlowTask.h"
#include "Misc/SlowTaskStack.h"

class FContextSupplier;
class SBuildProgressWidget;

/** A context for displaying modal warning messages. */
class CORE_API FFeedbackContext
	: public FOutputDevice
{
public:

	/** Ask the user a binary question, returning their answer */
	virtual bool YesNof( const FText& Question ) { return false; }
	
	/** Return whether the user has requested to cancel the current slow task */
	virtual bool ReceivedUserCancel() { return false; };
	
	/** Public const access to the current state of the scope stack */
	FORCEINLINE const FSlowTaskStack& GetScopeStack() const
	{
		return *ScopeStack;
	}

	/**** Legacy API - not deprecated as it's still in heavy use, but superceded by FScopedSlowTask ****/
	void BeginSlowTask( const FText& Task, bool ShowProgressDialog, bool bShowCancelButton=false );
	void UpdateProgress( int32 Numerator, int32 Denominator );
	void StatusUpdate( int32 Numerator, int32 Denominator, const FText& StatusText );
	void StatusForceUpdate( int32 Numerator, int32 Denominator, const FText& StatusText );
	void EndSlowTask();
	/**** end legacy API ****/

protected:

	/**
	 * Called to create a slow task
	 */
	virtual void StartSlowTask( const FText& Task, bool bShowCancelButton=false )
	{
		GIsSlowTask = true;
	}

	/**
	 * Called to destroy a slow task
	 */
	virtual void FinalizeSlowTask( )
	{
		GIsSlowTask = false;
	}

	/**
	 * Called when some progress has occurred
	 * @param	TotalProgressInterp		[0..1] Value indicating the total progress of the slow task
	 * @param	DisplayMessage			The message to display on the slow task
	 */
	virtual void ProgressReported( const float TotalProgressInterp, FText DisplayMessage ) {}

	/** Called to check whether we are playing in editor when starting a slow task */
	virtual bool IsPlayingInEditor() const;

public:
	virtual FContextSupplier* GetContext() const { return NULL; }
	virtual void SetContext( FContextSupplier* InSupplier ) {}

	/** Shows/Closes Special Build Progress dialogs */
	virtual TWeakPtr<class SBuildProgressWidget> ShowBuildProgressWindow() {return TWeakPtr<class SBuildProgressWidget>();}
	virtual void CloseBuildProgressWindow() {}

	bool	TreatWarningsAsErrors;

	FFeedbackContext();

	virtual ~FFeedbackContext();

	/** Gets warnings history */
	void GetWarnings(TArray<FString>& OutWarnings) const
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		OutWarnings = Warnings;
	}
	int32 GetNumWarnings() const
	{
		return Warnings.Num();
	}

	/** Gets errors history */
	void GetErrors(TArray<FString>& OutErrors) const
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		OutErrors = Errors;
	}
	int32 GetNumErrors() const
	{
		return Errors.Num();
	}

	/** Gets all errors and warnings and clears the history */
	void GetErrorsAndWarningsAndEmpty(TArray<FString>& OutWarningsAndErrors)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		OutWarningsAndErrors = MoveTemp(Errors);
		OutWarningsAndErrors += MoveTemp(Warnings);
	}
	/** Clears all history */
	void ClearWarningsAndErrors()
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Errors.Empty();
		Warnings.Empty();
	}

private:
	FFeedbackContext(const FFeedbackContext&);
	FFeedbackContext& operator=(const FFeedbackContext&);

	/** Warnings history */
	TArray<FString> Warnings;
	/** Errors history */
	TArray<FString> Errors;
	/** Guard for the errors and warnings history */
	mutable FCriticalSection WarningsAndErrorsCritical;

protected:
	
	friend FSlowTask;

	/** Stack of pointers to feedback scopes that are currently open */
	TSharedRef<FSlowTaskStack> ScopeStack;
	TArray<TUniquePtr<FSlowTask>> LegacyAPIScopes;

	/** Ask that the UI be updated as a result of the scope stack changing */
	void RequestUpdateUI(bool bForceUpdate = false);

	/** Update the UI as a result of the scope stack changing */
	void UpdateUI();

	/**
	 * Adds a new warning message to warnings history.
	 * @param InWarning Warning message
	 */
	void AddWarning(const FString& InWarning)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Warnings.Add(InWarning);
	}

	/**
	* Adds a new error message to errors history.
	* @param InWarning Error message
	*/
	void AddError(const FString& InError)
	{
		FScopeLock WarningsAndErrorsLock(&WarningsAndErrorsCritical);
		Errors.Add(InError);
	}
};
