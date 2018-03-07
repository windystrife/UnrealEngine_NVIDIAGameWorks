/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgdm.h - definitions of errors for data manager core subsystem.
 */

class MsgDm {

    public:
	static ErrorId DevMsg;
	static ErrorId DevErr;

	static ErrorId DescMissing;
	static ErrorId NoSuchChange;
	static ErrorId AlreadyCommitted;
	static ErrorId WrongClient;
	static ErrorId WrongUser;
	static ErrorId NoSuchCounter;
	static ErrorId NoSuchKey;
	static ErrorId NoSuchServerlog;
	static ErrorId MayNotBeNegative;
	static ErrorId MustBeNumeric;
	static ErrorId NotThatCounter;
	static ErrorId NoSuchDepot;
	static ErrorId NoSuchDepot2;
	static ErrorId NoSuchDomain;
	static ErrorId NoSuchDomain2;
	static ErrorId WrongDomain;
	static ErrorId TooManyClients;
	static ErrorId NoSuchJob;
	static ErrorId NoSuchJob2;
	static ErrorId NoSuchFix;
	static ErrorId NoPerms;
	static ErrorId OperatorNotAllowed;
	static ErrorId NoSuchRelease;
	static ErrorId ClientTooOld;
	static ErrorId NoProtect;
	static ErrorId PathNotUnder;
	static ErrorId TooManyUsers;
	static ErrorId MapNotUnder;
	static ErrorId MapNoListAccess;

	static ErrorId CheckFailed;
	static ErrorId InvalidType;
	static ErrorId IdTooLong;
	static ErrorId LineTooLong;
	static ErrorId IdHasDash;
	static ErrorId IdEmpty;
	static ErrorId IdNonPrint;
	static ErrorId IdHasComma;
	static ErrorId IdHasPercent;
	static ErrorId IdHasRev;
	static ErrorId IdHasSlash;
	static ErrorId IdNullDir;
	static ErrorId IdRelPath;
	static ErrorId IdWild;
	static ErrorId IdNumber;
	static ErrorId IdEmbeddedNul;
	static ErrorId BadOption;
	static ErrorId BadChange;
	static ErrorId BadMaxResult;
	static ErrorId BadTimeout;
	static ErrorId BadRevision;
	static ErrorId BadTypeMod;
	static ErrorId BadStorageCombo;
	static ErrorId BadVersionCount;
	static ErrorId BadTypeCombo;
	static ErrorId BadType;
	static ErrorId BadDigest;
	static ErrorId BadTypePartial;
	static ErrorId BadTypeAuto;
	static ErrorId NeedsUpgrades;
	static ErrorId PastUpgrade;
	static ErrorId Unicode;

	static ErrorId ParallelOptions;
	static ErrorId ParSubOptions;
	static ErrorId ParallelNotEnabled;
	static ErrorId ParThreadsTooMany;
	static ErrorId DepotMissing;
	static ErrorId UnloadDepotMissing;
	static ErrorId ReloadNotOwner;
	static ErrorId UnloadNotOwner;
	static ErrorId UnloadNotPossible;
	static ErrorId UnloadData;
	static ErrorId ReloadData;
	static ErrorId ReloadSuspicious;
	static ErrorId DepotVsDomains;
	static ErrorId RevVsRevCx;
	static ErrorId NoPrevRev;
	static ErrorId CantFindChange;
	static ErrorId BadIntegFlag;
	static ErrorId BadJobTemplate;
	static ErrorId NeedJobUpgrade;
	static ErrorId BadJobPresets;
	static ErrorId JobNameMissing;
	static ErrorId HaveVsRev;
	static ErrorId AddHaveVsRev;
	static ErrorId BadOpenFlag;
	static ErrorId NameChanged;
	static ErrorId WorkingVsLocked;
	static ErrorId IntegVsRev;
	static ErrorId IntegVsWork;
	static ErrorId ConcurrentFileChange;
	static ErrorId IntegVsShelve;
	static ErrorId AtMostOne;

