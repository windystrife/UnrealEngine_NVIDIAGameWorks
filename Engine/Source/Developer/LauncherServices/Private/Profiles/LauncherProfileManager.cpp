// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Profiles/LauncherProfileManager.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Launcher/LauncherProjectPath.h"
#include "Profiles/LauncherDeviceGroup.h"
#include "Profiles/LauncherProfile.h"


/* ILauncherProfileManager structors
 *****************************************************************************/

FLauncherProfileManager::FLauncherProfileManager()
{
}

/* ILauncherProfileManager interface
 *****************************************************************************/

void FLauncherProfileManager::Load()
{
	LoadDeviceGroups();
	LoadProfiles();
}

void FLauncherProfileManager::AddDeviceGroup( const ILauncherDeviceGroupRef& DeviceGroup )
{
	if (!DeviceGroups.Contains(DeviceGroup))
	{
		// replace the existing device group
		ILauncherDeviceGroupPtr ExistingGroup = GetDeviceGroup(DeviceGroup->GetId());

		if (ExistingGroup.IsValid())
		{
			RemoveDeviceGroup(ExistingGroup.ToSharedRef());
		}

		// add the new device group
		DeviceGroups.Add(DeviceGroup);
		SaveDeviceGroups();

		DeviceGroupAddedDelegate.Broadcast(DeviceGroup);
	}
}


ILauncherDeviceGroupRef FLauncherProfileManager::AddNewDeviceGroup( )
{
	ILauncherDeviceGroupRef  NewGroup = MakeShareable(new FLauncherDeviceGroup(FGuid::NewGuid(), FString::Printf(TEXT("New Group %d"), DeviceGroups.Num())));

	AddDeviceGroup(NewGroup);

	return NewGroup;
}


ILauncherDeviceGroupRef FLauncherProfileManager::CreateUnmanagedDeviceGroup()
{
	ILauncherDeviceGroupRef  NewGroup = MakeShareable(new FLauncherDeviceGroup(FGuid::NewGuid(), TEXT("Simple Group")));
	return NewGroup;
}


ILauncherSimpleProfilePtr FLauncherProfileManager::FindOrAddSimpleProfile(const FString& DeviceName)
{
	// replace the existing profile
	ILauncherSimpleProfilePtr SimpleProfile = FindSimpleProfile(DeviceName);
	if (!SimpleProfile.IsValid())
	{
		SimpleProfile = MakeShareable(new FLauncherSimpleProfile(DeviceName));
		SimpleProfiles.Add(SimpleProfile);
	}
	
	return SimpleProfile;
}


ILauncherSimpleProfilePtr FLauncherProfileManager::FindSimpleProfile(const FString& DeviceName)
{
	for (int32 ProfileIndex = 0; ProfileIndex < SimpleProfiles.Num(); ++ProfileIndex)
	{
		ILauncherSimpleProfilePtr SimpleProfile = SimpleProfiles[ProfileIndex];

		if (SimpleProfile->GetDeviceName() == DeviceName)
		{
			return SimpleProfile;
		}
	}

	return nullptr;
}

ILauncherProfileRef FLauncherProfileManager::AddNewProfile()
{
	// find a unique name for the profile.
	int32 ProfileIndex = SavedProfiles.Num();
	FString ProfileName = FString::Printf(TEXT("New Profile %d"), ProfileIndex);

	for (int32 Index = 0; Index < SavedProfiles.Num(); ++Index)
	{
		if (SavedProfiles[Index]->GetName() == ProfileName)
		{
			ProfileName = FString::Printf(TEXT("New Profile %d"), ++ProfileIndex);
			Index = -1;

			continue;
		}
	}

	// create and add the profile
	ILauncherProfileRef NewProfile = MakeShareable(new FLauncherProfile(AsShared(), FGuid::NewGuid(), ProfileName));

	AddProfile(NewProfile);

	SaveJSONProfile(NewProfile);

	return NewProfile;
}

