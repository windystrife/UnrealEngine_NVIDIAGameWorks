using System;
using System.Collections;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.Globalization;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.IO;

namespace OneSky
{
    public enum ImportTaskStatus
    {
        Completed,
        InProgress,
        Failed
    }

    public class ImportTask
    {
        public int Id { get; set; }
        public ImportTaskStatus Status { get; set; }
    }

    public class UploadedFile
    {
        public string Filename { get; set; }
        public int StringCount { get; set; }
        public DateTime UploadedAt { get; set; }
        public ImportTask LastImport { get; set; }
        public Project OwningProject { get; set; }

        internal OneSkyService Connection { get; set; }

        public enum ExportTranslationState
        {
            Failure,
            NoContent,
            Success
        }

        public Task<ExportTranslationState> ExportTranslation(string cultureName, Stream destinationStream)
        {
            return Task.Factory.StartNew(() =>
            {
                var export = TranslationApi.Export(Connection, OwningProject.Id, Filename, cultureName, destinationStream);

                while (export == TranslationExport.Accepted) //Began exporting translations
                {
                    Task statusTask = TranslationApi.Status(Connection, OwningProject.Id, Filename, cultureName);
                    statusTask.Wait();

                    export = TranslationApi.Export(Connection, OwningProject.Id, Filename, cultureName, destinationStream);
                }

                if (export == TranslationExport.Completed)
                {
                    return ExportTranslationState.Success;
                }
                
                if (export == TranslationExport.NoContent)
                {
                    return ExportTranslationState.NoContent;
                }

                return ExportTranslationState.Failure;
            });
        }
    }

    public class ProjectType
    {
        public string Code { get; set; }
        public string DisplayName { get; set; }
    }

    public class Project
    {
        public int Id { get; set; }
        public string Name { get; set; }

        private string _description;

        public string Description 
        {
            get
            {
                if (_description == null)
                {
                    LoadDetails();
                }

                return _description;
            }
        }

        private int _stringCount = -1;
        public int StringCount
        {
            get
            {
                if (_stringCount == -1)
                {
                    LoadDetails();
                }

                return _stringCount;
            }
        }

        private int _wordCount = -1;
        public int WordsCount
        {
            get
            {
                if (_wordCount == -1)
                {
                    LoadDetails();
                }

                return _wordCount;
            }
        }

        private ProjectType _projectType;
        public ProjectType Type
        {
            get
            {
                if (_projectType == null)
                {
                    LoadDetails();
                }

                return _projectType;
            }
        }

        internal OneSkyService Connection { get; set; }

        internal Project()
        {

        }

        public Project(string name, string description, ProjectType type)
        {
            Name = name;
            _description = description;
            _projectType = type;
        }

        private string _baseCultureName;
        public string BaseCultureName
        {
            get
            {
                if (_baseCultureName == null)
                {
                    var response = ProjectApi.ListLanguages(Connection, Id);
                    _cultures = response.Data.Select(c => c.Code);

                    var baseLanguage = response.Data.FirstOrDefault(c => c.IsBaseLanguage);

                    if (baseLanguage != null)
                    {
                        _baseCultureName = baseLanguage.Code;
                    }
                }

                return _baseCultureName;
            }
        }

        private IEnumerable<string> _cultures;
        public IEnumerable<string> EnabledCultures
        {
            get
            {
                if (_cultures == null)
                {
                    var response = ProjectApi.ListLanguages(Connection, Id);
                    _cultures = response.Data.Select(c => c.Code);

                    var baseLanguage = response.Data.FirstOrDefault(c => c.IsBaseLanguage);

                    if (baseLanguage != null)
                    {
                        _baseCultureName = baseLanguage.Code;
                    }
                }

                return _cultures;
            }
        }

