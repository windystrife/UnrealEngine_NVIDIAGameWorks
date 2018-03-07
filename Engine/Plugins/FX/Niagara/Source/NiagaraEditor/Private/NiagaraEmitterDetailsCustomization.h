// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "IDetailCustomization.h"

class IDetailChildrenBuilder;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UNiagaraEmitter;

/*-----------------------------------------------------------------------------
FNiagaraEmitterDetails
-----------------------------------------------------------------------------*/

class FNiagaraEmitterDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(TWeakObjectPtr<UNiagaraEmitter> EmitterProperties);

	/** Constructor */
	FNiagaraEmitterDetails(TWeakObjectPtr<UNiagaraEmitter> EmitterProperties);

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;

	void OnGenerateConstantEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder);

	void OnGenerateScalarConstantEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder);
	void OnGenerateVectorConstantEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder);
	void OnGenerateEventGeneratorEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder);
	void OnGenerateEventReceiverEntry(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder);

	void BuildScriptProperties(TSharedRef<IPropertyHandle> ScriptPropsHandle, FName Name, FText DisplayName);
private:
	/** Object that stores all of the possible parameters we can edit */
	TWeakObjectPtr<UNiagaraEmitter> EmitterProps;

	IDetailLayoutBuilder* DetailLayout;
};

