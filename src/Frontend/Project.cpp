/*

Decoda
Copyright (C) 2007-2013 Unknown Worlds Entertainment, Inc. 

This file is part of Decoda.

Decoda is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Decoda is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Decoda.  If not, see <http://www.gnu.org/licenses/>.

*/

#include "Config.h"
#include "Project.h"
#include "XmlUtility.h"
#include "StlUtility.h"
#include "DebugFrontend.h"
#include "Symbol.h"

#include <wx/xml/xml.h>
#include <wx/dir.h>
#include <wx/filefn.h>

#include <algorithm>

unsigned int Project::s_lastFileId = 0;

wxString Project::File::GetDisplayName() const
{

    wxString displayName = fileName.GetFullName();
    
    if (displayName.IsEmpty())
    {
        displayName = tempName;
    }

    return displayName;

}

wxString Project::File::GetFileType() const
{
    if (!type.IsEmpty())
    {
        return type;
    }
    else
    {
        return fileName.GetExt();
    }
}

Project::Project()
{
    m_needsSave     = false;
    m_needsUserSave = false;
    m_tempIndex = 0;
}

Project::~Project()
{
    
    for (unsigned int i = 0; i < m_files.size(); ++i)
    {
        ClearVector(m_files[i]->symbols);
    }    
    
    for (unsigned int i = 0; i < m_directories.size(); ++i)
    {
      Directory *directory = m_directories[i];
      for (unsigned int j = 0; j < directory->files.size(); ++j)
      {
        File *file = directory->files[j];

        ClearVector(file->symbols);
      }
      delete directory;
    }

    ClearVector(m_files);

}

wxString Project::GetName() const
{

    wxFileName fileName = m_fileName;
    wxString name = fileName.GetName();
    
    if (name.empty())
    {
        return "Untitled";
    }
    else
    {
        return name;
    }
}

bool Project::Save(const wxString& fileName, bool dontsave_project)
{  
    if (fileName.empty())
        return false;
    if (fileName != m_fileName)
    {
        m_needsSave = true;
        m_needsUserSave = true;
        dontsave_project = false;
    }
    bool success = true;
    if (!dontsave_project && m_needsSave)
    {
        if (SaveGeneralSettings(fileName))
        {
            m_needsSave = false;
        }
        else
        {
            success = false;
        }
    }
 
    if (success && m_needsUserSave)
    {
        wxFileName userFileName(fileName);
        userFileName.SetExt("deuser");
        if (SaveUserSettings(userFileName.GetFullPath()))
        {
            m_needsUserSave = false;
        }
    }

    if (success)
    {
        m_fileName = fileName;
    }

    return success;
}

bool Project::GetNeedsSaveProject() const
{
    return m_needsSave;
}

bool Project::GetNeedsSaveUser() const
{
    return m_needsUserSave;
}

bool Project::Load(const wxString& fileName)
{

    if (!LoadGeneralSettings(fileName))
    {
        return false;
    }

    wxFileName userFileName(fileName);
    userFileName.SetExt("deuser");


    // Even if we can't load the user settings, we still treat the load as
    // successful. We do this since the user file may not be present.
    LoadUserSettings(userFileName.GetFullPath());

    m_fileName  = fileName;
    m_needsSave = false;

    return true;

}

const wxString& Project::GetFileName() const
{
    return m_fileName;
}

const wxString& Project::GetCommandLine() const
{
    return m_commandLine;
}

void Project::SetCommandLine(const wxString& commandLine)
{
    m_commandLine = commandLine;
    m_needsUserSave = true;
}

const wxString& Project::GetCommandArguments() const
{
    return m_commandArguments;
}

void Project::SetCommandArguments(const wxString& commandArguments)
{
    m_commandArguments  = commandArguments;
    m_needsUserSave = true;
}

const wxString& Project::GetWorkingDirectory() const
{
    return m_workingDirectory;
}

