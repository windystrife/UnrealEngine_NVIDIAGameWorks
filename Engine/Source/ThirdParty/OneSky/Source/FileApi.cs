using System;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Net;
using System.Net.Http;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Threading.Tasks;

namespace OneSky
{
    [DataContract]
    public class ListUploadedFilesResponse
    {
        [DataContract]
        public class ListUploadedFilesResponseMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }

            [DataMember(Name = "record_count")]
            public int RecordCount { get; set; }
        }

        [DataContract]
        public class UploadedFile
        {
            [DataContract]
            public class ImportInfo
            {
                [DataMember(Name = "id")]
                public int Id { get; set; }

                [DataMember(Name = "status")]
                public string Status { get; set; }
            }

            [DataMember(Name = "file_name")]
            public string Name { get; set; }

            [DataMember(Name = "string_count")]
            public int StringCount { get; set; }

            [DataMember(Name = "last_import")]
            public ImportInfo LastImport { get; set; }

            [DataMember(Name = "uploaded_at")]
            public string UploadedAt { get; set; }

            [DataMember(Name = "uploaded_at_timestamp")]
            public int UploadedAtTimestamp { get; set; }
        }

        [DataMember(Name = "meta")]
        public ListUploadedFilesResponseMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public UploadedFile[] Data { get; set; }
    }

    [DataContract]
    public class UploadFileResponse
    {
        [DataContract]
        public class UploadFileResponseMeta
        {
            [DataMember(Name = "status")]
            public int Status { get; set; }

            [DataMember(Name = "message")]
            public string Message { get; set; }
        }

        [DataContract]
        public class UploadedFile
        {
            [DataContract]
            public class LanguageInfo
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

            [DataContract]
            public class ImportInfo
            {
                [DataMember(Name = "id")]
                public int Id { get; set; }

                [DataMember(Name = "created_at")]
                public string CreatedAt { get; set; }

                [DataMember(Name = "created_at_timestamp")]
                public int CreatedAtTimestamp { get; set; }
            }

            [DataMember(Name = "name")]
            public string Name { get; set; }

            [DataMember(Name = "format")]
            public string Format { get; set; }

            [DataMember(Name = "language")]
            public LanguageInfo Language { get; set; }

            [DataMember(Name = "import")]
            public ImportInfo Import { get; set; }
        }

        [DataMember(Name = "meta")]
        public UploadFileResponseMeta Meta { get; set; }

        [DataMember(Name = "data")]
        public UploadedFile Data { get; set; }
    }

    public class FileApi
    {
        public static ListUploadedFilesResponse List(OneSkyService service, int projectId, int startPage, int itemsPerPage)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/file.md#upload---upload-a-file
            Debug.Assert(startPage >= 1);
            Debug.Assert(itemsPerPage >= 1);

            string url = service.AddAuthenticationParameters(String.Format("https://platform.api.onesky.io/1/projects/{0}/files", projectId));
			url += "&page=" + startPage + "&per_page=" + itemsPerPage;
	
            using (var client = new WebClient())
            using (var stream = client.OpenRead(url))
            {
                var jsonSerializer = new DataContractJsonSerializer(typeof(ListUploadedFilesResponse));
                return (ListUploadedFilesResponse)jsonSerializer.ReadObject(stream);
            }
        }


        public static UploadFileResponse HttpWebRequestUploadFile(OneSkyService service, string url, string file, string paramName, Stream fileStream, string contentType)
        {
            string boundary = "---------------------------" + DateTime.Now.Ticks.ToString("x");
            byte[] boundarybytes = System.Text.Encoding.ASCII.GetBytes("\r\n--" + boundary + "\r\n");

            HttpWebRequest req = (HttpWebRequest)WebRequest.Create(url);
            req.ContentType = "multipart/form-data; boundary=" + boundary;
            req.Method = "POST";
            req.KeepAlive = true;

            using (var DataStream = new BinaryWriter(req.GetRequestStream(), System.Text.Encoding.UTF8))
            {
                DataStream.Write(boundarybytes, 0, boundarybytes.Length);

                string headerTemplate = "Content-Disposition: form-data; name=\"{0}\"; filename=\"{1}\"\r\nContent-Type: {2}\r\n\r\n";
                string header = string.Format(headerTemplate, paramName, file, contentType);
                byte[] headerbytes = System.Text.Encoding.UTF8.GetBytes(header);
                DataStream.Write(headerbytes, 0, headerbytes.Length);


                byte[] buffer = new byte[4096];
                int bytesRead = 0;
                while ((bytesRead = fileStream.Read(buffer, 0, buffer.Length)) != 0)
                {
                    DataStream.Write(buffer, 0, bytesRead);
                }

                byte[] trailer = System.Text.Encoding.ASCII.GetBytes("\r\n--" + boundary + "--\r\n");
                DataStream.Write(trailer, 0, trailer.Length);
                DataStream.Flush();
                DataStream.Close();
            }
            
            try
            {
                using (HttpWebResponse Response = (HttpWebResponse)req.GetResponse())
                {

                    using (Stream rawResponseStream = Response.GetResponseStream())
                    {
                        var responseText = service.StreamToString(rawResponseStream);
                        if (!string.IsNullOrWhiteSpace(responseText))
                        {
                            using (var responseStream = service.StringToStream(responseText))
                            {
                                var jsonSerializer = new DataContractJsonSerializer(typeof(UploadFileResponse));
                                return (UploadFileResponse)jsonSerializer.ReadObject(responseStream);
                            }
                        }
                    }
                }
            }
            catch (WebException)
            {
            }
            return null;
        }


        public static Task<UploadFileResponse> Upload(OneSkyService service, int projectId, string filename, Stream stream, string cultureName)
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/resources/file.md#upload---upload-a-file
            Debug.Assert(stream != null);
            Debug.Assert(cultureName != null);

            string url = service.AddAuthenticationParameters(String.Format("https://platform.api.onesky.io/1/projects/{0}/files", projectId));

            string fileFormat = GetFileFormat(Path.GetExtension(filename));
            if (fileFormat == null)
            {
                return null;
            }

            url += "&file_format=" + fileFormat + "&locale=" + cultureName + "&is_keeping_all_strings=false";

            return Task.Factory.StartNew(() => {

                return HttpWebRequestUploadFile(service, url, filename, "file", stream, fileFormat);
            });
        }

        private static string GetFileFormat(string fileExtension)
        {
            switch (fileExtension)
            {
                case ".po":
                    return "GNU_PO";
                case ".pot":
                    return "GNU_POT";
                default:
                    return null;
            }
        }
    }
}
