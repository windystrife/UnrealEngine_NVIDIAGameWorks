// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Xml;
using System.Xml.Serialization;


namespace UnrealBuildTool
{
	/// <summary>
	/// Represents a color in the displayed graph.  Values are between 0.0 and 1.0 inclusive.
	/// </summary>
	struct GraphColor
	{
		public float R;
		public float G;
		public float B;
		public float A;
	}


	/// <summary>
	/// A single "node" in a directed graph
	/// </summary>
	class GraphNode
	{
		/// ID number, unique for all nodes.  This must also be the array index into the main nodes array
		public int Id;

		/// Optional text label for this node
		public string Label;

		// Color of the node
		public GraphColor Color = new GraphColor() { R = 1.0f, G = 1.0f, B = 1.0f, A = 1.0f };

		// Size
		public float Size = 1.0f;

		// Other attributes
		public Dictionary<string, Object> Attributes = new Dictionary<string, Object>(StringComparer.InvariantCultureIgnoreCase);
	}


	/// <summary>
	/// Describes a graph "edge" in a directed graph
	/// </summary>
	class GraphEdge
	{
		/// ID number, unique for all edges.  This must also be the array index into the main edges array.
		public int Id;

		/// Source node	(directed graph)
		public GraphNode Source;

		/// Target node (directed graph)
		public GraphNode Target;

		/// Optional edge weight
		public double Weight = 1.0f;

		// Edge color
		public GraphColor Color = new GraphColor() { R = 0.0f, G = 0.0f, B = 0.0f, A = 0.25f };

		// Thickness
		public float Thickness = 0.1f;
	}


	/// <summary>
	/// Describes a type of attribute.  This is derived internally from the attribute data on nodes.
	/// </summary>
	class GraphAttribute
	{
		/// Gexf ID for this attribute
		public int ID;

		/// Name of the attribute
		public string Name;

		/// Gexf type name
		public string TypeName;
	}


