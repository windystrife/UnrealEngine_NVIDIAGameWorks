/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * dberror.h - definitions of errors for Db subsystem.
 */

class MsgDb {

    public:

	static ErrorId JnlEnd;
	static ErrorId JnlWord2Big;
	static ErrorId JnlOct2Long;
	static ErrorId JnlOctSize;
	static ErrorId JnlNum2Big;
	static ErrorId JnlQuoting;
	static ErrorId JnlOpcode;
	static ErrorId JnlNoVers;
	static ErrorId JnlNoName;
	static ErrorId JnlNoTVers;
	static ErrorId JnlBadVers;
	static ErrorId JnlReplay;
	static ErrorId JnlDeleteFailed;
	static ErrorId JnlSeqBad;
	static ErrorId JnlFileBad;
	static ErrorId JnlBadMarker;
	static ErrorId JnlCaseUsageBad;
	static ErrorId JnlVersionMismatch;
	static ErrorId JnlVersionError;
	static ErrorId CheckpointNoOverwrite;
	static ErrorId TableCheckSum;
	static ErrorId DbOpen;
	static ErrorId WriteNoLock;
	static ErrorId Write;
	static ErrorId ReadNoLock;
	static ErrorId Read;
	static ErrorId Stumblebum;
	static ErrorId GetFormat;
	static ErrorId ScanNoLock;
	static ErrorId Scan;
	static ErrorId ScanFormat;
	static ErrorId DelNoLock;
	static ErrorId Delete;
	static ErrorId Locking;
	static ErrorId EndXact;
	static ErrorId GetNoGet;
	static ErrorId TableUnknown;
	static ErrorId TableObsolete;
	static ErrorId LockOrder;
	static ErrorId LockUpgrade;
	static ErrorId MaxMemory;
	static ErrorId KeyTooBig;
	static ErrorId XactOutstandingRestart;

	static ErrorId ParseErr;

	static ErrorId ExtraDots;
	static ErrorId ExtraStars;
	static ErrorId Duplicate;
	static ErrorId WildMismatch;
	static ErrorId TooWild;
	static ErrorId TooWild2;
	static ErrorId Juxtaposed;

	static ErrorId Field2Many;
	static ErrorId FieldBadVal;
	static ErrorId FieldWords;
	static ErrorId FieldMissing;
	static ErrorId FieldBadIndex;
	static ErrorId FieldUnknown;
	static ErrorId FieldTypeBad;
	static ErrorId FieldOptBad;
	static ErrorId NoEndQuote;
	static ErrorId Syntax;
	static ErrorId LineNo;

	static ErrorId LicenseExp;
	static ErrorId SupportExp;
	static ErrorId ServerTooNew;
	static ErrorId MustExpire;
	static ErrorId Checksum;
	static ErrorId WrongApp;
	static ErrorId PlatPre972;
	static ErrorId LicenseRead;
	static ErrorId LicenseBad;
	static ErrorId AddressChanged;
	static ErrorId LicenseNeedsApplication;
	static ErrorId BadIPservice;

	static ErrorId TreeCorrupt;
	static ErrorId TreeNotOpened;
	static ErrorId InternalUsage;
	static ErrorId TreeAllocation;
	static ErrorId TreeNotSupported;
	static ErrorId TreeAlreadyUpgraded;
	static ErrorId TreeInternal;
	static ErrorId ValidationFoundProblems;
	static ErrorId TreeNewerVersion;
	static ErrorId TreeOlderVersion;
	static ErrorId DoNotBlameTheDb;

	static ErrorId MapCheckFail;

	static ErrorId CaseMismatch;

	// Retired ErrorIds. We need to keep these so that clients 
	// built with newer apis can commnunicate with older servers 
	// still sending these.

	static ErrorId MaxResults; // DEPRECATED
	static ErrorId MaxScanRows; // DEPRECATED
	static ErrorId NotUnderRoot; // DEPRECATED
	static ErrorId NotUnderClient; // DEPRECATED
	static ErrorId ClientGone; // DEPRECATED
	static ErrorId CommandCancelled; // DEPRECATED
	static ErrorId DbStat; // DEPRECATED
} ;
