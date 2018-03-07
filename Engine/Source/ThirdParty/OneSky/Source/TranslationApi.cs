using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace OneSky
{
    public enum TranslationExport
    {
        Accepted,
        NoContent,
        Completed
    }

    [DataContract]
    public class TranslationStatusResponse
    {
        [DataContract]
        public class TranslationStatusResponseMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }

            [DataMember(Name = "message")]
            public string Message { get; set; }
        }

        [DataContract]
        public class TranslatedFile
        {
            [DataContract]
            public class LocaleInfo
            {
                [DataMember(Name = "code")]
                public string Code { get; set; }

                [DataMember(Name = "english_name")]
                public string EnglishName { get; set; }

                [DataMember(Name = "local_name")]
                public string LocaleName { get; set; }

                [DataMember(Name = "locale")]
                public string Locale { get; set; }

                [DataMember(Name = "region")]
                public string Region { get; set; }
            }

            [DataMember(Name = "file_name")]
            public string Name { get; set; }

            [DataMember(Name = "progress")]
            public string Progress { get; set; }

            [DataMember(Name = "string_count")]
            public int StringCount { get; set; }

            [DataMember(Name = "word_count")]
            public int WordCount { get; set; }

            [DataMember(Name = "locale")]
            public TranslatedFile.LocaleInfo Language { get; set; }
        }

        [DataMember(Name = "meta")]
        public TranslationStatusResponseMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public TranslationStatusResponse.TranslatedFile Data { get; set; }
    }

    public class TranslationApi
    {
        public static TranslationExport Export(OneSkyService service, int projectId, string sourceFilename, string cultureName, Stream destinationStream)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/translation.md
            Debug.Assert(!String.IsNullOrWhiteSpace(sourceFilename));
            Debug.Assert(cultureName != null);

            string url = service.AddAuthenticationParameters(String.Format("https://platform.api.onesky.io/1/projects/{0}/translations", projectId));
            url += "&locale=" + cultureName + "&source_file_name=" + sourceFilename;

            var req = (HttpWebRequest)WebRequest.Create(url);
            req.Method = "GET";
            using (HttpWebResponse Response = (HttpWebResponse)req.GetResponse()) 
            {
                if (Response.StatusCode == HttpStatusCode.Accepted)
                {
                    return TranslationExport.Accepted;
                }
                else if (Response.StatusCode == HttpStatusCode.NoContent)
                {
                    return TranslationExport.NoContent;
                }

                using (Stream rawResponseStream = Response.GetResponseStream())
                {
                    rawResponseStream.CopyTo(destinationStream);
                }
                return TranslationExport.Completed;
            }
        }

        public class TranslationExportStatus
        {
            public string Progress { get; set; }
        }

        public static Task Status(OneSkyService service, int projectId, string filename, string cultureName)
        {
            return Task.Factory.StartNew((obj) =>
            {
                //https://github.com/onesky/api-documentation-platform/blob/master/resources/translation.md
                Debug.Assert(!String.IsNullOrWhiteSpace(filename));
                Debug.Assert(cultureName != null);

                string url = service.AddAuthenticationParameters(String.Format("https://platform.api.onesky.io/1/projects/{0}/translations/status", projectId));
                url += "&locale=" + cultureName + "&file_name=" + filename;

                var state = (TranslationExportStatus)obj;
                TranslationStatusResponse response;

                using (var client = new WebClient())
                using (var stream = client.OpenRead(url))
                {
                    var jsonSerializer = new DataContractJsonSerializer(typeof(TranslationStatusResponse));
                    response = (TranslationStatusResponse)jsonSerializer.ReadObject(stream);
                }

                state.Progress = response.Data.Progress;

                while (response.Meta.Status == 200 && response.Data.Progress != "100%")
                {
                    Thread.Sleep(5000); // Wait 5 seconds
                    state.Progress = response.Data.Progress;

                    using (var client = new WebClient())
                    using (var stream = client.OpenRead(url))
                    {
                        var jsonSerializer = new DataContractJsonSerializer(typeof(TranslationStatusResponse));
                        response = (TranslationStatusResponse)jsonSerializer.ReadObject(stream);
                    }
                }
            }, new TranslationExportStatus());
        }
    }
}