	static ErrorId MissingDesc;
	static ErrorId BadJobView;
	static ErrorId NoModComChange;
	static ErrorId TheseCantChange;
	static ErrorId OwnerCantChange;
	static ErrorId UserCantChange;
	static ErrorId OpenFilesCantChange;
	static ErrorId OpenFilesCantChangeUser;
	static ErrorId CantOpenHere;
	static ErrorId PurgeFirst;
	static ErrorId SnapFirst;
	static ErrorId ReloadFirst;
	static ErrorId MustForceUnloadDepot;
	static ErrorId LockedUpdate;
	static ErrorId LockedDelete;
	static ErrorId OpenedDelete;
	static ErrorId OpenedSwitch;
	static ErrorId OpenedTaskSwitch;
	static ErrorId ClassicSwitch;
	static ErrorId PendingDelete;
	static ErrorId ShelvedDelete;
	static ErrorId ShelveNotChanged;
	static ErrorId NoSuchGroup;
	static ErrorId NoIntegOverlays;
	static ErrorId NoIntegHavemaps;
	static ErrorId BadMappedFileName;
	static ErrorId JobDescMissing;
	static ErrorId JobHasChanged;
	static ErrorId JobNameJob;
	static ErrorId JobFieldReadOnly;
	static ErrorId JobFieldAlways;
	static ErrorId BadSpecType;
	static ErrorId LameCodes;
	static ErrorId MultiWordDefault;
	static ErrorId ProtectedCodes;
	static ErrorId LabelOwner;
	static ErrorId LabelLocked;
	static ErrorId LabelHasRev;
	static ErrorId WildAdd;
	static ErrorId WildAddTripleDots;
	static ErrorId InvalidEscape;
	static ErrorId UserOrGroup;
	static ErrorId CantChangeUser;
	static ErrorId CantChangeUserAuth;
	static ErrorId CantChangeUserType;
	static ErrorId Passwd982;
	static ErrorId NoClearText;
	static ErrorId WrongUserDelete;
	static ErrorId DfltBranchView;
	static ErrorId LabelNoSync;
	static ErrorId FixBadVal;

	static ErrorId NoClient;
	static ErrorId NoDepot;
	static ErrorId NoArchive;
	static ErrorId EmptyRelate;
	static ErrorId BadCaller;
	static ErrorId DomainIsUnloaded;
	static ErrorId NotClientOrLabel;
	static ErrorId NotUnloaded;
	static ErrorId AlreadyUnloaded;
	static ErrorId CantChangeUnloadedOpt;
	static ErrorId NoUnloadedAutoLabel;
	static ErrorId StreamIsUnloaded;
	static ErrorId NoStorageDir;
	static ErrorId NotAsService;
	static ErrorId LockedClient;
	static ErrorId LockedHost;
	static ErrorId ClientBoundToServer;
	static ErrorId NotBoundToServer;
	static ErrorId BindingNotAllowed;
	static ErrorId BoundToOtherServer;
	static ErrorId TooManyCommitServers;
	static ErrorId EmptyFileName;
	static ErrorId NoRev;
	static ErrorId NoRevRange;
	static ErrorId NeedClient;
	static ErrorId ReferClient;
	static ErrorId BadAtRev;
	static ErrorId BadRevSpec;
	static ErrorId BadRevRel;
	static ErrorId BadRevPend;
	static ErrorId ManyRevSpec;
	static ErrorId LabelLoop;
	static ErrorId TwistedMap;
	static ErrorId EmptyResults;
	static ErrorId LimitBadArg;
	static ErrorId BadChangeMap;
	static ErrorId LabelNotAutomatic;
	static ErrorId LabelRevNotChange;

	static ErrorId NoDelete;
	static ErrorId NoCheckin;
	static ErrorId RmtError;
	static ErrorId TooOld;
	static ErrorId DbFailed;
	static ErrorId ArchiveFailed;
	static ErrorId NoRmtInterop;
	static ErrorId RmtAuthFailed;
	static ErrorId ServiceUserLogin;
	static ErrorId RmtSequenceFailed;
	static ErrorId OutOfSequence;
	static ErrorId ChangeExists;
	static ErrorId RmtJournalWaitFailed;
	static ErrorId NoRevisionOverwrite;

	static ErrorId RmtAddDomainFailed;
	static ErrorId RmtDeleteDomainFailed;
	static ErrorId RmtExclusiveLockFailed;
	static ErrorId RmtGlobalLockFailed;
	static ErrorId RemoteDomainExists;
	static ErrorId RemoteDomainMissing;

	static ErrorId RmtAddChangeFailed;
	static ErrorId RmtDeleteChangeFailed;
	static ErrorId RemoteChangeExists;
	static ErrorId RemoteChangeMissing;
	static ErrorId ChangeNotShelved;

	static ErrorId BadTemplate;
	static ErrorId FieldMissing;
	static ErrorId FieldCount;
	static ErrorId NoNotOp;
	static ErrorId SameCode;
	static ErrorId SameTag;
	static ErrorId NoDefault;
	static ErrorId SemiInDefault;