void Project::SetWorkingDirectory(const wxString& workingDirectory)
{
    m_workingDirectory = workingDirectory;
    m_needsUserSave = true;
}

const wxString& Project::GetSymbolsDirectory() const
{
    return m_symbolsDirectory;
}

void Project::SetSymbolsDirectory(const wxString& symbolsDirectory)
{
    m_symbolsDirectory = symbolsDirectory;
    m_needsUserSave = true;
}

Project::File* Project::GetFile(unsigned int fileIndex)
{
    return m_files[fileIndex];
}

const Project::File* Project::GetFile(unsigned int fileIndex) const
{
    return m_files[fileIndex];
}

const Project::Directory *Project::GetDirectory(unsigned int dirIndex) const
{
    return m_directories[dirIndex];
}

Project::Directory *Project::GetDirectory(unsigned int dirIndex)
{
    return m_directories[dirIndex];
}

Project::File* Project::GetFileById(unsigned int fileId)
{

    for (unsigned int fileIndex = 0; fileIndex < m_files.size(); ++fileIndex)
    {
        if (m_files[fileIndex]->fileId == fileId)
        {
            return m_files[fileIndex];
        }
    }

    for (unsigned int directoryIndex = 0; directoryIndex < m_directories.size(); ++directoryIndex)
    {
      Project::Directory *directory = m_directories[directoryIndex];
      for (unsigned int fileIndex = 0; fileIndex < directory->files.size(); ++fileIndex)
      {
        if (directory->files[fileIndex]->fileId == fileId)
        {
          return directory->files[fileIndex];
        }
      }
    }

    return NULL;

}

unsigned int Project::GetNumFiles() const
{
    return m_files.size();
}

unsigned int Project::GetNumDirectories() const
{
    return m_directories.size();
}

Project::File* Project::AddFile(const wxString& fileName)
{

    // Check if the file is already in the project.

    for (unsigned int i = 0; i < m_files.size(); ++i)
    {
        if (!m_files[i]->temporary && m_files[i]->fileName.SameAs(fileName))
        {
            return NULL;
        }
    }

    File* file = new File;

    file->state         = CodeState_Normal;
    file->scriptIndex   = -1;
    file->temporary     = false;
    file->fileName      = fileName;
    file->fileId        = ++s_lastFileId;

    if (fileName.IsEmpty())
    {
      file->tempName = CreateTempName();
    }
    
    if (fileName.Contains('\\'))
    {
      Directory *bestDirectory = nullptr;
      int bestCompare = 0;

      for (Directory *directory : m_directories)
      {
        int compare = fileName.CompareTo(directory->name);
        if (compare > bestCompare)
        {
          bestDirectory = directory;
          bestCompare = compare;
        }
      }

      if (bestDirectory == nullptr)
      {
        m_files.push_back(file);
      }
      else
      {
        wxFileName localPath(fileName);
        localPath.MakeRelativeTo(bestDirectory->name);

        file->localPath = localPath.GetPath();

        bestDirectory->files.push_back(file);
      }
    }
    else
    {
      m_files.push_back(file);
    }
    
    m_needsSave = true;
    
    return file;
}

