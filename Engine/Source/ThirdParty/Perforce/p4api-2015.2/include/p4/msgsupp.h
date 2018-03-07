/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgsupp.h - definitions of errors for misc supporting libraries
 */

class MsgSupp {

    public:

	static ErrorId NoTransVar;

	static ErrorId InvalidDate;
	static ErrorId InvalidCharset;

	static ErrorId TooMany;
	static ErrorId Invalid;
	static ErrorId NeedsArg;
	static ErrorId Needs2Arg;
	static ErrorId NeedsNonNegArg;
	static ErrorId ExtraArg;
	static ErrorId WrongArg;
	static ErrorId Usage;
	static ErrorId OptionData;

	static ErrorId NoParm;

	static ErrorId NoUnixReg;
	static ErrorId NoSuchVariable;
	static ErrorId HidesVar;
	static ErrorId NoP4Config;
	static ErrorId VariableData;

	static ErrorId PartialChar;
	static ErrorId NoTrans;
	static ErrorId ConvertFailed;

	static ErrorId BadMangleParams;
	static ErrorId BadOS;

	static ErrorId Deflate;
	static ErrorId DeflateEnd;
	static ErrorId DeflateInit;
	static ErrorId Inflate;
	static ErrorId InflateInit;
	static ErrorId MagicHeader;

	static ErrorId RegexError;