	static ErrorId LicensedClients;
	static ErrorId LicensedUsers;
	static ErrorId TryDelClient;
	static ErrorId TryDelUser;
	static ErrorId TooManyRoots;
	static ErrorId TryEvalLicense;

	static ErrorId UnidentifiedServer;
	static ErrorId ServiceNotProvided;

	static ErrorId AnnotateTooBig;

	static ErrorId NotBucket;
	static ErrorId BucketAdd;
	static ErrorId BucketRestore;
	static ErrorId BucketPurge;
	static ErrorId BucketSkipHead;
	static ErrorId BucketSkipLazy;
	static ErrorId BucketSkipType;
	static ErrorId BucketSkipBranched;
	static ErrorId BucketSkipBucketed;
	static ErrorId BucketSkipResolving;
	static ErrorId BucketSkipShelving;
	static ErrorId BucketNoFilesToArchive;
	static ErrorId BucketNoFilesToRestore;
	static ErrorId BucketNoFilesToPurge;
	static ErrorId CachePurgeFile;

	static ErrorId ChangeCreated;
	static ErrorId ChangeUpdated;
	static ErrorId ChangeDeleteOpen;
	static ErrorId ChangeDeleteHasFix;
	static ErrorId ChangeDeleteHasFiles;
	static ErrorId ChangeDeleteShelved;
	static ErrorId ChangeDeleteSuccess;
	static ErrorId ChangeNotOwner;
	static ErrorId CommittedNoPerm;
	static ErrorId PendingNoPerm;

	static ErrorId ChangesData;
	static ErrorId ChangesDataPending;

	static ErrorId PropertyData;
	static ErrorId PropertyDataUser;
	static ErrorId PropertyDataGroup;
	static ErrorId PropertyAllData;
	static ErrorId NoSuchProperty;
	static ErrorId BadSequence;
	static ErrorId ExPROPERTY;

	static ErrorId ConfigData;
	static ErrorId NoSuchConfig;
	static ErrorId ConfigWasNotSet;
	static ErrorId UseConfigure;

	static ErrorId CopyOpenTarget;
	static ErrorId CopyMoveMapFrom;
	static ErrorId CopyMoveNoFrom;
	static ErrorId CopyMoveExTo;
	static ErrorId CopyMapSummary;
	static ErrorId CopyChangeSummary;

	static ErrorId CountersData;

	static ErrorId DeleteMoved;

	static ErrorId DirsData;

	static ErrorId DepotSave;
	static ErrorId DepotNoChange;
	static ErrorId DepotDelete;
	static ErrorId DepotHasStreams;
	static ErrorId DepotNotEmptyNoChange;
	static ErrorId DepotSpecDup;
	static ErrorId DepotTypeDup;
	static ErrorId DepotUnloadDup;
	static ErrorId NoDepotTypeChange;
	static ErrorId DepotMapInvalid;
	static ErrorId DepotNotStream;
	static ErrorId DepotNotSpec;
	static ErrorId DepotDepthDiffers;
	static ErrorId DepotStreamDepthReq;
	static ErrorId ImportNotUnder;
	static ErrorId InvalidParent;
	static ErrorId StreamOverflow;
	static ErrorId NoStreamAtChange;
	static ErrorId NotStreamReady;
	static ErrorId MissingStream;
	static ErrorId InvalidStreamFmt;
	static ErrorId StreamNotRelative;
	static ErrorId StreamIncompatibleP;
	static ErrorId StreamIncompatibleC;
	static ErrorId StreamOwnerUpdate;
	static ErrorId StreamIsMainline;
	static ErrorId StreamIsVirtual;
	static ErrorId StreamNoFlow;
	static ErrorId StreamNoReparent;
	static ErrorId StreamNoConvert;
	static ErrorId StreamConverted;
	static ErrorId StreamParentIsTask;
	static ErrorId StreamBadConvert;
	static ErrorId StreamDepthDiffers;

	static ErrorId DepotsData;
	static ErrorId DepotsDataExtra;

	static ErrorId RemoteSave;
	static ErrorId RemoteNoChange;
	static ErrorId RemoteDelete;
	static ErrorId NoSuchRemote;
	static ErrorId RemotesData;

	static ErrorId ServerSave;
	static ErrorId ServerNoChange;
	static ErrorId ServerDelete;
	static ErrorId NoSuchServer;
	static ErrorId ServersData;
	static ErrorId ServerTypeMismatch;
	static ErrorId ServerViewMap;
	static ErrorId FiltersReplicaOnly;