Project::Directory* Project::AddDirectory(const wxString& directoryStr)
{
  wxString directoryName = directoryStr;

  wxString fullDir;
  wxFileName path(directoryName);
  path.Normalize(wxPATH_NORM_ALL, m_baseDirectory);
  path.MakeRelativeTo(m_baseDirectory);
  fullDir = path.GetFullPath();

  for (unsigned int i = 0; i < m_directories.size(); ++i)
  {
    if (m_directories[i]->path == fullDir)
    {
      return NULL;
    }
  }

  wxDir directory(m_baseDirectory + "\\" + fullDir);
  wxArrayString filenames;

  Directory *dir = new Directory;
  
  if (directory.IsOpened())
  {
    wxString dirPath = directory.GetName();

    wxFileName name(m_baseDirectory + "\\" + fullDir);
    name.Normalize();

    dir->name = name.GetFullPath();
    dir->path = fullDir;

    directory.GetAllFiles(dirPath, &filenames, wxEmptyString, wxDIR_DIRS | wxDIR_FILES);
  }

  for (auto &filename : filenames)
  {
    wxFileName localPath(filename);
    localPath.MakeRelativeTo(dir->name);
    wxString ext(localPath.GetExt().Lower());
    if (ext != "lua" && ext != "txt")
        continue;

    File* file = new File;

    file->state = CodeState_Normal;
    file->scriptIndex = -1;
    file->temporary = false;
    file->fileId = ++s_lastFileId;

    file->localPath = localPath.GetPath();
    file->directoryPath = dir->name;

    file->fileName = filename;
    if (file->fileName.IsRelative())
    {
      file->fileName.MakeAbsolute(m_baseDirectory);
    }

    dir->files.push_back(file);
  }

  m_directories.push_back(dir);
  m_needsSave = true;

  return dir;
}

Project::File* Project::AddTemporaryFile(unsigned int scriptIndex)
{   
    const DebugFrontend::Script* script = DebugFrontend::Get().GetScript(scriptIndex);
    assert(script != NULL);

    File* file = new File;

    file->state         = script->state;
    file->scriptIndex   = scriptIndex;
    file->temporary     = true;
    file->fileName      = script->name.c_str();
    file->fileId        = ++s_lastFileId;
    file->localPath     = "Debug Temporary Files";

    m_files.push_back(file);

    return file;
}

Project::File* Project::AddTemporaryFile(const wxString& fileName)
{
    
    File* file = new File;

    file->state         = CodeState_Normal;
    file->scriptIndex   = -1;
    file->temporary     = true;
    file->fileName      = fileName;
    file->fileId        = ++s_lastFileId;

    if (fileName.IsEmpty())
    {
        file->tempName = CreateTempName();
    }

    m_files.push_back(file);

    return file;

}

void Project::RemoveFile(File* file, bool exclude_only)
{

    std::vector<File*>::iterator iterator = m_files.begin();

    while (iterator != m_files.end())
    {
        if (file == *iterator)
        {
            m_files.erase(iterator);
            if (!file->temporary)
            {
                m_needsSave = true;
                m_needsUserSave = true;

            }

            ClearVector(file->symbols);
            break;
        }
        ++iterator;
    }

    std::vector<Directory*>::iterator dirIterator = m_directories.begin();

    while (dirIterator != m_directories.end())
    {
      iterator = (*dirIterator)->files.begin();
      while (iterator != (*dirIterator)->files.end())
      {
        if (file == *iterator)
        {
          (*dirIterator)->files.erase(iterator);
          if (!file->temporary)
          {
            if (!exclude_only)
                wxRemoveFile(file->fileName.GetFullPath());

            m_needsSave = true;
            m_needsUserSave = true;
          }
          break;
        }
        ++iterator;
      }
      ++dirIterator;
    }

    iterator = m_openFiles.begin();
    while (iterator != m_openFiles.end())
    {
      if (file == *iterator)
      {
        m_openFiles.erase(iterator);
        break;
      }
      ++iterator;
    }
    delete file;
}

void Project::RemoveDirectory(Directory* directory, bool exclude_only)
{
  std::vector<Directory*>::iterator iterator = m_directories.begin();
  while (iterator != m_directories.end())
  {
    if (directory == *iterator)
    {
      for (File *file : directory->files)
      {
        ClearVector(file->symbols);
        std::vector<File*>::iterator iterator = m_openFiles.begin();
        while (iterator != m_openFiles.end())
        {
          if (file == *iterator)
          {
            m_openFiles.erase(iterator);
            break;
          }
          ++iterator;
        }
        //delete file;
      }

      m_directories.erase(iterator);
      delete directory;
      m_needsSave = true;
      m_needsUserSave = true;
      break;
    }
    ++iterator;
  }
}