        public IEnumerable<UploadedFile> UploadedFiles
        {
            get
            {
                ListUploadedFilesResponse response;
                int startPage = 0;
                const int itemsPerPage = 50;

                do
                {
                    ++startPage;
                    response = FileApi.List(Connection, Id, startPage, itemsPerPage);

                    if (response == null)
                    {
                        continue;
                    }

                    foreach (var uploadedFileInfo in response.Data)
                    {
                        var importStatus = ImportTaskStatus.InProgress;
                        if (uploadedFileInfo.LastImport != null)
                        {
                            switch (uploadedFileInfo.LastImport.Status)
                            {
                                case "completed":
                                    importStatus = ImportTaskStatus.Completed;
                                    break;
                                case "in-progress":
                                    importStatus = ImportTaskStatus.InProgress;
                                    break;
                                case "failed":
                                    importStatus = ImportTaskStatus.Failed;
                                    break;
                            }
                        }

                        yield return new UploadedFile
                        {
                            Filename = uploadedFileInfo.Name,
                            StringCount = uploadedFileInfo.StringCount,
                            UploadedAt = new DateTime(uploadedFileInfo.UploadedAtTimestamp),
                            LastImport = (uploadedFileInfo.LastImport == null) ? null : new ImportTask
                            {
                                Id = uploadedFileInfo.LastImport.Id,
                                Status = importStatus
                            },
                            OwningProject = this,
                            Connection = Connection
                        };
                    }
                }
				while (response != null && response.Data.Count() > 0 && response.Meta.RecordCount > 0 && !(response.Meta.RecordCount < itemsPerPage) && response.Meta.Status == 200);
            }
        }

        public Task<UploadedFile> Upload(string filename, Stream stream, string cultureName)
        {
            return Task.Factory.StartNew(() =>
            {
                if (stream == null)
                {
                    return null;
                }

                var response = FileApi.Upload(Connection, Id, filename, stream, cultureName).Result;

                if (response != null && response.Meta.Status == 201)
                {
                    return new UploadedFile
                    {
                        Filename = response.Data.Name,
                        StringCount = 0,
                        UploadedAt = new DateTime(response.Data.Import.CreatedAtTimestamp),
                        LastImport = new ImportTask
                        {
                            Id = response.Data.Import.Id,
                            Status = ImportTaskStatus.InProgress
                        }
                    };
                }

                if (response != null && !string.IsNullOrWhiteSpace(response.Meta.Message))
                {
                    Console.WriteLine(response.Meta.Message);
                }

                return null;
            });
        }

        private void LoadDetails()
        {
            var response = ProjectApi.Show(Connection, Id);

            if (response.Data != null)
            {
                _description = response.Data.Description ?? string.Empty;
                _projectType = new ProjectType{ Code = response.Data.ProjectType.Code, DisplayName = response.Data.ProjectType.Name };
                _stringCount = response.Data.StringCount;
                _wordCount = response.Data.WordCount;
            }
        }

        public override bool Equals(Object obj)
        {
            // If parameter is null return false.
            if (obj == null)
            {
                return false;
            }

            // If parameter cannot be cast to Project return false.
            var p = obj as Project;
            if (p == null)
            {
                return false;
            }

            // Return true if the fields match:
            return (p.Id == Id);
        }

        public bool Equals(Project p)
        {
            // If parameter is null return false:
            if (p == null)
            {
                return false;
            }

            // Return true if the fields match:
            return (p.Id == Id);
        }

        public override int GetHashCode()
        {
            return Id;
        }
    }

    public class ProjectGroup
    {
        public int Id { get; set; }
        public string Name { get; set; }
        internal OneSkyService Connection { get; set; }

        private readonly MyCollection<Project> _projects;
        public MyCollection<Project> Projects
        {
            get { return _projects; }
        }

        public string BaseCultureName { get; set; }

        private IEnumerable<string> _cultures;
        public IEnumerable<string> EnabledCultures
        {
            get
            {
                if (_cultures == null)
                {
                    var response = ProjectGroupApi.ListEnabledLanguages(Connection, Id);
                    _cultures = response.Data.Select(c => c.Code);

                    var baseLanguage = response.Data.FirstOrDefault(c => c.IsBaseLanguage);

                    if (baseLanguage != null)
                    {
                        BaseCultureName = baseLanguage.Code;
                    }
                }

                return _cultures;
            }
        }

        internal ProjectGroup()
        {
            _projects = new MyCollection<Project>(OnLoadProjects, OnCreateProject, OnDeleteProject);
        }

        public ProjectGroup(string name, string baseCultureName)
        {
            Name = name;
            BaseCultureName = baseCultureName;

            _projects = new MyCollection<Project>(OnLoadProjects, OnCreateProject, OnDeleteProject);
        }