ILauncherProfileRef FLauncherProfileManager::CreateUnsavedProfile(FString ProfileName)
{
	// create and return the profile
	ILauncherProfileRef NewProfile = MakeShareable(new FLauncherProfile(AsShared(), FGuid(), ProfileName));
	
	AllProfiles.Add(NewProfile);
	
	return NewProfile;
}


void FLauncherProfileManager::AddProfile( const ILauncherProfileRef& Profile )
{
	if (!SavedProfiles.Contains(Profile))
	{
		// replace the existing profile
		ILauncherProfilePtr ExistingProfile = GetProfile(Profile->GetId());

		if (ExistingProfile.IsValid())
		{
			RemoveProfile(ExistingProfile.ToSharedRef());
		}

		if (!Profile->GetDeployedDeviceGroup().IsValid())
		{
			Profile->SetDeployedDeviceGroup(AddNewDeviceGroup());
		}

		// add the new profile
		SavedProfiles.Add(Profile);
		AllProfiles.Add(Profile);

		ProfileAddedDelegate.Broadcast(Profile);
	}
}


ILauncherProfilePtr FLauncherProfileManager::FindProfile( const FString& ProfileName )
{
	for (int32 ProfileIndex = 0; ProfileIndex < SavedProfiles.Num(); ++ProfileIndex)
	{
		ILauncherProfilePtr Profile = SavedProfiles[ProfileIndex];

		if (Profile->GetName() == ProfileName)
		{
			return Profile;
		}
	}

	return nullptr;
}


const TArray<ILauncherDeviceGroupPtr>& FLauncherProfileManager::GetAllDeviceGroups( ) const
{
	return DeviceGroups;
}


const TArray<ILauncherProfilePtr>& FLauncherProfileManager::GetAllProfiles( ) const
{
	return SavedProfiles;
}


ILauncherDeviceGroupPtr FLauncherProfileManager::GetDeviceGroup( const FGuid& GroupId ) const
{
	for (int32 GroupIndex = 0; GroupIndex < DeviceGroups.Num(); ++GroupIndex)
	{
		const ILauncherDeviceGroupPtr& Group = DeviceGroups[GroupIndex];

		if (Group->GetId() == GroupId)
		{
			return Group;
		}
	}

	return nullptr;
}


ILauncherProfilePtr FLauncherProfileManager::GetProfile( const FGuid& ProfileId ) const
{
	for (int32 ProfileIndex = 0; ProfileIndex < SavedProfiles.Num(); ++ProfileIndex)
	{
		ILauncherProfilePtr Profile = SavedProfiles[ProfileIndex];

		if (Profile->GetId() == ProfileId)
		{
			return Profile;
		}
	}

	return nullptr;
}


ILauncherProfilePtr FLauncherProfileManager::LoadProfile( FArchive& Archive )
{
	FLauncherProfile* Profile = new FLauncherProfile(AsShared());

	if (Profile->Serialize(Archive))
	{
		ILauncherDeviceGroupPtr DeviceGroup = GetDeviceGroup(Profile->GetDeployedDeviceGroupId());
		if (!DeviceGroup.IsValid())
		{
			DeviceGroup = AddNewDeviceGroup();	
		}
		Profile->SetDeployedDeviceGroup(DeviceGroup);

		return MakeShareable(Profile);
	}

	return nullptr;
}

ILauncherProfilePtr FLauncherProfileManager::LoadJSONProfile(FString ProfileFile)
{
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *ProfileFile))
	{
		return nullptr;
	}

	TSharedPtr<FJsonObject> Object;
	TSharedRef<TJsonReader<> > Reader = TJsonReaderFactory<>::Create(FileContents);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
	{
		return nullptr;
	}

	FLauncherProfile* Profile = new FLauncherProfile(AsShared());

	if (Profile->Load(*(Object.Get())))
	{
		ILauncherDeviceGroupPtr DeviceGroup = GetDeviceGroup(Profile->GetDeployedDeviceGroupId());
		if (!DeviceGroup.IsValid())
		{
			DeviceGroup = AddNewDeviceGroup();
		}
		Profile->SetDeployedDeviceGroup(DeviceGroup);

		return MakeShareable(Profile);
	}

	return nullptr;
}



