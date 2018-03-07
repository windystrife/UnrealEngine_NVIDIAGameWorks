#pragma once
#include "GenericPlatformFile.h"

/*
	A simple directory visitor - finds any files that start with the prefix and end with the extension.

*/
class FBlastDirectoryVisitor : public IPlatformFile::FDirectoryVisitor
{
public:
	FBlastDirectoryVisitor(IPlatformFile& inFileInterface, FString inFilePrefix, FString inFileExtension);
	virtual ~FBlastDirectoryVisitor() = default;

	virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override;

	TArray<FString> FilesFound;
private:
	IPlatformFile& FileInterface;
	FString FilePrefix;
	FString FileExtension;

};