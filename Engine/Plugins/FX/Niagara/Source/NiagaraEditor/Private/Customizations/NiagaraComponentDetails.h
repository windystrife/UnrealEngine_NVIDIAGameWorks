// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "EditorUndoClient.h"
#include "TickableEditorObject.h"
#include "Reply.h"
#include "Visibility.h"
#include "Misc/NotifyHook.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class FNiagaraParameterViewModelCustomDetails;
class INiagaraParameterViewModel;
class SNiagaraParameterEditor;
class FNiagaraParameterViewModelCustomDetails;
class UNiagaraSystem;

class FNiagaraComponentDetails : public IDetailCustomization, public FEditorUndoClient, public FTickableEditorObject
{
public:
	virtual ~FNiagaraComponentDetails();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	//~ FEditorUndoClient interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	// FTickableEditorObject
	virtual bool IsTickable() const override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	void OnSystemInstanceReset();
	void OnSystemInstanceDestroyed();
	FReply OnSystemOpenRequested(UNiagaraSystem* InSystem);
	void OnWorldDestroyed(class UWorld* InWorld);
	void OnPiEEnd();

	FNiagaraComponentDetails();
private:
	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	IDetailLayoutBuilder* Builder;
	bool bQueueForDetailsRefresh;

	FDelegateHandle OnInitDelegateHandle;
	FDelegateHandle OnResetDelegateHandle;
};
