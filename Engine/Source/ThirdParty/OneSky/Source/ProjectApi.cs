using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading.Tasks;

namespace OneSky
{
    [DataContract]
    public class ListProjectsResponse
    {
        [DataContract]
        public class ListProjectsMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }

            [DataMember(Name = "record_count")]
            public int RecordCount { get; set; }
        }

        [DataContract]
        public class Project
        {
            [DataMember(Name = "id")]
            public int Id { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }
        }

        [DataMember(Name = "meta")]
        public ListProjectsMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public Project[] Data { get; set; }
    }

    [DataContract]
    public class ShowProjectResponse
    {
        [DataContract]
        public class ShowProjectResponseMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }
        }

        [DataContract]
        public class ProjectDetails
        {
            [DataContract]
            public class ProjectTypeInfo
            {
                [DataMember(Name = "code")]
                public string Code { get; set; }

                [DataMember(Name = "name")]
                public string Name { get; set; }
            }

            [DataMember(Name = "id")]
            public int Id { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }

            [DataMember(Name = "description")]
            public string Description { get; set; }

            [DataMember(Name = "project_type")]
            public ProjectTypeInfo ProjectType { get; set; }

            [DataMember(Name = "string_count")]
            public int StringCount { get; set; }

            [DataMember(Name = "word_count")]
            public int WordCount { get; set; }
        }

        [DataMember(Name = "meta")]
        public ShowProjectResponseMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public ProjectDetails Data { get; set; }
    }

    [DataContract]
    public class CreateProjectResponse
    {
        [DataContract]
        public class CreateProjectMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }
        }

        [DataContract]
        public class CreatedProject
        {
            [DataContract]
            public class ProjectTypeInfo
            {
                [DataMember(Name = "name")]
                public string Name { get; set; }

                [DataMember(Name = "code")]
                public string Code { get; set; }
            }

            [DataMember(Name = "id")]
            public int Id { get; set; }

            [DataMember(Name = "name")]
            public string Name { get; set; }

            [DataMember(Name = "description")]
            public string Description { get; set; }

            [DataMember(Name = "project_type")]
            public ProjectTypeInfo ProjectType { get; set; }
        }

        [DataMember(Name = "meta")]
        public CreateProjectMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public CreatedProject Data { get; set; }
    }

    [DataContract]
    public class ListProjectLanguagesResponse
    {
        [DataContract]
        public class ListProjectLanguagesMeta
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

            [DataMember(Name = "is_ready_to_publish")]
            public bool IsReadyToPublish { get; set; }
        }

        [DataMember(Name = "meta")]
        public ListProjectLanguagesMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public LanguageInfo[] Data { get; set; }
    }

    public class ProjectApi
    {
        public static ListProjectsResponse List(OneSkyService service, int projectGroupId)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project.md

            string url = service.AddAuthenticationParameters(String.Format("https://platform.api.onesky.io/1/project-groups/{0}/projects", projectGroupId));

            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ListProjectsResponse));
                return (ListProjectsResponse)jsonSerializer.ReadObject(stream);
            }
        }

        public static ShowProjectResponse Show(OneSkyService service, int projectId)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project.md

            string url = service.AddAuthenticationParameters(String.Format("https://platform.api.onesky.io/1/projects/{0}", projectId));

            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ShowProjectResponse));
                return (ShowProjectResponse)jsonSerializer.ReadObject(stream);
            }
        }

        public static CreateProjectResponse Create(OneSkyService service, int projectGroupId, string name, string description, string projectType)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project.md

            string url = String.Format("https://platform.api.onesky.io/1/project-groups/{0}/projects", projectGroupId);
            string parameters = service.GetAuthenticationParameters() + "&name=" + name + "&description=" + description + "&project_type=" + projectType;

            url += "?" + parameters;

            using (var client = new WebClient())
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(CreateProjectResponse));
                using (var stream = service.StringToStream(client.UploadString(url, parameters)))
                {
                    return (CreateProjectResponse)jsonSerializer.ReadObject(stream);
                }
            }
        }

        public static bool Update(OneSkyService service, int projectId, string name, string description)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project.md

            string url = String.Format("https://platform.api.onesky.io/1/projects/{0}", projectId);
            string parameters = service.GetAuthenticationParameters() + "&name=" + name + "&description=" + description;

            url += "?" + parameters;

            try
            {
                using (var client = new WebClient())
                {
                    client.UploadString(url, "PUT", parameters);
                }
                return true;
            }
            catch (WebException)
            {
                return false;
            }
        }

        public static bool Delete(OneSkyService service, int projectId)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project.md

            string url = service.AddAuthenticationParameters(string.Format("https://platform.api.onesky.io/1/projects/{0}", projectId));

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

        public static ListProjectLanguagesResponse ListLanguages(OneSkyService service, int projectId)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/project.md

            string url = service.AddAuthenticationParameters(String.Format("https://platform.api.onesky.io/1/projects/{0}/languages", projectId));

            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ListProjectLanguagesResponse));
                return (ListProjectLanguagesResponse)jsonSerializer.ReadObject(stream);
            }
        }
    }
}
