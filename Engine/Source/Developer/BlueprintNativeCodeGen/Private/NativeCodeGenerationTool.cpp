// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NativeCodeGenerationTool.h"
#include "Input/Reply.h"
#include "Misc/Paths.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Editor.h"
#include "Widgets/Layout/SBorder.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SErrorText.h"
#include "EditorStyleSet.h"
#include "SourceCodeNavigation.h"
#include "DesktopPlatformModule.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintNativeCodeGenUtils.h"
//#include "Editor/KismetCompiler/Public/BlueprintCompilerCppBackendInterface.h"
#include "BlueprintCompilerCppBackendGatherDependencies.h"
#include "KismetCompilerModule.h"
#include "Widgets/Input/SDirectoryPicker.h"
#include "SourceCodeNavigation.h"

#define LOCTEXT_NAMESPACE "NativeCodeGenerationTool"

//
//	THE CODE SHOULD BE MOVED TO GAMEPROJECTGENERATION
//


struct FGeneratedCodeData
{
	FGeneratedCodeData(UBlueprint& InBlueprint) 
		: Blueprint(&InBlueprint)
	{
		FName GeneratedClassName, SkeletonClassName;
		InBlueprint.GetBlueprintClassNames(GeneratedClassName, SkeletonClassName);
		ClassName = GeneratedClassName.ToString();

		IBlueprintCompilerCppBackendModule& CodeGenBackend = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		BaseFilename = CodeGenBackend.ConstructBaseFilename(&InBlueprint, FCompilerNativizationOptions{});

		GatherUserDefinedDependencies(InBlueprint);
	}

	FString TypeDependencies;
	FString ErrorString;
	FString ClassName;
	FString BaseFilename;
	TWeakObjectPtr<UBlueprint> Blueprint;
	TSet<UField*> DependentObjects;
	TSet<UBlueprintGeneratedClass*> UnconvertedNeededClasses;

	void GatherUserDefinedDependencies(UBlueprint& InBlueprint)
	{
		FCompilerNativizationOptions BlankOptions{};
		FGatherConvertedClassDependencies ClassDependencies(InBlueprint.GeneratedClass, BlankOptions);
		for (auto Iter : ClassDependencies.ConvertedClasses)
		{
			DependentObjects.Add(Iter);
		}
		for (auto Iter : ClassDependencies.ConvertedStructs)
		{
			DependentObjects.Add(Iter);
		}
		for (auto Iter : ClassDependencies.ConvertedEnum)
		{
			DependentObjects.Add(Iter);
		}

		if (DependentObjects.Num())
		{
			TypeDependencies = LOCTEXT("ConvertedDependencies", "Detected Dependencies:\n").ToString();
		}
		else
		{
			TypeDependencies = LOCTEXT("NoConvertedAssets", "No dependencies found.\n").ToString();
		}

		for (auto Obj : DependentObjects)
		{
			TypeDependencies += FString::Printf(TEXT("%s \t%s\n"), *Obj->GetClass()->GetName(), *Obj->GetPathName());
		}
		DependentObjects.Add(InBlueprint.GeneratedClass);

		bool bUnconvertedHeader = false;
		for (auto Asset : ClassDependencies.Assets)
		{
			if (auto BPGC = Cast<UBlueprintGeneratedClass>(Asset))
			{
				UnconvertedNeededClasses.Add(BPGC);
				if (!bUnconvertedHeader)
				{
					bUnconvertedHeader = true;
					TypeDependencies += LOCTEXT("NoConvertedDependencies", "\nUnconverted Dependencies, that require a warpper struct:\n").ToString();
				}
				TypeDependencies += FString::Printf(TEXT("%s \t%s\n"), *BPGC->GetClass()->GetName(), *BPGC->GetPathName());
			}
		}
	}

	static FString DefaultHeaderDir()
	{
		auto DefaultSourceDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
		return FPaths::Combine(*DefaultSourceDir, TEXT("NativizationTest"), TEXT("Public"));
	}

