
/****** ADVANCED OPERATING SYSTEMS PROJECT 
       NAME- SAI SIRISHA NADIMINTI
       ROLL NO.- 2020201044 
 *******/


#include <iostream>
#include <dirent.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <pwd.h>
#include <grp.h>
#include <vector>
#include <math.h>
#include <iomanip>
#include <termios.h>
#include <algorithm>
#include <sys/ioctl.h>
#include <signal.h>
#include <sys/wait.h>
#include <stack>
#include <filesystem>
#include <fcntl.h>

using namespace std;

vector<pair<string, string>> filePaths;
char homeDir[PATH_MAX];
string home;
string currDir;
int currLine;
int terminalRows;
stack<string> previousPaths;
stack<string> nextPaths;
int doNotInsertInStack = 0;
int cursorPos = 0;
string currDirCommandMode;
bool exitedFromCommandMode = false;
int commands = 0;

//Returns the window size 
int getTerminalSize()
{
    struct winsize ts;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &ts);
    int lines = ts.ws_row;
    return (lines - 4);
}

string getHumanReadableSize(off_t &size)
{
    float fsize = (float)size; //Typecasting the size to float to allow decimals
    string hrsize;
    int count = 0;
    vector<string> extension = {"", "K", "M", "G", "T", "P"};
    while (fsize > 1024)
    {
        count++;
        fsize = fsize / 1024;
    }
    if ((int)fsize == ceil(fsize))
    {
        return to_string((int)fsize) + extension[count];
    }
    double d = fsize * 10.0;
    int i = d + 0.5;
    d = (float)i / 10.0;
    string result = to_string(d);
    int it = result.find('.') + 1;
    result = result.substr(0, it + 1);
    hrsize = result + extension[count];
    return hrsize;
}

string getPermissions(struct stat &s)
{
    /*
            S_IRUSR: User (owner) can read this file 
            S_IWUSR: User (owner) can write this file
            S_IXUSR: User (owner) can execute this file 
            S_IRGRP: Members of the owner’s group can read this file
            S_IWGRP: Members of the owner’s group can write this file
            S_IXGRP: Members of the owner’s group can execute this file          
            S_IROTH: Others (anyone) can read this file
            S_IWOTH: Others (anyone) can write this file
            S_IXOTH: Others (anyone) can execute this file         
    */
    string permissions = "";
    mode_t modes = s.st_mode;
    permissions = (S_ISDIR(s.st_mode)) ? "d" : "-";
    permissions += (modes & S_IRUSR) ? "r" : "-";
    permissions += (modes & S_IWUSR) ? "w" : "-";
    permissions += (modes & S_IXUSR) ? "x" : "-";
    permissions += (modes & S_IRGRP) ? "r" : "-";
    permissions += (modes & S_IWGRP) ? "w" : "-";
    permissions += (modes & S_IXGRP) ? "x" : "-";
    permissions += (modes & S_IROTH) ? "r" : "-";
    permissions += (modes & S_IWOTH) ? "w" : "-";
    permissions += (modes & S_IXOTH) ? "x" : "-";
    return permissions;
}

void createPathVector(string dirPath)
{
    terminalRows = getTerminalSize();
    currDir = dirPath;
    if (doNotInsertInStack == 0)
    {
        if (previousPaths.empty() || previousPaths.top() != dirPath)
        {
            previousPaths.push(dirPath);
        }
    }
    else
    {
        doNotInsertInStack = 0;
    }
    currLine = 0;
    DIR *dr;
    struct dirent *en;
    filePaths.clear();
    dr = opendir(dirPath.c_str()); //open the current director
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            char *filename = en->d_name;
            string filenameStr = filename;
            if (filenameStr == ".")
            {
                //This is the directory itself
                string folderItself = ".";
                filePaths.push_back(make_pair(folderItself, dirPath));
            }
            else if (filenameStr == "..")
            {
                //This is the parent directory
                int pos = dirPath.find_last_of('/');
                string parentDirectory = dirPath.substr(0, pos);
                filePaths.push_back(make_pair("..", parentDirectory));
            }
            else
                filePaths.push_back(make_pair(filename, dirPath + "/" + filename));
        }
        closedir(dr); //close all directory
        sort(filePaths.begin(), filePaths.end());
    }
    else
    {
        cout << "Directory cannot be opened";
    }
    if (filePaths.size() > terminalRows - 1)
    {
        currLine = filePaths.size() - (terminalRows - 1);
    }
}

void listFiles(int startIndex, int endIndex)
{
    printf("\033c");
    printf("\033[0;0H");
    for (int i = startIndex; i < endIndex; i++)
    {
        string filesize, userOwnership, groupOwnership, permissions, lastmodified;
        string filePath = filePaths[i].second;
        const char *filename = filePaths[i].first.c_str();
        struct stat s;
        stat(filePath.c_str(), &s);
        off_t size = s.st_size;
        string humanreadableSize = getHumanReadableSize(size);
        struct passwd *user = getpwuid(s.st_uid);
        struct group *group = getgrgid(s.st_gid);
        if (user != 0)
            userOwnership = user->pw_name;
        if (group != 0)
            groupOwnership = group->gr_name;
        permissions = getPermissions(s);
        string time = ctime(&s.st_mtime);
        cout << setw(30) << left << filename << " " << setw(10) << left << humanreadableSize << " " << setw(12) << left << userOwnership << " " << setw(12) << left << groupOwnership << " " << setw(12) << left << permissions << " " << setw(10) << left << time;
    }
    printf("\033[%d;0H", terminalRows - 1);
    printf("\n--------------Total files: %ld , Status: OK------------------", filePaths.size());
    printf("\033[%d;0H", terminalRows + 1);

    cout << "Command mode:" << endl;
    printf("\033[0;0H"); //Restore the position of the cursor
    cursorPos = 0;
}

//Example: copy foo.txt bar.txt : Give ONLY filenames. No folder
void copyFileToFile(string sourceFile, string destinationFile)
{
    char block[1024];
    int in, out;
    int nread;
    in = open(sourceFile.c_str(), O_RDONLY);
    out = open(destinationFile.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    while ((nread = read(in, block, sizeof(block))) > 0)
        write(out, block, nread);
    struct stat fs;
    stat(sourceFile.c_str(), &fs);
    //Setting back the same permissions to the copied file
    int perm = chmod(destinationFile.c_str(), fs.st_mode);
}

//In the form copy foo.txt to B where B is the destinationFolder
void copyFileToDirectory(string sourceFile, string destinationFolder)
{
    cout << "Copying " << sourceFile << " to " << destinationFolder << "..." << endl;
    string toBeCopied = destinationFolder + "/" + sourceFile;
    copyFileToFile(sourceFile, toBeCopied);
}

// Converts relative path to absolute path
string convertToAbsolutePath(string path)
{
    // Append the application home directory

    string absPath = path;
    if (path[0] == '~')
    {

        absPath = path.replace(0, 1, home);
    }
    //parent directory
    else if (path[0] == '.' && path[1] == '.')
    {
        string current = currDirCommandMode;
        int pos = current.find_last_of('/');
        absPath = current.substr(0, pos - 1);
    }
    else if (path[0] == '.')
    {
        absPath = path.replace(0, 1, currDirCommandMode);
    }
    return absPath;
}

bool IsRegularFile(string path)
{
    struct stat fs;
    stat(path.c_str(), &fs);
    return S_ISREG(fs.st_mode);
}

bool IsDirectory(string path)
{
    struct stat fs;
    stat(path.c_str(), &fs);
    return S_ISDIR(fs.st_mode);
}

// In the form copy FolderA FolderB
void copyDirectoryToDirectory(string sourceDirectory, string DestinationDirectory)
{
    string toMakeDir = DestinationDirectory + "/" + sourceDirectory;
    int makeDir = mkdir(toMakeDir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    DIR *dr;
    struct dirent *en;
    dr = opendir(sourceDirectory.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            char *filename = en->d_name;
            string filenameStr = filename;
            if (filenameStr != "." && filenameStr != "..")
            {
                if (IsRegularFile(sourceDirectory + "/" + filenameStr))
                {
                    copyFileToDirectory(sourceDirectory + "/" + filenameStr, DestinationDirectory);
                }
                else if (IsDirectory(sourceDirectory + "/" + filenameStr))
                {
                    //Recursively call directories
                    copyDirectoryToDirectory(sourceDirectory + "/" + filenameStr, DestinationDirectory);
                }
            }
        }
    }
    struct stat fs;
    stat(sourceDirectory.c_str(), &fs);
    int perm = chmod(toMakeDir.c_str(), fs.st_mode);
}

void copyCommand(vector<string> &commandArgs)
{
    int n = commandArgs.size();
    // The last element is the destination directory
    string destRelativePath = commandArgs[n - 1];
    string destAbsolutePath = convertToAbsolutePath(destRelativePath);

    for (int i = 1; i < n - 1; i++)
    {
        //Get the stats of the source so that we can understand if the source i file or directory
        string source = commandArgs[i];

        string sourceAbsolutePath = convertToAbsolutePath(source);
        if (IsRegularFile(sourceAbsolutePath))
        {
            if (IsDirectory(destAbsolutePath))
            {
                int pos = sourceAbsolutePath.find_last_of('/');
                string filename = sourceAbsolutePath.substr(pos + 1);

                copyFileToDirectory(sourceAbsolutePath, destAbsolutePath);
            }
            else if (IsRegularFile(destAbsolutePath))
            {
                copyFileToFile(sourceAbsolutePath, destAbsolutePath);
            }
        }
        else if (IsDirectory(sourceAbsolutePath))
        {
            copyDirectoryToDirectory(sourceAbsolutePath, destAbsolutePath);
        }
    }
}

//eg: path = foo.txt
void removeFile(string path)
{
    int r = remove(path.c_str());
}

//eg: path = B
void removeDirectory(string path)
{
    DIR *dr;
    struct dirent *en;
    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            char *filename = en->d_name;

            string filenameStr = filename;
            if (filenameStr != "." && filenameStr != "..")
            {
                if (IsRegularFile(path + "/" + filenameStr))
                {

                    removeFile(path + "/" + filenameStr);
                }
                else if (IsDirectory(path + "/" + filenameStr))
                {
                    //Recursively call directories
                    removeDirectory(path + "/" + filenameStr);
                }
            }
        }
    }
    int r = rmdir(path.c_str());
}

