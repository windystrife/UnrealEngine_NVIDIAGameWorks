using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Schema;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace UnrealBuildTool
{
	/// <summary>
	/// Implementation of XmlDocument which preserves line numbers for its elements
	/// </summary>
	class XmlConfigFile : XmlDocument
	{
		/// <summary>
		/// Root element for the XML document
		/// </summary>
		public const string RootElementName = "Configuration";

		/// <summary>
		/// Namespace for the XML schema
		/// </summary>
		public const string SchemaNamespaceURI = "https://www.unrealengine.com/BuildConfiguration";

		/// <summary>
		/// The file being read
		/// </summary>
		FileReference File;

		/// <summary>
		/// Interface to the LineInfo on the active XmlReader
		/// </summary>
		IXmlLineInfo LineInfo;

		/// <summary>
		/// Set to true if the reader encounters an error
		/// </summary>
		bool bHasErrors;

		/// <summary>
		/// Private constructor. Use XmlConfigFile.TryRead to read an XML config file.
		/// </summary>
		private XmlConfigFile(FileReference InFile)
		{
			File = InFile;
		}

		/// <summary>
		/// Overrides XmlDocument.CreateElement() to construct ScriptElements rather than XmlElements
		/// </summary>
		public override XmlElement CreateElement(string Prefix, string LocalName, string NamespaceUri)
		{
			return new XmlConfigFileElement(File, LineInfo.LineNumber, Prefix, LocalName, NamespaceUri, this);
		}

		/// <summary>
		/// Loads a script document from the given file
		/// </summary>
		/// <param name="File">The file to load</param>
		/// <param name="Schema">The schema to validate against</param>
		/// <param name="OutConfigFile">If successful, the document that was read</param>
		/// <returns>True if the document could be read, false otherwise</returns>
		public static bool TryRead(FileReference File, XmlSchema Schema, out XmlConfigFile OutConfigFile)
		{
			XmlConfigFile ConfigFile = new XmlConfigFile(File);

			XmlReaderSettings Settings = new XmlReaderSettings();
			Settings.Schemas.Add(Schema);
			Settings.ValidationType = ValidationType.Schema;
			Settings.ValidationEventHandler += ConfigFile.ValidationEvent;

			using (XmlReader Reader = XmlReader.Create(File.FullName, Settings))
			{
				// Read the document
				ConfigFile.LineInfo = (IXmlLineInfo)Reader;
				try
				{
					ConfigFile.Load(Reader);
				}
				catch (XmlException Ex)
				{
					if (!ConfigFile.bHasErrors)
					{
						Log.TraceError("{0}", Ex.Message);
						ConfigFile.bHasErrors = true;
					}
				}

				// If we hit any errors while parsing
				if (ConfigFile.bHasErrors)
				{
					OutConfigFile = null;
					return false;
				}

				// Check that the root element is valid. If not, we didn't actually validate against the schema.
				if (ConfigFile.DocumentElement.Name != RootElementName)
				{
					Log.TraceError("Script does not have a root element called '{0}'", RootElementName);
					OutConfigFile = null;
					return false;
				}
				if (ConfigFile.DocumentElement.NamespaceURI != SchemaNamespaceURI)
				{
					Log.TraceError("Script root element is not in the '{0}' namespace (add the xmlns=\"{0}\" attribute)", SchemaNamespaceURI);
					OutConfigFile = null;
					return false;
				}
			}

			OutConfigFile = ConfigFile;
			return true;
		}

		/// <summary>
		/// Callback for validation errors in the document
		/// </summary>
		/// <param name="Sender">Standard argument for ValidationEventHandler</param>
		/// <param name="Args">Standard argument for ValidationEventHandler</param>
		void ValidationEvent(object Sender, ValidationEventArgs Args)
		{
			if (Args.Severity == XmlSeverityType.Warning)
			{
				Log.TraceWarning("{0}({1}): {2}", File.FullName, Args.Exception.LineNumber, Args.Message);
			}
			else
			{
				Log.TraceError("{0}({1}): {2}", File.FullName, Args.Exception.LineNumber, Args.Message);
				bHasErrors = true;
			}
		}
	}

	/// <summary>
	/// Implementation of XmlElement which preserves line numbers
	/// </summary>
	class XmlConfigFileElement : XmlElement
	{
		/// <summary>
		/// The file containing this element
		/// </summary>
		public readonly FileReference File;

		/// <summary>
		/// The line number containing this element
		/// </summary>
		public readonly int LineNumber;

		/// <summary>
		/// Constructor
		/// </summary>
		public XmlConfigFileElement(FileReference InFile, int InLineNumber, string Prefix, string LocalName, string NamespaceUri, XmlConfigFile ConfigFile)
			: base(Prefix, LocalName, NamespaceUri, ConfigFile)
		{
			File = InFile;
			LineNumber = InLineNumber;
		}
	}
}
