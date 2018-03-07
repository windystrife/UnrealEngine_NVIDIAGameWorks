// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PluginDescriptor.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectDescriptor.h"

#define LOCTEXT_NAMESPACE "PluginDescriptor"

/**
 * Version numbers for plugin descriptors. These version numbers are not generally needed; serialization from JSON attempts to be tolerant of missing/added fields.
 */ 
enum class EPluginDescriptorVersion : uint8
{
	Invalid = 0,
	Initial = 1,
	NameHash = 2,
	ProjectPluginUnification = 3,
	// !!!!!!!!!! IMPORTANT: Remember to also update LatestPluginDescriptorFileVersion in Plugins.cs (and Plugin system documentation) when this changes!!!!!!!!!!!
	// -----<new versions can be added before this line>-------------------------------------------------
	// - this needs to be the last line (see note below)
	LatestPlusOne,
	Latest = LatestPlusOne - 1
};


FPluginDescriptor::FPluginDescriptor()
	: Version(0)
	, EnabledByDefault(EPluginEnabledByDefault::Unspecified)
	, bCanContainContent(false)
	, bIsBetaVersion(false)
	, bInstalled(false)
	, bRequiresBuildPlatform(false)
	, bIsHidden(false)
{ 
}


bool FPluginDescriptor::Load(const FString& FileName, FText& OutFailReason)
{
	// Read the file to a string
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *FileName))
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToLoadDescriptorFile", "Failed to open descriptor file '{0}'"), FText::FromString(FileName));
		return false;
	}
	return Read(FileContents, OutFailReason);
}


bool FPluginDescriptor::Read(const FString& Text, FText& OutFailReason)
{
	// Deserialize a JSON object from the string
	TSharedPtr< FJsonObject > ObjectPtr;
	TSharedRef< TJsonReader<> > Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, ObjectPtr) || !ObjectPtr.IsValid() )
	{
		OutFailReason = FText::Format(LOCTEXT("FailedToReadDescriptorFile", "Failed to read file. {0}"), FText::FromString(Reader->GetErrorMessage()));
		return false;
	}

	// Parse it as a plug-in descriptor
	return Read(*ObjectPtr.Get(), OutFailReason);
}


bool FPluginDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	// Read the file version
	int32 FileVersionInt32;
	if(!Object.TryGetNumberField(TEXT("FileVersion"), FileVersionInt32))
	{
		if(!Object.TryGetNumberField(TEXT("PluginFileVersion"), FileVersionInt32))
		{
			OutFailReason = LOCTEXT("InvalidProjectFileVersion", "File does not have a valid 'FileVersion' number.");
			return false;
		}
	}

	// Check that it's within range
	EPluginDescriptorVersion PluginFileVersion = (EPluginDescriptorVersion)FileVersionInt32;
	if ((PluginFileVersion <= EPluginDescriptorVersion::Invalid) || (PluginFileVersion > EPluginDescriptorVersion::Latest))
	{
		FText ReadVersionText = FText::FromString(FString::Printf(TEXT("%d"), (int32)PluginFileVersion));
		FText LatestVersionText = FText::FromString(FString::Printf(TEXT("%d"), (int32)EPluginDescriptorVersion::Latest));
		OutFailReason = FText::Format( LOCTEXT("ProjectFileVersionTooLarge", "File appears to be in a newer version ({0}) of the file format that we can load (max version: {1})."), ReadVersionText, LatestVersionText);
		return false;
	}

	// Read the other fields
	Object.TryGetNumberField(TEXT("Version"), Version);
	Object.TryGetStringField(TEXT("VersionName"), VersionName);
	Object.TryGetStringField(TEXT("FriendlyName"), FriendlyName);
	Object.TryGetStringField(TEXT("Description"), Description);

	if (!Object.TryGetStringField(TEXT("Category"), Category))
	{
		// Category used to be called CategoryPath in .uplugin files
		Object.TryGetStringField(TEXT("CategoryPath"), Category);
	}
        
	// Due to a difference in command line parsing between Windows and Mac, we shipped a few Mac samples containing
	// a category name with escaped quotes. Remove them here to make sure we can list them in the right category.
	if (Category.Len() >= 2 && Category.StartsWith(TEXT("\"")) && Category.EndsWith(TEXT("\"")))
	{
		Category = Category.Mid(1, Category.Len() - 2);
	}

	Object.TryGetStringField(TEXT("CreatedBy"), CreatedBy);
	Object.TryGetStringField(TEXT("CreatedByURL"), CreatedByURL);
	Object.TryGetStringField(TEXT("DocsURL"), DocsURL);
	Object.TryGetStringField(TEXT("MarketplaceURL"), MarketplaceURL);
	Object.TryGetStringField(TEXT("SupportURL"), SupportURL);
	Object.TryGetStringField(TEXT("EngineVersion"), EngineVersion);
	Object.TryGetStringArrayField(TEXT("SupportedTargetPlatforms"), SupportedTargetPlatforms);

	if (!FModuleDescriptor::ReadArray(Object, TEXT("Modules"), Modules, OutFailReason))
	{
		return false;
	}

	if (!FLocalizationTargetDescriptor::ReadArray(Object, TEXT("LocalizationTargets"), LocalizationTargets, OutFailReason))
	{
		return false;
	}

	bool bEnabledByDefault;
	if(Object.TryGetBoolField(TEXT("EnabledByDefault"), bEnabledByDefault))
	{
		EnabledByDefault = bEnabledByDefault? EPluginEnabledByDefault::Enabled : EPluginEnabledByDefault::Disabled;
	}

	Object.TryGetBoolField(TEXT("CanContainContent"), bCanContainContent);
	Object.TryGetBoolField(TEXT("IsBetaVersion"), bIsBetaVersion);
	Object.TryGetBoolField(TEXT("Installed"), bInstalled);
	Object.TryGetBoolField(TEXT("RequiresBuildPlatform"), bRequiresBuildPlatform);
	Object.TryGetBoolField(TEXT("Hidden"), bIsHidden);

	PreBuildSteps.Read(Object, TEXT("PreBuildSteps"));
	PostBuildSteps.Read(Object, TEXT("PostBuildSteps"));

	if (!FPluginReferenceDescriptor::ReadArray(Object, TEXT("Plugins"), Plugins, OutFailReason))
	{
		return false;
	}

	return true;
}

