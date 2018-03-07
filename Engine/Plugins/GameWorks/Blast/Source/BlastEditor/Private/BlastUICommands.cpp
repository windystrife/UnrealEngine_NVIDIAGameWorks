#include "BlastUICommands.h"

#define LOCTEXT_NAMESPACE "Blast"

void FBlastUICommands::RegisterCommands()
{
	UI_COMMAND(BuildBlast, "Build Blast Glue", "Builds Blast objects so that Glue works", EUserInterfaceActionType::Button, FInputChord());

}
#undef LOCTEXT_NAMESPACE
