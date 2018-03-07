#pragma once

#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraScript.h"
#include "Engine/DeveloperSettings.h"
#include "NiagaraSpawnShortcut.h"
#include "NiagaraEditorSettings.generated.h"


UCLASS(config = Engine, defaultconfig, meta=(DisplayName="Niagara"))
class UNiagaraEditorSettings : public UDeveloperSettings
{
public:
	GENERATED_UCLASS_BODY()
		
	/** System to duplicate as the base of all new System assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FStringAssetReference DefaultSystem;

	/** Emitter to duplicate as the base of all new emitter assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FStringAssetReference DefaultEmitter;

	/** Niagara script to duplicate as the base of all new script assets created. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	FStringAssetReference DefaultScript;

	/** Shortcut key bindings that if held down while doing a mouse click, will spawn the specified type of Niagara node.*/
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	TArray<FNiagaraSpawnShortcut> GraphCreationShortcuts;

	/** Whether or not auto-compile is enabled in the editors. */
	UPROPERTY(config, EditAnywhere, Category = Niagara)
	bool bAutoCompile;
	
	// Begin UDeveloperSettings Interface
	virtual FName GetCategoryName() const override;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
#endif
	// END UDeveloperSettings Interface

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

public:
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNiagaraEditorSettingsChanged, const FString&, const UNiagaraEditorSettings*);

	/** Gets a multicast delegate which is called whenever one of the parameters in this settings object changes. */
	static FOnNiagaraEditorSettingsChanged& OnSettingsChanged();

protected:
	static FOnNiagaraEditorSettingsChanged SettingsChangedDelegate;
#endif
};