void moveCommand(vector<string> &commandArgs)
{
    copyCommand(commandArgs);
    int n = commandArgs.size();
    string dest = commandArgs[n - 1];
    string absDest = convertToAbsolutePath(dest);
    for (int i = 1; i < n - 1; i++)
    {
        string source = commandArgs[i];
        string absSource = convertToAbsolutePath(source);
        if (IsRegularFile(absSource))
        {
            removeFile(absSource);
        }
        else if (IsDirectory(absSource))
        {
            removeDirectory(absSource);
        }
    }
}

void renameCommand(vector<string> &commandArgs)
{
    string source = commandArgs[1];
    string dest = commandArgs[2];
    copyFileToFile(convertToAbsolutePath(source), convertToAbsolutePath(dest));
    removeFile(source);
}

void createFile(vector<string> &commandArgs)
{
    string filename = commandArgs[1];
    string path = convertToAbsolutePath(commandArgs[2]);
    string pathToCreate = path + "/" + filename;
    int in = open(pathToCreate.c_str(), O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
    if (in == -1)
    {
        cout << "Could not create file" << endl;
        return;
    }
    close(in);
}

void createDirectory(vector<string> &commandArgs)
{
    string filename = commandArgs[1];
    string path = convertToAbsolutePath(commandArgs[2]);
    string pathToCreate = path + "/" + filename;
    int makeDir = mkdir(pathToCreate.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    if (makeDir == -1)
    {
        cout << "Could not create directory" << endl;
        return;
    }
}

void listFilesCommandMode(string path)
{
    DIR *dr;
    struct dirent *en;
    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            char *filename = en->d_name;

            string filenameStr = filename;
            if (filenameStr != "." && filenameStr != "..")
            {
                cout << filenameStr << " ";
            }
        }
    }
    cout << endl;
}

bool searchCommand(string path, string name)
{
    DIR *dr;
    struct dirent *en;
    dr = opendir(path.c_str());
    if (dr)
    {
        while ((en = readdir(dr)) != NULL)
        {
            char *filename = en->d_name;

            string filenameStr = filename;

            if (filenameStr != "." && filenameStr != "..")
            {
                if (filenameStr == name)
                {
                    return true;
                }
                searchCommand(path + "/" + filenameStr, name);
            }
        }
    }
}

