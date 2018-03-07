// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ProjectDescriptor.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ProjectDescriptor"

FProjectDescriptor::FProjectDescriptor()
{
	FileVersion = EProjectDescriptorVersion::Latest;
	EpicSampleNameHash = 0;
	bIsEnterpriseProject = false;
}

void FProjectDescriptor::Sign(const FString& FilePath)
{
	EpicSampleNameHash = GetTypeHash(FPaths::GetCleanFilename(FilePath));
}

bool FProjectDescriptor::IsSigned(const FString& FilePath) const
{
	return EpicSampleNameHash == GetTypeHash(FPaths::GetCleanFilename(FilePath));
}

int32 FProjectDescriptor::FindPluginReferenceIndex(const FString& PluginName) const
{
	for(int32 Idx = 0; Idx < Plugins.Num(); Idx++)
	{
		if(Plugins[Idx].Name == PluginName)
		{
			return Idx;
		}
	}
	return INDEX_NONE;
}

void FProjectDescriptor::UpdateSupportedTargetPlatforms(const FName& InPlatformName, bool bIsSupported)
{
	if ( bIsSupported )
	{
		TargetPlatforms.AddUnique(InPlatformName);
	}
	else
	{
		TargetPlatforms.Remove(InPlatformName);
	}
}

static bool IsRootedPath(const FString& Path)
{
	return Path[0] == TEXT('\\') || Path[0] == TEXT('/') || Path[1] == TEXT(':');
}

bool FProjectDescriptor::Load(const FString& FileName, FText& OutFailReason)
{
	// Read the file to a string
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FileName))
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToLoadDescriptorFile", "Failed to open descriptor file '{0}'"), FText::FromString(FileName));
		return false;
	}

	// Deserialize a JSON object from the string
	TSharedPtr< FJsonObject > Object;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);
	if ( !FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid() )
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToReadDescriptorFile", "Failed to read file. {0}"), FText::FromString(Reader->GetErrorMessage()));
		return false;
	}

	// Parse it as a project descriptor
	return Read(*Object.Get(), FPaths::GetPath(FileName), OutFailReason);
}

bool FProjectDescriptor::Read(const FJsonObject& Object, const FString& PathToProject, FText& OutFailReason)
{
	// Read the file version
	int32 FileVersionInt32;
	if(!Object.TryGetNumberField(TEXT("FileVersion"), FileVersionInt32))
	{
		if(!Object.TryGetNumberField(TEXT("ProjectFileVersion"), FileVersionInt32))
		{
			OutFailReason = LOCTEXT("InvalidProjectFileVersion", "File does not have a valid 'FileVersion' number.");
			return false;
		}
	}

	// Check that it's within range
	FileVersion = (EProjectDescriptorVersion::Type)FileVersionInt32;
	if ( FileVersion <= EProjectDescriptorVersion::Invalid || FileVersion > EProjectDescriptorVersion::Latest )
	{
		FText ReadVersionText = FText::FromString( FString::Printf( TEXT( "%d" ), (int32)FileVersion ) );
		FText LatestVersionText = FText::FromString( FString::Printf( TEXT( "%d" ), (int32)EProjectDescriptorVersion::Latest ) );
		OutFailReason = FText::Format( LOCTEXT("ProjectFileVersionTooLarge", "File appears to be in a newer version ({0}) of the file format that we can load (max version: {1})."), ReadVersionText, LatestVersionText);
		return false;
	}

	// Read simple fields
	Object.TryGetStringField(TEXT("EngineAssociation"), EngineAssociation);
	Object.TryGetStringField(TEXT("Category"), Category);
	Object.TryGetStringField(TEXT("Description"), Description);
	Object.TryGetBoolField(TEXT("Enterprise"), bIsEnterpriseProject);

	// Read the modules
	if(!FModuleDescriptor::ReadArray(Object, TEXT("Modules"), Modules, OutFailReason))
	{
		return false;
	}

	// Read the plugins
	if(!FPluginReferenceDescriptor::ReadArray(Object, TEXT("Plugins"), Plugins, OutFailReason))
	{
		return false;
	}

	// Read the list of additional plugin directories to scan
	const TArray< TSharedPtr<FJsonValue> >* AdditionalPluginDirectoriesValue;
	if (Object.TryGetArrayField(TEXT("AdditionalPluginDirectories"), AdditionalPluginDirectoriesValue))
	{
#if WITH_EDITOR
		for (int32 Idx = 0; Idx < AdditionalPluginDirectoriesValue->Num(); Idx++)
		{
			FString AdditionalDir;
			if ((*AdditionalPluginDirectoriesValue)[Idx]->TryGetString(AdditionalDir))
			{
				if (IsRootedPath(AdditionalDir))
				{
					AddPluginDirectory(AdditionalDir);
				}
				else
				{
					// This is path relative to the project, so convert to absolute
					AddPluginDirectory(IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*(PathToProject / AdditionalDir)));
				}
			}
		}
#endif
		// If this is a packaged build and there are additional directories, they need to be remapped to the packaged location
		if (FPlatformProperties::RequiresCookedData() && AdditionalPluginDirectoriesValue->Num() > 0)
		{
			AdditionalPluginDirectories.Empty();
			FString RemappedDir = FPaths::ProjectDir() + TEXT("../RemappedPlugins/");
			AddPluginDirectory(RemappedDir);
		}
	}

	// Read the target platforms
	const TArray< TSharedPtr<FJsonValue> > *TargetPlatformsValue;
	if(Object.TryGetArrayField(TEXT("TargetPlatforms"), TargetPlatformsValue))
	{
		for(int32 Idx = 0; Idx < TargetPlatformsValue->Num(); Idx++)
		{
			FString TargetPlatform;
			if((*TargetPlatformsValue)[Idx]->TryGetString(TargetPlatform))
			{
				TargetPlatforms.Add(*TargetPlatform);
			}
		}
	}

	// Get the sample name hash
	Object.TryGetNumberField(TEXT("EpicSampleNameHash"), EpicSampleNameHash);

	// Read the custom build steps
	PreBuildSteps.Read(Object, TEXT("PreBuildSteps"));
	PostBuildSteps.Read(Object, TEXT("PostBuildSteps"));

	return true;
}