void FLauncherProfileManager::LoadSettings( )
{
	LoadDeviceGroups();
	LoadProfiles();
}


void FLauncherProfileManager::RemoveDeviceGroup( const ILauncherDeviceGroupRef& DeviceGroup )
{
	if (DeviceGroups.Remove(DeviceGroup) > 0)
	{
		SaveDeviceGroups();

		DeviceGroupRemovedDelegate.Broadcast(DeviceGroup);
	}
}


void FLauncherProfileManager::RemoveSimpleProfile(const ILauncherSimpleProfileRef& SimpleProfile)
{
	if (SimpleProfiles.Remove(SimpleProfile) > 0)
	{
		// delete the persisted simple profile on disk
		FString SimpleProfileFileName = FLauncherProfile::GetProfileFolder() / SimpleProfile->GetDeviceName() + TEXT(".uslp");
		IFileManager::Get().Delete(*SimpleProfileFileName);
	}
}


void FLauncherProfileManager::RemoveProfile( const ILauncherProfileRef& Profile )
{
	AllProfiles.Remove(Profile);
	if (SavedProfiles.Remove(Profile) > 0)
	{
		if (Profile->GetId().IsValid())
		{
			// delete the persisted profile on disk
			FString ProfileFileName = Profile->GetFilePath();

			// delete the profile
			IFileManager::Get().Delete(*ProfileFileName);

			ProfileRemovedDelegate.Broadcast(Profile);
		}
	}
}


bool FLauncherProfileManager::SaveProfile(const ILauncherProfileRef& Profile)
{
	if (Profile->GetId().IsValid())
	{
		FString ProfileFileName = Profile->GetFilePath();
		FArchive* ProfileFileWriter = IFileManager::Get().CreateFileWriter(*ProfileFileName);

		if (ProfileFileWriter != nullptr)
		{
			Profile->Serialize(*ProfileFileWriter);

			delete ProfileFileWriter;

			return true;
		}
	}
	return false;
}


bool FLauncherProfileManager::SaveJSONProfile(const ILauncherProfileRef& Profile)
{
	if (Profile->GetId().IsValid())
	{
		FString Text;
		TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&Text);
		Profile->Save(Writer.Get());
		Writer->Close();
		return FFileHelper::SaveStringToFile(Text, *Profile->GetFilePath());
	}
	return false;
}

void FLauncherProfileManager::ChangeProfileName(const ILauncherProfileRef& Profile, FString Name)
{
	FString OldName = Profile->GetName();
	FString OldProfileFileName = Profile->GetFilePath();

	//change name and save to new location
	Profile->SetName(Name);
	if (SaveJSONProfile(Profile))
	{
		//delete the old profile if the location moved.  File names should be uppercase so this compare works on case sensitive and insensitive platforms
		if (OldProfileFileName.Compare(Profile->GetFilePath()) != 0)
		{
			
			IFileManager::Get().Delete(*OldProfileFileName);
		}
	}
	else
	{
		//if we couldn't save successfully, change the name back to keep files/profiles matching.
		Profile->SetName(OldName);
	}	
}

void FLauncherProfileManager::RegisterProfileWizard(const ILauncherProfileWizardPtr& InProfileWizard)
{
	ProfileWizards.Add(InProfileWizard);
}

void FLauncherProfileManager::UnregisterProfileWizard(const ILauncherProfileWizardPtr& InProfileWizard)
{
	ProfileWizards.Remove(InProfileWizard);
}

const TArray<ILauncherProfileWizardPtr>& FLauncherProfileManager::GetProfileWizards() const
{
	return ProfileWizards;
}

