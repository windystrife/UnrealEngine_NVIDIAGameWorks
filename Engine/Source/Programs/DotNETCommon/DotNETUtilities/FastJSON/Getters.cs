using System;
using System.Collections.Generic;

#if !__MonoCS__
#pragma warning disable CS1591
#endif
namespace fastJSON
{
    public sealed class DatasetSchema
    {
        public List<string> Info { get; set; }
        public string Name { get; set; }
    }
}
#if !__MonoCS__
#pragma warning restore CS1591
#endif
