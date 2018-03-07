// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PluginReferenceDescriptor.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectDescriptor.h"

#define LOCTEXT_NAMESPACE "PluginDescriptor"

FPluginReferenceDescriptor::FPluginReferenceDescriptor( const FString& InName, bool bInEnabled )
	: Name(InName)
	, bEnabled(bInEnabled)
	, bOptional(false)
{ }


bool FPluginReferenceDescriptor::IsEnabledForPlatform( const FString& Platform ) const
{
	// If it's not enabled at all, return false
	if(!bEnabled)
	{
		return false;
	}

	// If there is a list of whitelisted platforms, and this isn't one of them, return false
	if(WhitelistPlatforms.Num() > 0 && !WhitelistPlatforms.Contains(Platform))
	{
		return false;
	}

	// If this platform is blacklisted, also return false
	if(BlacklistPlatforms.Contains(Platform))
	{
		return false;
	}

	return true;
}

bool FPluginReferenceDescriptor::IsEnabledForTarget(const FString& Target) const
{
    // If it's not enabled at all, return false
    if (!bEnabled)
    {
        return false;
    }

    // If there is a list of whitelisted platforms, and this isn't one of them, return false
    if (WhitelistTargets.Num() > 0 && !WhitelistTargets.Contains(Target))
    {
        return false;
    }

    // If this platform is blacklisted, also return false
    if (BlacklistTargets.Contains(Target))
    {
        return false;
    }

    return true;
}

bool FPluginReferenceDescriptor::IsSupportedTargetPlatform(const FString& Platform) const
{
	return SupportedTargetPlatforms.Num() == 0 || SupportedTargetPlatforms.Contains(Platform);
}

bool FPluginReferenceDescriptor::Read( const FJsonObject& Object, FText& OutFailReason )
{
	// Get the name
	if(!Object.TryGetStringField(TEXT("Name"), Name))
	{
		OutFailReason = LOCTEXT("PluginReferenceWithoutName", "Plugin references must have a 'Name' field");
		return false;
	}

	// Get the enabled field
	if(!Object.TryGetBoolField(TEXT("Enabled"), bEnabled))
	{
		OutFailReason = LOCTEXT("PluginReferenceWithoutEnabled", "Plugin references must have an 'Enabled' field");
		return false;
	}

	// Read the optional field
	Object.TryGetBoolField(TEXT("Optional"), bOptional);

	// Read the metadata for users that don't have the plugin installed
	Object.TryGetStringField(TEXT("Description"), Description);
	Object.TryGetStringField(TEXT("MarketplaceURL"), MarketplaceURL);

	// Get the platform lists
	Object.TryGetStringArrayField(TEXT("WhitelistPlatforms"), WhitelistPlatforms);
	Object.TryGetStringArrayField(TEXT("BlacklistPlatforms"), BlacklistPlatforms);

	// Get the target lists
	Object.TryGetStringArrayField(TEXT("WhitelistTargets"), WhitelistTargets);
	Object.TryGetStringArrayField(TEXT("BlacklistTargets"), BlacklistTargets);

	// Get the supported platform list
	Object.TryGetStringArrayField(TEXT("SupportedTargetPlatforms"), SupportedTargetPlatforms);

	return true;
}


bool FPluginReferenceDescriptor::ReadArray( const FJsonObject& Object, const TCHAR* Name, TArray<FPluginReferenceDescriptor>& OutPlugins, FText& OutFailReason )
{
	const TArray< TSharedPtr<FJsonValue> > *Array;

	if (Object.TryGetArrayField(Name, Array))
	{
		for (const TSharedPtr<FJsonValue> &Item : *Array)
		{
			const TSharedPtr<FJsonObject> *ObjectPtr;

			if (Item.IsValid() && Item->TryGetObject(ObjectPtr))
			{
				FPluginReferenceDescriptor Plugin;

				if (!Plugin.Read(*ObjectPtr->Get(), OutFailReason))
				{
					return false;
				}

				OutPlugins.Add(Plugin);
			}
		}
	}

	return true;
}


void FPluginReferenceDescriptor::Write( TJsonWriter<>& Writer ) const
{
	Writer.WriteObjectStart();
	Writer.WriteValue(TEXT("Name"), Name);
	Writer.WriteValue(TEXT("Enabled"), bEnabled);

	if (bEnabled && bOptional)
	{
		Writer.WriteValue(TEXT("Optional"), bOptional);
	}

	if (Description.Len() > 0)
	{
		Writer.WriteValue(TEXT("Description"), Description);
	}

	if (MarketplaceURL.Len() > 0)
	{
		Writer.WriteValue(TEXT("MarketplaceURL"), MarketplaceURL);
	}

	if (WhitelistPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("WhitelistPlatforms"));

		for (int Idx = 0; Idx < WhitelistPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(WhitelistPlatforms[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	if (BlacklistPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("BlacklistPlatforms"));

		for (int Idx = 0; Idx < BlacklistPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(BlacklistPlatforms[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	if (WhitelistTargets.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("WhitelistTargets"));

		for (int Idx = 0; Idx < WhitelistTargets.Num(); Idx++)
		{
			Writer.WriteValue(WhitelistTargets[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	if (BlacklistTargets.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("BlacklistTargets"));

		for (int Idx = 0; Idx < BlacklistTargets.Num(); Idx++)
		{
			Writer.WriteValue(BlacklistTargets[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	if (SupportedTargetPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("SupportedTargetPlatforms"));

		for (int Idx = 0; Idx < SupportedTargetPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(SupportedTargetPlatforms[Idx]);
		}

		Writer.WriteArrayEnd();
	}

	Writer.WriteObjectEnd();
}


void FPluginReferenceDescriptor::WriteArray( TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FPluginReferenceDescriptor>& Plugins )
{
	if( Plugins.Num() > 0)
	{
		Writer.WriteArrayStart(Name);

		for (int Idx = 0; Idx < Plugins.Num(); Idx++)
		{
			Plugins[Idx].Write(Writer);
		}

		Writer.WriteArrayEnd();
	}
}

#undef LOCTEXT_NAMESPACE