void Project::CleanUpAfterSession()
{

    for (unsigned int i = 0; i < m_files.size(); ++i)
    {
        m_files[i]->scriptIndex = -1;
    }

    for (unsigned int i = 0; i < m_directories.size(); ++i)
    {
      Directory *directory = m_directories[i];
      for (unsigned int j = 0; j < directory->files.size(); ++j)
      {
        File *file = directory->files[j];

        file->scriptIndex = -1;
      }
    }
}

Project::File* Project::GetFileForScript(unsigned int scriptIndex) const
{

    for (unsigned int i = 0; i < m_files.size(); ++i)
    {
        if (m_files[i]->scriptIndex == scriptIndex)
        {
            return m_files[i];
        }
    }

    for (unsigned int i = 0; i < m_directories.size(); ++i)
    {
      Directory *directory = m_directories[i];
      for (unsigned int j = 0; j < directory->files.size(); ++j)
      {
        File *file = directory->files[j];

        if (file->scriptIndex == scriptIndex)
          return file;
      }
    }

    return NULL;

}

Project::File* Project::GetFileForFileName(const wxFileName& fileName) const
{

    for (unsigned int i = 0; i < m_files.size(); ++i)
    {
        if (m_files[i]->fileName.GetFullName().CmpNoCase(fileName.GetFullName()) == 0)
        {
            return m_files[i];
        }
    }

    for (unsigned int i = 0; i < m_directories.size(); ++i)
    {
      Directory *directory = m_directories[i];
      for (unsigned int j = 0; j < directory->files.size(); ++j)
      {
        File *file = directory->files[j];

        if (file->fileName.GetFullName().CmpNoCase(fileName.GetFullName()) == 0)
          return file;
      }
    }

    return NULL;

}

void Project::SetBreakpoint(unsigned int scriptIndex, unsigned int line, bool set)
{

    for (unsigned int i = 0; i < m_files.size(); ++i)
    {
        if (m_files[i]->scriptIndex == scriptIndex)
        {

            std::vector<unsigned int>::iterator iterator;
            iterator = std::find(m_files[i]->breakpoints.begin(), m_files[i]->breakpoints.end(), line);

            if (set)
            {
                if (iterator == m_files[i]->breakpoints.end())
                {
                    m_files[i]->breakpoints.push_back(line);
                    if (!m_files[i]->temporary)
                    {
                        m_needsUserSave = true;
                    }
                }
            }
            else
            {
                if (iterator != m_files[i]->breakpoints.end())
                {
                    m_files[i]->breakpoints.erase(iterator);
                    if (!m_files[i]->temporary)
                    {
                        m_needsUserSave = true;
                    }
                }
            }

        }
    }

    for (unsigned int i = 0; i < m_directories.size(); ++i)
    {
      Directory *directory = m_directories[i];
      for (unsigned int j = 0; j < directory->files.size(); ++j)
      {
        File *file = directory->files[j];

        if (file->scriptIndex == scriptIndex)
        {

          std::vector<unsigned int>::iterator iterator;
          iterator = std::find(file->breakpoints.begin(), file->breakpoints.end(), line);

          if (set)
          {
            if (iterator == file->breakpoints.end())
            {
              file->breakpoints.push_back(line);
              if (!file->temporary)
              {
                m_needsUserSave = true;
              }
            }
          }
          else
          {
            if (iterator != file->breakpoints.end())
            {
              file->breakpoints.erase(iterator);
              if (!file->temporary)
              {
                m_needsUserSave = true;
              }
            }
          }

        }
      }
    }

}

bool Project::ToggleBreakpoint(File* file, unsigned int line)
{

    std::vector<unsigned int>::iterator iterator;
    iterator = std::find(file->breakpoints.begin(), file->breakpoints.end(), line);

    bool set = false;

    if (iterator == file->breakpoints.end())
    {
        file->breakpoints.push_back(line);
        set = true;
    }
    else
    {
        file->breakpoints.erase(iterator);
        set = false;
    }

    if (!file->temporary)
    {
        m_needsUserSave = true;
    }

    return set;

}

