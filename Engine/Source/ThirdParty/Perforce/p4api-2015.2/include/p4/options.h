/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * Options::Parse() - parse command line options
 *
 *	The "opts" string list flags.  Each (single character) flag x
 *	can be followed by an optional modifier:
 *
 *		x.	- flag takes an argument (-xarg)
 *		x:	- flag takes an argument (-xarg or -x arg)
 *		x?	- flag takes an optional argument (--long=arg only)
 *		x+	- flag takes a flag and arg (-xyarg or -xy arg)
 *		x#	- flag takes a non-neg numeric arg (-xN or -x N)
 */

const int N_OPTS = 256;

enum OptFlag {
	// Bitwise selectors

	OPT_ONE = 0x01,		// exactly one
	OPT_TWO = 0x02,		// exactly two
	OPT_THREE = 0x04,	// exactly three
	OPT_MORE = 0x08,	// more than two
	OPT_NONE = 0x10,	// require none
	OPT_MAKEONE = 0x20,	// if none, make one that points to null

	// combos of the above

	OPT_OPT = 0x11,		// NONE, or ONE
	OPT_ANY = 0x1F,		// ONE, TWO, THREE, MORE, or NONE
	OPT_DEFAULT = 0x2F,	// ONE, TWO, THREE, MORE, or MAKEONE
	OPT_SOME = 0x0F		// ONE, TWO, THREE, or MORE
} ;

struct ErrorId;

class Options
{
    public:
			Options() { optc = 0; }

	enum Opt {

		// options which are used commonly, across many commands:

	                All            = 'a',
			Archive        = 'A',
	                Change         = 'c',
	                Delete         = 'd',
	                Depot          = 'D',
	                Expression     = 'e',
	                NoCaseExpr     = 'E',
	                Force          = 'f',
	                Filter         = 'F',
			Input          = 'i',
			JournalPrefix  = 'J',
	                Long           = 'l',
	                Max            = 'm',
	                Preview        = 'n',
			Output         = 'o',
			OutputFlags    = 'O',
			Port           = 'p',
			Parent         = 'P',
	                Quiet          = 'q',
	                Reverse        = 'r',
			Short          = 's',
			Stream         = 'S',
			Filetype       = 't',
			Tags           = 'T',
			User           = 'u',
	                Variable       = 'v',
	                Wipe           = 'w',
	                Compress       = 'z',

		// options which are relatively uncommon, but have existing
		// short-form versions:

			InfrequentShortFormOptions = 1000,

			Version        , // -V
			Client         , // -c client
			Shelf          , // -s shelf
			DiffFlags      , // -d<diff-flags>
			Inherited      , // -i on changes, filelog, etc.
			ClientName     , // -C client
			Charset        , // p4 -C charset
			CmdCharset     , // p4 -Q charset
			Help           , // -h for main programs
	                Batchsize      , // -b N
	                MessageType    , // 'p4 -s'
	                Xargs          , // 'p4 -x file'
			Exclusive      , // opened -x
			Directory      , // p4 -d dir
			Host           , // p4 -H host
			Password       , // -P password
			Retries        , // p4 -r retries
			Progress       , // p4 -I
			NoIgnore       , // add -I
			Downgrade      , // add -d
			Unload         , // -U for unloaded objects
			UnloadLimit    , // backup -u #
			CentralUsers   , // users -c
			ReplicaUsers   , // users -r
			Branch         , // unshelve -b
			FullBranch     , // branch -F
			SpecFixStatus  , // change -s, submit -s
			ChangeType     , // change -t
			ChangeUpdate   , // change -u
			Original       , // change -O, describe -O
			ChangeUser     , // change -U
			Template       , // client -t, label -t
			Switch         , // client -s
	                Temporary      , // client -x
	                Owner          , // group -a
	                Administrator  , // group -A
	                Global         , // label/labelsync/tag -g
	                GlobalLock     , // lock/opened -g
	                StreamType     , // stream -t
	                VirtualStream  , // stream -v
	                Brief          , // changes -L, filelog -L
	                ShowTime       , // changes -t, filelog -t
	                ChangeStatus   , // changes -s
	                Exists         , // files -e
	                Blocksize      , // sizes -b
			Shelved        , // sizes -S, describe -S
	                Summary        , // sizes -s
	                OmitLazy       , // sizes -z
	                Human1024      , // sizes -h
	                Human1000      , // sizes -H
	                LimitClient    , // list -C
	                LabelName      , // list -l
	                RunOnMaster    , // list -M
	                LeaveKeywords  , // print -k
	                OutputFile     , // print -o
	                Content        , // filelog -h
	                OmitPromoted   , // filelog -p
	                OmitMoved      , // filelog -1
			KeepClient     , // edit -k, delete -k, sync -k
			FileCharset    , // add/edit -Q
			Virtual        , // delete -v
			Generate       , // server -g
			Usage          , // license -u
			Job            , // fixes -j
			Increment      , // counter -i
			FixStatus      , // fix -s
			Replace        , // shelve -r
			ShelveOpts     , // shelve -a
			SubmitShelf    , // submit -e
			SubmitOpts     , // submit -f
			Reopen         , // submit -r
			Description    , // submit -d
			Tamper         , // submit -t
			Date           , // unload -d
			StreamName     , // unload -s, reload -s
			Unchanged      , // revert -a
			KeepHead       , // archive -h
			Purge          , // archive -p
			ForceText      , // archive -t
			BinaryAsText   , // -t on annotate, diff, diff2,
	                                 // grep, resolve, merge3, ...
			BypassFlow     , // -F on copy, interchanges, merge
			ShowChange     , // annotate -c
			FollowBranch   , // annotate -i
			FollowInteg    , // annotate -I
			SourceFile     , // -s on copy, integrate, merge,
			                 // interchanges, populate
			ResolveFlags   , // resolve -A<flags>
			AcceptFlags    , // resolve -a<flags>
			IntegFlags     , // integrate -R<flags>
			DeleteFlags    , // integrate -D<flags>
			RestrictFlags  , // fstat -R<flags>
			SortFlags      , // fstat -S<flags>
			ForceFlag      , // client -d -f -F<flags>
			UseList        , // sync -L, fstat -L
			Safe           , // sync -s
			Publish        , // sync -p
			IsGroup        , // groups -g
			IsUser         , // groups -u
			IsOwner        , // groups -o
			Verbose        , // groups -v
	                LineNumber     , // grep -n
			InvertMatch    , // grep -v
	                FilesWithMatches,// grep -l
	                FilesWithoutMatch,// grep -L
			NoMessages     , // grep -s
	                FixedStrings   , // grep -F
			BasicRegexp    , // grep -G
	                ExtendedRegexp , // grep -E
			PerlRegexp     , // grep -P
	                Regexp         , // grep -e
	                AfterContext   , // grep -A
			BeforeContext  , // grep -B
	                Context        , // grep -C
			IgnoreCase     , // grep -i
	                Repeat         , // pull -i
	                Backoff        , // pull -b
	                ArchiveData    , // pull -u
	                Status         , // pull -l
	                LocalJournal   , // pull -L
	                JournalPosition, // pull -j
	                PullServerid   , // pull -P
	                ExcludeTables  , // pull -T
	                File           , // pull -f
	                Revision       , // pull -r
	                Append         , // logappend -a, property -a
	                Sequence       , // logger -c
	                Counter        , // logger -t
	                HostName       , // login -h
	                Print          , // login -p
	                LoginStatus    , // login -s
	                StartPosition  , // logparse -s, logtail -s
	                Encoded        , // logparse -e
	                LogName        , // logtail -l
	                CompressCkp    , // p4 admin checkpoint -Z
	                SpecType       , // p4 admin updatespecdepot -s
	                MaxAccess      , // p4 protects -m
	                GroupName      , // p4 protects -g
	                ShowFiles      , // p4 interchanges -f
	                Name           , // attribute -n, property -n
	                Value          , // attribute -v, property -v
	                Propagating    , // attribute -p
	                OpenAdd        , // reconcile -a
	                OpenEdit       , // reconcile -e
	                OpenDelete     , // reconcile -d
	                UseModTime     , // reconcile -m
	                Local          , // reconcile -l
	                OutputBase     , // resolved -o
	                System         , // set -s
	                Service        , // set -S
	                Histogram      , // dbstat -h
	                TableNotUnlocked,// dbverify -U
	                TableName      , // dbverify -t
	                AllClients     , // lockstat -C
	                CheckSize      , // verify -s
	                Transfer       , // verify -t
	                Update         , // verify -u
	                Verify         , // verify -v
	                NoArchive      , // verify -X
	                Serverid       , // clients -s, labels -s
	                Unified        , // diff2 -u
	                PreviewNC      , // resolve -N
	                Estimates      , // sync -N/flush -N/update -N
	                Locked         , // unload -L
	                UnloadAll      , // unload -a
			KeepHave       , // integrate -h
			Yes            , // trust -y
			No             , // trust -n
			InputValue     , // trust -i
			Replacement    , // trust -r
			Rebuild        , // jobs -R
			Equal          , // fstat -e
			AttrPattern    , // fstat -A
			DiffListFlag   , // diff -s
			Arguments      , // monitor show -a
			Environment    , // monitor show -e
			TaskStatus     , // monitor show -s
			AllUsers       , // property -A
			Promote        , // shelve -p
			Test           , // ldap -t, ldaps -t
			Active         , // ldaps -A
			GroupMode      , // ldapsync -g
			Create         , // switch -c
			List           , // switch -l
			Mainline       , // switch -m
			MoveChanges    , // switch -r
			ReplicationStatus, // servers --replication-status, servers -J
	                DepotType     , // depot -t
	                Users         , // annotate -u

		// options which have only long-form option names go here:

			LongFormOnlyOptions = 2000,

	                NoRejournal    , // pull --no-rejournal
	                From           , // renameuser --from
	                To             , // renameuser --to
	                Parallel       , // sync --parallel
	                ParallelSubmit , // submit --parallel
	                InputFile      , // reload --input-file
	                PidFile        , // p4d --pid-file
	                NoRetransfer   , // submit --noretransfer
	                ForceNoRetransfer, // submit --forcenoretransfer
			DurableOnly    , // journalcopy --durable-only
			NonAcknowledging, // journalcopy --non-acknowledging
			BypassExclusiveLock, // open --bypass-exclusive-lock
			RetainLbrRevisions, // unzip --retain-lbr-revisions
			JavaProtocol   , // p4d -i --java
			PullBatch      , // pull -u --batch=N
			EnableDVCSTriggers, // unzip --enable-dvcs-triggers

			UnusedLastOption
	} ;

	void		Parse( int &argc, char **&argv, const char *opts, 
		    		int flag, const ErrorId &usage, Error *e );

	void		ParseLong( int &argc, char **&argv, const char *opts, 
		    		const int *longOpts,
		    		int flag, const ErrorId &usage, Error *e );

	void		Parse( int &argc, StrPtr *&argv, const char *opts, 
		    		int flag, const ErrorId &usage, Error *e );

	void		ParseLong( int &argc, StrPtr *&argv, const char *opts, 
		    		const int *longOpts,
		    		int flag, const ErrorId &usage, Error *e );

	StrPtr *	operator [](int opt) 
			{ return GetValue( opt, 0, 0 ); }

	StrPtr *	GetValue( int opt, int subopt )
			{ return GetValue( opt, 0, subopt ); }

	StrPtr *	GetValue( int opt, char flag2, int subopt );

	int		FormatOption( int i, Error *e );
	int		FormatOption( int i, StrBuf & f) const;
	int		HasOption( int i );
	void		GetOptionName( int i, StrBuf &sb );
	void		GetOptionValue( int i, StrBuf &sb );

    private:
	int 	optc;

	int	flags[ N_OPTS ];
	char	flags2[ N_OPTS ];
	StrRef	vals[ N_OPTS ];

	static struct OptionInfo {
	    const char *name;
	    int   optionCode;
	    int   shortForm;
	    int   valueType;
	    const ErrorId *help;
	} list[];
} ;

