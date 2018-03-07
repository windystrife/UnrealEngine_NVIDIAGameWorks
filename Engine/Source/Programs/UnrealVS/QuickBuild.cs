// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using EnvDTE;
using EnvDTE80;
using Microsoft.VisualStudio;
using Microsoft.VisualStudio.Shell;
using Microsoft.VisualStudio.Shell.Interop;
using System;
using System.Collections.Generic;
using System.ComponentModel.Design;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace UnrealVS
{
	/// <summary>
	/// Ole Command class for handling the quick build menu items (individual configs inside platform menus)
	/// One of these handles all the items in a platform menu.
	/// </summary>
	class QuickBuildMenuCommand : OleMenuCommand
	{
		/// The function used to select the range of commands handled by this object
		private Predicate<int> OnMatchItem;

		/// <summary>
		/// Create a QuickBuildMenuCommand with a delegate for determining which command numbers it handles
		/// The other params are used to construct the base OleMenuCommand
		/// </summary>
		public QuickBuildMenuCommand(CommandID InDynamicStartCommandId, Predicate<int> InOnMatchItem, EventHandler InOnInvokedDynamicItem, EventHandler InOnBeforeQueryStatusDynamicItem)
			: base(InOnInvokedDynamicItem, null /*changeHandler*/, InOnBeforeQueryStatusDynamicItem, InDynamicStartCommandId)
		{
			if (InOnMatchItem == null)
			{
				throw new ArgumentNullException("InOnMatchItem");
			}

			OnMatchItem = InOnMatchItem;
		}

		/// <summary>
		/// Allows a dynamic item command to match the subsequent items in its list.
		/// Overridden from OleMenuCommand.
		/// Calls OnMatchItem to choose with command numbers are valid.
		/// </summary>
		public override bool DynamicItemMatch(int cmdId)
		{
			// Call the supplied predicate to test whether the given cmdId is a match.
			// If it is, store the command id in MatchedCommandid 
			// for use by any BeforeQueryStatus handlers, and then return that it is a match.
			// Otherwise clear any previously stored matched cmdId and return that it is not a match.
			if (OnMatchItem(cmdId))
			{
				MatchedCommandId = cmdId;
				return true;
			}
			MatchedCommandId = 0;
			return false;
		}
	}

	/// <summary>
	/// Class represents a single platform, quick build menu. Wraps QuickBuildMenuCommand.
	/// </summary>
	class PlaformMenuContents
	{
		/// The command id of the placeholder command that sets the start of the command range for this menu
		private readonly int DynamicStartCommandId;
		/// The name of the platform. Matches the platform string in the solution configs exactly.
		private readonly string PlaformName;

		/// <summary>
		/// Construct a platform menu for a given, named platform, starting at the given command id
		/// </summary>
		/// <param name="Name">The name of the platform</param>
		/// <param name="InDynamicStartCommandId">The start of the command range for this menu</param>
		public PlaformMenuContents(string Name, int InDynamicStartCommandId)
		{
			DynamicStartCommandId = InDynamicStartCommandId;
			PlaformName = Name;

			// Create the dynamic menu
			var MenuOleCommand = new QuickBuildMenuCommand(
				new CommandID(GuidList.UnrealVSCmdSet, DynamicStartCommandId),
				IsValidDynamicItem,
				OnInvokedDynamicItem,
				OnBeforeQueryStatusDynamicItem
				);
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(MenuOleCommand);
		}

		/// <summary>
		/// The callback function passed to this object's QuickBuildMenuCommand, used to select the range of commands handled by this object
		/// It uses the cached list of solution configs in QuickBuild to determine how many commands should be in the menu. 
		/// </summary>
		/// <param name="cmdId">Command id the validate</param>
		/// <returns>True, if the command is valid for this menu</returns>
		private bool IsValidDynamicItem(int cmdId)
		{
			int ConfigCount = QuickBuild.CachedSolutionConfigNames.Length;

			// The match is valid if the command ID is >= the id of our root dynamic start item 
			// and the command ID minus the ID of our root dynamic start item
			// is less than or equal to the number of projects in the solution.
			return (cmdId >= DynamicStartCommandId) && ((cmdId - DynamicStartCommandId) < ConfigCount);
		}

		/// <summary>
		/// The invoke handler passed to this object's QuickBuildMenuCommand, called by the base OleMenuCommand when an item is clicked
		/// </summary>
		private void OnInvokedDynamicItem(object sender, EventArgs args)
		{
			var MenuCommand = (QuickBuildMenuCommand)sender;

			// Get the project clicked in the solution explorer by accessing the current selection and converting to a Project if possible.
			IntPtr HierarchyPtr, SelectionContainerPtr;
			uint ProjectItemId;
			IVsMultiItemSelect MultiItemSelect;
			UnrealVSPackage.Instance.SelectionManager.GetCurrentSelection(out HierarchyPtr, out ProjectItemId, out MultiItemSelect, out SelectionContainerPtr);
			if (HierarchyPtr == null) return;

			IVsHierarchy SelectedHierarchy = Marshal.GetTypedObjectForIUnknown(HierarchyPtr, typeof(IVsHierarchy)) as IVsHierarchy;
			if (SelectedHierarchy == null) return;

			var SelectedProject = Utils.HierarchyObjectToProject(SelectedHierarchy);
			if (SelectedProject == null) return;

			// Builds the selected project with the clicked platform and config
			Utils.ExecuteProjectBuild(SelectedProject, MenuCommand.Text, PlaformName, BatchBuilderToolControl.BuildJob.BuildJobType.Build, null, null);
		}

		/// <summary>
		/// Before-query handler passed to this object's QuickBuildMenuCommand, called by the base OleMenuCommand to update the state of an item.
		/// </summary>
		private void OnBeforeQueryStatusDynamicItem(object sender, EventArgs args)
		{
			var MenuCommand = (QuickBuildMenuCommand)sender;
			MenuCommand.Enabled = QuickBuild.IsActive;
			MenuCommand.Visible = true;

			// Determine the index of the item in the menu
			bool isRootItem = MenuCommand.MatchedCommandId == 0;
			int CommandId = isRootItem ? DynamicStartCommandId : MenuCommand.MatchedCommandId;
			int DynCommandIdx = CommandId - DynamicStartCommandId;

			// Set the text label based on the index and the solution configs array cached in QuickBuild
			if (DynCommandIdx < QuickBuild.CachedSolutionConfigNames.Length)
			{
				MenuCommand.Text = QuickBuild.CachedSolutionConfigNames[DynCommandIdx];
			}

			// Clear the id now that the query is done
			MenuCommand.MatchedCommandId = 0;
		}
	}

	/// <summary>
	/// The Quick Build menu system
	/// </summary>
	public class QuickBuild
	{
		/** classes */

		/// All the basic data needed to build each platform-specific submenu
		struct SubMenu
		{
			public string Name { get; set; }
			public int SubMenuId { get; set; }
			public int DynamicStartCommandId { get; set; }
		}

		/** constants */

		private const int ProjectQuickBuildMenuID = 0x1410;		// must match the values in the vsct file

		/** fields */

		/// Solution configs and platforms cached once before each time the Quick Build menu opens
		private static string[] SolutionConfigNames = new string[0];
		private static string[] SolutionConfigPlatforms = new string[0];

		/// Hide/shows the while Quick Build menu tree
		private static bool bIsActive = false;

		/// List of submenus and their details - must match the values in the vsct file
		private readonly SubMenu[] SubMenus = new[]
			{
				new SubMenu {Name = "Win64", SubMenuId = 0x1500, DynamicStartCommandId = 0x1530},
				new SubMenu {Name = "Win32", SubMenuId = 0x1600, DynamicStartCommandId = 0x1630},
				new SubMenu {Name = "Mac", SubMenuId = 0x1700, DynamicStartCommandId = 0x1730},
				new SubMenu {Name = "XboxOne", SubMenuId = 0x1800, DynamicStartCommandId = 0x1830},
				new SubMenu {Name = "PS4", SubMenuId = 0x1900, DynamicStartCommandId = 0x1930},
				new SubMenu {Name = "IOS", SubMenuId = 0x1A00, DynamicStartCommandId = 0x1A30},
				new SubMenu {Name = "Android", SubMenuId = 0x1B00, DynamicStartCommandId = 0x1B30},
				new SubMenu {Name = "WinRT", SubMenuId = 0x1C00, DynamicStartCommandId = 0x1C30},
				new SubMenu {Name = "WinRT_ARM", SubMenuId = 0x1D00, DynamicStartCommandId = 0x1D30},
				new SubMenu {Name = "HTML5", SubMenuId = 0x1E00, DynamicStartCommandId = 0x1E30},
				new SubMenu {Name = "Linux", SubMenuId = 0x1F00, DynamicStartCommandId = 0x1F30},
			};

		/// The main root command of the Quick Build menu hierarchy - used to hide it when not active
		private readonly OleMenuCommand QuickBuildCommand;

		/// <summary>
		/// These represent the items that can be added to the menu that lists the platforms.
		/// Each one is a submenu containing items for each config.
		/// </summary>
		private readonly Dictionary<string, OleMenuCommand> AllPlaformMenus = new Dictionary<string, OleMenuCommand>();

		/// <summary>
		/// These represent the items shown in the menu that lists the platforms.
		/// It is a subset of AllPlaformMenus with only the loaded platforms.
		/// Each one is a submenu containing items for each config.
		/// </summary>
		private readonly Dictionary<string, OleMenuCommand> ActivePlaformMenus = new Dictionary<string, OleMenuCommand>();

		/// <summary>
		/// These represent the items in plaform-specific menus.
		/// </summary>
		private readonly Dictionary<string, PlaformMenuContents> PlaformMenusContents = new Dictionary<string, PlaformMenuContents>();

		/// VSConstants.UICONTEXT_SolutionBuilding translated into a cookie used to access UI ctxt state
		private readonly uint SolutionBuildingUIContextCookie;

		/** properties */

		public static bool IsActive { get { return bIsActive; } }
		public static string[] CachedSolutionConfigNames { get { return SolutionConfigNames; } }
		public static string[] CachedSolutionConfigPlatforms { get { return SolutionConfigPlatforms; } }

		/** methods */

		public QuickBuild()
		{
			// root menu
			QuickBuildCommand = new OleMenuCommand(null, null, OnQuickBuildQuery, new CommandID(GuidList.UnrealVSCmdSet, ProjectQuickBuildMenuID));
			QuickBuildCommand.BeforeQueryStatus += OnQuickBuildQuery;
			UnrealVSPackage.Instance.MenuCommandService.AddCommand(QuickBuildCommand);

			// platform sub-menus
			foreach (var SubMenu in SubMenus)
			{
				var SubMenuCommand = new OleMenuCommand(null, new CommandID(GuidList.UnrealVSCmdSet, SubMenu.SubMenuId));
				SubMenuCommand.BeforeQueryStatus += OnQuickBuildSubMenuQuery;
				UnrealVSPackage.Instance.MenuCommandService.AddCommand(SubMenuCommand);
				AllPlaformMenus.Add(SubMenu.Name, SubMenuCommand);
				PlaformMenusContents.Add(SubMenu.Name, new PlaformMenuContents(SubMenu.Name, SubMenu.DynamicStartCommandId));
			}

			// cache the cookie for UICONTEXT_SolutionBuilding
			UnrealVSPackage.Instance.SelectionManager.GetCmdUIContextCookie(VSConstants.UICONTEXT_SolutionBuilding, out SolutionBuildingUIContextCookie);

			// Initialize the active state based on whether the IDE is building anything
			UpdateActiveState();

			UnrealVSPackage.Instance.OnUIContextChanged += OnUIContextChanged;
			UnrealVSPackage.Instance.OnSolutionOpened += OnSolutionChanged;
		}

		/// <summary>
		/// Updates the bIsActive flag using the UICONTEXT_SolutionBuilding state. Hides the Quick Build feature when the solution is building.
		/// </summary>
		private void UpdateActiveState()
		{
			int bIsBuilding;
			UnrealVSPackage.Instance.SelectionManager.IsCmdUIContextActive(SolutionBuildingUIContextCookie, out bIsBuilding);
			bIsActive = bIsBuilding == 0;
		}

		/// <summary>
		/// Before-query handler passed to the root menu item's OleMenuCommand and called to update the state of the item.
		/// </summary>
		private void OnQuickBuildQuery(object sender, EventArgs e)
		{
			// Always cache the list of solution build configs when the project menu is opening
			CacheBuildConfigs();
		}

		/// <summary>
		/// Before-query handler passed to the sub-menu item's OleMenuCommand and called to update the state of the item.
		/// </summary>
		private void OnQuickBuildSubMenuQuery(object sender, EventArgs e)
		{
			var SubMenuCommand = (OleMenuCommand)sender;
			SubMenuCommand.Visible = ActivePlaformMenus.ContainsValue(SubMenuCommand);
			SubMenuCommand.Enabled = bIsActive;
		}

		/// <summary>
		/// Called when a solution loads. Caches the solution build configs and sets the platform menus' visibility.
		/// Only platforms found in the laoded solution's list are shown.
		/// </summary>
		private void OnSolutionChanged()
		{
			CacheBuildConfigs();

			ActivePlaformMenus.Clear();
			foreach (var SubMenu in SubMenus)
			{
				if (SolutionConfigPlatforms.Any(Platform => string.Compare(Platform, SubMenu.Name, StringComparison.InvariantCultureIgnoreCase) == 0))
				{
					ActivePlaformMenus.Add(SubMenu.Name, AllPlaformMenus[SubMenu.Name]);
				}
			}
		}

		/// <summary>
		/// Caches the solution build configs
		/// </summary>
		private void CacheBuildConfigs()
		{
			SolutionConfigurations SolutionConfigs =
				UnrealVSPackage.Instance.DTE.Solution.SolutionBuild.SolutionConfigurations;

			SolutionConfigPlatforms =
				(from SolutionConfiguration2 Sc in SolutionConfigs select Sc.PlatformName).Distinct().ToArray();

			SolutionConfigNames =
							(from SolutionConfiguration2 Sc in SolutionConfigs select Sc.Name).Distinct().ToArray();
		}

		/// <summary>
		/// Called when the UI Context changes. If the building state changed, update the active state.
		/// </summary>
		private void OnUIContextChanged(uint CmdUICookie, bool bActive)
		{
			if (SolutionBuildingUIContextCookie == CmdUICookie)
			{
				UpdateActiveState();
			}
		}
	}
}
