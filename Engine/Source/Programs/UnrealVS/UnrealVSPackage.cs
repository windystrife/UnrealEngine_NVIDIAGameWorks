// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.ComponentModel.Design;
using System.Threading;
using Microsoft.Win32;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell.Interop;
using Microsoft.VisualStudio.OLE.Interop;
using Microsoft.VisualStudio.Shell;
using EnvDTE;
using EnvDTE80;
using Thread = System.Threading.Thread;

namespace UnrealVS
{
	// This attribute tells the PkgDef creation utility (CreatePkgDef.exe) that this class is a VS package.
	[PackageRegistration( UseManagedResourcesOnly = true )]

	// This attribute is used to register the informations needed to show the this package in the Help/About dialog of Visual Studio.
	[InstalledProductRegistration("#110", "#112", VersionString, IconResourceID = 400)]

	// Adds data for our interface elements defined in UnrealVS.vsct.  The name "Menus.ctmenu" here is arbitrary, but must
	// match the ItemGroup inside the UnrealVS.csproj MSBuild file (hand-typed.)
	[ProvideMenuResource( "Menus.ctmenu", 1 )]

	// Force the package to load whenever a solution exists
	[ProvideAutoLoad(UIContextGuids80.SolutionExists)]

	// Register the settings implementing class as providing support to UnrealVSPackage.
	//[ProvideProfile(typeof (ProfileManager), "UnrealVS", "UnrealVSPackage", 110, 113, false)]

	// GUID for this class.  This needs to match in quite a few places in the project.
	[Guid( GuidList.UnrealVSPackageString )]

	// This attribute registers a tool window exposed by this package.
	[ProvideToolWindow(typeof(BatchBuilderToolWindow))]

	// This attribute registers an options page for the package.
	[ProvideOptionPage(typeof(UnrealVsOptions), ExtensionName, "General", 101, 102, true)]