void Project::DeleteAllBreakpoints(File* file)
{

    if (!file->breakpoints.empty())
    {
        
        file->breakpoints.clear();
    
        if (!file->temporary)
        {
            m_needsUserSave = true;
        }    

    }
}

void Project::SetWatches(const std::vector<wxString>& watches)
{
    m_watches.assign(watches.begin(), watches.end());
    m_needsUserSave = true;
}

const std::vector<wxString>& Project::GetWatches()
{
    return m_watches;
}

wxXmlNode* Project::SaveFileNode(const wxString& baseDirectory, const File* file)
{

    // Temporary nodes should not be saved.
    assert(!file->temporary);

    wxFileName fileName = file->fileName;
    fileName.MakeRelativeTo(baseDirectory);

    wxXmlNode* node = new wxXmlNode(wxXML_ELEMENT_NODE, "file");
    node->AddChild(WriteXmlNode("filename", fileName.GetFullPath()));

    return node;

}

wxXmlNode* Project::SaveDirectoryNode(const Directory* dir)
{
  //Convert empty string to current directory.
  wxString path = dir->path;
  if (path == "")
    path = ".\\";

  return WriteXmlNode("directory", path);
}

wxXmlNode* Project::SaveUserFileNode(const wxString& baseDirectory, const File* file) const
{

    wxXmlNode* fileNode = NULL;

    if (!file->breakpoints.empty() && !file->temporary)
    {

        wxFileName fileName = file->fileName;
        fileName.MakeRelativeTo(baseDirectory);

        fileNode = new wxXmlNode(wxXML_ELEMENT_NODE, "file");
        fileNode->AddChild(WriteXmlNode("filename", fileName.GetFullPath()));

        for (unsigned int i = 0; i < file->breakpoints.size(); ++i)
        {
            wxXmlNode* breakpointNode = new wxXmlNode(wxXML_ELEMENT_NODE, "breakpoint"); 
            breakpointNode->AddChild(WriteXmlNode("line", file->breakpoints[i]));
            fileNode->AddChild(breakpointNode);
        }

    }

    return fileNode;

}

bool Project::LoadUserFilesNode(const wxString& baseDirectory, wxXmlNode* root)
{

    if (root->GetName() != "files")
    {
        return false;
    }

    wxXmlNode* node = root->GetChildren();
    
    while (node != NULL)
    {
        LoadUserFileNode(baseDirectory, node);
        node = node->GetNext();
    }

    return true;

}

bool Project::LoadUserFileNode(const wxString& baseDirectory, wxXmlNode* root)
{
    
    if (root->GetName() != "file")
    {
        return false;
    }

    wxString fileName;
    std::vector<unsigned int> breakpoints;

    wxXmlNode* node = root->GetChildren();

    while (node != NULL)
    {

        ReadXmlNode(node, wxT("filename"), fileName)
        || LoadBreakpointNode(node, breakpoints);

        node = node->GetNext();
    }
    
    if (!fileName.IsEmpty())
    {

        wxFileName temp = fileName;

        if (temp.IsRelative())
        {
            temp.MakeAbsolute(baseDirectory);
        }

        Project::File* file = NULL;

        for (unsigned int fileIndex = 0; fileIndex < m_files.size(); ++fileIndex)
        {
            if (m_files[fileIndex]->fileName == temp)
            {
                file = m_files[fileIndex];
                break;
            }
        }

        if (file != NULL)
        {
            file->breakpoints = breakpoints;
        }

    }

    return true;

}