	static ErrorId DescribeChange;
	static ErrorId DescribeChangePending;
	static ErrorId DescribeData;
	static ErrorId DescribeMove;
	static ErrorId DescribeDiff;

	static ErrorId DiffData;

	static ErrorId Diff2DataLeft;
	static ErrorId Diff2DataRight;
	static ErrorId Diff2DataRightPre041;
	static ErrorId Diff2DataContent;
	static ErrorId Diff2DataTypes;
	static ErrorId Diff2DataIdentical;
	static ErrorId Diff2DataUnified;
	static ErrorId Diff2DataUnifiedDiffer;

	static ErrorId DomainSave;
	static ErrorId DomainNoChange;
	static ErrorId DomainDelete;
	static ErrorId DomainSwitch;

	static ErrorId DomainsDataClient;
	static ErrorId DomainsData;

	static ErrorId DupOK;
	static ErrorId DupExists;
	static ErrorId DupLocked;

	static ErrorId FilelogData;
	static ErrorId FilelogRevDefault;
	static ErrorId FilelogRevMessage;
	static ErrorId FilelogInteg;

	static ErrorId FilesData;
	static ErrorId FilesSummary;
	static ErrorId FilesDiskUsage;
	static ErrorId FilesSummaryHuman;
	static ErrorId FilesDiskUsageHuman;

	static ErrorId FixAdd;
	static ErrorId FixDelete;

	static ErrorId FixesData;

	static ErrorId GrepOutput;
	static ErrorId GrepFileOutput;
	static ErrorId GrepWithLineNumber;
	static ErrorId GrepLineTooLong;
	static ErrorId GrepMaxRevs;
	static ErrorId GrepSeparator;

	static ErrorId GroupCreated;
	static ErrorId GroupNotCreated;
	static ErrorId GroupDeleted;
	static ErrorId GroupNotUpdated;
	static ErrorId GroupUpdated;
	static ErrorId GroupNotOwner;
	static ErrorId GroupExists;
	static ErrorId GroupLdapIncomplete;
	static ErrorId GroupLdapNoOwner;

	static ErrorId GroupsData;
	static ErrorId GroupsDataVerbose;

	static ErrorId HaveData;

	static ErrorId IntegAlreadyOpened;
	static ErrorId IntegIntoReadOnly;
	static ErrorId IntegXOpened;
	static ErrorId IntegBadAncestor;
	static ErrorId IntegBadBase;
	static ErrorId IntegBadAction;
	static ErrorId IntegBadClient;
	static ErrorId IntegBadUser;
	static ErrorId IntegCantAdd;
	static ErrorId IntegCantModify;
	static ErrorId IntegMustSync;
	static ErrorId IntegOpenOkayBase;
	static ErrorId IntegSyncBranch;
	static ErrorId IntegSyncIntegBase;
	static ErrorId IntegNotHandled;
	static ErrorId IntegTooVirtual;
	static ErrorId IntegIncompatable;
	static ErrorId IntegMovedUnmapped;
	static ErrorId IntegMovedNoAccess;
	static ErrorId IntegMovedOutScope;
	static ErrorId IntegMovedNoFrom;
	static ErrorId IntegMovedNoFromS;
	static ErrorId IntegBaselessMove;
	static ErrorId IntegPreviewResolve;
	static ErrorId IntegPreviewResolveMove;
	static ErrorId IntegPreviewResolved;

	static ErrorId IntegedData;

	static ErrorId JobSave;
	static ErrorId JobNoChange;
	static ErrorId JobDelete;
	static ErrorId JobDescription;
	static ErrorId JobDeleteHasFix;

	static ErrorId LabelSyncAdd;
	static ErrorId LabelSyncDelete;
	static ErrorId LabelSyncReplace;
	static ErrorId LabelSyncUpdate;

	static ErrorId LdapConfBadPerms;
	static ErrorId LdapConfBadOwner;
	static ErrorId BadPortNumber;
	static ErrorId LdapData;
	static ErrorId LdapSave;
	static ErrorId LdapNoChange;
	static ErrorId LdapDelete;
	static ErrorId NoSuchLdap;
	static ErrorId LdapRequiredField0;
	static ErrorId LdapRequiredField1;
	static ErrorId LdapRequiredField2;
	static ErrorId LdapRequiredField3;


	static ErrorId LicenseSave;
	static ErrorId LicenseNoChange;

	static ErrorId LockBadUnicode;
	static ErrorId LockUtf16NotSupp;
	static ErrorId LockSuccess;
	static ErrorId LockAlready;
	static ErrorId LockAlreadyOther;
	static ErrorId LockAlreadyCommit;
	static ErrorId LockNoPermission;

	static ErrorId UnLockSuccess;
	static ErrorId UnLockAlready;
	static ErrorId UnLockAlreadyOther;

	static ErrorId LoggerData;

	static ErrorId MergeBadBase;

	static ErrorId MoveSuccess;
	static ErrorId MoveBadAction;
	static ErrorId MoveDeleted;
	static ErrorId MoveExists;
	static ErrorId MoveMisMatch;
	static ErrorId MoveNoMatch;
	static ErrorId MoveNoInteg;
	static ErrorId MoveReadOnly;
	static ErrorId MoveNotSynced;
	static ErrorId MoveNotResolved;
	static ErrorId MoveNeedForce;

	static ErrorId OpenAlready;
	static ErrorId OpenReadOnly;
	static ErrorId OpenXOpened;
	static ErrorId OpenXOpenedFailed;
	static ErrorId OpenBadAction;
	static ErrorId OpenBadClient;
	static ErrorId OpenBadUser;
	static ErrorId OpenBadChange;
	static ErrorId OpenBadType;
	static ErrorId OpenReOpen;
	static ErrorId OpenUpToDate;
	static ErrorId OpenCantExists;
	static ErrorId OpenCantDeleted;
	static ErrorId OpenCantMissing;
	static ErrorId OpenSuccess;
	static ErrorId OpenMustResolve;
	static ErrorId OpenIsLocked;
	static ErrorId OpenIsOpened;
	static ErrorId OpenWarnExists;
	static ErrorId OpenWarnDeleted;
	static ErrorId OpenWarnMoved;
	static ErrorId OpenWarnOpenStream;
	static ErrorId OpenWarnOpenNotStream;
	static ErrorId OpenWarnFileNotMapped;
	static ErrorId OpenWarnChangeMap;
	static ErrorId OpenOtherDepot;
	static ErrorId OpenTaskNotMapped;
	static ErrorId OpenExclOrphaned;
	static ErrorId OpenExclLocked;
	static ErrorId OpenExclOther;
	static ErrorId OpenAttrRO;
	static ErrorId OpenHasResolve;
	static ErrorId OpenWarnReaddMoved;

	static ErrorId PopulateDesc;
	static ErrorId PopulateTargetExists;
	static ErrorId PopulateTargetMixed;
	static ErrorId PopulateInvalidStream;
	static ErrorId PopulateMultipleStreams;

	static ErrorId ReconcileBadName;
	static ErrorId ReconcileNeedForce;
	static ErrorId StatusSuccess;
	static ErrorId StatusOpened;

	static ErrorId OpenedData;
	static ErrorId OpenedOther;
	static ErrorId OpenedLocked;
	static ErrorId OpenedOtherLocked;
	static ErrorId OpenedXData;
	static ErrorId OpenedXOther;

	static ErrorId OpenedDataS;
	static ErrorId OpenedOtherS;
	static ErrorId OpenedLockedS;
	static ErrorId OpenedOtherLockedS;
	static ErrorId OpenedXDataS;
	static ErrorId OpenedXOtherS;

	static ErrorId ProtectSave;
	static ErrorId ProtectNoChange;

	static ErrorId ProtectsData;
	static ErrorId ProtectsMaxData;
	static ErrorId ProtectsEmpty;
	static ErrorId ProtectsNoSuper;

	static ErrorId PurgeSnapData;
	static ErrorId PurgeDeleted;
	static ErrorId PurgeCheck;
	static ErrorId PurgeNoRecords;
	static ErrorId PurgeData;
	static ErrorId PurgeActiveTask;

	static ErrorId ReleaseHasPending;
	static ErrorId ReleaseAbandon;
	static ErrorId ReleaseClear;
	static ErrorId ReleaseDelete;
	static ErrorId ReleaseRevert;
	static ErrorId ReleaseUnlockAbandon;
	static ErrorId ReleaseUnlockClear;
	static ErrorId ReleaseUnlockDelete;
	static ErrorId ReleaseUnlockRevert;
	static ErrorId ReleaseNotOwner;
	static ErrorId ReleaseHasMoved;

	static ErrorId ReopenData;
	static ErrorId ReopenDataNoChange;
	static ErrorId ReopenCharSet;
	static ErrorId ReopenBadType;

	static ErrorId ResolveAction;
	static ErrorId ResolveActionMove;
	static ErrorId ResolveDelete;
	static ErrorId ResolveShelveDelete;
	static ErrorId ResolveDoBranch;
	static ErrorId ResolveDoBranchActionT;
	static ErrorId ResolveDoBranchActionY;
	static ErrorId ResolveDoDelete;
	static ErrorId ResolveDoDeleteActionT;
	static ErrorId ResolveDoDeleteActionY;
	static ErrorId ResolveFiletype;
	static ErrorId ResolveFiletypeAction;
	static ErrorId ResolveMove;
	static ErrorId ResolveMoveAction;
	static ErrorId ResolveTrait;
	static ErrorId ResolveTraitActionT;
	static ErrorId ResolveTraitActionY;
	static ErrorId ResolveTraitActionM;
	static ErrorId ResolveCharset;
	static ErrorId ResolveCharsetActionT;
	static ErrorId ResolveCharsetActionY;
	static ErrorId Resolve2WayRaw;
	static ErrorId Resolve3WayRaw;
	static ErrorId Resolve3WayText;
	static ErrorId Resolve3WayTextBase;
	static ErrorId ResolveMustIgnore;

	static ErrorId ResolvedAction;
	static ErrorId ResolvedActionMove;
	static ErrorId ResolvedData;
	static ErrorId ResolvedDataBase;

	static ErrorId RetypeData;

	static ErrorId ReviewData;

	static ErrorId ReviewsData;

	static ErrorId SpecSave;
	static ErrorId SpecNoChange;
	static ErrorId SpecDeleted;
	static ErrorId SpecNotDefined;

	static ErrorId ShelveCantUpdate;
	static ErrorId ShelveLocked;
	static ErrorId ShelveUnlocked;
	static ErrorId ShelveIncompatible;
	static ErrorId ShelveMaxFiles;
	static ErrorId ShelveNoPerm;
	static ErrorId ShelveNeedsResolve;
	static ErrorId ShelveOpenResolves;

	static ErrorId StreamDepthErr;
	static ErrorId StreamDoubleSlash;
	static ErrorId StreamEqDepot;
	static ErrorId StreamNested;
	static ErrorId StreamNotOwner;
	static ErrorId StreamTargetExists;
	static ErrorId StreamRootErr;
	static ErrorId StreamEndSlash;
	static ErrorId StreamsData;
	static ErrorId StreamVsDomains;
	static ErrorId StreamVsTemplate;
	static ErrorId StreamPathRooted;
	static ErrorId StreamPathSlash;
	static ErrorId StreamHasChildren;
	static ErrorId StreamHasClients;
	static ErrorId StreamOwnerReq;
	static ErrorId StreamOpened;
	static ErrorId StreamIsOpen;
	static ErrorId StreamReverted;
	static ErrorId StreamShelveMismatch;
	static ErrorId StreamNotOpen;
	static ErrorId StreamSwitchOpen;
	static ErrorId StreamMustResolve;
	static ErrorId StreamShelved;
	static ErrorId StreamUnshelved;
	static ErrorId StreamOpenBadType;
	static ErrorId StreamTaskAndImport;
	static ErrorId ClientNoSwitch;

	static ErrorId StreamResolve;
	static ErrorId StreamResolved;
	static ErrorId StreamResolveField;
	static ErrorId StreamResolveAction;

	static ErrorId SubmitUpToDate;
	static ErrorId SubmitWasAdd;
	static ErrorId SubmitWasDelete;
	static ErrorId SubmitWasDeleteCanReadd;
	static ErrorId SubmitMustResolve;
	static ErrorId SubmitTransfer;
	static ErrorId SubmitRefresh;
	static ErrorId SubmitReverted;
	static ErrorId SubmitMovedToDefault;
	static ErrorId SubmitResolve;
	static ErrorId SubmitNewResolve;
	static ErrorId SubmitChanges;
	static ErrorId ShelvedHasWorking;

	static ErrorId SyncAdd;
	static ErrorId SyncDelete;
	static ErrorId SyncReplace;
	static ErrorId SyncCantDelete;
	static ErrorId SyncCantReplace;
	static ErrorId SyncUpdate;
	static ErrorId SyncRefresh;
	static ErrorId SyncIntegUpdate;
	static ErrorId SyncIntegDelete;
	static ErrorId SyncIntegBackwards;
	static ErrorId SyncUptodate;
	static ErrorId SyncResolve;
	static ErrorId SyncCantPublishIsOpen;
	static ErrorId SyncCantPublishOnHave;
	static ErrorId SyncMissingMoveSource;
	static ErrorId SyncNotSafeAdd;
	static ErrorId SyncNotSafeDelete;
	static ErrorId SyncNotSafeUpdate;
	static ErrorId SyncNotSafeReplace;
	static ErrorId SyncIndexOutOfBounds;