void FLauncherProfileManager::SaveSettings( )
{
	SaveDeviceGroups();
	SaveSimpleProfiles();
	SaveProfiles();
}

FString FLauncherProfileManager::GetProjectName() const
{
	return FLauncherProjectPath::GetProjectName(ProjectPath);
}

FString FLauncherProfileManager::GetProjectBasePath() const
{
	return FLauncherProjectPath::GetProjectBasePath(ProjectPath);
}

FString FLauncherProfileManager::GetProjectPath() const
{
	return ProjectPath;
}

void FLauncherProfileManager::SetProjectPath(const FString& InProjectPath)
{
	if (ProjectPath != InProjectPath)
	{
		ProjectPath = InProjectPath;
		for (ILauncherProfilePtr Profile : AllProfiles)
		{
			if (Profile.IsValid())
			{
				Profile->FallbackProjectUpdated();
			}
		}
	}
}

void FLauncherProfileManager::LoadDeviceGroups( )
{
	if (GConfig != nullptr)
	{
		FConfigSection* LoadedDeviceGroups = GConfig->GetSectionPrivate(TEXT("Launcher.DeviceGroups"), false, true, GEngineIni);

		if (LoadedDeviceGroups != nullptr)
		{
			// parse the configuration file entries into device groups
			for (FConfigSection::TIterator It(*LoadedDeviceGroups); It; ++It)
			{
				if (It.Key() == TEXT("DeviceGroup"))
				{
					DeviceGroups.Add(ParseDeviceGroup(It.Value().GetValue()));
				}
			}
		}
	}
}


void FLauncherProfileManager::LoadProfiles( )
{
	TArray<FString> ProfileFileNames;

	//load and move legacy profiles
	{
		IFileManager::Get().FindFilesRecursive(ProfileFileNames, *GetLegacyProfileFolder(), TEXT("*.ulp"), true, false);
		for (TArray<FString>::TConstIterator It(ProfileFileNames); It; ++It)
		{
			FString ProfileFilePath = *It;
			FArchive* ProfileFileReader = IFileManager::Get().CreateFileReader(*ProfileFilePath);

			if (ProfileFileReader != nullptr)
			{
				ILauncherProfilePtr LoadedProfile = LoadProfile(*ProfileFileReader);
				delete ProfileFileReader;

				//re-save profile to new location
				if (LoadedProfile.IsValid())
				{
					SaveProfile(LoadedProfile.ToSharedRef());
				}

				//delete legacy profile.
				IFileManager::Get().Delete(*ProfileFilePath);				
			}
		}
	}

	//load and re-save legacy profiles
	{
		IFileManager::Get().FindFilesRecursive(ProfileFileNames, *FLauncherProfile::GetProfileFolder(), TEXT("*.ulp"), true, false);
		for (TArray<FString>::TConstIterator It(ProfileFileNames); It; ++It)
		{
			FString ProfileFilePath = *It;
			FArchive* ProfileFileReader = IFileManager::Get().CreateFileReader(*ProfileFilePath);

			if (ProfileFileReader != nullptr)
			{
				ILauncherProfilePtr LoadedProfile = LoadProfile(*ProfileFileReader);
				delete ProfileFileReader;

				//re-save profile to the new format
				if (LoadedProfile.IsValid())
				{
					if (ProfileFilePath.Contains("NotForLicensees"))
					{
						LoadedProfile->SetNotForLicensees();
					}
					SaveJSONProfile(LoadedProfile.ToSharedRef());
				}

				//delete legacy profile.
				IFileManager::Get().Delete(*ProfileFilePath);
			}
		}
	}

	ProfileFileNames.Reset();
	IFileManager::Get().FindFilesRecursive(ProfileFileNames, *FLauncherProfile::GetProfileFolder(), TEXT("*.ulp2"), true, false);
	
	for (TArray<FString>::TConstIterator It(ProfileFileNames); It; ++It)
	{
		FString ProfileFilePath = *It;
		ILauncherProfilePtr LoadedProfile = LoadJSONProfile(*ProfileFilePath);

		if (LoadedProfile.IsValid())
		{
			if (ProfileFilePath.Contains("NotForLicensees"))
			{
				LoadedProfile->SetNotForLicensees();
			}
			AddProfile(LoadedProfile.ToSharedRef());
		}
		else
		{
			IFileManager::Get().Delete(*ProfileFilePath);
		}
	}
}


