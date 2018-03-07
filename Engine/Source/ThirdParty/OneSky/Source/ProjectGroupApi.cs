using System;
using System.Diagnostics;
using System.Globalization;
using System.Net;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;

namespace OneSky
{
    [DataContract]
    public class ListProjectGroupsResponse
    {
        [DataContract]
        public class ListProjectGroupsMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }

            [DataMember(Name = "record_count")]
            public int RecordCount { get; set; }
        }

        [DataContract]
        public class ProjectGroup
        {
            [DataMember(Name = "id")]
            public int Id { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }
        }

        [DataMember(Name = "meta")]
        public ListProjectGroupsMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public ProjectGroup[] Data { get; set; }
    }

    [DataContract]
    public class ShowProjectGroupResponse
    {
        [DataContract]
        public class ShowProjectGroupMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }
        }

        [DataContract]
        public class ProjectGroupDetails
        {
            [DataContract]
            public class LanguageInfo
            {
                [DataMember(Name = "code")]
                public string Code { get; set; }

                [DataMember(Name = "english_name")]
                public string EnglishName { get; set; }

                [DataMember(Name = "local_name")]
                public string LocalName { get; set; }

                [DataMember(Name = "locale")]
                public string Locale { get; set; }

                [DataMember(Name = "region")]
                public string Region { get; set; }   
            }
            
            [DataMember(Name = "id")]
            public int Id { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }

            [DataMember(Name = "base_language")]
            public LanguageInfo BaseLanguage { get; set; }

            [DataMember(Name = "enabled_language_count")]
            public int EnabledLanguageCount { get; set; }

            [DataMember(Name = "project_count")]
            public int ProjectCount { get; set; }
        }

        [DataMember(Name = "meta")]
        public ShowProjectGroupMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public ProjectGroupDetails Data { get; set; }
    }

    [DataContract]
    public class CreateProjectGroupResponse
    {
        [DataContract]
        public class CreateProjectGroupMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }
        }

        [DataContract]
        public class CreatedProjectGroup
        {
            [DataContract]
            public class LanguageInfo
            {
                [DataMember(Name = "code")]
                public string Code { get; set; }

                [DataMember(Name = "english_name")]
                public string EnglishName { get; set; }

                [DataMember(Name = "local_name")]
                public string LocalName { get; set; }

                [DataMember(Name = "locale")]
                public string Locale { get; set; }

                [DataMember(Name = "region")]
                public string Region { get; set; }
            }

            [DataMember(Name = "id")]
            public int Id { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }

            [DataMember(Name = "base_language")]
            public LanguageInfo BaseLanguage { get; set; }
        }

        [DataMember(Name = "meta")]
        public CreateProjectGroupMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public CreatedProjectGroup Data { get; set; }
    }

    [DataContract]
    public class ListProjectGroupLanguagesResponse
    {
        [DataContract]
        public class ListProjectGroupLanguagesMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }

            [DataMember(Name = "record_count")]
            public int RecordCount { get; set; }
        }

        [DataContract]
        public class LanguageInfo
        {
            [DataMember(Name = "code")]
            public string Code { get; set; }

            [DataMember(Name = "english_name")]
            public string EnglishName { get; set; }

            [DataMember(Name = "local_name")]
            public string LocalName { get; set; }

            [DataMember(Name = "locale")]
            public string Locale { get; set; }

            [DataMember(Name = "region")]
            public string Region { get; set; }

            [DataMember(Name = "is_base_language")]
            public bool IsBaseLanguage { get; set; }
        }

        [DataMember(Name = "meta")]
        public ListProjectGroupLanguagesMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public LanguageInfo[] Data { get; set; }
    }

    public class ProjectGroupApi
    {
        public static ListProjectGroupsResponse List(OneSkyService service, int startPage, int itemsPerPage)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project_group.md
            Debug.Assert(startPage >= 1);
            Debug.Assert(itemsPerPage >= 1);

            string url = service.AddAuthenticationParameters("https://platform.api.onesky.io/1/project-groups");
            url += "&page=" + startPage + "&per_page=" + itemsPerPage;

            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ListProjectGroupsResponse));
                return (ListProjectGroupsResponse)jsonSerializer.ReadObject(stream);
            }
        }

        public static ShowProjectGroupResponse Show(OneSkyService service, int projectGroupId)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project_group.md

            string url = service.AddAuthenticationParameters(string.Format("https://platform.api.onesky.io/1/project-groups/{0}", projectGroupId));

            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ShowProjectGroupResponse));
                return (ShowProjectGroupResponse)jsonSerializer.ReadObject(stream);
            }
        }

        public static CreateProjectGroupResponse Create(OneSkyService service, string name, string cultureName)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project_group.md
            string url = "https://platform.api.onesky.io/1/project-groups";
            string parameters = service.GetAuthenticationParameters() + "&name=" + name + "&locale=" + cultureName;

            url += "?" + parameters;

            using (var client = new WebClient())
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(CreateProjectGroupResponse));
                using (var stream = service.StringToStream(client.UploadString(url, parameters)))
                {
                    return (CreateProjectGroupResponse)jsonSerializer.ReadObject(stream);
                }
            }
        }

        public static bool Delete(OneSkyService service, int projectGroupId)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project_group.md
 
            string url = service.AddAuthenticationParameters(string.Format("https://platform.api.onesky.io/1/project-groups/{0}", projectGroupId));

            try
            {
                using (var client = new WebClient())
                {
                    client.UploadString(url, "DELETE", "");
                }
                return true;
            }
            catch (WebException)
            {
                return false;
            }
        }

        public static ListProjectGroupLanguagesResponse ListEnabledLanguages(OneSkyService service, int projectGroupId)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project_group.md

            string url = service.AddAuthenticationParameters(string.Format("https://platform.api.onesky.io/1/project-groups/{0}/languages", projectGroupId));

            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ListProjectGroupLanguagesResponse));
                return (ListProjectGroupLanguagesResponse)jsonSerializer.ReadObject(stream);
            }
        }
    }
}
