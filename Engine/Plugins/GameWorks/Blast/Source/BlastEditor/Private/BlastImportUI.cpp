#include "BlastImportUI.h"

#include "IMainFrameModule.h"
#include "ModuleManager.h"
#include "JsonObjectConverter.h"

#include "Public/FbxImporter.h"
#include "Factories/FbxSkeletalMeshImportData.h"

#include "BlastImportOptionWindow.h"

#define LOCTEXT_NAMESPACE "Blast"

UBlastImportUI::UBlastImportUI()
{
	FBXImportUI = CreateDefaultSubobject<UFbxImportUI>(GET_MEMBER_NAME_CHECKED(UBlastImportUI, FBXImportUI));
	FBXImportUI->bImportAsSkeletal = true;
	FBXImportUI->bImportMesh = true;
	FBXImportUI->bIsObjImport = false;
	FBXImportUI->bImportAnimations = false;
	FBXImportUI->SetMeshTypeToImport();

	//Set some hardcoded options for skeletal mesh
	FBXImportUI->SkeletalMeshImportData->bBakePivotInVertex = false;
	FBXImportUI->SkeletalMeshImportData->bTransformVertexToAbsolute = true;
}

bool UBlastImportUI::GetBlastImportOptions(const FString& FullPath)
{
	TSharedPtr<SWindow> ParentWindow;

	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(LOCTEXT("BlastImportOpionsTitle", "Blast Import Options"))
		.SizingRule(ESizingRule::UserSized)
		.ClientSize(FVector2D(400, 700))
		.SupportsMaximize(false)
		.SupportsMinimize(false);

	TSharedPtr<SBlastImportOptionWindow> BlastOptionWindow;
	Window->SetContent
	(
		SAssignNew(BlastOptionWindow, SBlastImportOptionWindow)
		.ImportUI(this)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(FullPath))
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (BlastOptionWindow->ShouldImport())
	{
		SaveConfig();
		FBXImportUI->SaveConfig();
		FBXImportUI->SkeletalMeshImportData->SaveConfig();
		return true;
	}

	return false;
}

void UBlastImportUI::ParseFromJson(TSharedRef<class FJsonObject> ImportSettingsJson)
{
	// Skip instanced object references. 
	int64 SkipFlags = CPF_InstancedReference;
	FJsonObjectConverter::JsonObjectToUStruct(ImportSettingsJson, GetClass(), this, 0, SkipFlags);

	const TSharedPtr<FJsonObject>* FBXImportUIImportJson = nullptr;
	ImportSettingsJson->TryGetObjectField(TEXT("FBXImportUI"), FBXImportUIImportJson);
	if (FBXImportUIImportJson)
	{
		FBXImportUI->ParseFromJson(FBXImportUIImportJson->ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE

