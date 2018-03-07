using System.Net;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;

namespace OneSky
{
    [DataContract]
    public class ListProjectTypesResponse
    {
        [DataContract]
        public class ListProjectTypesMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }

            [DataMember(Name = "record_count")]
            public int RecordCount { get; set; }
        }

        [DataContract]
        public class ProjectTypeInfo
        {
            [DataMember(Name = "code")]
            public string Code { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }
        }

        [DataMember(Name = "meta")]
        public ListProjectTypesMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public ProjectTypeInfo[] Data { get; set; }
    }

    public class ProjectTypeApi
    {
        public static ListProjectTypesResponse List(OneSkyService service)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project_type.md

            string url = service.AddAuthenticationParameters("https://platform.api.onesky.io/1/project-types");

            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ListProjectTypesResponse));
                return (ListProjectTypesResponse)jsonSerializer.ReadObject(stream);
            }
        }
    }
}
