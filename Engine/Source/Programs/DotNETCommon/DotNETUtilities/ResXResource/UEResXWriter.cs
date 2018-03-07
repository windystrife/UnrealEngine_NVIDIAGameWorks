using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;

namespace Tools.DotNETUtilities
{
	public class UEResXWriter
	{
		public UEResXWriter(string InPath)
		{

			Filename = InPath;
			Document = new XmlDocument();
			Document.AppendChild(Document.CreateXmlDeclaration("1.0", "utf-8", null));
			RootElement = Document.CreateElement("root");
			Document.AppendChild(RootElement);

			RootElement.AppendChild(CreateEntry("resheader", "resmimetype", "text/microsoft-resx", null));
			RootElement.AppendChild(CreateEntry("resheader", "version", "2.0", null));
			RootElement.AppendChild(CreateEntry("resheader", "reader", typeof(UEResXReader).AssemblyQualifiedName, null));
			RootElement.AppendChild(CreateEntry("resheader", "writer", typeof(UEResXWriter).AssemblyQualifiedName, null));
		}

		public void Close()
		{
			Document.Save(Filename);
		}

		public void AddResource(string InName, string InValue)
		{
			RootElement.AppendChild(CreateEntry("data", InName, InValue, "preserve"));
		}

		private XmlNode CreateEntry(string InRootName, string InName, string InValue, string InSpace)
		{
			XmlElement Value = Document.CreateElement("value");
			Value.InnerText = InValue;

			XmlElement Data = Document.CreateElement("data");

			{
				XmlAttribute Attr = Document.CreateAttribute("name");
				Attr.Value = InName;
				Data.Attributes.Append(Attr);
			}

			if (!string.IsNullOrEmpty(InSpace))
			{
				XmlAttribute Attr = Document.CreateAttribute("xml:space");
				Attr.Value = "preserve";
				Data.Attributes.Append(Attr);
			}

			Data.AppendChild(Value);

			return Data;
		}

		private string Filename;
		private XmlDocument Document;
		private XmlElement RootElement;
	}
}
