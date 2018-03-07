// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/DeveloperSettings.h"
#include "OnlinePIESettings.generated.h"

struct FPropertyChangedEvent;

/**
 * Stores PIE login credentials
 */
USTRUCT()
struct FPIELoginSettingsInternal
{
public:

	GENERATED_USTRUCT_BODY()

	/** Id of the user logging in (email, display name, facebook id, etc) */
	UPROPERTY(EditAnywhere, Category = "Logins", meta = (DisplayName = "User Id", Tooltip = "Id of the user logging in (email, display name, facebook id, etc)"))
	FString Id;
	/** Credentials of the user logging in (password or auth token) */
	UPROPERTY(EditAnywhere, Transient, Category = "Logins", meta = (DisplayName = "Password", Tooltip = "Credentials of the user logging in (password or auth token)", PasswordField = true))
	FString Token;
	/** Type of account. Needed to identity the auth method to use (epic, internal, facebook, etc) */
	UPROPERTY(EditAnywhere, Category = "Logins", meta = (DisplayName = "Type", Tooltip = "Type of account. Needed to identity the auth method to use (epic, internal, facebook, etc)"))
	FString Type;
	/** Token stored as an array of bytes, encrypted */
	UPROPERTY()
	TArray<uint8> TokenBytes;

	/** @return true if the credentials are valid, false otherwise */
	bool IsValid() const { return !Id.IsEmpty() && !Token.IsEmpty() && !Type.IsEmpty(); }

	/**
	 * Encrypt the Token field into the TokenBytes field
	 */
	void Encrypt();
	/**
	 * Decrypt the TokenBytes field into the Token field
	 */
	void Decrypt();
};

/**
 * Setup up login credentials for the Play In Editor (PIE) feature
 */
UCLASS(config=EditorPerProjectUserSettings, meta = (DisplayName = "Play Credentials"))
class UOnlinePIESettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UOnlinePIESettings(const FObjectInitializer& ObjectInitializer);

	/** Will Play In Editor (PIE) attempt to login to a platform service before launching the instance */
	UPROPERTY(config, EditAnywhere, Category = "Logins", meta = (DisplayName = "Enable Logins", Tooltip = "Attempt to login with user credentials on a backend service before launching the PIE instance."))
	bool bOnlinePIEEnabled;

	/** Array of credentials to use, one for each Play In Editor (PIE) instance */
	UPROPERTY(config, EditAnywhere, Category = "Logins", meta = (DisplayName = "Credentials", Tooltip = "Login credentials, at least one for each instance of PIE that is intended to be run"))
	TArray<FPIELoginSettingsInternal> Logins;

	// Begin UObject Interface
	virtual void PostInitProperties() override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
#endif // WITH_EDITOR
	// End UObject Interface
};
