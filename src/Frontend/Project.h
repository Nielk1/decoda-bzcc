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

#ifndef PROJECT_H
#define PROJECT_H

#include "wx/wx.h"
#include "wx/filename.h"

#include "Protocol.h"

#include <vector>

#undef RemoveDirectory
// 
// Forward declarations.
//

class wxXmlNode;
class Symbol;

/**
 * Stores information about a project that is open in the debugger.
 */
class Project
{

public:

    struct File
    {

        wxString GetDisplayName() const;
        wxString GetFileType() const;

        CodeState                   state;
        bool                        temporary;
        unsigned int                scriptIndex;
        wxString                    localPath;
        wxFileName                  fileName;
        std::vector<unsigned int>   breakpoints;
        wxString                    tempName;

        wxString                    type;       // Lua, etc.        
        
        unsigned int                fileId;     // Unique id we use to match up symbol parsing results.
        std::vector<Symbol*>        symbols;
        wxString                    directoryPath;
        bool                        symbolsUpdated = false;
    };

    struct Directory
    {
      ~Directory()
      {
        for (File *p : files)
        {
          delete p;
        }
      }

      wxString path;
      wxString name;
      std::vector<File *> files;
    };

    struct Template
    {
      Template(wxString name, wxString ext, wxString cont)
        :name(name), extension(ext), content(cont)
      {}

      wxString name;
      wxString extension;
      wxString content;
    };

    /**
     * Constructor.
     */
    Project();

    /**
     * Destructor.
     */
    virtual ~Project();

    /**
     * Returns the name of the project. This is derived from the file name.
     */
    wxString GetName() const;

    /**
     * Saves the project settings to the specified file. This updates the internally
     * stored file name so that the next time Save is called it will use that file.
     */
    bool Save(const wxString& fileName, bool dontsave_project);

    /**
     * Loads the project setting from the specified file.
     */
    bool Load(const wxString& fileName);

    /**
     * Returns the file name where the project was last saved/loaded file.
     */
    const wxString& GetFileName() const;

    /**
     * Returns true if the project settings/user settings have been modified since they were
     * last saved/loaded.
     */
    bool GetNeedsSaveProject() const;
    bool GetNeedsSaveUser() const;

    /**
     * Returns the command line to launch the application associated with the
     * project (i.e. the debugee).
     */
    const wxString& GetCommandLine() const;

    /**
     * Sets the command line for launching the application associated with the
     * project (i.e. the debugee).
     */
    void SetCommandLine(const wxString& commandLine);

    /**
     * Returns the command line argument used to launch the application.
     */
    const wxString& GetCommandArguments() const;

    /**
     * Sets the command line arguments supplied to the application.
     */
    void SetCommandArguments(const wxString& commandLine);

    /**
     * Returns the startup directory when the application associated with the
     * project is started.
     */
    const wxString& GetWorkingDirectory() const;

    /**
     * Sets the startup directory when the application associated with the
     * project is started.
     */
    void SetWorkingDirectory(const wxString& workingdirectory);

    /**
     * Returns the directory when the application will look for PDB files.
     */
    const wxString& GetSymbolsDirectory() const;

    /**
     * Sets the directory when the application will look for PDB files.
     */
    void SetSymbolsDirectory(const wxString& symbolsDirectory);

    /** 
     * Gets the open file that matches the script specified by the scriptIndex.
     * If there is no open file matching the scriptIndex the method returns NULL.
     */
    File* GetFileForScript(unsigned int scriptIndex) const;

    /**
     * Gets the file that matches the file name.
     */
    File* GetFileForFileName(const wxFileName& fileName) const;

    /**
     * Adds a file to the project. The new file is returned.
     */
    File* AddFile(const wxString& fileName);

    /**
    * Adds a directory to the project. The new directory is returned.
    */
    Directory* AddDirectory(const wxString& fileName);

    /**
     * Adds a script to the project temporarily (during this debug sesssion). The
     * new file is returned.
     */
    File* AddTemporaryFile(unsigned int scriptIndex);

    /**
     * Adds a file to the project temporarily (during this debug sesssion). The
     * new file is returned.
     */
    File* AddTemporaryFile(const wxString& fileName);

    /**
     * Removes a file from the project.
     */
    void RemoveFile(File* file, bool exclude_only);

    /**
    * Removes a directory from the project.
    */
    void RemoveDirectory(Directory* directory, bool exclude_only);

    /**
     * Gets the specified file.
     */
    File* GetFile(unsigned int fileIndex);

    /**
     * Gets the specified file.
     */
    const File* GetFile(unsigned int fileIndex) const;


    /**
    * Gets the specified directory.
    */
    Directory* GetDirectory(unsigned int dirIndex);

    /**
    * Gets the specified directory.
    */
    const Directory *GetDirectory(unsigned int dirIndex) const;