bool Project::LoadOpenFilesNode(wxXmlNode* root)
{

  if (root->GetName() != "openfiles")
  {
    return false;
  }

  wxXmlNode* node = root->GetChildren();

  while (node != NULL)
  {
    wxString filename;
    wxString localPath;

    wxXmlNode* child = node->GetChildren();
    while (child != NULL)
    {
      ReadXmlNode(child, wxT("filename"), filename);
      ReadXmlNode(child, wxT("localpath"), localPath);
      child = child->GetNext();
    }

    for (File *file : m_files)
    {
      if (file->fileName.GetFullName() == filename && file->localPath == localPath)
      {
        m_openFiles.push_back(file);
        break;
      }
    }

    for (Directory *directory : m_directories)
    {
      for (File *file : directory->files)
      {
        if (file->fileName.GetFullName() == filename && file->localPath == localPath)
        {
          m_openFiles.push_back(file);
          break;
        }
      }
    }

    node = node->GetNext();
  }

  return true;

}

bool Project::LoadBreakpointNode(wxXmlNode* root, std::vector<unsigned int>& breakpoints)
{

    if (root->GetName() != "breakpoint")
    {
        return false;
    }

    wxXmlNode* node = root->GetChildren();

    while (node != NULL)
    {

        unsigned int line;

        if (ReadXmlNode(node, wxT("line"), line))
        {
            breakpoints.push_back(line);
        }

        node = node->GetNext();
    
    }

    return true;

}

bool Project::LoadWatchesNode(wxXmlNode* root)
{
    if (root->GetName() != "watches")
       return false;

    m_watches.clear();
    wxXmlNode* node = root->GetChildren();
    while (node != NULL)
    {
        wxString watch;
        if (ReadXmlNode(node, wxT("watch"), watch))
        {
            m_watches.push_back(watch);
        }
        node = node->GetNext();    
    }
    return true;
}

bool Project::LoadFileNode(const wxString& baseDirectory, wxXmlNode* node)
{

    if (node->GetName() != "file")
    {
        return false;
    }

    File* file = new File;

    file->state         = CodeState_Normal;
    file->scriptIndex   = -1;
    file->temporary     = false;
    file->fileId        = ++s_lastFileId;

    wxXmlNode* child = node->GetChildren();
    wxString fileName;

    while (child != NULL)
    {
        if (ReadXmlNode(child, "filename", fileName))
        {
            
            file->fileName = fileName;

            if (file->fileName.IsRelative())
            {
                file->fileName.MakeAbsolute(baseDirectory);
            }

            wxString temp = file->fileName.GetFullPath();
            int a  =0;

            
        }
        child = child->GetNext();
    }

    m_files.push_back(file);
    return true;

}

bool Project::LoadDirectoryNode(const wxString& baseDirectory, wxXmlNode* node)
{
  if (node->GetName() != "directory")
  {
    return false;
  }
  wxString directoryName;
  wxArrayString filenames;
  

  if (ReadXmlNode(node, "directory", directoryName))
  {
    wxString fullDir;
    wxFileName path(directoryName);
    path.Normalize(wxPATH_NORM_ALL, m_baseDirectory);
    path.MakeRelativeTo(m_baseDirectory);
    fullDir = path.GetFullPath();

    wxDir directory(m_baseDirectory + "\\" + fullDir);

    Directory *dir = new Directory;

    if (directory.IsOpened())
    {
      wxString dirPath = directory.GetName();

      wxFileName name(m_baseDirectory + "\\" + fullDir);
      name.Normalize();

      dir->name = name.GetFullPath();
      dir->path = fullDir;
      directory.GetAllFiles(dirPath, &filenames, wxEmptyString, wxDIR_DIRS | wxDIR_FILES);
    }

    for (auto &filename : filenames)
    {
      wxFileName localPath(filename);
      localPath.MakeRelativeTo(dir->name);
      wxString ext(localPath.GetExt().Lower()); 
      if (ext != "lua" && ext != "txt")
        continue;

      File* file = new File;

      file->state = CodeState_Normal;
      file->scriptIndex = -1;
      file->temporary = false;
      file->fileId = ++s_lastFileId;

      file->localPath = localPath.GetPath(); 
      file->directoryPath = dir->name;
      file->fileName = filename;

      dir->files.push_back(file);
    }

    m_directories.push_back(dir);
  }

  return true;

}

bool Project::LoadTemplateNodes(const wxString& baseDirectory, wxXmlNode* node)
{
  wxXmlNode* child = node->GetChildren();
  while (child != NULL)
  {
    wxString name = child->GetAttribute("name");
    wxString ext  = child->GetAttribute("extension");
    wxString temp;
    ReadXmlNode(child, "template", temp);

    m_templates.push_back(Template(name, ext, temp));
    child = child->GetNext();
  }
  return true;
}

std::vector<Project::File*> Project::GetSortedFileList()
{
	struct SortByDisplayName
    {
        bool operator()(const File* file1, const File* file2) const
        {
            return file1->GetDisplayName() < file2->GetDisplayName();
        }
    };

	// Now build a copy of m_files and sort it by file name
	std::vector<Project::File*> fileList(m_files);

	std::sort(fileList.begin(), fileList.end(), SortByDisplayName());

	return fileList;
}

bool Project::SaveGeneralSettings(const wxString& fileName)
{

  // Disable logging.
  wxLogNull logNo;

  wxString baseDirectory = wxFileName(fileName).GetPath();
    
  wxXmlDocument document;
    
  wxXmlNode* root = new wxXmlNode(wxXML_ELEMENT_NODE, "project");
  document.SetRoot(root);

	// Sort file names before saving, to lessen conflicts when synching to source control
	std::vector<Project::File*> sortedFileList = GetSortedFileList();
	assert(sortedFileList.size() == m_files.size());

	// Now save them out
    for (unsigned int i = 0; i < sortedFileList.size(); ++i)
    {
		File* file = sortedFileList[i];
		assert(file != NULL);

        if (!file->temporary)
        {
            wxXmlNode* node = SaveFileNode(baseDirectory, file);
            root->AddChild(node);
        }
    }

    for (unsigned int i = 0; i < m_directories.size(); ++i)
    {
      Directory* dir = m_directories[i];
      assert(dir != NULL);

      wxXmlNode* node = SaveDirectoryNode(dir);
      root->AddChild(node);
    }

    wxXmlNode *templates = WriteXmlNode("templates", "");

    for (Template &t : m_templates)
    {
      wxXmlNode *temp = WriteXmlNode("template", t.content);
      temp->AddAttribute("name", t.name);
      temp->AddAttribute("extension", t.extension);
      templates->AddChild(temp);
    }

    root->AddChild(templates);

    return document.Save(fileName);
}