ILauncherDeviceGroupPtr FLauncherProfileManager::ParseDeviceGroup( const FString& GroupString )
{
	TSharedPtr<FLauncherDeviceGroup> Result;

	FString GroupIdString;

	if (FParse::Value(*GroupString, TEXT("Id="), GroupIdString))
	{
		FGuid GroupId;

		if (!FGuid::Parse(GroupIdString, GroupId))
		{
			GroupId = FGuid::NewGuid();
		}

		FString GroupName;
		FParse::Value(*GroupString, TEXT("Name="), GroupName);

		FString DevicesString;
		FParse::Value(*GroupString, TEXT("Devices="), DevicesString);

		Result = MakeShareable(new FLauncherDeviceGroup(GroupId, GroupName));

		TArray<FString> DeviceList;
		DevicesString.ParseIntoArray(DeviceList, TEXT(", "), false);

		for (int32 Index = 0; Index < DeviceList.Num(); ++Index)
		{
			Result->AddDevice(DeviceList[Index]);
		}
	}

	return Result;
}


void FLauncherProfileManager::SaveDeviceGroups( )
{
	if (GConfig != nullptr)
	{
		GConfig->EmptySection(TEXT("Launcher.DeviceGroups"), GEngineIni);

		TArray<FString> DeviceGroupStrings;

		// create a string representation of all groups and their devices
		for (int32 GroupIndex = 0; GroupIndex < DeviceGroups.Num(); ++GroupIndex)
		{
			const ILauncherDeviceGroupPtr& Group = DeviceGroups[GroupIndex];
			const TArray<FString>& Devices = Group->GetDeviceIDs();

			FString DeviceListString;

			for (int32 DeviceIndex = 0; DeviceIndex < Devices.Num(); ++DeviceIndex)
			{
				if (DeviceIndex > 0)
				{
					DeviceListString += ", ";
				}

				DeviceListString += Devices[DeviceIndex];
			}

			FString DeviceGroupString = FString::Printf(TEXT("(Id=\"%s\", Name=\"%s\", Devices=\"%s\")" ), *Group->GetId().ToString(), *Group->GetName(), *DeviceListString);

			DeviceGroupStrings.Add(DeviceGroupString);
		}

		// save configuration
		GConfig->SetArray(TEXT("Launcher.DeviceGroups"), TEXT("DeviceGroup"), DeviceGroupStrings, GEngineIni);
		GConfig->Flush(false, GEngineIni);
	}
}


void FLauncherProfileManager::SaveSimpleProfiles()
{
	for (TArray<ILauncherSimpleProfilePtr>::TIterator It(SimpleProfiles); It; ++It)
	{
		FString SimpleProfileFileName = FLauncherProfile::GetProfileFolder() / (*It)->GetDeviceName() + TEXT(".uslp");
		FString Text;
		TSharedRef< TJsonWriter<> > Writer = TJsonWriterFactory<>::Create(&Text);
		(*It)->Save(Writer.Get());
		Writer->Close();
		FFileHelper::SaveStringToFile(Text, *SimpleProfileFileName);
	}
}


void FLauncherProfileManager::SaveProfiles( )
{
	for (TArray<ILauncherProfilePtr>::TIterator It(SavedProfiles); It; ++It)
	{
		SaveJSONProfile((*It).ToSharedRef());
	}
}
