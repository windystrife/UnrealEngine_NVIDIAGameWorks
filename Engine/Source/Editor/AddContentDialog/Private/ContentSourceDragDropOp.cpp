// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ContentSourceDragDropOp.h"
#include "ViewModels/ContentSourceViewModel.h"

TSharedRef<FContentSourceDragDropOp> FContentSourceDragDropOp::CreateShared(TSharedPtr<FContentSourceViewModel> InContentSource)
{
	TSharedPtr<FContentSourceDragDropOp> DragDropOp = MakeShareable(new FContentSourceDragDropOp(InContentSource));
	DragDropOp->MouseCursor = EMouseCursor::GrabHandClosed;
	DragDropOp->Construct();
	return DragDropOp.ToSharedRef();
}

FContentSourceDragDropOp::FContentSourceDragDropOp(TSharedPtr<FContentSourceViewModel> InContentSource)
{
	ContentSource = InContentSource;
}

TSharedPtr<SWidget> FContentSourceDragDropOp::GetDefaultDecorator() const
{
	return SNew(SImage)
		.Image(ContentSource->GetIconBrush().Get());
}

TSharedPtr<FContentSourceViewModel> FContentSourceDragDropOp::GetContentSource()
{
	return ContentSource;
}