	static FString DefaultSourceDir()
	{
		auto DefaultSourceDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectIntermediateDir());
		return FPaths::Combine(*DefaultSourceDir, TEXT("NativizationTest"), TEXT("Private"));
	}

	FString HeaderFileName() const
	{
		return BaseFilename + TEXT(".h");
	}

	FString SourceFileName() const
	{
		return BaseFilename + TEXT(".cpp");
	}

	bool Save(const FString& HeaderDirPath, const FString& CppDirPath)
	{
		if (!Blueprint.IsValid())
		{
			ErrorString += LOCTEXT("InvalidBlueprint", "Invalid Blueprint\n").ToString();
			return false;
		}

		const int WorkParts = 3 +(4 * DependentObjects.Num());
		FScopedSlowTask SlowTask(WorkParts, LOCTEXT("GeneratingCppFiles", "Generating C++ files.."));
		SlowTask.MakeDialog();

		IBlueprintCompilerCppBackendModule& CodeGenBackend = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();

		TArray<FString> CreatedFiles;
		//for(auto Obj : DependentObjects)
		UObject* Obj = Blueprint->GeneratedClass;
		{
			SlowTask.EnterProgressFrame();

			TSharedPtr<FString> HeaderSource(new FString());
			TSharedPtr<FString> CppSource(new FString());
			TSharedPtr<FNativizationSummary> NativizationSummary(new FNativizationSummary());
			FBlueprintNativeCodeGenUtils::GenerateCppCode(Obj, HeaderSource, CppSource, NativizationSummary, FCompilerNativizationOptions{});
			SlowTask.EnterProgressFrame();

			const FString BackendBaseFilename = CodeGenBackend.ConstructBaseFilename(Obj, FCompilerNativizationOptions{});

			const FString FullHeaderFilename = FPaths::Combine(*HeaderDirPath, *(BackendBaseFilename + TEXT(".h")));
			const bool bHeaderSaved = FFileHelper::SaveStringToFile(*HeaderSource, *FullHeaderFilename);
			if (!bHeaderSaved)
			{
				ErrorString += FString::Printf(*LOCTEXT("HeaderNotSaved", "Header file wasn't saved. Check log for details. %s\n").ToString(), *Obj->GetPathName());
			}
			else
			{
				CreatedFiles.Add(FullHeaderFilename);
			}

			SlowTask.EnterProgressFrame();
			if (!CppSource->IsEmpty())
			{
				const FString NewCppFilename = FPaths::Combine(*CppDirPath, *(BackendBaseFilename + TEXT(".cpp")));
				const bool bCppSaved = FFileHelper::SaveStringToFile(*CppSource, *NewCppFilename);
				if (!bCppSaved)
				{
					ErrorString += FString::Printf(*LOCTEXT("CppNotSaved", "Cpp file wasn't saved. Check log for details. %s\n").ToString(), *Obj->GetPathName());
				}
				else
				{
					CreatedFiles.Add(NewCppFilename);
				}
			}
		}

		SlowTask.EnterProgressFrame();

		bool bSuccess = ErrorString.IsEmpty();
		if (bSuccess && CreatedFiles.Num() > 0)
		{
			// assume the last element is the target cpp file
			FSourceCodeNavigation::OpenSourceFile(CreatedFiles.Last());
		}
		return bSuccess;
	}
};

class SNativeCodeGenerationDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNativeCodeGenerationDialog){}
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ARGUMENT(TSharedPtr<FGeneratedCodeData>, GeneratedCodeData)
	SLATE_END_ARGS()

private:
	
	// Child widgets
	TSharedPtr<SDirectoryPicker> HeaderDirectoryBrowser;
	TSharedPtr<SDirectoryPicker> SourceDirectoryBrowser;
	TSharedPtr<SErrorText> ErrorWidget;
	// 
	TWeakPtr<SWindow> WeakParentWindow;
	TSharedPtr<FGeneratedCodeData> GeneratedCodeData;
	bool bSaved;

	void CloseParentWindow()
	{
		auto ParentWindow = WeakParentWindow.Pin();
		if (ParentWindow.IsValid())
		{
			ParentWindow->RequestDestroyWindow();
		}
	}

	bool IsEditable() const
	{
		return !bSaved && GeneratedCodeData->ErrorString.IsEmpty();
	}

	FReply OnButtonClicked()
	{
		bSaved = GeneratedCodeData->Save(HeaderDirectoryBrowser->GetDirectory(), SourceDirectoryBrowser->GetDirectory());
		ErrorWidget->SetError(GeneratedCodeData->ErrorString);
		
		return FReply::Handled();
	}

	FText ButtonText() const
	{
		return IsEditable() ? LOCTEXT("Generate", "Generate") : LOCTEXT("Regenerate", "Regenerate");
	}

	FText GetClassName() const
	{
		return GeneratedCodeData.IsValid() ? FText::FromString(GeneratedCodeData->ClassName) : FText::GetEmpty();
	}

