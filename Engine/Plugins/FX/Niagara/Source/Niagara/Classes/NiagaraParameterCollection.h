// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraParameterStore.h"
#include "NiagaraCommon.h"
#include "NiagaraParameterCollection.generated.h"

class UNiagaraParameterCollection;

UCLASS()
class NIAGARA_API UNiagaraParameterCollectionInstance : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	virtual ~UNiagaraParameterCollectionInstance();

	//~UObject interface
	virtual void PostLoad()override;
	//~UObject interface
	
	bool IsDefaultInstance()const;
	
	void SetParent(UNiagaraParameterCollection* InParent);
	UNiagaraParameterCollection* GetParent()const { return Collection; }

	FNiagaraParameterStore& GetParameterStore() { return ParameterStorage; }

	bool AddParameter(const FNiagaraVariable& Parameter);
	bool RemoveParameter(const FNiagaraVariable& Parameter);
	void RenameParameter(const FNiagaraVariable& Parameter, FName NewName);
	void Empty();
	void GetParameters(TArray<FNiagaraVariable>& OutParameters);

	void Tick();

	//TODO: Abstract to some interface to allow a hierarchy like UMaterialInstance?
	UPROPERTY(EditAnywhere, Category=Instance)
	UNiagaraParameterCollection* Collection;

	/**
	When editing instances, we must track which parameters are overridden so we can pull in any changes to the default.
	*/
	UPROPERTY()
	TArray<FNiagaraVariable> OverridenParameters;
	bool OverridesParameter(const FNiagaraVariable& Parameter)const;
	void SetOverridesParameter(const FNiagaraVariable& Parameter, bool bOverrides);

	/** Synchronizes this instance with any changes with it's parent collection. */
	void SyncWithCollection();
	
private:

	UPROPERTY()
	FNiagaraParameterStore ParameterStorage;

	//TODO: These overrides should be settable per platform.
	//UPROPERTY()
	//TMap<FString, FNiagaraParameterStore>

public:
	//Accessors from Blueprint. For now just exposing common types but ideally we can expose any somehow in future.
	
	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Bool Parameter"))
	bool GetBoolParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Float Parameter"))
	float GetFloatParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Int Parameter"))
	int32 GetIntParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Vector2D Parameter"))
	FVector2D GetVector2DParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Vector Parameter"))
	FVector GetVectorParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Vector4 Parameter"))
	FVector4 GetVector4Parameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Get Color Parameter"))
	FLinearColor GetColorParameter(const FString& InVariableName);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Bool Parameter"))
	void SetBoolParameter(const FString& InVariableName, bool InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Float Parameter"))
	void SetFloatParameter(const FString& InVariableName, float InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Int Parameter"))
	void SetIntParameter(const FString& InVariableName, int32 InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Vector2D Parameter"))
	void SetVector2DParameter(const FString& InVariableName, FVector2D InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Vector Parameter"))
	void SetVectorParameter(const FString& InVariableName, FVector InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Vector4 Parameter"))
	void SetVector4Parameter(const FString& InVariableName, const FVector4& InValue);

	UFUNCTION(BlueprintCallable, Category = Niagara, meta = (DisplayName = "Set Color Parameter"))
	void SetColorParameter(const FString& InVariableName, FLinearColor InValue);
};

/** Asset containing a collection of global parameters usable by Niagara. */
UCLASS()
class NIAGARA_API UNiagaraParameterCollection : public UObject
{
	GENERATED_UCLASS_BODY()
public:

	//~UObject interface
	virtual void PostInitProperties()override;
	virtual void PostLoad()override;
	//~UObject interface

	int32 IndexOfParameter(const FNiagaraVariable& Var);

	int32 AddParameter(FName Name, FNiagaraTypeDefinition Type);
	void RemoveParameter(const FNiagaraVariable& Parameter);
	void RenameParameter(FNiagaraVariable& Parameter, FName NewName);

	TArray<FNiagaraVariable>& GetParameters() { return Parameters; }

	//TODO: Optional per platform overrides of the above.
	//TMap<FString, UNiagaraParameterCollectionOverride> PerPlatformOverrides;

	FORCEINLINE UNiagaraParameterCollectionInstance* GetDefaultInstance() { return DefaultInstance; }
	
	/**
	Takes the friendly name presented to the UI and converts to the real parameter name used under the hood.
	Converts from "ParameterName" to "CollectionUniqueName_ParameterName".
	*/
	FString ParameterNameFromFriendlyName(FString FriendlyName)const;
	/**
	Takes the real parameter name used under the hood and converts to the friendly name for use in the UI.
	Converts from "CollectionUniqueName_ParameterName" to "ParameterName".
	*/

	FNiagaraVariable CollectionParameterFromFriendlyParameter(const FNiagaraVariable& FriendlyParameter)const;
	FNiagaraVariable FriendlyParameterFromCollectionParameter(const FNiagaraVariable& CollectionParameter)const;

	FString FriendlyNameFromParameterName(FString ParameterName)const;
protected:

	FString GetUniqueName()const;
	
	UPROPERTY()
	TArray<FNiagaraVariable> Parameters;
	
	UPROPERTY()
	UNiagaraParameterCollectionInstance* DefaultInstance;

	/** Unique name used by parameters in this collection and the scripts referencing them to link them with this collection. */
	UPROPERTY()
	FString UniqueName;
};