void commandMode()
{
    char ch;
    if (getcwd(homeDir, PATH_MAX) != 0)
    {
        home = homeDir;
    }
    currDirCommandMode = homeDir;

    bool whileFlag = true;
    while (true && whileFlag)
    {
        vector<string> commandArgs;
        string command, word;
        char ch;

        printf("\033[%d;0H", terminalRows + 2);
        printf("\033[2K");

        printf("\033[%d;0H", terminalRows + 3);
        cout << "--------------Commands executed: " << (commands++) << ", Status: OK--------------" << endl;
        printf("\033[%d;0H", terminalRows + 2);

        if ((ch = getchar()) != 27)
        {
            getline(cin, command);
            command = ch + command;
            stringstream ss(command);

            while (getline(ss, word, ' '))
            {
                commandArgs.push_back(word);
            }
            string cmd = commandArgs[0];
            if (cmd == "copy")
            {
                copyCommand(commandArgs);
            }
            else if (cmd == "move")
            {
                moveCommand(commandArgs);
            }
            else if (cmd == "rename")
            {
                renameCommand(commandArgs);
            }
            else if (cmd == "create_file")
            {
                createFile(commandArgs);
            }
            else if (cmd == "create_dir")
            {
                createDirectory(commandArgs);
            }
            else if (cmd == "delete_file")
            {
                removeFile(convertToAbsolutePath(commandArgs[1]));
            }
            else if (cmd == "delete_dir")
            {
                removeDirectory(convertToAbsolutePath(commandArgs[1]));
            }
            else if (cmd == "goto")
            {

                string path = convertToAbsolutePath(commandArgs[1]);
                previousPaths.push(path);
                int gotoDir = chdir(path.c_str());
                if (gotoDir == -1)
                {
                    cout << "Could not go to the path you mentioned..." << endl;
                }
                else
                {
                    currDirCommandMode = path;
                    listFilesCommandMode(path);
                }
            }

            else if (cmd == "search")
            {
                string nameOfComponent = commandArgs[1];
                if (searchCommand(currDirCommandMode, nameOfComponent))
                    cout << "True" << endl;
                else
                    cout << "False" << endl;
            }
        }
        else
        {
            whileFlag = false;
        }
    }
    if (ch == 0)
    {
        exitedFromCommandMode = true;
        // Escape key is pressed
        createPathVector(currDir);
        int begin = filePaths.size() + 1 - terminalRows;
        if (begin >= 0)
        {
            listFiles(begin, filePaths.size());
        }
        else
        {
            listFiles(0, filePaths.size());
        }
    }
}