bool FPluginDescriptor::Save(const FString& FileName, FText& OutFailReason) const
{
	// Write the descriptor to text
	FString Text;
	Write(Text);

	// Save it to a file
	if ( !FFileHelper::SaveStringToFile(Text, *FileName) )
	{
		OutFailReason = FText::Format( LOCTEXT("FailedToWriteOutputFile", "Failed to write output file '{0}'. Perhaps the file is Read-Only?"), FText::FromString(FileName) );
		return false;
	}
	return true;
}

void FPluginDescriptor::Write(FString& Text) const
{
	// Write the contents of the descriptor to a string. Make sure the writer is destroyed so that the contents are flushed to the string.
	TSharedRef< TJsonWriter<> > WriterRef = TJsonWriterFactory<>::Create(&Text);
	TJsonWriter<>& Writer = WriterRef.Get();
	Writer.WriteObjectStart();
	Write(Writer);
	Writer.WriteObjectEnd();
	Writer.Close();
}

void FPluginDescriptor::Write(TJsonWriter<>& Writer) const
{
	Writer.WriteValue(TEXT("FileVersion"), EProjectDescriptorVersion::Latest);
	Writer.WriteValue(TEXT("Version"), Version);
	Writer.WriteValue(TEXT("VersionName"), VersionName);
	Writer.WriteValue(TEXT("FriendlyName"), FriendlyName);
	Writer.WriteValue(TEXT("Description"), Description);
	Writer.WriteValue(TEXT("Category"), Category);
	Writer.WriteValue(TEXT("CreatedBy"), CreatedBy);
	Writer.WriteValue(TEXT("CreatedByURL"), CreatedByURL);
	Writer.WriteValue(TEXT("DocsURL"), DocsURL);
	Writer.WriteValue(TEXT("MarketplaceURL"), MarketplaceURL);
	Writer.WriteValue(TEXT("SupportURL"), SupportURL);
	if (EngineVersion.Len() > 0)
	{
		Writer.WriteValue(TEXT("EngineVersion"), EngineVersion);
	}
	if(EnabledByDefault != EPluginEnabledByDefault::Unspecified)
	{
		Writer.WriteValue(TEXT("EnabledByDefault"), (EnabledByDefault == EPluginEnabledByDefault::Enabled));
	}
	Writer.WriteValue(TEXT("CanContainContent"), bCanContainContent);
	Writer.WriteValue(TEXT("IsBetaVersion"), bIsBetaVersion);
	Writer.WriteValue(TEXT("Installed"), bInstalled);

	if(SupportedTargetPlatforms.Num() > 0)
	{
		Writer.WriteValue(TEXT("SupportedTargetPlatforms"), SupportedTargetPlatforms);
	}

	FModuleDescriptor::WriteArray(Writer, TEXT("Modules"), Modules);

	FLocalizationTargetDescriptor::WriteArray(Writer, TEXT("LocalizationTargets"), LocalizationTargets);

	if(bRequiresBuildPlatform)
	{
		Writer.WriteValue(TEXT("RequiresBuildPlatform"), bRequiresBuildPlatform);
	}

	if (bIsHidden)
	{
		Writer.WriteValue(TEXT("Hidden"), bIsHidden);
	}

	if(!PreBuildSteps.IsEmpty())
	{
		PreBuildSteps.Write(Writer, TEXT("PreBuildSteps"));
	}

	if(!PostBuildSteps.IsEmpty())
	{
		PostBuildSteps.Write(Writer, TEXT("PostBuildSteps"));
	}

	FPluginReferenceDescriptor::WriteArray(Writer, TEXT("Plugins"), Plugins);
}

bool FPluginDescriptor::SupportsTargetPlatform(const FString& Platform) const
{
	return SupportedTargetPlatforms.Num() == 0 || SupportedTargetPlatforms.Contains(Platform);
}

#undef LOCTEXT_NAMESPACE
