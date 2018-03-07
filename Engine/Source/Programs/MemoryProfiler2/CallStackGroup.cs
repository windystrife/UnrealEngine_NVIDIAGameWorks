// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Drawing;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;
using System.ComponentModel;
using System.Xml.Serialization;
using System.Globalization;

namespace MemoryProfiler2
{
    public class ClassGroup
    {
        [DescriptionAttribute("The name of this class collection.")]
        [XmlAttribute]
        public string Name { get; set; }

        [DescriptionAttribute("Whether to include this class group in the query.")]
        [XmlAttribute]
        public bool bFilter { get; set; }

        [DescriptionAttribute("The color to use for this group when rendering graphs.")]
        [XmlIgnore]
        public Color Color { get; set; }

        [XmlAttribute("Color")]
        [Browsable(false)]
        public string ColorString
        {
            // Specify the invariant culture when converting to and from string
            // format to avoid problems caused by differing locale settings.
            get { return TypeDescriptor.GetConverter(typeof(Color)).ConvertToString(null, CultureInfo.InvariantCulture, Color); }
            set { Color = (Color)TypeDescriptor.GetConverter(typeof(Color)).ConvertFrom(null, CultureInfo.InvariantCulture, value); }
        }

        private CallStackPattern[] callStackPatterns;
		[DescriptionAttribute( "A list of stack frames that this pattern matches." )]
        [XmlElement]
        public CallStackPattern[] CallStackPatterns
        {
            get
            {
                return callStackPatterns;
            }

            set
            {
                callStackPatterns = value;

                if (callStackPatterns == null)
                {
                    // Being able to assume that this is never null in
                    // the rest of the app means we don't clutter up the
                    // code with null checks everywhere.
                    callStackPatterns = new CallStackPattern[0];
                }
                else
                {
                    foreach (CallStackPattern pattern in callStackPatterns)
                    {
                        pattern.Group = this;
                    }
                }

            }
        }

        // Internal use only - don't allow the user to edit this property
        [Browsable(false)]
        [XmlIgnore]
        public int databaseId;

        public ClassGroup()
        {
            Name = "<Unknown>";
            bFilter = false;
            Color = Color.Black;
            callStackPatterns = new CallStackPattern[0];
        }
    }

    public enum PatternOrderGroup
    {
        // Specific patterns that need to be evaluated first,
        // e.g. Texture Pool.
        Specific,
        // Useful for patterns that represent groups of allocations
        // you might want to consider separately from everything else.
        // For example, allocations from middleware such as PhysX.
        SubPools,
        // More general patterns, e.g. Animations, Shaders, etc.
        // Most patterns are in this category.
        General,
        // Catch-all groups to pick up the rest of the callstacks
        // that the more specific ones miss. e.g. Misc UObjects
        CatchAll
    }

    public class CallStackPattern
    {
        // A list of stack frames that this Pattern Matches.
        // ALL of the listed patterns must match stack frames in the
        // callstack for this Pattern to match. The stack frames have
        // to be in the same order as the frame patterns, but they
        // don't have to be consecutive.
        //
        // For example, consider the Pattern: {"A::Function1()", "B::Function2()"}
        // The following callstack Matches it:
        // Blah()
        // A::Function1()
        // Blah2()
        // Blah3()
        // B::Function2()
        // Blah4()
        // 
        // The following callstack doesn't:
        // B::Function2()
        // A::Function1()
        // 
        // nor does this:
        // Blah()
        // A::Function1()
        // Blah2()
        //
        // Most callstack patterns will just have a single frame
        // Pattern, but allowing multiple frames gives a bit more
        // flexibility.
        private string[] _StackFramePatterns;
        [Description("A list of stack frames that this Pattern should match.")]
        [XmlElement]
        public string[] StackFramePatterns
        {
            get
            {
				return _StackFramePatterns;
            }

            set
            {
				_StackFramePatterns = value;
                MakeRegexes();
            }
        }

        private bool bUseRegexes;
        [Description("If true, this option causes all StackFramePatterns to be interpreted as regular expressions.")]
        [XmlAttribute]
        public bool UseRegexes
        {
            get
            {
                return bUseRegexes;
            }

            set
            {
                bUseRegexes = value;
                MakeRegexes();
            }
        }

