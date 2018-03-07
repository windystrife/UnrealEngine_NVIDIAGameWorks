#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Widgets/Input/SButton.h"

#include "BlastImportUI.h"

class SBlastImportOptionWindow : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SBlastImportOptionWindow )
		: _ImportUI(nullptr)
		, _WidgetWindow()
		, _FullPath()
		{}

		SLATE_ARGUMENT( UBlastImportUI*, ImportUI )
		SLATE_ARGUMENT( TSharedPtr<SWindow>, WidgetWindow )
		SLATE_ARGUMENT( FText, FullPath)
	SLATE_END_ARGS()
public:
	SBlastImportOptionWindow() : ImportUI(nullptr), bShouldImport(false) { }

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	FReply OnImport()
	{
		bShouldImport = true;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnCancel()
	{
		bShouldImport = false;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldImport() const
	{
		return bShouldImport;
	}

private:
	UBlastImportUI*			ImportUI;
	TWeakPtr<SWindow>		WidgetWindow;
	TSharedPtr<SButton>		ImportButton;

	bool					bShouldImport;

	bool CanImport() const;
};