bool Project::SaveUserSettings(const wxString& fileName)
{

    // Disable logging.
    wxLogNull logNo;

    wxXmlDocument document;
    
    wxXmlNode* root = new wxXmlNode(wxXML_ELEMENT_NODE, "project");
    document.SetRoot(root);

    root->AddChild(WriteXmlNode("command",              m_commandLine));
    root->AddChild(WriteXmlNode("working_directory",    m_workingDirectory));
    root->AddChild(WriteXmlNode("symbols_directory",    m_symbolsDirectory));
    root->AddChild(WriteXmlNode("command_arguments",    m_commandArguments));

    wxString baseDirectory = wxFileName(fileName).GetPath();

    wxXmlNode* filesNode = NULL;

    for (unsigned int fileIndex = 0; fileIndex < m_files.size(); ++fileIndex)
    {
        const File* file = m_files[fileIndex];
        wxXmlNode* node = SaveUserFileNode(baseDirectory, file);
        if (node != NULL)
        {
            if (filesNode == NULL)
            {
                filesNode = new wxXmlNode(wxXML_ELEMENT_NODE, "files");
            }
            filesNode->AddChild(node);
        }
    }
    
    const std::vector<wxString>& w = GetWatches();
    if (!w.empty()) 
    {
        wxXmlNode* watches = new wxXmlNode(wxXML_ELEMENT_NODE, "watches");
        for (int i = 0,e=w.size(); i<e; ++i)
          watches->AddChild(WriteXmlNode("watch", w[i]));
        root->AddChild(watches);
    }

    if (filesNode != NULL)
    {
        root->AddChild(filesNode);
    }

    wxXmlNode* openFilesNode = NULL;
    for (unsigned int fileIndex = 0; fileIndex < m_openFiles.size(); ++fileIndex)
    {
      const File* file = m_openFiles[fileIndex];

      wxXmlNode* node = new wxXmlNode(wxXML_ELEMENT_NODE, "file");
      node->AddChild(WriteXmlNode("filename", file->fileName.GetFullName()));
      node->AddChild(WriteXmlNode("localpath", file->localPath));

      if (node != NULL)
      {
        if (openFilesNode == NULL)
        {
          openFilesNode = new wxXmlNode(wxXML_ELEMENT_NODE, "openfiles");
        }
        openFilesNode->AddChild(node);
      }
    }

    if (openFilesNode != NULL)
    {
      root->AddChild(openFilesNode);
    }

    return document.Save(fileName);

}

bool Project::LoadUserSettings(const wxString& fileName)
{

    // Disable logging.
    wxLogNull logNo;

    wxXmlDocument document;

    if (!document.Load(fileName))
    {
        return false;
    }

    wxXmlNode* root = document.GetRoot();
    
    if (root->GetName() != "project")
    {
        return false;
    }

    wxString baseDirectory = wxFileName(fileName).GetPath();

    wxXmlNode* node = root->GetChildren();
    
    while (node != NULL)
    {

           ReadXmlNode(node, "command_arguments",   m_commandArguments)
        || ReadXmlNode(node, "command",             m_commandLine)
        || ReadXmlNode(node, "working_directory",   m_workingDirectory)
        || ReadXmlNode(node, "symbols_directory",   m_symbolsDirectory)
        || LoadUserFilesNode(baseDirectory, node)
        || LoadOpenFilesNode(node)
        || LoadWatchesNode(node);
        
        node = node->GetNext();

    }

    return true;

}

bool Project::LoadGeneralSettings(const wxString& fileName)
{

    // Disable logging.
    wxLogNull logNo;

    m_baseDirectory = wxFileName(fileName).GetPath();

    wxXmlDocument document;

    if (!document.Load(fileName))
    {
        return false;
    }

    wxXmlNode* root = document.GetRoot();
    
    if (root->GetName() != "project")
    {
        return false;
    }
    
    wxXmlNode* node = root->GetChildren();
    
    while (node != NULL)
    {
        if (node->GetName() == "file")
          LoadFileNode(m_baseDirectory, node);
        else if (node->GetName() == "directory")
          LoadDirectoryNode(m_baseDirectory, node);
        else if (node->GetName() == "templates")
          LoadTemplateNodes(m_baseDirectory, node);
        node = node->GetNext();
    }

    return true;

}

wxString Project::CreateTempName()
{
    ++m_tempIndex;
    return wxString::Format("Untitled%d", m_tempIndex);
}

wxString Project::GetBaseDirectory() const
{
  return m_baseDirectory;
}

void Project::AddOpenedFile(Project::File *file)
{
  m_openFiles.push_back(file);
  m_needsUserSave = true;
}

void Project::RemoveOpenedFile(Project::File *file)
{
  std::vector<File*>::iterator iterator = m_openFiles.begin();
  while (iterator != m_openFiles.end())
  {
    if (file == *iterator)
    {
      m_openFiles.erase(iterator);
      break;
    }
    ++iterator;
  }

  m_needsUserSave = true;
}

std::vector<Project::File *> &Project::GetOpenFiles()
{
  return m_openFiles;
}