void takeCursorInput()
{
    int terminalRows = getTerminalSize();
    //changeToNonCanonical();
    struct termios oldt;
    struct termios newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int flagNormal = 0;
    while (1)
    {
        if (flagNormal == 0)
        {
            if (exitedFromCommandMode)
            {
                int dummy;
                dummy = getchar();
                exitedFromCommandMode = false;
            }

            int x;
            x = getchar();
            int y = ' ';
            int z = ' ';
            switch (x)
            {
            case 58:
            {
                // Colon key is pressed
                //restore old settings
                tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

                commandMode();

                // converting to non canonical
                struct termios oldt;
                struct termios newt;
                tcgetattr(STDIN_FILENO, &oldt);
                newt = oldt;
                newt.c_lflag &= ~(ICANON | ECHO);
                tcsetattr(STDIN_FILENO, TCSANOW, &newt);

                break;
            }

            case 127:
            {
                // Backspace is pressed
                if (!previousPaths.empty())
                {
                    currDir = previousPaths.top();
                    int pos = currDir.find_last_of('/');
                    string parentDirectory = currDir.substr(0, pos);
                    createPathVector(parentDirectory);
                    int begin = filePaths.size() + 1 - terminalRows;
                    if (begin >= 0)
                        listFiles(begin, filePaths.size());
                    else
                    {
                        listFiles(0, filePaths.size());
                    }
                }
                break;
            }
            case 10:
            {
                terminalRows = getTerminalSize();
                // Enter key is pressed
                string filepath = filePaths[currLine].second;
                struct stat fs;
                stat(filepath.c_str(), &fs);
                mode_t filePathmode = fs.st_mode;
                //Checking if the file is a directory and listing its contents
                if (S_ISDIR(fs.st_mode))
                {
                    createPathVector(filepath);
                    int begin = filePaths.size() + 1 - terminalRows;
                    if (begin >= 0)
                        listFiles(begin, filePaths.size());
                    else
                    {
                        listFiles(0, filePaths.size());
                    }
                }
                //Checking if the file is a text file and opening it in vi editor
                else if (S_ISREG(fs.st_mode))
                {
                    int pos = filepath.find_last_of("/");
                    string filename = filepath.substr(pos + 1);
                    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
                    flagNormal = 1;
                    pid_t processID;
                    if (fork() == 0)
                    {
                        execlp("vi", "vi", filename.c_str(), NULL);
                    }
                    else
                    {
                        processID = wait(NULL);
                    }
                    flagNormal = 0;
                    // Change to canonical
                    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
                }
                break;
            }
            case ((int)'k'):
            {
                // k key is pressed
                terminalRows = getTerminalSize();
                if (currLine > 0)
                {
                    if (currLine - 2 + terminalRows < filePaths.size())
                    {
                        listFiles(currLine - 1, currLine - 2 + terminalRows);
                    }
                    else
                    {
                        listFiles(currLine - 1, filePaths.size());
                    }
                    currLine--;
                }
                break;
            }

            case ((int)'l'):
            {
                terminalRows = getTerminalSize();
                // l is pressed
                if (currLine < filePaths.size() - 1)
                {
                    if (terminalRows - 1 + currLine < filePaths.size())
                    {
                        listFiles(currLine + 1, terminalRows - 1 + currLine);
                    }
                    else
                    {
                        listFiles(currLine + 1, filePaths.size());
                    }
                    currLine++;
                }

                break;
            }

            case ((int)'h'):
            {

                terminalRows = getTerminalSize();
                // h key is pressed

                createPathVector(home);
                int begin = filePaths.size() + 1 - terminalRows;
                if (begin >= 0)
                    listFiles(begin, filePaths.size());
                else
                {
                    listFiles(0, filePaths.size());
                }

                break;
            }
            case 27:
            {
                y = getchar();
                z = getchar();
                switch (z)
                {

                case 65:
                { // Up key is pressed
                    terminalRows = getTerminalSize();
                    if (currLine > 0 && cursorPos > 0)
                    {
                        printf("\033[1A"); //used to go one line above the cursor
                        cursorPos--;
                        currLine--;
                    }
                    break;
                }
                case 66:
                {
                    terminalRows = getTerminalSize();
                    // Down key is pressed

                    if (currLine < filePaths.size() - 1 && cursorPos < terminalRows - 2)
                    {
                        printf("\033[1B"); //- Move the cursor down 1 line:
                        if (currLine < filePaths.size() - 1)
                            currLine++;
                        cursorPos++;
                    }
                    break;
                }
                case 67:
                {

                    // Right key is pressed
                    if (!nextPaths.empty())
                    {
                        string currentDirectory = nextPaths.top();
                        nextPaths.pop();
                        previousPaths.push(currentDirectory);
                        createPathVector(currentDirectory);
                        int begin = filePaths.size() + 1 - terminalRows;
                        if (begin >= 0)
                            listFiles(begin, filePaths.size());
                        else
                        {
                            listFiles(0, filePaths.size());
                        }
                    }

                    break;
                }

                case 68:
                {
                    // Left key is pressed
                    if (!previousPaths.empty())
                    {
                        string curentDirectory = previousPaths.top();
                        previousPaths.pop();
                        nextPaths.push(curentDirectory);
                        if (!previousPaths.empty())
                        {
                            createPathVector(previousPaths.top());
                            int begin = filePaths.size() + 1 - terminalRows;
                            if (begin >= 0)
                                listFiles(begin, filePaths.size());
                            else
                            {
                                listFiles(0, filePaths.size());
                            }
                        }
                    }

                    break;
                }
                case 72:
                {
                    // Home key is pressed directly
                    createPathVector(homeDir);
                    int begin = filePaths.size() + 1 - terminalRows;
                    if (begin >= 0)
                        listFiles(begin, filePaths.size());
                    else
                    {
                        listFiles(0, filePaths.size());
                    }
                    break;
                }
                case 47:
                {
                    //  Home key (7 key without numlock) is pressed
                    createPathVector(currDir);
                    int begin = filePaths.size() + 1 - terminalRows;
                    if (begin >= 0)
                        listFiles(begin, filePaths.size());
                    else
                    {
                        listFiles(0, filePaths.size());
                    }
                    break;
                }

                default:
                    break;
                }
            }
            }
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt); //return ch; /*return received char */
}

void handleResize(int sigwinchID)
{
    if (sigwinchID == 28)
    {
        terminalRows = getTerminalSize();
        createPathVector(currDir);
        doNotInsertInStack = 1;
        int begin = filePaths.size() + 1 - terminalRows;
        if (begin >= 0)
            listFiles(begin, filePaths.size());
        else
        {
            listFiles(0, filePaths.size());
        }
    }
}

int main(void)
{
    terminalRows = getTerminalSize();

    // Used to catch the window resize signal
    signal(28, handleResize);

    if (getcwd(homeDir, PATH_MAX) != 0)
    {
        createPathVector(homeDir);
        home = homeDir;
        int begin = filePaths.size() + 1 - terminalRows;
        if (begin >= 0)
        {
            listFiles(begin, filePaths.size());
        }
        else
        {
            listFiles(0, filePaths.size());
        }
    }
    takeCursorInput();
    return 0;
}