	static ErrorId OptionChange;
	static ErrorId OptionPort;
	static ErrorId OptionUser;
	static ErrorId OptionClient;
	static ErrorId OptionPreview;
	static ErrorId OptionDelete;
	static ErrorId OptionForce;
	static ErrorId OptionInput;
	static ErrorId OptionOutput;
	static ErrorId OptionMax;
	static ErrorId OptionQuiet;
	static ErrorId OptionShort;
	static ErrorId OptionLong;
	static ErrorId OptionAll;
	static ErrorId OptionFiletype;
	static ErrorId OptionStream;
	static ErrorId OptionParent;
	static ErrorId OptionClientName;
	static ErrorId OptionHost;
	static ErrorId OptionPassword;
	static ErrorId OptionCharset;
	static ErrorId OptionCmdCharset;
	static ErrorId OptionVariable;
	static ErrorId OptionHelp;
	static ErrorId OptionVersion;
	static ErrorId OptionBatchsize;
	static ErrorId OptionMessageType;
	static ErrorId OptionXargs;
	static ErrorId OptionExclusive;
	static ErrorId OptionProgress;
	static ErrorId OptionDowngrade;
	static ErrorId OptionDirectory;
	static ErrorId OptionRetries;
	static ErrorId OptionNoIgnore;
	static ErrorId OptionCentralUsers;
	static ErrorId OptionReplicaUsers;
	static ErrorId OptionFullBranch;
	static ErrorId OptionSpecFixStatus;
	static ErrorId OptionChangeType;
	static ErrorId OptionChangeUpdate;
	static ErrorId OptionChangeUser;
	static ErrorId OptionOriginal;
	static ErrorId OptionTemplate;
	static ErrorId OptionSwitch;
	static ErrorId OptionTemporary;
	static ErrorId OptionOwner;
	static ErrorId OptionAdministrator;
	static ErrorId OptionGlobal;
	static ErrorId OptionStreamType;
	static ErrorId OptionVirtualStream;
	static ErrorId OptionBrief;
	static ErrorId OptionInherited;
	static ErrorId OptionChangeStatus;
	static ErrorId OptionShowTime;
	static ErrorId OptionLimitClient;
	static ErrorId OptionLabelName;
	static ErrorId OptionRunOnMaster;
	static ErrorId OptionArchive;
	static ErrorId OptionBlocksize;
	static ErrorId OptionHuman1024;
	static ErrorId OptionHuman1000;
	static ErrorId OptionSummary;
	static ErrorId OptionShelved;
	static ErrorId OptionUnload;
	static ErrorId OptionUnloadLimit;
	static ErrorId OptionOmitLazy;
	static ErrorId OptionLeaveKeywords;
	static ErrorId OptionOutputFile;
	static ErrorId OptionExists;
	static ErrorId OptionContent;
	static ErrorId OptionOmitPromoted;
	static ErrorId OptionOmitMoved;
	static ErrorId OptionKeepClient;
	static ErrorId OptionFileCharset;
	static ErrorId OptionVirtual;
	static ErrorId OptionGenerate;
	static ErrorId OptionUsage;
	static ErrorId OptionTags;
	static ErrorId OptionFilter;
	static ErrorId OptionJob;
	static ErrorId OptionExpression;
	static ErrorId OptionNoCaseExpr;
	static ErrorId OptionIncrement;
	static ErrorId OptionDiffFlags;
	static ErrorId OptionFixStatus;
	static ErrorId OptionShelf;
	static ErrorId OptionReplace;
	static ErrorId OptionShelveOpts;
	static ErrorId OptionBranch;
	static ErrorId OptionSubmitShelf;
	static ErrorId OptionSubmitOpts;
	static ErrorId OptionReopen;
	static ErrorId OptionDescription;
	static ErrorId OptionTamper;
	static ErrorId OptionCompress;
	static ErrorId OptionDate;
	static ErrorId OptionStreamName;
	static ErrorId OptionReverse;
	static ErrorId OptionWipe;
	static ErrorId OptionUnchanged;
	static ErrorId OptionDepot;
	static ErrorId OptionDepotType;
	static ErrorId OptionKeepHead;
	static ErrorId OptionPurge;
	static ErrorId OptionForceText;
	static ErrorId OptionBinaryAsText;
	static ErrorId OptionBypassFlow;
	static ErrorId OptionShowChange;
	static ErrorId OptionFollowBranch;
	static ErrorId OptionFollowInteg;
	static ErrorId OptionSourceFile;
	static ErrorId OptionOutputFlags;
	static ErrorId OptionShowFlags;
	static ErrorId OptionForceFlag;
	static ErrorId OptionResolveFlags;
	static ErrorId OptionAcceptFlags;
	static ErrorId OptionIntegFlags;
	static ErrorId OptionDeleteFlags;
	static ErrorId OptionRestrictFlags;
	static ErrorId OptionSortFlags;
	static ErrorId OptionUseList;
	static ErrorId OptionPublish;
	static ErrorId OptionSafe;
	static ErrorId OptionIsGroup;
	static ErrorId OptionIsUser;
	static ErrorId OptionIsOwner;
	static ErrorId OptionVerbose;
	static ErrorId OptionLineNumber;
	static ErrorId OptionInvertMatch;
	static ErrorId OptionFilesWithMatches;
	static ErrorId OptionFilesWithoutMatch;
	static ErrorId OptionNoMessages;
	static ErrorId OptionFixedStrings;
	static ErrorId OptionBasicRegexp;
	static ErrorId OptionExtendedRegexp;
	static ErrorId OptionPerlRegexp;
	static ErrorId OptionRegexp;
	static ErrorId OptionAfterContext;
	static ErrorId OptionBeforeContext;
	static ErrorId OptionContext;
	static ErrorId OptionIgnoreCase;
	static ErrorId OptionJournalPrefix;
	static ErrorId OptionRepeat;
	static ErrorId OptionBackoff;
	static ErrorId OptionArchiveData;
	static ErrorId OptionStatus;
	static ErrorId OptionJournalPosition;
	static ErrorId OptionPullServerid;
	static ErrorId OptionExcludeTables;
	static ErrorId OptionFile;
	static ErrorId OptionRevision;
	static ErrorId OptionLocalJournal;
	static ErrorId OptionNoRejournal;
	static ErrorId OptionAppend;
	static ErrorId OptionSequence;
	static ErrorId OptionCounter;
	static ErrorId OptionHostName;
	static ErrorId OptionPrint;
	static ErrorId OptionStartPosition;
	static ErrorId OptionEncoded;
	static ErrorId OptionLogName;
	static ErrorId OptionLoginStatus;
	static ErrorId OptionCompressCkp;
	static ErrorId OptionSpecType;
	static ErrorId OptionMaxAccess;
	static ErrorId OptionGroupName;
	static ErrorId OptionShowFiles;
	static ErrorId OptionName;
	static ErrorId OptionValue;
	static ErrorId OptionPropagating;
	static ErrorId OptionOpenAdd;
	static ErrorId OptionOpenEdit;
	static ErrorId OptionOpenDelete;
	static ErrorId OptionUseModTime;
	static ErrorId OptionLocal;
	static ErrorId OptionOutputBase;
	static ErrorId OptionSystem;
	static ErrorId OptionService;
	static ErrorId OptionHistogram;
	static ErrorId OptionTableNotUnlocked;
	static ErrorId OptionTableName;
	static ErrorId OptionAllClients;
	static ErrorId OptionCheckSize;
	static ErrorId OptionTransfer;
	static ErrorId OptionUpdate;
	static ErrorId OptionVerify;
	static ErrorId OptionNoArchive;
	static ErrorId OptionServerid;
	static ErrorId OptionUnified;
	static ErrorId OptionPreviewNC;
	static ErrorId OptionEstimates;
	static ErrorId OptionLocked;
	static ErrorId OptionUnloadAll;
	static ErrorId OptionKeepHave;
	static ErrorId OptionYes;
	static ErrorId OptionNo;
	static ErrorId OptionInputValue;
	static ErrorId OptionReplacement;
	static ErrorId OptionFrom;
	static ErrorId OptionTo;
	static ErrorId OptionRebuild;
	static ErrorId OptionEqual;
	static ErrorId OptionAttrPattern;
	static ErrorId OptionDiffListFlag;
	static ErrorId OptionArguments;
	static ErrorId OptionEnvironment;
	static ErrorId OptionTaskStatus;
	static ErrorId OptionAllUsers;
	static ErrorId OptionParallel;
	static ErrorId OptionParallelSubmit;
	static ErrorId OptionPromote;
	static ErrorId OptionInputFile;
	static ErrorId OptionPidFile;
	static ErrorId OptionTest;
	static ErrorId OptionActive;
	static ErrorId OptionNoRetransfer;
	static ErrorId OptionForceNoRetransfer;
	static ErrorId OptionDurableOnly;
	static ErrorId OptionNonAcknowledging;
	static ErrorId OptionReplicationStatus;
	static ErrorId OptionGroupMode;
	static ErrorId OptionBypassExlusiveLock;
	static ErrorId OptionCreate;
	static ErrorId OptionList;
	static ErrorId OptionMainline;
	static ErrorId OptionMoveChanges;
	static ErrorId OptionRetainLbrRevisions;
	static ErrorId OptionJavaProtocol;
	static ErrorId OptionPullBatch;
	static ErrorId OptionGlobalLock;
	static ErrorId OptionEnableDVCSTriggers;
	static ErrorId OptionUsers;
	static ErrorId TooManyLockTrys;

	// Retired ErrorIds. We need to keep these so that clients 
	// built with newer apis can commnunicate with older servers 
	// still sending these.

	static ErrorId ZCLoadLibFailed; // DEPRECATED 2013.1 removed ZeroConf
	static ErrorId ZCInvalidName; // DEPRECATED 2013.1 removed ZeroConf
	static ErrorId ZCRequireName; // DEPRECATED 2013.1 removed ZeroConf
	static ErrorId ZCNameConflict; // DEPRECATED 2013.1 removed ZeroConf
	static ErrorId ZCRegistryFailed; // DEPRECATED 2013.1 removed ZeroConf
	static ErrorId ZCBrowseFailed; // DEPRECATED 2013.1 removed ZeroConf
} ;