	static ErrorId TangentBadSource;
	static ErrorId TangentBlockedDepot;
	static ErrorId TangentBranchedFile;
	static ErrorId TangentMovedFile;

	static ErrorId TraitCleared;
	static ErrorId TraitNotSet;
	static ErrorId TraitSet;
	static ErrorId TraitIsOpen;

	static ErrorId TriggerSave;
	static ErrorId TriggerNoChange;
	static ErrorId TriggerNoDepotFile;
	static ErrorId TriggerNoArchiveType;

	static ErrorId TypeMapSave;
	static ErrorId TypeMapNoChange;

	static ErrorId UnshelveBadAction;
	static ErrorId UnshelveBadAdd;
	static ErrorId UnshelveBadEdit;
	static ErrorId UnshelveBadClientView;
	static ErrorId UnshelveSuccess;
	static ErrorId UnshelveIsLocked;
	static ErrorId UnshelveResolve;
	static ErrorId UnshelveNotTask;
	static ErrorId UnshelveFromRemote;
	static ErrorId UnshelveBadChangeView;

	static ErrorId UserSave;
	static ErrorId UserNoChange;
	static ErrorId UserNotExist;
	static ErrorId UserCantDelete;
	static ErrorId UserDelete;

	static ErrorId UsersData;
	static ErrorId UsersDataLong;

	static ErrorId VerifyData;
	static ErrorId VerifyDataProblem;

	static ErrorId WhereData;

	static ErrorId ExARCHIVES;
	static ErrorId ExCHANGE;
	static ErrorId ExSTREAM;
	static ErrorId ExUSER;

	static ErrorId ExSTREAMOPEN;

	static ErrorId ExVIEW;
	static ErrorId ExVIEW2;
	static ErrorId ExSVIEW;
	static ErrorId ExTVIEW;
	static ErrorId ExBVIEW;

	static ErrorId ExPROTECT;
	static ErrorId ExPROTECT2;
	static ErrorId ExPROTNAME;
	static ErrorId ExPROTNAME2;

	static ErrorId ExINTEGPEND;
	static ErrorId ExINTEGPERM;
	static ErrorId ExINTEGMOVEDEL;

	static ErrorId ExDIFF;
	static ErrorId ExDIFFPre101;
	static ErrorId ExDIGESTED;
	static ErrorId ExUNLOADED;
	static ErrorId ExFILE;
	static ErrorId ExHAVE;
	static ErrorId ExINTEGED;
	static ErrorId ExLABEL;
	static ErrorId ExLABSYNC;
	static ErrorId ExOPENALL;
	static ErrorId ExOPENCHANGE;
	static ErrorId ExOPENCLIENT;
	static ErrorId ExOPENNOTEDIT;
	static ErrorId ExOPENNOTEDITADD;
	static ErrorId ExOPENDFLT;
	static ErrorId ExRESOLVED;
	static ErrorId ExTORESOLVE;
	static ErrorId ExTORETYPE;
	static ErrorId ExUPTODATE;
	static ErrorId ExTOUNSHELVE;
	static ErrorId ExTORECONCILE;
	static ErrorId ExUNLOCKCHANGE;

	static ErrorId ExABOVECHANGE;
	static ErrorId ExABOVEDATE;
	static ErrorId ExABOVEHAVE;
	static ErrorId ExABOVELABEL;
	static ErrorId ExABOVEREVISION;

	static ErrorId ExATCHANGE;
	static ErrorId ExATDATE;
	static ErrorId ExATHAVE;
	static ErrorId ExATLABEL;
	static ErrorId ExATREVISION;
	static ErrorId ExATACTION;
	static ErrorId ExATTEXT;

	static ErrorId ExBELOWCHANGE;
	static ErrorId ExBELOWDATE;
	static ErrorId ExBELOWHAVE;
	static ErrorId ExBELOWLABEL;
	static ErrorId ExBELOWREVISION;

	static ErrorId OpenWarnPurged;
	static ErrorId MonitorData;
	static ErrorId MonitorClear;
	static ErrorId MonitorPause;
	static ErrorId MonitorResume;
	static ErrorId MonitorTerminate;
	static ErrorId MonitorCantTerminate;