	/// <summary>
	/// UnrealVSPackage implements Package abstract class.  This is the main class that is registered
	/// with Visual Studio shell and serves as the entry point into our extension
	/// </summary>
	public sealed class UnrealVSPackage :
		Package,		// We inherit from Package which allows us to become a "plugin" within the Visual Studio shell
		IVsSolutionEvents,		// This interface allows us to register to be notified of events such as opening a project
		IVsUpdateSolutionEvents,// Allows us to register to be notified of events such as active config changes
		IVsSelectionEvents,		// Allows us to be notified when the startup project has changed to a different project
		IVsHierarchyEvents,		// Allows us to be notified when a hierarchy (the startup project) has had properties changed
		IDisposable
	{
		/** Constants */

		private const string VersionString = "v1.48";
		private const string UnrealSolutionFileNamePrefix = "UE4";
		private const string ExtensionName = "UnrealVS";
		private const string CommandLineOptionKey = ExtensionName + "CommandLineMRU";
		private const string BatchBuildSetsOptionKey = ExtensionName + "BatchBuildSetsV002";
		private readonly TimeSpan TickPeriod = TimeSpan.FromSeconds(1.0);

		/** Events */

		/// Called when a new startup project is set in Visual Studio
		public delegate void OnStartupProjectChangedDelegate(Project NewStartupProject);
		[System.Diagnostics.CodeAnalysis.SuppressMessage( "Microsoft.Design", "CA1009" )]
		public event OnStartupProjectChangedDelegate OnStartupProjectChanged;

		/// Called when a project is opened or created in Visual Studio
		public delegate void OnProjectOpenedDelegate(Project OpenedProject);
		[System.Diagnostics.CodeAnalysis.SuppressMessage( "Microsoft.Design", "CA1009" )]
		public event OnProjectOpenedDelegate OnProjectOpened;

		/// Called right before a project is closed
		public delegate void OnProjectClosedDelegate(Project ClosedProject);
		[System.Diagnostics.CodeAnalysis.SuppressMessage( "Microsoft.Design", "CA1009" )]
		public event OnProjectClosedDelegate OnProjectClosed;

		/// Called when the startup project is edited in Visual Studio
		public delegate void OnStartupProjectPropertyChangedDelegate(UInt32 itemid, Int32 propid, UInt32 flags);
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1009")]
		public event OnStartupProjectPropertyChangedDelegate OnStartupProjectPropertyChanged;

		/// Called right after a solution is opened
		public delegate void OnSolutionOpenedDelegate();
		[System.Diagnostics.CodeAnalysis.SuppressMessage( "Microsoft.Design", "CA1009" )]
		public event OnSolutionOpenedDelegate OnSolutionOpened;

		/// Called right before/after a solution is closed
		public delegate void OnSolutionClosedDelegate();
		[System.Diagnostics.CodeAnalysis.SuppressMessage( "Microsoft.Design", "CA1009" )]
		public event OnSolutionClosedDelegate OnSolutionClosing;
		[System.Diagnostics.CodeAnalysis.SuppressMessage( "Microsoft.Design", "CA1009" )]
		public event OnSolutionClosedDelegate OnSolutionClosed;

		/// Called when the active project config changes for any project
		public delegate void OnStartupProjectConfigChangedDelegate(Project Project);
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1009")]
		public event OnStartupProjectConfigChangedDelegate OnStartupProjectConfigChanged;

		/// Called when a build/update action begins
		public delegate void OnBuildBeginDelegate(out int Cancel);
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1009")]
		public event OnBuildBeginDelegate OnBuildBegin;

		/// Called when a build/update action completes
		public delegate void OnBuildDoneDelegate(bool bSucceeded, bool bModified, bool bWasCancelled);
		[System.Diagnostics.CodeAnalysis.SuppressMessage( "Microsoft.Design", "CA1009" )]
		public event OnBuildDoneDelegate OnBuildDone;

		/// Called when the UIContext changes
		public delegate void OnUIContextChangedDelegate(uint CmdUICookie, bool bActive);
		[System.Diagnostics.CodeAnalysis.SuppressMessage("Microsoft.Design", "CA1009")]
		public event OnUIContextChangedDelegate OnUIContextChanged;

		/** Public Fields & Properties */

		/// Returns singleton instance of UnrealVSPackage
		public static UnrealVSPackage Instance
		{
			get { return PrivateInstance; }
		}

		/// Visual Studio menu command service
		public IMenuCommandService MenuCommandService	{ get; private set; }

		/// Visual Studio solution build manager interface.  This is used to change the active startup
		/// Project, among other things.  We expose public access to the solution build manager through
		/// our singleton instance
		public IVsSolutionBuildManager2 SolutionBuildManager { get; private set; }


		/// Visual Studio solution "manager" interface.  We register with this to receive events
		/// about projects being added and such.  This needs to be cleaned up at shutdown.
		public IVsSolution2 SolutionManager { get; private set; }

		/// Our startup project selector component
		public StartupProjectSelector StartupProjectSelector { get; private set; }

		/// Our quick build component
		public QuickBuild QuickBuilder { get; private set; }

		/// Visual Studio shell selection manager interface.  Used to receive notifications about
		/// startup projects changes, among other things.
		public IVsMonitorSelection SelectionManager { get; private set; }

		/// Variable keeps track of whether a supported Unreal solution is loaded
		public bool IsUE4Loaded { get; private set; }

		/// Variable keeps track of the loaded solution
		private string _SolutionFilepath;
		public string SolutionFilepath { get { return _SolutionFilepath; } }

		/** Methods */

		/// <summary>
		/// Package constructor.  The package is being created but Visual Studio isn't fully initialized yet, so
		/// it's NOT SAFE to call into Visual Studio services from here.  Do that in Initialize() instead.
		/// </summary>
		public UnrealVSPackage()
		{
			// Register this key string so the package can save command line data to solution options files.
			// See OnLoadOptions() & OnSaveOptions()
			AddOptionKey(CommandLineOptionKey);
			AddOptionKey(BatchBuildSetsOptionKey);

			// Setup singleton instance
			PrivateInstance = this;

			Logging.Initialize(ExtensionName, VersionString);
			Logging.WriteLine("Loading UnrealVS extension package...");
		}


		/// <summary>
		/// Initializes the package right after it's been "sited" into the fully-initialized Visual Studio IDE.
		/// </summary>
		protected override void Initialize()
		{
			Logging.WriteLine("Initializing UnrealVS extension...");

			// Grab the MenuCommandService
			MenuCommandService = GetService( typeof( IMenuCommandService ) ) as OleMenuCommandService;
			
			// Get access to Visual Studio's DTE object.  This object has various hooks into the Visual Studio
			// shell that are useful for writing extensions.
			DTE = (DTE)GetGlobalService( typeof( DTE ) );
			Logging.WriteLine("DTE version " + DTE.Version);

			// Get selection manager and register to receive events
			SelectionManager =
				ServiceProvider.GlobalProvider.GetService(typeof (SVsShellMonitorSelection)) as IVsMonitorSelection;
			SelectionManager.AdviseSelectionEvents( this, out SelectionEventsHandle );

			// Get solution and register to receive events
			SolutionManager = ServiceProvider.GlobalProvider.GetService( typeof( SVsSolution ) ) as IVsSolution2;
			UpdateUnrealLoadedStatus();
			SolutionManager.AdviseSolutionEvents( this, out SolutionEventsHandle );

			// Grab the solution build manager.  We need this in order to change certain things about the Visual
			// Studio environment, like what the active startup project is
			// Get solution build manager
			SolutionBuildManager =
				ServiceProvider.GlobalProvider.GetService(typeof (SVsSolutionBuildManager)) as IVsSolutionBuildManager2;
			SolutionBuildManager.AdviseUpdateSolutionEvents(this, out UpdateSolutionEventsHandle);

			// Create our command-line editor
			CommandLineEditor = new CommandLineEditor();

			// Create our startup project selector
			StartupProjectSelector = new StartupProjectSelector();

			// Create 'BuildStartupProject' instance
			BuildStartupProject = new BuildStartupProject();

			// Create 'CompileSingleFile' instance
			CompileSingleFile = new CompileSingleFile();

			// Create 'GenerateProjectFiles' tools
			GenerateProjectFiles = new GenerateProjectFiles();

			// Create Batch Builder tools
			BatchBuilder = new BatchBuilder();

			// Create the project menu quick builder
			QuickBuilder = new QuickBuild();

			// Call parent implementation
			base.Initialize();

			if (DTE.Solution.IsOpen)
			{
				StartTicker();
			}
		}

		private void StartTicker()
		{
			// Create a "ticker" on a background thread that ticks the package on the UI thread
			Interlocked.Exchange(ref bCancelTicker, 0);
			Ticker = new Thread(TickAsyncMain);
			Ticker.Priority = ThreadPriority.Lowest;
			Ticker.Start();			
		}

		private void StopTicker()
		{
			if (bCancelTicker == 0)
			{
				Interlocked.Exchange(ref bCancelTicker, 1);
			}
		}

		/// <summary>
		/// Tick loop on worker thread
		/// </summary>
		private void TickAsyncMain()
		{
			try
			{
				while (true)
				{
					if (bCancelTicker != 0) return; 
					ThreadHelper.Generic.BeginInvoke(Tick);

					if (bCancelTicker != 0) return; 
					Thread.Sleep(TickPeriod);
				}
			}
			catch (ThreadAbortException)
			{
			}
		}

		/// <summary>
		/// Tick function on main UI thread
		/// </summary>
		private void Tick()
		{
			BatchBuilder.Tick();
		}

		/// <summary>
		/// Implementation from IDisposable
		/// </summary>
		public void Dispose()
		{
			Dispose(true);
		}

		/// IDispose pattern lets us clean up our stuff!
		protected override void Dispose( bool disposing )
		{
			if (Ticker != null && Ticker.IsAlive)
			{
				Thread.Sleep(TickPeriod + TickPeriod);
				if (Ticker.IsAlive)
				{
					Logging.WriteLine("WARNING: Force aborting Ticker thread");
					Ticker.Abort();
				}
			}

			base.Dispose(disposing);

			// Clean up singleton instance
			PrivateInstance = null;

			CommandLineEditor = null;
			StartupProjectSelector = null;
			BatchBuilder = null;
			QuickBuilder = null;

			if(CompileSingleFile != null)
			{
				CompileSingleFile.Dispose();
				CompileSingleFile = null;
			}
			
			// No longer want solution events
			if( SolutionEventsHandle != 0 )
			{
				SolutionManager.UnadviseSolutionEvents( SolutionEventsHandle );
				SolutionEventsHandle = 0;
			}
			SolutionManager = null;

			// No longer want selection events
			if( SelectionEventsHandle != 0 )
			{
				SelectionManager.UnadviseSelectionEvents( SelectionEventsHandle );
				SelectionEventsHandle = 0;	
			}
			SelectionManager = null;

			// No longer want update solution events
			if (UpdateSolutionEventsHandle != 0)
			{
				SolutionBuildManager.UnadviseUpdateSolutionEvents(UpdateSolutionEventsHandle);
				UpdateSolutionEventsHandle = 0;
			}
			SolutionBuildManager = null;

			Logging.WriteLine("Closing UnrealVS extension");
			Logging.Close();
		}


		/// Visual Studio shell DTE interface
		public DTE DTE
		{
			get;
			private set;
		}

		/// Visual Studio shell DTE2 interface
		private DTE2 _DTE2;
		public DTE2 DTE2
		{
			get
			{
				if (_DTE2 == null)
				{
					// Get the interface when first used.
					// This method fails during the Initialize() of the 
					// package due to the strange method it requires.
					_DTE2 = GetDTE2ForCurrentInstance(DTE);
				}
				return _DTE2;
			}
		}

		/// The package's options page
		public UnrealVsOptions OptionsPage
		{
			get { return (UnrealVsOptions) GetDialogPage(typeof (UnrealVsOptions)); }
		}

		/// <summary>
		/// Launches a program
		/// </summary>
		/// <param name="ProgramFile">Path to the program to run</param>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="OnExit">Optional callback whent he program exits</param>
		/// <param name="OutputHandler">If supplied, std-out and std-error will be redirected to this function and no shell window will be created</param>
		/// <returns>The newly-created process, or null if it wasn't able to start.  Exceptions are swallowed, but a debug output string is emitted on errors</returns>
		public System.Diagnostics.Process LaunchProgram( string ProgramFile, string Arguments, EventHandler OnExit = null, DataReceivedEventHandler OutputHandler = null, bool bWaitForCompletion=false )
		{
			// Create the action's process.
			ProcessStartInfo ActionStartInfo = new ProcessStartInfo();
			ActionStartInfo.FileName = ProgramFile;
			ActionStartInfo.Arguments = Arguments;

			if( OutputHandler != null )
			{
				ActionStartInfo.RedirectStandardInput = true;
				ActionStartInfo.RedirectStandardOutput = true;
				ActionStartInfo.RedirectStandardError = true;

				// True to use a DOS box to run the program in, otherwise false to run directly.  In order to redirect
				// output, UseShellExecute must be disabled
				ActionStartInfo.UseShellExecute = false;

				// Don't show the DOS box, since we're redirecting everything
				ActionStartInfo.CreateNoWindow = true;
			}


			Logging.WriteLine(String.Format("Executing: {0} {1}", ActionStartInfo.FileName, ActionStartInfo.Arguments));

			System.Diagnostics.Process ActionProcess;

			try
			{
				ActionProcess = new System.Diagnostics.Process();
				ActionProcess.StartInfo = ActionStartInfo;

				if( OnExit != null )
				{
					ActionProcess.EnableRaisingEvents = true;
					ActionProcess.Exited += OnExit;
				}

				if( ActionStartInfo.RedirectStandardOutput )
				{
					ActionProcess.EnableRaisingEvents = true;
					ActionProcess.OutputDataReceived += OutputHandler;
					ActionProcess.ErrorDataReceived += OutputHandler;
				}

				// Launch the program
				ActionProcess.Start();

				if( ActionStartInfo.RedirectStandardOutput )
				{
					ActionProcess.BeginOutputReadLine();
					ActionProcess.BeginErrorReadLine();
				}

				if (bWaitForCompletion)
				{
					while ((ActionProcess != null) && (!ActionProcess.HasExited))
					{
						ActionProcess.WaitForExit(50);
					}
				}
			}
			catch( Exception Ex)
			{
				// Couldn't launch program
				Logging.WriteLine("Couldn't launch program: " + ActionStartInfo.FileName);
				Logging.WriteLine("Exception: " + Ex.Message);
				ActionProcess = null;
			}

			return ActionProcess;
		}


		/// <summary>
		/// Gets a Visual Studio pane to output text to, or creates one if not visible.  Does not bring the pane to front (you can call Activate() to do that.)
		/// </summary>
		/// <returns>The pane to output to, or null on error</returns>
		public IVsOutputWindowPane GetOutputPane()
		{
			return GetOutputPane(VSConstants.OutputWindowPaneGuid.BuildOutputPane_guid, "Build");
		}

		public IEnumerable<string> GetLoadedProjectPaths()
		{
			return LoadedProjectPaths;
		}

		/// <summary>
		/// Overrides Package.OnLoadOptions()
		/// Invoked by the package class when there are options to be read out of the solution file.
		/// </summary>
		/// <param name="key">The name of the option key to load.</param>
		/// <param name="stream">The stream to load the option data from.</param>
		protected override void OnLoadOptions(string key, Stream stream)
		{
			try
			{
				if (0 == string.Compare(key, CommandLineOptionKey))
				{
					Logging.WriteLine("Restoring CommandLineEditor options");
					CommandLineEditor.LoadOptions(stream);
				}
				else if (0 == string.Compare(key, BatchBuildSetsOptionKey))
				{
					Logging.WriteLine("Restoring BatchBuilder options");
					BatchBuilder.LoadOptions(stream);
				}
			}
			catch (Exception Ex)
			{
				// Couldn't load options
				Exception AppEx = new ApplicationException("OnLoadOptions() failed with key " + key, Ex);
				Logging.WriteLine(AppEx.ToString());
				throw AppEx;
			}
		}


		/// <summary>
		/// Overrides Package.OnSaveOptions()
		/// Invoked by the Package class when there are options to be saved to the solution file.
		/// </summary>
		/// <param name="key">The name of the option key to save.</param>
		/// <param name="stream">The stream to save the option data to.</param>
		protected override void OnSaveOptions(string key, Stream stream)
		{
			try
			{
				if (0 == string.Compare(key, CommandLineOptionKey))
				{
					Logging.WriteLine("Saving CommandLineEditor options");
					CommandLineEditor.SaveOptions(stream);
				}
				else if (0 == string.Compare(key, BatchBuildSetsOptionKey))
				{
					Logging.WriteLine("Saving BatchBuilder options");
					BatchBuilder.SaveOptions(stream);
				}
			}
			catch (Exception Ex)
			{
				// Couldn't save options
				Exception AppEx = new ApplicationException("OnSaveOptions() failed with key " + key, Ex);
				Logging.WriteLine(AppEx.ToString());
				throw AppEx;
			}
		}


		///
		/// IVsSolutionEvents implementation
		///

		int IVsSolutionEvents.OnAfterCloseSolution( object pUnkReserved )
		{
			UpdateUnrealLoadedStatus();

			if (OnSolutionClosed != null)
			{
				OnSolutionClosed();
			}

			return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnAfterLoadProject( IVsHierarchy pStubHierarchy, IVsHierarchy pRealHierarchy )
		{
			return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnAfterOpenProject( IVsHierarchy pHierarchy, int fAdded )
		{
			// This function is called after a Visual Studio project is opened (or a new project is created.)

			// Get the actual Project object from the IVsHierarchy object that was supplied
			var OpenedProject = Utils.HierarchyObjectToProject( pHierarchy );
			Utils.OnProjectListChanged();
			if (OpenedProject != null && OnProjectOpened != null)
			{
				LoadedProjectPaths.Add(OpenedProject.FullName);
                OnProjectOpened(OpenedProject);
            }

		    return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnAfterOpenSolution( object pUnkReserved, int fNewSolution )
		{
			UpdateUnrealLoadedStatus();

			StartTicker();

            if (OnSolutionOpened != null)
            {
                OnSolutionOpened();
            }

		    return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnBeforeCloseProject( IVsHierarchy pHierarchy, int fRemoved )
		{
			// This function is called after a Visual Studio project is closed

			// Get the actual Project object from the IVsHierarchy object that was supplied
			var ClosedProject = Utils.HierarchyObjectToProject( pHierarchy );
			if (ClosedProject != null && OnProjectClosed != null)
            {
				LoadedProjectPaths.Remove(ClosedProject.FullName);
                OnProjectClosed(ClosedProject);
            }

			return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnBeforeCloseSolution( object pUnkReserved )
		{
			StopTicker();

            if (OnSolutionClosing != null)
            {
                OnSolutionClosing();
            }

			return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnBeforeUnloadProject( IVsHierarchy pRealHierarchy, IVsHierarchy pStubHierarchy )
		{
			return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnQueryCloseProject( IVsHierarchy pHierarchy, int fRemoving, ref int pfCancel )
		{
			return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnQueryCloseSolution( object pUnkReserved, ref int pfCancel )
		{
			if (BatchBuilder.IsBusy)
			{
				pfCancel = 1;
			}

			return VSConstants.S_OK;
		}

		int IVsSolutionEvents.OnQueryUnloadProject( IVsHierarchy pRealHierarchy, ref int pfCancel )
		{
			return VSConstants.S_OK;
		}


		///
		/// IVsSelectionEvents implementation
		///

		int IVsSelectionEvents.OnCmdUIContextChanged( uint dwCmdUICookie, int fActive )
		{
			if (OnUIContextChanged != null)
			{
				OnUIContextChanged(dwCmdUICookie, fActive != 0);
			}

			return VSConstants.S_OK;
		}

		int IVsSelectionEvents.OnElementValueChanged( uint elementid, object varValueOld, object varValueNew )
		{
			// This function is called when selection changes in various Visual Studio tool windows
			// and sub-systems.

			// Handle startup project changes
	        if( elementid == (uint)VSConstants.VSSELELEMID.SEID_StartupProject )
            {
				// If we are registered to a project hierarchy for events, unregister
				var OldStartupProjectHierarchy = (IVsHierarchy)varValueOld;
				if (OldStartupProjectHierarchy != null && ProjectHierarchyEventsHandle != 0)
				{
					OldStartupProjectHierarchy.UnadviseHierarchyEvents(ProjectHierarchyEventsHandle);
					ProjectHierarchyEventsHandle = 0;
				}

				Project NewStartupProject = null;

				// Incoming hierarchy object could be null (if no startup project is set yet, or during shutdown.)
				var NewStartupProjectHierarchy = (IVsHierarchy)varValueNew;
				if( NewStartupProjectHierarchy != null )
				{
					// Get the actual Project object from the IVsHierarchy object that was supplied
					NewStartupProject = Utils.HierarchyObjectToProject( NewStartupProjectHierarchy );

					if (NewStartupProject != null)
					{
						// Register for events from the project
						NewStartupProjectHierarchy.AdviseHierarchyEvents(this, out ProjectHierarchyEventsHandle);
					}
				}

				if (NewStartupProject != null)
				{
					OnStartupProjectChanged(NewStartupProject);
				}
            }

			return VSConstants.S_OK;
		}

		int IVsSelectionEvents.OnSelectionChanged(IVsHierarchy pHierOld, uint itemidOld, IVsMultiItemSelect pMISOld,
		                                          ISelectionContainer pSCOld, IVsHierarchy pHierNew, uint itemidNew,
		                                          IVsMultiItemSelect pMISNew, ISelectionContainer pSCNew)
		{
    		return VSConstants.S_OK;
		}


		// IVsHierarchyEvents Interface

		Int32 IVsHierarchyEvents.OnItemAdded(UInt32 itemidParent, UInt32 itemidSiblingPrev, UInt32 itemidAdded)
		{
			return VSConstants.S_OK;
		}

		Int32 IVsHierarchyEvents.OnPropertyChanged(UInt32 itemid, Int32 propid, UInt32 flags)
		{
			if (OnStartupProjectPropertyChanged != null)
			{
				OnStartupProjectPropertyChanged(itemid, propid, flags);
			}
			return VSConstants.S_OK;
		}

		Int32 IVsHierarchyEvents.OnItemsAppended(UInt32 itemidParent)
		{
			return VSConstants.S_OK;
		}

		Int32 IVsHierarchyEvents.OnItemDeleted(UInt32 itemid)
		{
			return VSConstants.S_OK;
		}

		Int32 IVsHierarchyEvents.OnInvalidateItems(UInt32 itemidParent)
		{
			return VSConstants.S_OK;
		}

		Int32 IVsHierarchyEvents.OnInvalidateIcon(IntPtr hicon)
		{
			return VSConstants.S_OK;
		}

		// IVsUpdateSolutionEvents Interface

		public int UpdateSolution_Begin(ref int pfCancelUpdate)
		{
			if (OnBuildBegin != null)
			{
				OnBuildBegin(out pfCancelUpdate);
			}
			return VSConstants.S_OK;
		}

		public int UpdateSolution_Done(int fSucceeded, int fModified, int fCancelCommand)
		{
			if (OnBuildDone != null)
			{
				OnBuildDone(fSucceeded != 0, fModified != 0, fCancelCommand != 0);
			}
			return VSConstants.S_OK;
		}

		public int UpdateSolution_StartUpdate(ref int pfCancelUpdate)
		{
			return VSConstants.S_OK;
		}

		public int UpdateSolution_Cancel()
		{
			return VSConstants.S_OK;
		}

		public int OnActiveProjectCfgChange(IVsHierarchy pIVsHierarchy)
		{
			// This function is called after a Visual Studio project has its active config changed

			// Check whether the project is the current startup project
			IVsHierarchy StartupProjectHierarchy;
			SolutionBuildManager.get_StartupProject(out StartupProjectHierarchy);
			if (StartupProjectHierarchy != null && StartupProjectHierarchy == pIVsHierarchy)
			{
				// Get the actual Project object from the IVsHierarchy object that was supplied
				var Project = Utils.HierarchyObjectToProject(pIVsHierarchy);
				if (Project != null && OnStartupProjectConfigChanged != null)
				{
					OnStartupProjectConfigChanged(Project);
				}
			}
			return VSConstants.S_OK;
		}

		private void UpdateUnrealLoadedStatus()
		{
			if (!DTE.Solution.IsOpen)
			{
				IsUE4Loaded = false;
				return;
			}

			string SolutionDirectory, UserOptsFile;
			SolutionManager.GetSolutionInfo(out SolutionDirectory, out _SolutionFilepath, out UserOptsFile);

			var SolutionLines = new string[0];
			try
			{
				SolutionLines = File.ReadAllLines(_SolutionFilepath);
			}
			catch
			{
			}

			const string UBTTag = "# UnrealEngineGeneratedSolutionVersion=";
			var UBTLine = SolutionLines.FirstOrDefault(TextLine => TextLine.Trim().StartsWith(UBTTag));
			if (UBTLine != null)
			{
				_UBTVersion = UBTLine.Trim().Substring(UBTTag.Length);
				IsUE4Loaded = true;
			}
			else
			{
				_UBTVersion = string.Empty;
				IsUE4Loaded =
					(
						_SolutionFilepath != null &&
						Path.GetFileName(_SolutionFilepath).StartsWith(UnrealSolutionFileNamePrefix, StringComparison.OrdinalIgnoreCase)
					);
			}
		}

		/** Private Fields & Properties */

		private static UnrealVSPackage PrivateInstance = null;

		/// Handle that we used at shutdown to unregister for selection manager events
		private UInt32 SelectionEventsHandle;

		/// Handle that we use at shutdown to unregister for events about solution activity
		private UInt32 SolutionEventsHandle;

		/// Handle that we use at shutdown to unregister for events about solution build activity
		private UInt32 UpdateSolutionEventsHandle;

		/// Handle that we use to unregister for events about startup project hierarchy activity
		UInt32 ProjectHierarchyEventsHandle;

		/// Our command-line editing component
		private CommandLineEditor CommandLineEditor;

		/// BuildStartupProject feature
		private BuildStartupProject BuildStartupProject;

		/// CompileSingleFile feature
		private CompileSingleFile CompileSingleFile;

		/// Project file generator button
		private GenerateProjectFiles GenerateProjectFiles;

		/// Batch Builder button/command handler
		private BatchBuilder BatchBuilder;

		/// Ticker thread
		private Thread Ticker;

		/// Ticker thread cancel flag
		private int bCancelTicker = 0;

		private string _UBTVersion = string.Empty;

		private readonly List<string> LoadedProjectPaths = new List<string>(); 

		/// Obtains the DTE2 interface for this instance of VS from the RunningObjectTable
		private static DTE2 GetDTE2ForCurrentInstance(DTE DTE)
		{
			// Find the ROT entry for visual studio running under current process.
			IRunningObjectTable Rot;
			NativeMethods.GetRunningObjectTable(0, out Rot);
			IEnumMoniker EnumMoniker;
			Rot.EnumRunning(out EnumMoniker);
			EnumMoniker.Reset();
			uint Fetched = 0;
			IMoniker[] Moniker = new IMoniker[1];
			while (EnumMoniker.Next(1, Moniker, out Fetched) == 0)
			{
				object ComObject;
				Rot.GetObject(Moniker[0], out ComObject);
				DTE2 CandidateDTE2 = ComObject as DTE2;

				if (CandidateDTE2 != null)
				{
					if (CandidateDTE2.DTE == DTE)
					{
						return CandidateDTE2;
					}
				}
			}
			return null;
		}
	}

	internal static class NativeMethods
	{
		/// ROT function in ole32.dll needed by GetDTE2ForCurrentInstance()
		[DllImport("ole32.dll")]
		internal static extern int GetRunningObjectTable(int reserved, out IRunningObjectTable prot);
	}
}
