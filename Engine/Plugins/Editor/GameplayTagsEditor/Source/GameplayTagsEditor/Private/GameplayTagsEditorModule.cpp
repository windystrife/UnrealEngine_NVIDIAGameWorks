// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagsEditorModule.h"
#include "Misc/Paths.h"
#include "PropertyEditorModule.h"
#include "Factories/Factory.h"
#include "EdGraphUtilities.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "GameplayTagsManager.h"
#include "AssetData.h"
#include "Misc/ConfigCacheIni.h"
#include "GameplayTagsGraphPanelPinFactory.h"
#include "GameplayTagsGraphPanelNodeFactory.h"
#include "GameplayTagContainerCustomization.h"
#include "GameplayTagQueryCustomization.h"
#include "GameplayTagCustomization.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsSettingsCustomization.h"
#include "GameplayTagsModule.h"
#include "ISettingsModule.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "AssetRegistryModule.h"
#include "Editor.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "HAL/PlatformFile.h"
#include "HAL/PlatformFilemanager.h"
#include "Stats/StatsMisc.h"
#include "GameplayTagReferenceHelperDetails.h"

#define LOCTEXT_NAMESPACE "GameplayTagEditor"


class FGameplayTagsEditorModule
	: public IGameplayTagsEditorModule
{
public:

	// IModuleInterface

	virtual void StartupModule() override
	{
		// Register the details customizer
		{
			FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagContainer", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagContainerCustomization::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTag", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagCustomizationPublic::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagQuery", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagQueryCustomization::MakeInstance));

			PropertyModule.RegisterCustomClassLayout(UGameplayTagsList::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FGameplayTagsSettingsCustomization::MakeInstance));

			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagReferenceHelper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagReferenceHelperDetails::MakeInstance));
			PropertyModule.RegisterCustomPropertyTypeLayout("GameplayTagCreationWidgetHelper", FOnGetPropertyTypeCustomizationInstance::CreateStatic(&FGameplayTagCreationWidgetHelperDetails::MakeInstance));

			PropertyModule.NotifyCustomizationModuleChanged();
		}

		TSharedPtr<FGameplayTagsGraphPanelPinFactory> GameplayTagsGraphPanelPinFactory = MakeShareable( new FGameplayTagsGraphPanelPinFactory() );
		FEdGraphUtilities::RegisterVisualPinFactory(GameplayTagsGraphPanelPinFactory);

		TSharedPtr<FGameplayTagsGraphPanelNodeFactory> GameplayTagsGraphPanelNodeFactory = MakeShareable(new FGameplayTagsGraphPanelNodeFactory());
		FEdGraphUtilities::RegisterVisualNodeFactory(GameplayTagsGraphPanelNodeFactory);

		// These objects are not UDeveloperSettings because we only want them to register if the editor plugin is enabled

		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->RegisterSettings("Project", "Project", "GameplayTags",
				LOCTEXT("GameplayTagSettingsName", "GameplayTags"),
				LOCTEXT("GameplayTagSettingsNameDesc", "GameplayTag Settings"),
				GetMutableDefault<UGameplayTagsSettings>()
				);

			SettingsModule->RegisterSettings("Project", "Project", "GameplayTags Developer",
				LOCTEXT("GameplayTagDeveloperSettingsName", "GameplayTags Developer"),
				LOCTEXT("GameplayTagDeveloperSettingsNameDesc", "GameplayTag Developer Settings"),
				GetMutableDefault<UGameplayTagsDeveloperSettings>()
				);
		}

		GameplayTagPackageName = FGameplayTag::StaticStruct()->GetOutermost()->GetFName();
		GameplayTagStructName = FGameplayTag::StaticStruct()->GetFName();

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().OnEditSearchableName(GameplayTagPackageName, GameplayTagStructName).BindRaw(this, &FGameplayTagsEditorModule::OnEditGameplayTag);
		
		// Hook into notifications for object re-imports so that the gameplay tag tree can be reconstructed if the table changes
		if (GIsEditor)
		{
			AssetImportHandle = FEditorDelegates::OnAssetPostImport.AddRaw(this, &FGameplayTagsEditorModule::OnObjectReimported);
			SettingsChangedHandle = IGameplayTagsModule::OnTagSettingsChanged.AddRaw(this, &FGameplayTagsEditorModule::OnEditorSettingsChanged);
		}
	}

	void OnObjectReimported(UFactory* ImportFactory, UObject* InObject)
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		// Re-construct the gameplay tag tree if the base table is re-imported
		if (GIsEditor && !IsRunningCommandlet() && InObject && Manager.GameplayTagTables.Contains(Cast<UDataTable>(InObject)))
		{
			Manager.EditorRefreshGameplayTagTree();
		}
	}

	virtual void ShutdownModule() override
	{
		// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
		// we call this function before unloading the module.
	
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			SettingsModule->UnregisterSettings("Project", "Project", "GameplayTags");
			SettingsModule->UnregisterSettings("Project", "Project", "GameplayTags Developer");
		}

		if (AssetImportHandle.IsValid())
		{
			FEditorDelegates::OnAssetPostImport.Remove(AssetImportHandle);
		}

		if (SettingsChangedHandle.IsValid())
		{
			IGameplayTagsModule::OnTagSettingsChanged.Remove(SettingsChangedHandle);
		}

		FAssetRegistryModule* AssetRegistryModule = FModuleManager::FModuleManager::GetModulePtr<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule)
		{
			AssetRegistryModule->Get().OnEditSearchableName(GameplayTagPackageName, GameplayTagStructName).Unbind();
		}

	}

	void OnEditorSettingsChanged()
	{
		// This is needed to make networking changes as well, so let's always refresh
		UGameplayTagsManager::Get().EditorRefreshGameplayTagTree();

		// Attempt to migrate the settings if needed
		MigrateSettings();
	}

	bool OnEditGameplayTag(const FAssetIdentifier& AssetId)
	{
		if (ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings"))
		{
			// TODO: Select tag maybe?
			SettingsModule->ShowViewer("Project", "Project", "GameplayTags");
		}

		return true;
	}

	void ShowNotification(const FText& TextToDisplay, float TimeToDisplay)
	{
		FNotificationInfo Info(TextToDisplay);
		Info.ExpireDuration = TimeToDisplay;

		FSlateNotificationManager::Get().AddNotification(Info);
	}

	void MigrateSettings()
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		FString DefaultEnginePath = FString::Printf(TEXT("%sDefaultEngine.ini"), *FPaths::SourceConfigDir());

		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
		
		// The refresh has already set the in memory version of this to be correct, just need to save it out now
		if (!GConfig->GetSectionPrivate(TEXT("GameplayTags"), false, true, DefaultEnginePath))
		{
			// Already migrated or no data
			return;
		}

		// Check out defaultengine
		GameplayTagsUpdateSourceControl(DefaultEnginePath);

		// Delete gameplay tags section entirely. This modifies the disk version
		GConfig->EmptySection(TEXT("GameplayTags"), DefaultEnginePath);

		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate(TEXT("/Script/Engine.Engine"), false, false, DefaultEnginePath);

		if (PackageRedirects)
		{
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("+GameplayTagRedirects"))
				{
					It.RemoveCurrent();
				}
			}
		}

		// This will remove comments, etc. It is expected for someone to diff this before checking in to manually fix it
		GConfig->Flush(false, DefaultEnginePath);

		// Write out gameplaytags.ini
		GameplayTagsUpdateSourceControl(Settings->GetDefaultConfigFilename());
		Settings->UpdateDefaultConfigFile();

		GConfig->LoadFile(Settings->GetDefaultConfigFilename());
		
		// Write out all other tag lists
		TArray<const FGameplayTagSource*> Sources;

		Manager.FindTagSourcesWithType(EGameplayTagSourceType::TagList, Sources);

		for (const FGameplayTagSource* Source : Sources)
		{
			UGameplayTagsList* TagList = Source->SourceTagList;
			if (TagList)
			{
				GameplayTagsUpdateSourceControl(TagList->ConfigFileName);
				TagList->UpdateDefaultConfigFile(TagList->ConfigFileName);

				// Reload off disk
				GConfig->LoadFile(TagList->ConfigFileName);
				//FString DestFileName;
				//FConfigCacheIni::LoadGlobalIniFile(DestFileName, *FString::Printf(TEXT("Tags/%s"), *Source->SourceName.ToString()), nullptr, true);

				// Explicitly remove user tags section
				GConfig->EmptySection(TEXT("UserTags"), TagList->ConfigFileName);
			}
		}

		ShowNotification(LOCTEXT("MigrationText", "Migrated Tag Settings, check DefaultEngine.ini before checking in!"), 10.0f);
	}

	void GameplayTagsUpdateSourceControl(const FString& RelativeConfigFilePath)
	{
		FString ConfigPath = FPaths::ConvertRelativePathToFull(RelativeConfigFilePath);

		if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath))
		{
			return;
		}

		if (ISourceControlModule::Get().IsEnabled())
		{
			FText ErrorMessage;

			if (!SourceControlHelpers::CheckoutOrMarkForAdd(ConfigPath, FText::FromString(ConfigPath), NULL, ErrorMessage))
			{
				ShowNotification(ErrorMessage, 3.0f);
			}
		}
		else
		{
			if (!FPlatformFileManager::Get().GetPlatformFile().SetReadOnly(*ConfigPath, false))
			{
				ShowNotification(FText::Format(LOCTEXT("FailedToMakeWritable", "Could not make {0} writable."), FText::FromString(ConfigPath)), 3.0f);
			}
		}
	}

	bool DeleteTagRedirector(const FString& TagToDelete)
	{
		FName TagName = FName(*TagToDelete);

		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		for (int32 i = 0; i < Settings->GameplayTagRedirects.Num(); i++)
		{
			if (Settings->GameplayTagRedirects[i].OldTagName == TagName)
			{
				Settings->GameplayTagRedirects.RemoveAt(i);

				GameplayTagsUpdateSourceControl(Settings->GetDefaultConfigFilename());
				Settings->UpdateDefaultConfigFile();
				GConfig->LoadFile(Settings->GetDefaultConfigFilename());

				Manager.EditorRefreshGameplayTagTree();

				ShowNotification(FText::Format(LOCTEXT("RemoveTagRedirect", "Deleted tag redirect {0}"), FText::FromString(TagToDelete)), 5.0f);

				return true;
			}
		}

		return false;
	}

	virtual bool AddNewGameplayTagToINI(const FString& NewTag, const FString& Comment, FName TagSourceName) override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		if (NewTag.IsEmpty())
		{
			return false;
		}

		if (Manager.ShouldImportTagsFromINI() == false)
		{
			return false;
		}

		UGameplayTagsSettings*			Settings = GetMutableDefault<UGameplayTagsSettings>();
		UGameplayTagsDeveloperSettings* DevSettings = GetMutableDefault<UGameplayTagsDeveloperSettings>();

		// Delete existing redirector
		DeleteTagRedirector(NewTag);

		// Already in the list as an explicit tag, ignore. Note we want to add if it is in implicit tag. (E.g, someone added A.B.C then someone tries to add A.B)
		if (Manager.IsDictionaryTag(FName(*NewTag)))
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure", "Failed to add gameplay tag {0}, already exists!"), FText::FromString(NewTag)), 10.0f);

			return false;
		}

		if ((TagSourceName == NAME_None || TagSourceName == FGameplayTagSource::GetDefaultName()) && DevSettings && !DevSettings->DeveloperConfigName.IsEmpty())
		{
			// Try to use developer config file
			TagSourceName = FName(*FString::Printf(TEXT("%s.ini"), *DevSettings->DeveloperConfigName));
		}

		if (TagSourceName == NAME_None)
		{
			// If not set yet, set to default
			TagSourceName = FGameplayTagSource::GetDefaultName();
		}

		const FGameplayTagSource* TagSource = Manager.FindTagSource(TagSourceName);

		if (!TagSource)
		{
			// Create a new one
			TagSource = Manager.FindOrAddTagSource(TagSourceName, EGameplayTagSourceType::TagList);
		}

		if (TagSource && TagSource->SourceTagList)
		{
			UGameplayTagsList* TagList = TagSource->SourceTagList;

			TagList->GameplayTagList.AddUnique(FGameplayTagTableRow(FName(*NewTag), Comment));

			TagList->SortTags();

			GameplayTagsUpdateSourceControl(TagList->ConfigFileName);

			// Check source control before and after writing, to make sure it gets created or checked out

			TagList->UpdateDefaultConfigFile(TagList->ConfigFileName);
			GameplayTagsUpdateSourceControl(TagList->ConfigFileName);
			GConfig->LoadFile(TagList->ConfigFileName);
		}
		else
		{
			ShowNotification(FText::Format(LOCTEXT("AddTagFailure", "Failed to add gameplay tag {0} to dictionary {1}!"), FText::FromString(NewTag), FText::FromName(TagSourceName)), 10.0f);

			return false;
		}

		{
			FString PerfMessage = FString::Printf(TEXT("ConstructGameplayTagTree GameplayTag tables after adding new tag"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)

			Manager.EditorRefreshGameplayTagTree();
		}

		return true;
	}

	virtual bool DeleteTagFromINI(const FString& TagToDelete) override
	{
		FName TagName = FName(*TagToDelete);

		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();

		FString Comment;
		FName TagSourceName;

		if (DeleteTagRedirector(TagToDelete))
		{
			return true;
		}

		if (!Manager.GetTagEditorData(TagName, Comment, TagSourceName))
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoTag", "Cannot delete tag {0}, does not exist!"), FText::FromString(TagToDelete)), 10.0f);

			return false;
		}

		const FGameplayTagSource* TagSource = Manager.FindTagSource(TagSourceName);

		// Verify tag source
		if (!TagSource)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoSource", "Cannot delete tag {0} as it is implicit, remove children manually"), FText::FromString(TagToDelete)), 10.0f);

			return false;
		}
		else if (!TagSource->SourceTagList)
		{
			ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource", "Cannot delete tag {0} from source {1}, remove manually"), FText::FromString(TagToDelete), FText::FromName(TagSourceName)), 10.0f);

			return false;
		}

		FGameplayTag ActualTag = Manager.RequestGameplayTag(TagName);
		FGameplayTagContainer ChildTags = Manager.RequestGameplayTagChildrenInDictionary(ActualTag);

		TArray<FName> TagsThatWillBeDeleted;

		TagsThatWillBeDeleted.Add(TagName);

		FGameplayTag ParentTag = ActualTag.RequestDirectParent();
		while (ParentTag.IsValid())
		{
			// See if there are more children than the one we are about to delete
			FGameplayTagContainer ParentChildTags = Manager.RequestGameplayTagChildrenInDictionary(ParentTag);

			ensure(ParentChildTags.HasTagExact(ActualTag));
			if (ParentChildTags.Num() == 1)
			{
				// This is the only tag, add to deleted list
				TagsThatWillBeDeleted.Add(ParentTag.GetTagName());
				ParentTag = ParentTag.RequestDirectParent();
			}
			else
			{
				break;
			}
		}

		for (FName TagNameToDelete : TagsThatWillBeDeleted)
		{
			// Verify references
			FAssetIdentifier TagId = FAssetIdentifier(FGameplayTag::StaticStruct(), TagName);
			TArray<FAssetIdentifier> Referencers;

			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			AssetRegistryModule.Get().GetReferencers(TagId, Referencers, EAssetRegistryDependencyType::SearchableName);

			if (Referencers.Num() > 0)
			{
				ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureBadSource", "Cannot delete tag {0}, still referenced by {1} and possibly others"), FText::FromName(TagNameToDelete), FText::FromString(Referencers[0].ToString())), 10.0f);

				return false;
			}
		}

		// Passed, delete and save
		UGameplayTagsList* TagList = TagSource->SourceTagList;

		for (int32 i = 0; i < TagList->GameplayTagList.Num(); i++)
		{
			if (TagList->GameplayTagList[i].Tag == TagName)
			{
				TagList->GameplayTagList.RemoveAt(i);

				TagList->UpdateDefaultConfigFile(TagList->ConfigFileName);
				GameplayTagsUpdateSourceControl(TagList->ConfigFileName);
				GConfig->LoadFile(TagList->ConfigFileName);

				// See if we still live due to child tags

				if (ChildTags.Num() > 0)
				{
					ShowNotification(FText::Format(LOCTEXT("RemoveTagChildrenExist", "Deleted explicit tag {0}, still exists implicitly due to children"), FText::FromString(TagToDelete)), 5.0f);
				}
				else
				{
					ShowNotification(FText::Format(LOCTEXT("RemoveTag", "Deleted tag {0}"), FText::FromString(TagToDelete)), 5.0f);
				}

				// This invalidates all local variables, need to return right away
				Manager.EditorRefreshGameplayTagTree();

				return true;
			}
		}

		ShowNotification(FText::Format(LOCTEXT("RemoveTagFailureNoTag", "Cannot delete tag {0}, does not exist!"), FText::FromString(TagToDelete)), 10.0f);
		
		return false;
	}

	virtual bool RenameTagInINI(const FString& TagToRename, const FString& TagToRenameTo) override
	{
		FName OldTagName = FName(*TagToRename);
		FName NewTagName = FName(*TagToRenameTo);

		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		UGameplayTagsSettings* Settings = GetMutableDefault<UGameplayTagsSettings>();

		FString OldComment, NewComment;
		FName OldTagSourceName, NewTagSourceName;

		// Delete existing redirector
		DeleteTagRedirector(TagToRenameTo);
		DeleteTagRedirector(TagToRename);

		if (Manager.GetTagEditorData(OldTagName, OldComment, OldTagSourceName))
		{
			// Add new tag if needed
			if (!Manager.GetTagEditorData(NewTagName, NewComment, NewTagSourceName))
			{
				if (!AddNewGameplayTagToINI(TagToRenameTo, OldComment, OldTagSourceName))
				{
					// Failed to add new tag, so fail
					return false;
				}
			}

			// Delete old tag if possible, still make redirector if this fails
			const FGameplayTagSource* OldTagSource = Manager.FindTagSource(OldTagSourceName);

			if (OldTagSource && OldTagSource->SourceTagList)
			{
				UGameplayTagsList* TagList = OldTagSource->SourceTagList;

				for (int32 i = 0; i < TagList->GameplayTagList.Num(); i++)
				{
					if (TagList->GameplayTagList[i].Tag == OldTagName)
					{
						TagList->GameplayTagList.RemoveAt(i);

						TagList->UpdateDefaultConfigFile(TagList->ConfigFileName);
						GameplayTagsUpdateSourceControl(TagList->ConfigFileName);
						GConfig->LoadFile(TagList->ConfigFileName);

						break;
					}
				}
			}
			else
			{
				ShowNotification(FText::Format(LOCTEXT("RenameFailure", "Tag {0} redirector was created but original tag was not destroyed as it has children"), FText::FromString(TagToRename)), 10.0f);
			}
		}

		// Add redirector no matter what
		FGameplayTagRedirect Redirect;
		Redirect.OldTagName = OldTagName;
		Redirect.NewTagName = NewTagName;

		Settings->GameplayTagRedirects.AddUnique(Redirect);

		GameplayTagsUpdateSourceControl(Settings->GetDefaultConfigFilename());
		Settings->UpdateDefaultConfigFile();
		GConfig->LoadFile(Settings->GetDefaultConfigFilename());

		ShowNotification(FText::Format(LOCTEXT("AddTagRedirect", "Renamed tag {0} to {1}"), FText::FromString(TagToRename), FText::FromString(TagToRenameTo)), 3.0f);

		Manager.EditorRefreshGameplayTagTree();

		return true;
	}

	virtual bool AddTransientEditorGameplayTag(const FString& NewTransientTag) override
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

		if (NewTransientTag.IsEmpty())
		{
			return false;
		}

		Manager.TransientEditorTags.Add(*NewTransientTag);

		{
			FString PerfMessage = FString::Printf(TEXT("ConstructGameplayTagTree GameplayTag tables after adding new transient tag"));
			SCOPE_LOG_TIME_IN_SECONDS(*PerfMessage, nullptr)

			Manager.EditorRefreshGameplayTagTree();
		}

		return true;
	}

	FDelegateHandle AssetImportHandle;
	FDelegateHandle SettingsChangedHandle;

	FName GameplayTagPackageName;
	FName GameplayTagStructName;
};


IMPLEMENT_MODULE(FGameplayTagsEditorModule, GameplayTagsEditor)

#undef LOCTEXT_NAMESPACE