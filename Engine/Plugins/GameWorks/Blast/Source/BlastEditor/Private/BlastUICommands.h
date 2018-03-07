#pragma once
#include "EditorStyleSet.h"
#include "Framework/Commands/Commands.h"

class FBlastUICommands : public TCommands<FBlastUICommands>
{
public:

	FBlastUICommands() : TCommands<FBlastUICommands>(
		"BlastUICommands",
		NSLOCTEXT("Blast", "BlastUICommands", "BlastUICommands"),
		NAME_None,
		FEditorStyle::GetStyleSetName())
	{
	}

	TSharedPtr<FUICommandInfo>	BuildBlast;

	virtual void RegisterCommands() override;

};