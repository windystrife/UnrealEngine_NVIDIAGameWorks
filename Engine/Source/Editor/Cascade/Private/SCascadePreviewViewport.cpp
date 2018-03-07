// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SCascadePreviewViewport.h"
#include "CascadeParticleSystemComponent.h"
#include "Cascade.h"
#include "Framework/Application/SlateApplication.h"
#include "Slate/SceneViewport.h"
#include "CascadePreviewViewportClient.h"
#include "SCascadePreviewToolbar.h"
#include "Widgets/Docking/SDockTab.h"



void SCascadePreviewViewport::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SEditorViewport::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	JustTicked = true;
}


SCascadePreviewViewport::~SCascadePreviewViewport()
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->Viewport = NULL;
	}
}

void SCascadePreviewViewport::Construct(const FArguments& InArgs)
{
	CascadePtr = InArgs._Cascade;

	SEditorViewport::Construct( SEditorViewport::FArguments() );
}

void SCascadePreviewViewport::RefreshViewport()
{
	SceneViewport->Invalidate();
}

bool SCascadePreviewViewport::IsVisible() const
{
	return ViewportWidget.IsValid() && (!ParentTab.IsValid() || ParentTab.Pin()->IsForeground()) && SEditorViewport::IsVisible();
}

TSharedPtr<FSceneViewport> SCascadePreviewViewport::GetViewport() const
{
	return SceneViewport;
}

TSharedPtr<FCascadeEdPreviewViewportClient> SCascadePreviewViewport::GetViewportClient() const
{
	return ViewportClient;
}

TSharedPtr<SViewport> SCascadePreviewViewport::GetViewportWidget() const
{
	return ViewportWidget;
}

TSharedRef<FEditorViewportClient> SCascadePreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FCascadeEdPreviewViewportClient(CascadePtr, SharedThis(this)));

	ViewportClient->bSetListenerPosition = false;

	ViewportClient->SetRealtime(true);
	ViewportClient->VisibilityDelegate.BindSP(this, &SCascadePreviewViewport::IsVisible);

	return ViewportClient.ToSharedRef();
}

TSharedPtr<SWidget> SCascadePreviewViewport::MakeViewportToolbar()
{
	return
	SNew(SCascadePreviewViewportToolBar)
		.CascadePtr(CascadePtr)
		.IsEnabled(FSlateApplication::Get().GetNormalExecutionAttribute());
}


void SCascadePreviewViewport::OnFocusViewportToSelection()
{
	UParticleSystemComponent* Component = CascadePtr.Pin()->GetParticleSystemComponent();

	if( Component )
	{
		ViewportClient->FocusViewportOnBox( Component->Bounds.GetBox() );
	}
}
