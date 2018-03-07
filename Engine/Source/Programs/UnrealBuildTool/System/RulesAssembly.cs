using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.Serialization;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores information about a compiled rules assembly and the types it contains
	/// </summary>
	public class RulesAssembly
	{
		/// <summary>
		/// The compiled assembly
		/// </summary>
		private Assembly CompiledAssembly;

		/// <summary>
		/// All the plugins included in this assembly
		/// </summary>
		private IReadOnlyList<PluginInfo> Plugins;

		/// <summary>
		/// Maps module names to their actual xxx.Module.cs file on disk
		/// </summary>
		private Dictionary<string, FileReference> ModuleNameToModuleFile = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Maps target names to their actual xxx.Target.cs file on disk
		/// </summary>
		private Dictionary<string, FileReference> TargetNameToTargetFile = new Dictionary<string, FileReference>(StringComparer.InvariantCultureIgnoreCase);

		/// <summary>
		/// Mapping from module file to its plugin info.
		/// </summary>
		private Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo;

		/// <summary>
		/// Cache for whether a module has source code
		/// </summary>
		private Dictionary<FileReference, bool> ModuleHasSource = new Dictionary<FileReference, bool>();

		/// <summary>
		/// The parent rules assembly that this assembly inherits. Game assemblies inherit the engine assembly, and the engine assembly inherits nothing.
		/// </summary>
		private RulesAssembly Parent;

		/// <summary>
		/// Constructor. Compiles a rules assembly from the given source files.
		/// </summary>
		/// <param name="Plugins">All the plugins included in this assembly</param>
		/// <param name="ModuleFiles">List of module files to compile</param>
		/// <param name="TargetFiles">List of target files to compile</param>
		/// <param name="ModuleFileToPluginInfo">Mapping of module file to the plugin that contains it</param>
		/// <param name="AssemblyFileName">The output path for the compiled assembly</param>
		/// <param name="Parent">The parent rules assembly</param>
		public RulesAssembly(IReadOnlyList<PluginInfo> Plugins, List<FileReference> ModuleFiles, List<FileReference> TargetFiles, Dictionary<FileReference, PluginInfo> ModuleFileToPluginInfo, FileReference AssemblyFileName, RulesAssembly Parent)
		{
			this.Plugins = Plugins;
			this.ModuleFileToPluginInfo = ModuleFileToPluginInfo;
			this.Parent = Parent;

			// Find all the source files
			List<FileReference> AssemblySourceFiles = new List<FileReference>();
			AssemblySourceFiles.AddRange(ModuleFiles);
			AssemblySourceFiles.AddRange(TargetFiles);

			// Compile the assembly
			if (AssemblySourceFiles.Count > 0)
			{
				List<string> PreprocessorDefines = GetPreprocessorDefinitions();
				CompiledAssembly = DynamicCompilation.CompileAndLoadAssembly(AssemblyFileName, AssemblySourceFiles, PreprocessorDefines: PreprocessorDefines);
			}

			// Setup the module map
			foreach (FileReference ModuleFile in ModuleFiles)
			{
				string ModuleName = ModuleFile.GetFileNameWithoutAnyExtensions();
				if (!ModuleNameToModuleFile.ContainsKey(ModuleName))
				{
					ModuleNameToModuleFile.Add(ModuleName, ModuleFile);
				}
			}

			// Setup the target map
			foreach (FileReference TargetFile in TargetFiles)
			{
				string TargetName = TargetFile.GetFileNameWithoutAnyExtensions();
				if (!TargetNameToTargetFile.ContainsKey(TargetName))
				{
					TargetNameToTargetFile.Add(TargetName, TargetFile);
				}
			}

			// Write any deprecation warnings for methods overriden from a base with the [ObsoleteOverride] attribute. Unlike the [Obsolete] attribute, this ensures the message
			// is given because the method is implemented, not because it's called.
			if (CompiledAssembly != null)
			{
				foreach (Type CompiledType in CompiledAssembly.GetTypes())
				{
					foreach (MethodInfo Method in CompiledType.GetMethods(BindingFlags.Instance | BindingFlags.Public | BindingFlags.DeclaredOnly))
					{
						ObsoleteOverrideAttribute Attribute = Method.GetCustomAttribute<ObsoleteOverrideAttribute>(true);
						if (Attribute != null)
						{
							FileReference Location;
							if (!TryGetFileNameFromType(CompiledType, out Location))
							{
								Location = new FileReference(CompiledAssembly.Location);
							}
							Log.TraceWarning("{0}: warning: {1}", Location, Attribute.Message);
						}
					}
					if(CompiledType.BaseType == typeof(ModuleRules))
					{
						ConstructorInfo Constructor = CompiledType.GetConstructor(new Type[] { typeof(TargetInfo) });
						if(Constructor != null)
						{
							FileReference Location;
							if (!TryGetFileNameFromType(CompiledType, out Location))
							{
								Location = new FileReference(CompiledAssembly.Location);
							}
							Log.TraceWarning("{0}: warning: Module constructors should take a ReadOnlyTargetRules argument (rather than a TargetInfo argument) and pass it to the base class constructor from 4.15 onwards. Please update the method signature.", Location);
						}
					}
				}
			}
		}

		/// <summary>
		/// Finds all the preprocessor definitions that need to be set for the current engine.
		/// </summary>
		/// <returns>List of preprocessor definitions that should be set</returns>
		public static List<string> GetPreprocessorDefinitions()
		{
			List<string> PreprocessorDefines = new List<string>();
			PreprocessorDefines.Add("WITH_FORWARDED_MODULE_RULES_CTOR");
			PreprocessorDefines.Add("WITH_FORWARDED_TARGET_RULES_CTOR");

			// Define macros for the UE4 version, starting with 4.17
			BuildVersion Version;
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out Version))
			{
				for(int MinorVersion = 17; MinorVersion <= Version.MinorVersion; MinorVersion++)
				{
					PreprocessorDefines.Add(String.Format("UE_4_{0}_OR_LATER", MinorVersion));
				}
			}
			return PreprocessorDefines;
		}

		/// <summary>
		/// Fills a list with all the module names in this assembly (or its parent)
		/// </summary>
		/// <param name="ModuleNames">List to receive the module names</param>
		public void GetAllModuleNames(List<string> ModuleNames)
		{
			if (Parent != null)
			{
				Parent.GetAllModuleNames(ModuleNames);
			}
			ModuleNames.AddRange(ModuleNameToModuleFile.Keys);
		}

		/// <summary>
		/// Tries to get the filename that declared the given type
		/// </summary>
		/// <param name="ExistingType"></param>
		/// <param name="File"></param>
		/// <returns>True if the type was found, false otherwise</returns>
		public bool TryGetFileNameFromType(Type ExistingType, out FileReference File)
		{
			if (ExistingType.Assembly == CompiledAssembly)
			{
				string Name = ExistingType.Name;
				if (ModuleNameToModuleFile.TryGetValue(Name, out File))
				{
					return true;
				}

				string NameWithoutTarget = Name;
				if (NameWithoutTarget.EndsWith("Target"))
				{
					NameWithoutTarget = NameWithoutTarget.Substring(0, NameWithoutTarget.Length - 6);
				}
				if (TargetNameToTargetFile.TryGetValue(NameWithoutTarget, out File))
				{
					return true;
				}
			}
			else
			{
				if (Parent != null && Parent.TryGetFileNameFromType(ExistingType, out File))
				{
					return true;
				}
			}

			File = null;
			return false;
		}

		/// <summary>
		/// Gets the source file containing rules for the given module
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <returns>The filename containing rules for this module, or an empty string if not found</returns>
		public FileReference GetModuleFileName(string ModuleName)
		{
			FileReference ModuleFile;
			if (ModuleNameToModuleFile.TryGetValue(ModuleName, out ModuleFile))
			{
				return ModuleFile;
			}
			else
			{
				return (Parent == null) ? null : Parent.GetModuleFileName(ModuleName);
			}
		}

		/// <summary>
		/// Gets the source file containing rules for the given target
		/// </summary>
		/// <param name="TargetName">The name of the target</param>
		/// <returns>The filename containing rules for this target, or an empty string if not found</returns>
		public FileReference GetTargetFileName(string TargetName)
		{
			FileReference TargetFile;
			if (TargetNameToTargetFile.TryGetValue(TargetName, out TargetFile))
			{
				return TargetFile;
			}
			else
			{
				return (Parent == null) ? null : Parent.GetTargetFileName(TargetName);
			}
		}

		/// <summary>
		/// Creates an instance of a module rules descriptor object for the specified module name
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="ReferenceChain">Chain of references leading to this module</param>
		/// <param name="Target">Information about the target associated with this module</param>
		/// <returns>Compiled module rule info</returns>
		public ModuleRules CreateModuleRules(string ModuleName, ReadOnlyTargetRules Target, string ReferenceChain)
		{
			FileReference ModuleFileName;
			return CreateModuleRules(ModuleName, Target, ReferenceChain, out ModuleFileName);
		}

		/// <summary>
		/// Creates an instance of a module rules descriptor object for the specified module name
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <param name="Target">Information about the target associated with this module</param>
		/// <param name="ReferenceChain">Chain of references leading to this module</param>
		/// <param name="ModuleFileName">The original source file name for the Module.cs file for this module</param>
		/// <returns>Compiled module rule info</returns>
		public ModuleRules CreateModuleRules(string ModuleName, ReadOnlyTargetRules Target, string ReferenceChain, out FileReference ModuleFileName)
		{
			// Currently, we expect the user's rules object type name to be the same as the module name
			string ModuleTypeName = ModuleName;

			// Make sure the module file is known to us
			if (!ModuleNameToModuleFile.TryGetValue(ModuleName, out ModuleFileName))
			{
				if (Parent == null)
				{
					throw new BuildException("Could not find definition for module '{0}' (referenced via {1})", ModuleName, ReferenceChain);
				}
				else
				{
					return Parent.CreateModuleRules(ModuleName, Target, ReferenceChain, out ModuleFileName);
				}
			}

			// The build module must define a type named 'Rules' that derives from our 'ModuleRules' type.  
			Type RulesObjectType = CompiledAssembly.GetType(ModuleName);

			if (RulesObjectType == null)
			{
				// Temporary hack to avoid System namespace collisions
				// @todo projectfiles: Make rules assemblies require namespaces.
				RulesObjectType = CompiledAssembly.GetType("UnrealBuildTool.Rules." + ModuleName);
			}

			if (RulesObjectType == null)
			{
				throw new BuildException("Expecting to find a type to be declared in a module rules named '{0}' in {1}.  This type must derive from the 'ModuleRules' type defined by Unreal Build Tool.", ModuleTypeName, CompiledAssembly.FullName);
			}

			// Create an instance of the module's rules object
			ModuleRules RulesObject;
			try
			{
				// Create an uninitialized ModuleRules object and initialize some fields on it while we're still supporting the deprecated parameterless constructor.
				RulesObject = (ModuleRules)FormatterServices.GetUninitializedObject(RulesObjectType);
				typeof(ModuleRules).GetField("Target").SetValue(RulesObject, Target);

				// Call the constructor
				ConstructorInfo Constructor = RulesObjectType.GetConstructor(new Type[] { typeof(ReadOnlyTargetRules) });
				if(Constructor != null)
				{
					Constructor.Invoke(RulesObject, new object[] { Target });
				}
				else
				{
					ConstructorInfo DeprecatedConstructor = RulesObjectType.GetConstructor(new Type[] { typeof(TargetInfo) });
					if(DeprecatedConstructor == null)
					{
						throw new Exception("No valid constructor found.");
					}	
					DeprecatedConstructor.Invoke(RulesObject, new object[] { new TargetInfo(Target) });
				}
			}
			catch (Exception Ex)
			{
				Exception MessageEx = (Ex is TargetInvocationException && Ex.InnerException != null)? Ex.InnerException : Ex;
				throw new BuildException(Ex, "Unable to instantiate module '{0}': {1}\n(referenced via {2})", ModuleName, MessageEx.ToString(), ReferenceChain);
			}

			return RulesObject;
		}

		/// <summary>
		/// Determines whether the given module name is a game module (as opposed to an engine module)
		/// </summary>
		public bool IsGameModule(string InModuleName)
		{
			FileReference ModuleFileName = GetModuleFileName(InModuleName);
			return (ModuleFileName != null && !UnrealBuildTool.IsUnderAnEngineDirectory(ModuleFileName.Directory));
		}

		/// <summary>
		/// Construct an instance of the given target rules
		/// </summary>
		/// <param name="TypeName">Type name of the target rules</param>
		/// <param name="TargetInfo">Target configuration information to pass to the constructor</param>
		/// <returns>Instance of the corresponding TargetRules</returns>
		protected TargetRules CreateTargetRulesInstance(string TypeName, TargetInfo TargetInfo)
		{
			// The build module must define a type named '<TargetName>Target' that derives from our 'TargetRules' type.  
			Type RulesObjectType = CompiledAssembly.GetType(TypeName);
			if (RulesObjectType == null)
			{
				throw new BuildException("Expecting to find a type to be declared in a target rules named '{0}'.  This type must derive from the 'TargetRules' type defined by Unreal Build Tool.", TypeName);
			}

			// Create an instance of the module's rules object. To avoid breaking backwards compatibility and requiring this information be passed through to the base class 
			// constructor, we construct these objects and call the constructor manually for now.
			TargetRules RulesObject = (TargetRules)FormatterServices.GetUninitializedObject(RulesObjectType);
			typeof(TargetRules).GetField("Name").SetValue(RulesObject, TargetInfo.Name);
			typeof(TargetRules).GetField("Platform").SetValue(RulesObject, TargetInfo.Platform);
			typeof(TargetRules).GetField("Configuration").SetValue(RulesObject, TargetInfo.Configuration);
			typeof(TargetRules).GetField("Architecture").SetValue(RulesObject, TargetInfo.Architecture);
			typeof(TargetRules).GetField("ProjectFile").SetValue(RulesObject, TargetInfo.ProjectFile);

			// Find the constructor
			ConstructorInfo Constructor = RulesObjectType.GetConstructor(new Type[] { typeof(TargetInfo) });
			if(Constructor == null)
			{
				throw new BuildException("No constructor found on {0} which takes an argument of type TargetInfo.", RulesObjectType.Name);
			}

			// Invoke the regular constructor
			try
			{
				Constructor.Invoke(RulesObject, new object[] { TargetInfo });
			}
			catch (Exception Ex)
			{
				throw new BuildException(Ex, "Unable to instantiate instance of '{0}' object type from compiled assembly '{1}'.  Unreal Build Tool creates an instance of your module's 'Rules' object in order to find out about your module's requirements.  The CLR exception details may provide more information:  {2}", TypeName, Path.GetFileNameWithoutExtension(CompiledAssembly.Location), Ex.ToString());
			}

			RulesObject.SetOverridesForTargetType();
			return RulesObject;
		}

		/// <summary>
		/// Creates a target rules object for the specified target name.
		/// </summary>
		/// <param name="TargetName">Name of the target</param>
		/// <param name="Platform">The platform that the target is being built for</param>
		/// <param name="Configuration">The configuration the target is being built for</param>
		/// <param name="Architecture">The architecture the target is being built for</param>
		/// <param name="ProjectFile">The project containing the target being built</param>
		/// <param name="bInEditorRecompile">Whether this is an editor recompile, where we need to guess the name of the editor target</param>
		/// <returns>The build target rules for the specified target</returns>
		public TargetRules CreateTargetRules(string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, FileReference ProjectFile, bool bInEditorRecompile)
		{
			FileReference TargetFileName;
			return CreateTargetRules(TargetName, Platform, Configuration, Architecture, ProjectFile, bInEditorRecompile, out TargetFileName);
		}

		/// <summary>
		/// Creates a target rules object for the specified target name.
		/// </summary>
		/// <param name="TargetName">Name of the target</param>
		/// <param name="Platform">Platform being compiled</param>
		/// <param name="Configuration">Configuration being compiled</param>
		/// <param name="Architecture">Architecture being built</param>
		/// <param name="ProjectFile">Path to the project file for this target</param>
		/// <param name="bInEditorRecompile">Whether this is an editor recompile, where we need to guess the name of the editor target</param>
		/// <param name="TargetFileName">The original source file name of the Target.cs file for this target</param>
		/// <returns>The build target rules for the specified target</returns>
		public TargetRules CreateTargetRules(string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, FileReference ProjectFile, bool bInEditorRecompile, out FileReference TargetFileName)
		{
			// Make sure the target file is known to us
			bool bFoundTargetName = TargetNameToTargetFile.ContainsKey(TargetName);
			if (bFoundTargetName == false)
			{
				if (Parent == null)
				{
					//				throw new BuildException("Couldn't find target rules file for target '{0}' in rules assembly '{1}'.", TargetName, RulesAssembly.FullName);
					string ExceptionMessage = "Couldn't find target rules file for target '";
					ExceptionMessage += TargetName;
					ExceptionMessage += "' in rules assembly '";
					ExceptionMessage += CompiledAssembly.FullName;
					ExceptionMessage += "'." + Environment.NewLine;

					ExceptionMessage += "Location: " + CompiledAssembly.Location + Environment.NewLine;

					ExceptionMessage += "Target rules found:" + Environment.NewLine;
					foreach (KeyValuePair<string, FileReference> entry in TargetNameToTargetFile)
					{
						ExceptionMessage += "\t" + entry.Key + " - " + entry.Value + Environment.NewLine;
					}

					throw new BuildException(ExceptionMessage);
				}
				else
				{
					return Parent.CreateTargetRules(TargetName, Platform, Configuration, Architecture, ProjectFile, bInEditorRecompile, out TargetFileName);
				}
			}

			// Return the target file name to the caller
			TargetFileName = TargetNameToTargetFile[TargetName];

			// Currently, we expect the user's rules object type name to be the same as the module name + 'Target'
			string TargetTypeName = TargetName + "Target";

			// The build module must define a type named '<TargetName>Target' that derives from our 'TargetRules' type.  
			TargetRules RulesObject = CreateTargetRulesInstance(TargetTypeName, new TargetInfo(TargetName, Platform, Configuration, Architecture, ProjectFile));
			if (bInEditorRecompile)
			{
				// Make sure this is an editor module.
				if (RulesObject != null)
				{
					if (RulesObject.Type != TargetType.Editor)
					{
						// Not the editor... determine the editor project
						string TargetSourceFolderString = TargetFileName.FullName;
						Int32 SourceFolderIndex = -1;
						if (Utils.IsRunningOnMono)
						{
							TargetSourceFolderString = TargetSourceFolderString.Replace("\\", "/");
							SourceFolderIndex = TargetSourceFolderString.LastIndexOf("/Source/", StringComparison.InvariantCultureIgnoreCase);
						}
						else
						{
							TargetSourceFolderString = TargetSourceFolderString.Replace("/", "\\");
							SourceFolderIndex = TargetSourceFolderString.LastIndexOf("\\Source\\", StringComparison.InvariantCultureIgnoreCase);
						}
						if (SourceFolderIndex != -1)
						{
							DirectoryReference TargetSourceFolder = new DirectoryReference(TargetSourceFolderString.Substring(0, SourceFolderIndex + 7));
							foreach (KeyValuePair<string, FileReference> CheckEntry in TargetNameToTargetFile)
							{
								if (CheckEntry.Value.IsUnderDirectory(TargetSourceFolder))
								{
									if (CheckEntry.Key.Equals(TargetName, StringComparison.InvariantCultureIgnoreCase) == false)
									{
										// We have found a target in the same source folder that is not the original target found.
										// See if it is the editor project
										string CheckTargetTypeName = CheckEntry.Key + "Target";
										TargetRules CheckRulesObject = CreateTargetRulesInstance(CheckTargetTypeName, new TargetInfo(CheckEntry.Key, Platform, Configuration, Architecture, ProjectFile));
										if (CheckRulesObject != null)
										{
											if (CheckRulesObject.Type == TargetType.Editor)
											{
												// Found it
												// NOTE: This prevents multiple Editor targets from co-existing...
												RulesObject = CheckRulesObject;
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}

			return RulesObject;
		}

		/// <summary>
		/// Enumerates all the plugins that are available
		/// </summary>
		/// <returns></returns>
		public IEnumerable<PluginInfo> EnumeratePlugins()
		{
			return global::UnrealBuildTool.Plugins.FilterPlugins(EnumeratePluginsInternal());
		}

		/// <summary>
		/// Enumerates all the plugins that are available
		/// </summary>
		/// <returns></returns>
		protected IEnumerable<PluginInfo> EnumeratePluginsInternal()
		{
			if (Parent == null)
			{
				return Plugins;
			}
			else
			{
				return Plugins.Concat(Parent.EnumeratePluginsInternal());
			}
		}

		/// <summary>
		/// Tries to find the PluginInfo associated with a given module file
		/// </summary>
		/// <param name="ModuleFile">The module to search for</param>
		/// <param name="Plugin">The matching plugin info, or null.</param>
		/// <returns>True if the module belongs to a plugin</returns>
		public bool TryGetPluginForModule(FileReference ModuleFile, out PluginInfo Plugin)
		{
			if (ModuleFileToPluginInfo.TryGetValue(ModuleFile, out Plugin))
			{
				return true;
			}
			else
			{
				return (Parent == null) ? false : Parent.TryGetPluginForModule(ModuleFile, out Plugin);
			}
		}

		/// <summary>
		/// Determines if a module in this rules assembly has source code.
		/// </summary>
		/// <param name="ModuleName">Name of the module to check</param>
		/// <returns>True if the module has source files, false if the module was not found, or does not have source files.</returns>
		public bool DoesModuleHaveSource(string ModuleName)
		{
			FileReference ModuleFile;
			if (ModuleNameToModuleFile.TryGetValue(ModuleName, out ModuleFile))
			{
				bool HasSource;
				if (!ModuleHasSource.TryGetValue(ModuleFile, out HasSource))
				{
					foreach (string FileName in Directory.EnumerateFiles(ModuleFile.Directory.FullName, "*.cpp", SearchOption.AllDirectories))
					{
						HasSource = true;
						break;
					}
					ModuleHasSource.Add(ModuleFile, HasSource);
				}
				return HasSource;
			}
			return (Parent == null) ? false : Parent.DoesModuleHaveSource(ModuleName);
		}
	}
}
