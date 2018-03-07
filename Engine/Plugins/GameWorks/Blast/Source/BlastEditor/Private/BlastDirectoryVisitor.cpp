#include "BlastDirectoryVisitor.h"

#define LOCTEXT_NAMESPACE "Blast"

FBlastDirectoryVisitor::FBlastDirectoryVisitor(IPlatformFile& inFileInterface, FString inFilePrefix, FString inFileExtension):
	FileInterface(inFileInterface),
	FilePrefix(inFilePrefix),
	FileExtension(inFileExtension)
{

}

bool FBlastDirectoryVisitor::Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory)
{
	if (bIsDirectory)
	{
		return true;
	}

	FString RelativeFilename = FilenameOrDirectory;
	FPaths::MakeStandardFilename(RelativeFilename);

	FString BaseFilename = FPaths::GetCleanFilename(RelativeFilename);

	if (BaseFilename.StartsWith(FilePrefix, ESearchCase::IgnoreCase) && BaseFilename.EndsWith(FileExtension, ESearchCase::IgnoreCase))
	{
		FilesFound.Add(RelativeFilename);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE