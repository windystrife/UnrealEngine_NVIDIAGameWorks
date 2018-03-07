#pragma once

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "NiagaraSettings.generated.h"


UCLASS(config = Engine, defaultconfig, meta=(DisplayName="Niagara"))
class NIAGARA_API UNiagaraSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category = Niagara, meta = (AllowedClasses = "ScriptStruct"))
	TArray<FSoftObjectPath> AdditionalParameterTypes;

	UPROPERTY(config, EditAnywhere, Category = Niagara, meta = (AllowedClasses = "ScriptStruct"))
	TArray<FSoftObjectPath> AdditionalPayloadTypes;

	UPROPERTY(config, EditAnywhere, Category = Niagara, meta = (AllowedClasses = "Enum"))
	TArray<FSoftObjectPath> AdditionalParameterEnums;
	
	// Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif
	// END UDeveloperSettings Interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNiagaraSettingsChanged, const FString&, const UNiagaraSettings*);

	/** Gets a multicast delegate which is called whenever one of the parameters in this settings object changes. */
	static FOnNiagaraSettingsChanged& OnSettingsChanged();

protected:
	static FOnNiagaraSettingsChanged SettingsChangedDelegate;
#endif
};