	static ErrorId UnknownReplicationMode;
	static ErrorId UnknownReplicationTarget;

	static ErrorId AdminSpecData;
	static ErrorId AdminPasswordData;
	static ErrorId AdminSetLdapUserData;
	static ErrorId AdminSetLdapUserNoSuper;

	static ErrorId NotUnderRoot;
	static ErrorId NotUnderClient;

	static ErrorId CommandCancelled;
	static ErrorId MaxResults;
	static ErrorId MaxScanRows;
	static ErrorId MaxLockTime;

	static ErrorId AdminLockDataEx;
	static ErrorId AdminLockDataSh;

	static ErrorId DiskSpaceMinimum;
	static ErrorId DiskSpaceEstimated;

	static ErrorId JoinMax1TooSmall;

	static ErrorId LocWild;
	static ErrorId EmbWild;
	static ErrorId EmbSpecChar;
	static ErrorId PosWild;

	static ErrorId ResourceAlreadyLocked;
	static ErrorId NoSuchResource;

	static ErrorId ServersJnlAckData;

	static ErrorId NoSharedRevision;
	static ErrorId NoSharedHistory;
	static ErrorId ImportNoPermission;
	static ErrorId ImportNoDepot;
	static ErrorId ImportDepotReadOnly;
	static ErrorId UnrecognizedRevision;
	static ErrorId NoLazySource;
	static ErrorId ZipIntegMismatch;
	static ErrorId ZipBranchDidntMap;
	static ErrorId RevisionAlreadyPresent;
	static ErrorId SharedActionMismatch;
	static ErrorId SharedDigestMismatch;
	static ErrorId ImportedChange;
	static ErrorId ImportedFile;
	static ErrorId ImportedIntegration;
	static ErrorId ImportSkippedChange;
	static ErrorId ImportWouldAddChange;
	static ErrorId ImportSkippedFile;
	static ErrorId ImportSkippedInteg;
	static ErrorId ImportDanglingInteg;
	static ErrorId InvalidZipFormat;
	static ErrorId UnzipCouldntLock;
	static ErrorId UnzipNoSuchArchive;
	static ErrorId UnzipIsTaskStream;
	static ErrorId UnzipIsLocked;
	static ErrorId UnzipChangePresent;
	static ErrorId UnzipRevisionPresent;
	static ErrorId UnzipIntegrationPresent;
	static ErrorId UnzipArchiveUnknown;
	static ErrorId ResubmitNoFiles;
	static ErrorId ResubmitStreamClassic;
	static ErrorId ResubmitMultiStream;
	static ErrorId UnsubmittedChange;
	static ErrorId UnsubmittedRenamed;
	static ErrorId UnsubmitNotHead;
	static ErrorId UnsubmitNoTraits;
	static ErrorId UnsubmitOpened;
	static ErrorId UnsubmitArchived;
	static ErrorId UnsubmitTaskStream;
	static ErrorId UnsubmitNotSubmitted;
	static ErrorId UnsubmitEmptyChange;
	static ErrorId UnsubmitWrongUser;
	static ErrorId UnsubmitWrongClient;
	static ErrorId UnsubmitIntegrated;
	static ErrorId UnsubmitNoInteg;
	static ErrorId UnsubmitNoChanges;
	static ErrorId ChangeIdentityAlready;
	static ErrorId ReservedClientName;
	static ErrorId CannotChangeStorageType;
	static ErrorId ServerLocksOrder;
	static ErrorId RevChangedDuringPush;

	// Retired ErrorIds. We need to keep these so that clients 
	// built with newer apis can commnunicate with older servers 
	// still sending these.

	static ErrorId BadMaxScanRow; // DEPRECATED
	static ErrorId ErrorInSpec; // DEPRECATED
	static ErrorId JobName101; // DEPRECATED
	static ErrorId NoCodeZero; // DEPRECATED
	static ErrorId FixAddDefault; // DEPRECATED
	static ErrorId FixesDataDefault; // DEPRECATED
	static ErrorId InfoUnknownDomain; // DEPRECATED
	static ErrorId InfoDomain; // DEPRECATED
	static ErrorId EditSpecSave; // DEPRECATED
	static ErrorId EditSpecNoChange; // DEPRECATED
	static ErrorId ExTOINTEG; // DEPRECATED
	static ErrorId IntegOpenOkay; // DEPRECATED
	static ErrorId IntegSyncDelete; //DEPRECATED
	static ErrorId NoNextRev; // replaced by NoPrevRev in 2011.1
} ;