bool FProjectDescriptor::Save(const FString& FileName, FText& OutFailReason)
{
	// Write the contents of the descriptor to a string. Make sure the writer is destroyed so that the contents are flushed to the string.
	FString Text;
	TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&Text);
	Write(Writer.Get(), FPaths::GetPath(FileName));
	Writer->Close();

	// Save it to a file
	if ( FFileHelper::SaveStringToFile(Text, *FileName) )
	{
		return true;
	}
	else
	{
		OutFailReason = FText::Format( LOCTEXT("FailedToWriteOutputFile", "Failed to write output file '{0}'. Perhaps the file is Read-Only?"), FText::FromString(FileName) );
		return false;
	}
}

void FProjectDescriptor::Write(TJsonWriter<>& Writer, const FString& PathToProject) const
{
	Writer.WriteObjectStart();

	// Write all the simple fields
	Writer.WriteValue(TEXT("FileVersion"), EProjectDescriptorVersion::Latest);
	Writer.WriteValue(TEXT("EngineAssociation"), EngineAssociation);
	Writer.WriteValue(TEXT("Category"), Category);
	Writer.WriteValue(TEXT("Description"), Description);

	// Write the enterprise flag
	if (bIsEnterpriseProject)
	{
		Writer.WriteValue(TEXT("Enterprise"), bIsEnterpriseProject);
	}

	// Write the module list
	FModuleDescriptor::WriteArray(Writer, TEXT("Modules"), Modules);

	// Write the plugin list
	FPluginReferenceDescriptor::WriteArray(Writer, TEXT("Plugins"), Plugins);

	// Write out the additional plugin directories to scan
	if (AdditionalPluginDirectories.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("AdditionalPluginDirectories"));
		for (const FString& Dir : AdditionalPluginDirectories)
		{
			// Convert to relative path if possible before writing it out
			Writer.WriteValue(MakePathRelativeToProject(Dir, PathToProject));
		}
		Writer.WriteArrayEnd();
	}

	// Write the target platforms
	if(TargetPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("TargetPlatforms"));
		for(int Idx = 0; Idx < TargetPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(TargetPlatforms[Idx].ToString());
		}
		Writer.WriteArrayEnd();
	}

	// If it's a signed sample, write the name hash
	if(EpicSampleNameHash != 0)
	{
		Writer.WriteValue(TEXT("EpicSampleNameHash"), FString::Printf(TEXT("%u"), EpicSampleNameHash));
	}

	// Write the custom build steps
	if(!PreBuildSteps.IsEmpty())
	{
		PreBuildSteps.Write(Writer, TEXT("PreBuildSteps"));
	}
	if(!PostBuildSteps.IsEmpty())
	{
		PostBuildSteps.Write(Writer, TEXT("PostBuildSteps"));
	}

	Writer.WriteObjectEnd();
}

FString FProjectDescriptor::GetExtension()
{
	static const FString ProjectExtension(TEXT("uproject"));
	return ProjectExtension;
}

/** @return the path relative to this project if possible */
const FString FProjectDescriptor::MakePathRelativeToProject(const FString& Dir, const FString& PathToProject) const
{
	FString ProjectDir = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*(PathToProject / TEXT("")));
	FPaths::MakePlatformFilename(ProjectDir);
	FString ModifiedDir(Dir);
	FPaths::MakePathRelativeTo(ModifiedDir, *ProjectDir);
	return ModifiedDir;
}

void FProjectDescriptor::AddPluginDirectory(const FString& AdditionalDir)
{
	check(!AdditionalDir.StartsWith(IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::ProjectPluginsDir())));
	check(!AdditionalDir.StartsWith(IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*FPaths::EnginePluginsDir())));

	// Detect calls where the path is not absolute
#if WITH_EDITOR
	checkf(IsRootedPath(AdditionalDir), TEXT("%s is not rooted"), *AdditionalDir);
#endif
	
	AdditionalPluginDirectories.AddUnique(AdditionalDir);
}

void FProjectDescriptor::RemovePluginDirectory(const FString& Dir)
{
	// Detect calls where the path is not absolute
	checkf(IsRootedPath(Dir), TEXT("%s is not rooted"), *Dir);
	AdditionalPluginDirectories.RemoveSingle(Dir);
}

#undef LOCTEXT_NAMESPACE