        private IEnumerable<Project> OnLoadProjects()
        {
            var response = ProjectApi.List(Connection, Id);
            return response.Data.Select(p => new Project
            {
                Id = p.Id,
                Name = p.Name,
                Connection = Connection
            });
        }

        private Project OnCreateProject(Project newProject)
        {
            var response = ProjectApi.Create(Connection, Id, newProject.Name, newProject.Description, newProject.Type.Code);

            if (response.Meta.Status == 201 && response.Data != null)
            {
                newProject.Id = response.Data.Id;
                newProject.Connection = Connection;
            }
            else
            {
                newProject = null;
            }

            return newProject;
        }

        private bool OnDeleteProject(Project project)
        {
            return ProjectApi.Delete(Connection, project.Id);
        }

        public override bool Equals(Object obj)
        {
            // If parameter is null return false.
            if (obj == null)
            {
                return false;
            }

            // If parameter cannot be cast to ProjectGroup return false.
            var p = obj as ProjectGroup;
            if (p == null)
            {
                return false;
            }

            // Return true if the fields match:
            return (p.Id == Id);
        }

        public bool Equals(ProjectGroup p)
        {
            // If parameter is null return false:
            if (p == null)
            {
                return false;
            }

            // Return true if the fields match:
            return (p.Id == Id);
        }

        public override int GetHashCode()
        {
            return Id;
        }
    }

    public class MyCollection<T> : ICollection<T>
        where T : class
    {
        private readonly Func<IEnumerable<T>> _load;
        private readonly Func<int, int, IEnumerable<T>> _loadByPage;
        private readonly Func<T, T> _create;
        private readonly Func<T, bool> _delete;

        private readonly bool _isReadOnly;

        private int _loadedPage;
        private readonly List<T> _items = new List<T>();

        public MyCollection(Func<int, int, IEnumerable<T>> loadByPage)
        {
            _loadByPage = loadByPage;
            _isReadOnly = true;
        }

        public MyCollection(Func<IEnumerable<T>> load)
        {
            _load = load;
            _isReadOnly = true;
        }

        public MyCollection(Func<int, int, IEnumerable<T>> loadByPage, Func<T, T> create, Func<T, bool> delete )
        {
            _loadByPage = loadByPage;
            _create = create;
            _delete = delete;
            _isReadOnly = false;
        }

        public MyCollection(Func<IEnumerable<T>> load, Func<T, T> create, Func<T, bool> delete)
        {
            _load = load;
            _create = create;
            _delete = delete;
            _isReadOnly = false;
        }

        IEnumerator IEnumerable.GetEnumerator()
        {
            return (IEnumerator)GetEnumerator();
        }

        public IEnumerator<T> GetEnumerator()
        {
            const int itemsPerPage = 50;
            int currentPage = 0;

            do
            {
                if (_loadByPage != null && currentPage == _loadedPage && _loadedPage >= 0)
                {
                    ++_loadedPage;
                    ++currentPage;
                    IEnumerable<T> pageResults = _loadByPage(currentPage, itemsPerPage);

                    int newPageResultsCount = 0;
                    if (pageResults != null)
                    {
                        int currentItemCount = _items.Count;
                        _items.AddRange(pageResults);

                        newPageResultsCount = _items.Count - currentItemCount;
                    }

                    if (newPageResultsCount != itemsPerPage)
                    {
                        _loadedPage = -1;
                    }
                }

                if (_load != null && _items.Count == 0)
                {
                    _items.AddRange(_load());
                }

                for (int index = 0; index < _items.Count; index++)
                {
                    yield return _items[index];
                }
            }
            while (_loadByPage != null && _loadedPage >= 0);
        }

        private void LoadAll()
        {
            var iter = GetEnumerator();
            while (iter.MoveNext()) { }
        }

        public void Add(T item)
        {
            item = _create(item);

            if (item != null)
            {
                _items.Add(item);
            }
        }

        public void Clear()
        {
            _items.Clear();
        }

        public bool Contains(T item)
        {
            LoadAll();
            return _items.Contains(item);
        }

        public void CopyTo(T[] array, int arrayIndex)
        {
            LoadAll();
            _items.CopyTo(array, arrayIndex);
        }

        public bool Remove(T item)
        {
            if (_delete(item))
            {
                _items.Remove(item);
                return true;
            }

            return false;
        }

        public int Count
        {
            get
            {
                LoadAll(); 
                return _items.Count;
            }
        }

        public bool IsReadOnly
        {
            get { return _isReadOnly; }
        }
    }