        [Description("Group that determines the order in which patterns are applied. Specific is applied first, CatchAll last.")]
        [XmlAttribute]
        public PatternOrderGroup PatternOrderGroup { get; set; }

        [Description("By changing this property you can control how patterns are ordered within a group. The lower the number, the earlier the Pattern will be applied. Negative numbers are allowed.")]
        [XmlAttribute]
        public short OrderBias { get; set; }

		/// <summary> Compiled Regexes generated from StackFramePatterns. </summary>
        protected List<Regex> Regexes;

        [XmlIgnore]
        protected List<FCallStack> CallStacks = new List<FCallStack>();

		[XmlIgnore]
		protected HashSet<FCallStack> CallStacksSet = new HashSet<FCallStack>();

		/// <summary> Used for sorting. A single number that callstack patterns can be sorted on. </summary>
		[Browsable(false)]
        public int Ordinal
        {
            // Note that this value can go negative if PatternOrderGroup
            // is 0 and OrderBias is negative. This isn't a problem for
            // the current use cases though.
            get { return (int)PatternOrderGroup * (ushort.MaxValue + 1) + OrderBias; }
        }

        protected string _Description;

        [Browsable(false)]
        public string Description
        {
            get
            {
				if( _Description != null )
                {
					return _Description;
                }

                if (StackFramePatterns.Length == 0)
                {
                    return "Empty";
                }

				_Description = StackFramePatterns[ 0 ];
				return _Description;
            }
        }

        [XmlIgnore]
        public ClassGroup Group;

        public CallStackPattern()
        {
            StackFramePatterns = new string[0];
            PatternOrderGroup = PatternOrderGroup.General;
        }

		public void AddCallStack(FCallStack NewCallStack)
		{
			CallStacks.Add(NewCallStack);
			CallStacksSet.Add(NewCallStack);
		}

		public void AddCallStacks(IEnumerable<FCallStack> NewCallStacks)
		{
			foreach (FCallStack NewCallStack in NewCallStacks)
			{
				CallStacks.Add(NewCallStack);
				CallStacksSet.Add(NewCallStack);
			}
		}

		public bool ContainsCallStack(FCallStack OtherCallStack)
		{
			return CallStacksSet.Contains(OtherCallStack);
		}

		public IEnumerable<FCallStack> GetCallStacks()
		{
			return CallStacks;
		}

		public bool Matches(CallStackPattern Other)
        {
            if (Other.StackFramePatterns.Length != StackFramePatterns.Length || Other.bUseRegexes != bUseRegexes)
            {
                return false;
            }

            for (int i = 0; i < StackFramePatterns.Length; i++)
            {
                if (Other.StackFramePatterns[i] != StackFramePatterns[i])
                {
                    return false;
                }
            }
            
            return true;
        }

        public bool Matches(FCallStack CallStack)
        {
            if (StackFramePatterns.Length == 0)
            {
                return false;
            }

            int PatternIndex = 0;

			if( bUseRegexes )
			{
				for( int i = 0; i < CallStack.AddressIndices.Count; i++ )
				{
					string CallStackMethod = FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ CallStack.AddressIndices[ i ] ].FunctionIndex ];
					if( Regexes[ PatternIndex ].Match( CallStackMethod ).Success )
					{
						PatternIndex++;
						if( PatternIndex >= StackFramePatterns.Length )
						{
							return true;
						}
					}
				}
			}
			else
			{
				for( int i = 0; i < CallStack.AddressIndices.Count; i++ )
				{
					string CallStackMethod = FStreamInfo.GlobalInstance.NameArray[ FStreamInfo.GlobalInstance.CallStackAddressArray[ CallStack.AddressIndices[ i ] ].FunctionIndex ];
					if( CallStackMethod == StackFramePatterns[ PatternIndex ] )
					{
						PatternIndex++;
						if( PatternIndex >= StackFramePatterns.Length )
						{
							return true;
						}
					}
				}
			}

            return false;
        }


        private void MakeRegexes()
        {
            if (bUseRegexes)
            {
                Regexes = new List<Regex>();
                foreach (string Pattern in StackFramePatterns)
                {
                    Regexes.Add(new Regex(Pattern, RegexOptions.Compiled | RegexOptions.CultureInvariant));
                }
            }
            else
            {
                Regexes = null;
            }
        }
    }
}