public:

	void Construct(const FArguments& InArgs)
	{
		GeneratedCodeData = InArgs._GeneratedCodeData;
		bSaved = false;
		WeakParentWindow = InArgs._ParentWindow;

		ChildSlot
		[
			SNew(SBorder)
			.Padding(4.f)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ClassName", "Class Name"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SNativeCodeGenerationDialog::GetClassName)
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HeaderPath", "Header Path"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SAssignNew(HeaderDirectoryBrowser, SDirectoryPicker)
					.Directory(FGeneratedCodeData::DefaultHeaderDir())
					.File(GeneratedCodeData->HeaderFileName())
					.Message(LOCTEXT("HeaderDirectory", "Header Directory"))
					.IsEnabled(this, &SNativeCodeGenerationDialog::IsEditable)
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("SourcePath", "Source Path"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SAssignNew(SourceDirectoryBrowser, SDirectoryPicker)
					.Directory(FGeneratedCodeData::DefaultSourceDir())
					.File(GeneratedCodeData->SourceFileName())
					.Message(LOCTEXT("SourceDirectory", "Source Directory"))
					.IsEnabled(this, &SNativeCodeGenerationDialog::IsEditable)
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Dependencies", "Dependencies"))
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SNew(SBox)
					.WidthOverride(360.0f)
					.HeightOverride(200.0f)
					[
						SNew(SMultiLineEditableTextBox)
						.IsReadOnly(true)
						.Text(FText::FromString(GeneratedCodeData->TypeDependencies))
					]
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				[
					SAssignNew(ErrorWidget, SErrorText)
				]
				+ SVerticalBox::Slot()
				.Padding(4.0f)
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.Text(this, &SNativeCodeGenerationDialog::ButtonText)
					.OnClicked(this, &SNativeCodeGenerationDialog::OnButtonClicked)
				]
			]
		];

		ErrorWidget->SetError(GeneratedCodeData->ErrorString);
	}
};

void FNativeCodeGenerationTool::Open(UBlueprint& Blueprint, TSharedRef< class FBlueprintEditor> Editor)
{
	TSharedRef<FGeneratedCodeData> GeneratedCodeData(new FGeneratedCodeData(Blueprint));

	TSharedRef<SWindow> PickerWindow = SNew(SWindow)
		.Title(LOCTEXT("GenerateNativeCode", "Generate Native Code"))
		.SizingRule(ESizingRule::Autosized)
		.ClientSize(FVector2D(0.f, 300.f))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedRef<SNativeCodeGenerationDialog> CodeGenerationDialog = SNew(SNativeCodeGenerationDialog)
		.ParentWindow(PickerWindow)
		.GeneratedCodeData(GeneratedCodeData);

	PickerWindow->SetContent(CodeGenerationDialog);
	GEditor->EditorAddModalWindow(PickerWindow);
}

bool FNativeCodeGenerationTool::CanGenerate(const UBlueprint& Blueprint)
{
	return (Blueprint.Status == EBlueprintStatus::BS_UpToDate || Blueprint.Status == EBlueprintStatus::BS_UpToDateWithWarnings)
		&& (Blueprint.BlueprintType == EBlueprintType::BPTYPE_Normal || Blueprint.BlueprintType == EBlueprintType::BPTYPE_FunctionLibrary)
		&& Cast<UBlueprintGeneratedClass>(Blueprint.GeneratedClass);
}

#undef LOCTEXT_NAMESPACE
