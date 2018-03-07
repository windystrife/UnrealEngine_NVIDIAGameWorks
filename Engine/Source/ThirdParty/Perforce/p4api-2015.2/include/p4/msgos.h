/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgos.h - definitions of operating system specific errors
 */

class MsgOs {

    public:

	static ErrorId Sys;
	static ErrorId Sys2;
	static ErrorId SysUn;
	static ErrorId SysUn2;
	static ErrorId Net;
	static ErrorId NetUn;

	static ErrorId TooMany;
	static ErrorId Deleted;
	static ErrorId NoSuch;
	static ErrorId ChmodBetrayal;

	static ErrorId EmptyFork;
	static ErrorId NameTooLong;

	static ErrorId ZipExists;
	static ErrorId ZipOpenEntryFailed;
	static ErrorId ZipCloseEntryFailed;
	static ErrorId ZipWriteFailed;
	static ErrorId ZipMissing;
	static ErrorId ZipNoEntry;
	static ErrorId ZipOpenEntry;
	static ErrorId ZipReadFailed;

	// Retired ErrorIds. We need to keep these so that clients 
	// built with newer apis can commnunicate with older servers 
	// still sending these.
} ;