    public class OneSkyService
    {
        private readonly string _apiKey;
        private readonly string _apiSecret;

        private readonly MyCollection<ProjectGroup> _projectGroups;
        public MyCollection<ProjectGroup> ProjectGroups
        {
            get { return _projectGroups; }
        }

        private readonly MyCollection<ProjectType> _projectTypes;
        public MyCollection<ProjectType> ProjectTypes
        {
            get { return _projectTypes; }
        }

        public OneSkyService(string apiKey, string apiSecret)
        {
            _apiKey = apiKey;
            _apiSecret = apiSecret;

            _projectGroups = new MyCollection<ProjectGroup>(OnLoadProjectGroups, OnCreateProjectGroup, OnDeleteProjectGroup);
            _projectTypes = new MyCollection<ProjectType>(OnLoadProjectTypes);
        }

        private IEnumerable<ProjectType> OnLoadProjectTypes()
        {
            var response = ProjectTypeApi.List(this);
            return response.Data.Select(p => new ProjectType
            {
                Code = p.Code,
                DisplayName = p.Name,
            });
        }

        private IEnumerable<ProjectGroup> OnLoadProjectGroups(int page, int itemsPerPage)
        {
            var response = ProjectGroupApi.List(this, page, itemsPerPage);

            if (response != null)
            {
                foreach (var projectGroupInfo in response.Data)
                {
                    yield return new ProjectGroup
                    {
                        Id = projectGroupInfo.Id,
                        Name = new string(projectGroupInfo.Name.TakeWhile(c => c != '\0').ToArray()),
                        Connection = this
                    };
                }
            }
        }

        private ProjectGroup OnCreateProjectGroup(ProjectGroup newProjectGroup)
        {
            var response = ProjectGroupApi.Create(this, newProjectGroup.Name, newProjectGroup.BaseCultureName);

            if (response.Meta.Status == 201 && response.Data != null)
            {
                newProjectGroup.Id = response.Data.Id;
                newProjectGroup.Connection = this;
            }
            else
            {
                newProjectGroup = null;
            }

            return newProjectGroup;
        }

        private bool OnDeleteProjectGroup(ProjectGroup projectGroup)
        {
            return ProjectGroupApi.Delete(this, projectGroup.Id);
        }

        internal string AddAuthenticationParameters(string url)
        {
            return url + "?" + GetAuthenticationParameters();
        }

        internal string GetAuthenticationParameters()
        {
            //https://github.com/onesky/api-documentation-platform/blob/master/README.md#authentication

            string timestamp = ((Int32)(DateTime.UtcNow.Subtract(new DateTime(1970, 1, 1))).TotalSeconds).ToString(CultureInfo.InvariantCulture);
            string hash = CalculateMD5Hash(timestamp + _apiSecret);

            return "api_key=" + _apiKey + "&dev_hash=" + hash + "&timestamp=" + timestamp;
        }

        private string CalculateMD5Hash(string input)
        {
            // step 1, calculate MD5 hash from input
            MD5 md5 = MD5.Create();
            byte[] inputBytes = Encoding.ASCII.GetBytes(input);
            byte[] hash = md5.ComputeHash(inputBytes);

            // step 2, convert byte array to hex string
            var sb = new StringBuilder();
            for (int i = 0; i < hash.Length; i++)
            {
                sb.Append(hash[i].ToString("X2"));
            }

            return sb.ToString().ToLower();
        }

        internal bool IsValidCultureInfoName(string name)
        {
            return
                CultureInfo
                .GetCultures(CultureTypes.AllCultures)
                .Any(c => c.Name == name);
        }






        internal static void WriteResponseToLog(string url, string response)
        {
            Console.Write(url);
            Console.Write(response);
        }

        public string StreamToString(Stream stream)
        {
            using (var reader = new StreamReader(stream, Encoding.UTF8))
            {
                return reader.ReadToEnd();
            }
        }

        public MemoryStream StringToStream(string value)
        {
            byte[] byteArray = Encoding.UTF8.GetBytes(value);
            return new MemoryStream(byteArray);
        }
    }
}