	static class GraphVisualization
	{
		/// <summary>
		/// Writes a GEXF graph file for the specified graph nodes and edges
		/// </summary>
		/// <param name="Filename">The file name to write</param>
		/// <param name="Description">The description to include in the graph file's metadata</param>
		/// <param name="GraphNodes">List of all graph nodes.  Index order is important and must match with the individual node Id members!</param>
		/// <param name="GraphEdges">List of all graph edges.  Index order is important and must match with the individual edge Id members!</param>
		public static void WriteGraphFile(string Filename, string Description, List<GraphNode> GraphNodes, List<GraphEdge> GraphEdges)
		{
			XmlWriterSettings Settings = new XmlWriterSettings();
			Settings.Indent = true;
			Settings.IndentChars = "    ";

			// Figure out all of the custom attribute types we're dealing with
			Dictionary<string, GraphAttribute> AllAttributes = new Dictionary<string, GraphAttribute>(StringComparer.InvariantCultureIgnoreCase);
			int NextAttributeID = 0;
			foreach (GraphNode GraphNode in GraphNodes)
			{
				foreach (string AttributeName in GraphNode.Attributes.Keys)
				{
					object AttributeValue = GraphNode.Attributes[AttributeName];

					string AttributeTypeName;
					if (AttributeValue.GetType() == typeof(int))
					{
						AttributeTypeName = "integer";
					}
					else if (AttributeValue.GetType() == typeof(float))
					{
						AttributeTypeName = "float";
					}
					else if (AttributeValue.GetType() == typeof(double))
					{
						AttributeTypeName = "double";
					}
					else if (AttributeValue.GetType() == typeof(string))
					{
						AttributeTypeName = "string";
					}
					else if (AttributeValue.GetType() == typeof(bool))
					{
						AttributeTypeName = "boolean";
					}
					else
					{
						// No other types supported yet!
						throw new InvalidOperationException("Unsupported attribute data type encountered on graph node!");
					}


					GraphAttribute Attribute;
					if (!AllAttributes.TryGetValue(AttributeName, out Attribute))
					{
						Attribute = new GraphAttribute();
						Attribute.ID = NextAttributeID++;
						Attribute.Name = AttributeName;
						Attribute.TypeName = AttributeTypeName;

						AllAttributes[AttributeName] = Attribute;
					}
					else
					{
						if (!Attribute.TypeName.Equals(AttributeTypeName))
						{
							throw new InvalidOperationException("Multiple graph nodes with the same attribute name but different types encountered!");
						}
					}
				}
			}


			using (XmlWriter Writer = XmlWriter.Create(Filename, Settings))
			{
				// NOTE: The GEXF XML format is defined here:  http://gexf.net/1.2draft/gexf-12draft-primer.pdf

				string GEXFNamespace = "http://www.gexf.net/1.2-draft";
				string SchemaNamespace = "http://www.w3.org/2001/XMLSchema-instance";
				string VizNamespace = "http://www.gexf.net/1.2draft/viz";

				Writer.WriteStartElement("gexf", GEXFNamespace);
				Writer.WriteAttributeString("xmlns", "xsi", null, SchemaNamespace);
				Writer.WriteAttributeString("schemaLocation", SchemaNamespace, "http://www.gexf.net/1.2draft http://www.gexf.net/1.2draft/gexf.xsd");
				Writer.WriteAttributeString("xmlns", "viz", null, VizNamespace);
				Writer.WriteAttributeString("version", "1.2");

				Writer.WriteStartElement("meta");
				{
					Writer.WriteAttributeString("creator", "UnrealBuildTool");
					Writer.WriteAttributeString("description", Description);
				}
				Writer.WriteEndElement();	// meta

				{
					Writer.WriteStartElement("graph");
					{
						Writer.WriteAttributeString("mode", "static");
						Writer.WriteAttributeString("defaultedgetype", "directed");

						if (AllAttributes.Count > 0)
						{
							Writer.WriteStartElement("attributes");
							{
								// @todo: Add support for edge attributes, not just node attributes
								Writer.WriteAttributeString("class", "node");	// Node attributes, not edges!

								foreach (GraphAttribute Attribute in AllAttributes.Values)
								{
									Writer.WriteStartElement("attribute");
									{
										Writer.WriteAttributeString("id", Attribute.ID.ToString());
										Writer.WriteAttributeString("title", Attribute.Name);
										Writer.WriteAttributeString("type", Attribute.TypeName);
									}
									Writer.WriteEndElement();	// attribute
								}

								// @todo: Add support for attribute type default values
							}
							Writer.WriteEndElement();	// attributes

						}

						Writer.WriteStartElement("nodes");
						{
							foreach (GraphNode GraphNode in GraphNodes)
							{
								Writer.WriteStartElement("node");
								{
									Writer.WriteAttributeString("id", GraphNode.Id.ToString());
									Writer.WriteAttributeString("label", GraphNode.Label);

									Writer.WriteStartElement("color", VizNamespace);
									{
										Writer.WriteAttributeString("r", ((int)(GraphNode.Color.R * 255.0f)).ToString());
										Writer.WriteAttributeString("g", ((int)(GraphNode.Color.G * 255.0f)).ToString());
										Writer.WriteAttributeString("b", ((int)(GraphNode.Color.B * 255.0f)).ToString());
										Writer.WriteAttributeString("a", GraphNode.Color.A.ToString());
									}
									Writer.WriteEndElement();	// viz:color

									Writer.WriteStartElement("size", VizNamespace);
									{
										Writer.WriteAttributeString("value", GraphNode.Size.ToString());
									}
									Writer.WriteEndElement();	// viz:size

									Writer.WriteStartElement("shape", VizNamespace);
									{
										// NOTE: Valid shapes are:  disc, square, triangle, diamond, image
										Writer.WriteAttributeString("value", "disc");
									}
									Writer.WriteEndElement();	// viz:shape

									if (GraphNode.Attributes.Count > 0)
									{
										Writer.WriteStartElement("attvalues");
										{
											foreach (KeyValuePair<string, object> AttributeHashEntry in GraphNode.Attributes)
											{
												string AttributeName = AttributeHashEntry.Key;
												object AttributeValue = AttributeHashEntry.Value;

												GraphAttribute Attribute = AllAttributes[AttributeName];

												Writer.WriteStartElement("attvalue");
												{
													Writer.WriteAttributeString("for", Attribute.ID.ToString());
													Writer.WriteAttributeString("value", AttributeValue.ToString());
												}
												Writer.WriteEndElement();
											}
										}
										Writer.WriteEndElement();	// attvalues
									}
								}
								Writer.WriteEndElement();	// node
							}
						}
						Writer.WriteEndElement();	// nodes


						Writer.WriteStartElement("edges");
						{
							foreach (GraphEdge GraphEdge in GraphEdges)
							{
								Writer.WriteStartElement("edge");
								{
									Writer.WriteAttributeString("id", GraphEdge.Id.ToString());
									Writer.WriteAttributeString("source", GraphEdge.Source.Id.ToString());
									Writer.WriteAttributeString("target", GraphEdge.Target.Id.ToString());
									Writer.WriteAttributeString("weight", GraphEdge.Weight.ToString());

									Writer.WriteStartElement("color", VizNamespace);
									{
										Writer.WriteAttributeString("r", ((int)(GraphEdge.Color.R * 255.0f)).ToString());
										Writer.WriteAttributeString("g", ((int)(GraphEdge.Color.G * 255.0f)).ToString());
										Writer.WriteAttributeString("b", ((int)(GraphEdge.Color.B * 255.0f)).ToString());
										Writer.WriteAttributeString("a", GraphEdge.Color.A.ToString());
									}
									Writer.WriteEndElement();	// viz:color

									Writer.WriteStartElement("thickness", VizNamespace);
									{
										Writer.WriteAttributeString("value", GraphEdge.Thickness.ToString());
									}
									Writer.WriteEndElement();	// viz:thickness

									Writer.WriteStartElement("shape", VizNamespace);
									{
										// NOTE: Valid shapes are:  solid, dotted, dashed, double
										Writer.WriteAttributeString("value", "solid");
									}
									Writer.WriteEndElement();	// viz:shape
								}
								Writer.WriteEndElement();	// edge
							}
						}
						Writer.WriteEndElement();	// nodes
					}

					Writer.WriteEndElement();	// graph
				}
				Writer.WriteEndElement();	// gexf

				Writer.Flush();
			}
		}
	}

}