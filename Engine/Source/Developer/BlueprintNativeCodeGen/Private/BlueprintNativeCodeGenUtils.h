// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "Misc/FeedbackContext.h"
#include "Engine/Blueprint.h" // for FCompilerNativizationOptions

class SBuildProgressWidget;
struct FBlueprintNativeCodeGenManifest;

// Forward declares
class  SBuildProgressWidget;
struct FBlueprintNativeCodeGenManifest;

DECLARE_LOG_CATEGORY_EXTERN(LogBlueprintCodeGen, Log, All);

/**  */
struct FBlueprintNativeCodeGenUtils
{
public:
	/**
	 * Generated module build file, module source files, and plugin description file for the provided manifest.
	 *
	 * @param	Manifest		The manifest containing lists of converted files, etc
	 */
	static bool FinalizePlugin(const FBlueprintNativeCodeGenManifest& Manifest);

	/**
	 * Recompiles the bytecode of a blueprint only. Should only be run for
	 * recompiling dependencies during compile on load.
	 * 
	 * @param  Obj				The asset object that you want to generate source code for (a Blueprint, or UserDefinedEnum/Struct)
	 * @param  OutHeaderSource	Output destination for the header source.
	 * @param  OutCppSource		Output destination for the cpp source.
	 */
	static void GenerateCppCode(UObject* Obj, TSharedPtr<FString> OutHeaderSource, TSharedPtr<FString> OutCppSource, TSharedPtr<FNativizationSummary> NativizationSummary, const FCompilerNativizationOptions& NativizationOptions);

public: 
	/** 
	 * A utility for catching errors/warnings that were logged in nested/scoped calls.
	 */
	class FScopedFeedbackContext : public FFeedbackContext
	{
	public:
		FScopedFeedbackContext();
		virtual ~FScopedFeedbackContext();

		/**  */
		bool HasErrors();

		//~ Begin FOutputDevice interface
		virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override;
		virtual void Flush() override;
		virtual void TearDown() override { OldContext->TearDown();  }
		virtual bool CanBeUsedOnAnyThread() const override { return false; }
		//~ End FOutputDevice interface

		//~ Begin FFeedbackContext interface
		virtual bool YesNof(const FText& Question) override { return OldContext->YesNof(Question); }
		virtual bool ReceivedUserCancel() override { return OldContext->ReceivedUserCancel(); }
		virtual FContextSupplier* GetContext() const override { return OldContext->GetContext(); }
		virtual void SetContext(FContextSupplier* InSupplier) override { OldContext->SetContext(InSupplier); }
		virtual TWeakPtr<SBuildProgressWidget> ShowBuildProgressWindow() override { return OldContext->ShowBuildProgressWindow(); }
		virtual void CloseBuildProgressWindow() override { OldContext->CloseBuildProgressWindow(); }

	protected:
		//virtual void StartSlowTask(const FText& Task, bool bShowCancelButton = false) override { OldContext->StartSlowTask(Task, bShowCancelButton); }
		//virtual void FinalizeSlowTask() override { OldContext->FinalizeSlowTask(); }
		//virtual void ProgressReported(const float TotalProgressInterp, FText DisplayMessage) override { OldContext->ProgressReported(TotalProgressInterp, DisplayMessage); }
		//~ End FFeedbackContext interface

	private:
		FFeedbackContext* OldContext;
		int32 ErrorCount;
		int32 WarningCount;
	};
};