    /**
     * Returns the file with the specified file id, or NULL if the file doesn't
     * exist in the project.
     */
    File* GetFileById(unsigned int fileId);

    /**
     * Gets the number of files in the project.
     */
    unsigned int GetNumFiles() const;

    /**
    * Gets the number of directories in the project.
    */
    unsigned int GetNumDirectories() const;


    /**
     * Clears the script indices for all of the files.
     */
    void CleanUpAfterSession();

    /**
     * Sets or unsets a break point in the specified line of a script.
     */
    void SetBreakpoint(unsigned int scriptIndex, unsigned int line, bool set);

    /**
     * Toggles a break point in a file that has not be encounted by the script
     * system. Returns true if the break point was added, and false if it was
     * removed.
     */
    bool ToggleBreakpoint(File* file, unsigned int line);

    /**
     * Deletes all of the breakpoints from the specified file.
     */
    void DeleteAllBreakpoints(File* file);

    // Load/save watches list
    void SetWatches(const std::vector<wxString>& watches);
    const std::vector<wxString>& GetWatches();

    /**
    * Gets the base directory path
    */
    wxString GetBaseDirectory() const;

    /**
    * Adds a file to the list of open files
    */
    void AddOpenedFile(Project::File *file);

    /**
    * Removes a file to the list of open files
    */
    void RemoveOpenedFile(Project::File *file);

    /**
    * Gets the list of open files
    */
    std::vector<Project::File *> &GetOpenFiles();

    std::vector<Template> &GetTemplates()
    {
      return m_templates;
    }


private:

    /**
     * Creates a new XML node representing a file. The file name is stored
     * relative to the specified base directory.
     */
    wxXmlNode* SaveFileNode(const wxString& baseDirectory, const File* file);
    
    /**
    * Creates a new XML node representing a directory. The directory name is stored
      relative to the project.
    */
    wxXmlNode* SaveDirectoryNode(const Directory* file);

    /**
     * Creates a new XML node representing the user settings for a  file. The
     * file name is stored relative to the specified base directory.
     */
    wxXmlNode* SaveUserFileNode(const wxString& baseDirectory, const File* file) const;

    /**
     * Creates a new file from an XML node and adds it to the list of files.
     * if the XML node was not a file node, the method returns false. Any relative
     * file names stored in the project are made relative to the specified base
     * directory.
     */
    bool LoadFileNode(const wxString& baseDirectory, wxXmlNode* node);

    bool LoadDirectoryNode(const wxString& baseDirectory, wxXmlNode* node);

    bool LoadTemplateNodes(const wxString& baseDirectory, wxXmlNode* node);


    /**
     * Loads the user data for a file from an XML node. If the XML node is not 
     * a file node, the method returns false.
     */
    bool LoadUserFileNode(const wxString& baseDirectory, wxXmlNode* node);

    /**
     * Loads the user data for a files from an XML node. If the XML node is not 
     * a files node, the method returns false.
     */
    bool LoadUserFilesNode(const wxString& baseDirectory, wxXmlNode* node);

    /**
    * Loads the user data for a opened files from an XML node. If the XML node is not
    * a open file node, the method returns false.
    */
    bool LoadOpenFilesNode(wxXmlNode* node);

    /**
     * Loads the breakpoint node. If the XML node is not a breakpoint node,
     * the method returns false.
     */
    bool LoadBreakpointNode(wxXmlNode* root, std::vector<unsigned int>& breakpoints);

    bool LoadWatchesNode(wxXmlNode* root);

    /**
     * Writes the user options file (which stores project information like the
     * executable to run that's specific to a user) to the specified file.
     */
    bool SaveUserSettings(const wxString& fileName);

    /**
     * Writes the project options file to the specified file.
     */
    bool SaveGeneralSettings(const wxString& fileName);

    /**
     * Reads the user options file (which stores project information like the
     * executable to run that's specific to a user) to the specified file.
     */
    bool LoadUserSettings(const wxString& fileName);

    /**
     * Reads the project options file to the specified file.     
     */
    bool LoadGeneralSettings(const wxString& fileName);
    
    /**
     * Generates a new name that is displayed for a file that has no file name.
     */
    wxString CreateTempName();

private:

	// Returns vector for temporary internal use
	std::vector<File*>		GetSortedFileList();

    static unsigned int      s_lastFileId;
                             
    bool                     m_needsSave;
    bool                     m_needsUserSave;
    wxString                 m_fileName;
                             
    wxString                 m_commandLine;
    wxString                 m_commandArguments;
    wxString                 m_workingDirectory;
    wxString                 m_symbolsDirectory;
                             
    std::vector<File*>       m_files;
    std::vector<Directory *> m_directories;

    std::vector<File *>      m_openFiles;
    std::vector<Template>    m_templates;
                             
    unsigned int             m_tempIndex;
               
    wxString                 m_baseDirectory;

    
    std::vector<wxString>    m_watches;
};

#endif
