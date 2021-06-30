//g++ siriClient.cpp -o siriClient -lpthread -lssl -lcrypto
// ./siriClient 127.0.0.1:12345 tracker_info.txt

#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <fstream>
#include <vector>
#include <sstream>
#include <map>
#include <unordered_map>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <openssl/sha.h>
#include <iomanip> //For converting SHA to readable hex string
#include <math.h>  // Ceil for calculating number of chunks
#include <algorithm>
#include <cstdlib> // Generate random chunk number
#include <semaphore.h>
#define CHUNCK_SIZE 10 //CHANGE THIS TO 512KB LATER -- Just for testing, I have taken 10
#define deb(x) cout << #x << " :" << x << endl

using namespace std;

//19 minutes
sem_t mutex;
bool uploading = false;
bool logged = false;
string user_id;
string password;
struct chunkInfo
{
    long long int fileSize = 0;
    long long int numberOfChunks = 0;
    vector<char> chunkVector;
    vector<string> shaOfChunks;
    bool share = true;
};

//Contains the filepath and the bit vector for the chunks it has
//Key is group_id, chunkInfo is struct with filename and bitvector
unordered_map<string, unordered_map<string, chunkInfo>> IHaveTheseFilesWithChunks;

vector<pair<string, vector<long long int>>> AllPeersChunkInfo;
// struct downloadInfo
// {
//     int portNumberOfUploader;
//     string sourceFilename;
//     string groupid;
//     long long int chunkNumber;
//     string destinationPath;
// };

struct trackerInfo
{
    string portNumber;
    string IPAddress;
};

vector<string> splitString(string command, char del)
{
    vector<string> vecCommand;
    stringstream ss(command);
    char ch = del;
    string a;
    while (ss >> a)
    {
        vecCommand.push_back(a);
    }
    return vecCommand;
}

long getFileSize(string filePath)

{
    long long int fileSize;
    int fd = open(filePath.c_str(), O_RDONLY);
    if (fd == -1)
    {
        cout << "Could not open the file you uploaded..." << endl;
        return -1;
    }
    struct stat stat_buf;
    int rc = fstat(fd, &stat_buf);
    if (rc == -1)
    {
        cout << "Could not get size of file" << endl;
        return -1;
    }
    else
    {
        fileSize = stat_buf.st_size;
        deb(fileSize);
    }

    return fileSize;
}

map<string, trackerInfo> trackers;

/*
vector<string> getShaOfChunks(string filePath, int numberOfChunks, long long int fSize)
{
    vector<string> shaOfChunks;
    int countStart = 0;
    FILE *fd = NULL;
    unsigned char buff[CHUNCK_SIZE];
    unsigned char hash[SHA_DIGEST_LENGTH]; // == 20
    memset(buff, 0, sizeof(buff));

    fd = fopen(filePath.c_str(), "rw+");

    if (fd == NULL)
    {
        printf("File could not be opened\n");
        return shaOfChunks;
    }

    printf("\n File opened successfully through fopen()\n");
    int bytesRead = 0;
    while (countStart < numberOfChunks)
    {
        // The first argument is a pointer to buffer used for reading/writing
        //the data. The data read/written is in the form of ‘nmemb’
        //elements each ‘size’ bytes long.
        //5 elemets each of 1 B stored in buff
        memset(buff, 0, sizeof(buff));
        bytesRead += fread(buff, 1, CHUNCK_SIZE, fd);

        //Buffer has the entire chunk now.
        printf("\n The bytes read are [%s]\n", buff);
        countStart++;
        const unsigned char *str = buff;
        SHA1(str, CHUNCK_SIZE, hash);
        //cout << "Sha for " << countStart << " chunck is: " << hash << endl;
        //string hashTemp = hash;
        //shaOfChunks.push_back(hash);

        for (int i = 0; i < SHA_DIGEST_LENGTH; i++)
            printf("%02x", hash[i]);

        std::ostringstream oss;
        for (int i = 0; i < SHA_DIGEST_LENGTH; ++i)
        {
            oss << std::hex << std::setw(2) << std::setfill('0') << +hash[i];
        }
        string hashString = oss.str();
        //cout<<"Added hash: "<<hash<<" to vector"<<endl;
        shaOfChunks.push_back(hashString);
        //      cout << endl;
        memset(buff, 0, sizeof(buff));
    }

    cout << "Bytes read are: " << bytesRead << endl;
    fclose(fd);

    printf("\n File stream closed through fclose()\n");

    if (bytesRead == fSize)
    {
        cout << "Successful SHA operation..." << endl;
    }
    else
    {

        cout << "Unsuccessful SHA operation..." << endl;
    }
    return shaOfChunks;
}
*/

void *startUploading(void *arg)
{

    cout << "startUploading thread started" << endl;
    //This is the server side functionality
    int newSd = *((int *)arg);
    // if (logged)
    //  {
    char msg[1500];
    cout << "Starting upload..." << endl;
    cout << "Connected with client!" << endl;
    int bytesRead, bytesWritten = 0;
    cout << "Awaiting client response..." << endl;
    memset(&msg, 0, sizeof(msg)); //clear the buffer
    //Receive the initiation message from client
    //msg has the string of sourceFile+" "+group_id+" "+to_string(chunkNumber)
    bytesRead += recv(newSd, (char *)&msg, sizeof(msg), 0);

    string request = msg;
    cout << "Got the peer's request as: " << msg << endl;
    vector<string> requestSplit = splitString(request, ' ');
    if (requestSplit[0] == "ASK")
    {
        string reply = "";
        string filename = requestSplit[2];
        string group_id = requestSplit[3];
        vector<char> v = IHaveTheseFilesWithChunks[group_id][filename].chunkVector;
        for (int i = 0; i < v.size(); i++)
        {
            if (v[i] == 'C' || v[i] == 'U')
            {
                reply += to_string(i) + " ";
            }
        }
        memset(&msg, 0, sizeof(msg)); //clear the buffer
        strcpy(msg, reply.c_str());
        deb(msg);

        //send the message of data+" "+hashString to client
        bytesWritten += send(newSd, (char *)&msg, strlen(msg), 0);
        cout << "Sent this message: " << msg << "to client" << endl;
    }
    else
    {

        string sourceFileName = requestSplit[0];
        deb(sourceFileName);
        string group_id = requestSplit[1];
        deb(group_id);
        long long int chunkNo = stoll(requestSplit[2]);
        deb(chunkNo);
        long long int numberOfChunks = stoll(requestSplit[3]);
        if (IHaveTheseFilesWithChunks[group_id][sourceFileName].share && (IHaveTheseFilesWithChunks[group_id][sourceFileName].chunkVector[chunkNo] != 'C' && IHaveTheseFilesWithChunks[group_id][sourceFileName].chunkVector[chunkNo] != 'U'))
        {
            //The peer does not have this chunk
            string data = "NO";
            memset(&msg, 0, sizeof(msg)); //clear the buffer
            strcpy(msg, data.c_str());
            deb(msg);
            //send the message of data+" "+hashString to client
            bytesWritten += send(newSd, (char *)&msg, strlen(msg), 0);
            cout << "Sent this message: " << msg << "to client" << endl;
            pthread_exit(NULL);
        }
        //Sending the file data to the user

        //Open the sourcefile, Send the data in the chunk number along with its SHA

        FILE *fd = NULL;

        unsigned char buff[1500]; // will store the data in the chunk requested
        //unsigned char hash[SHA_DIGEST_LENGTH]; // == 20

        memset(buff, 0, sizeof(buff));

        fd = fopen(sourceFileName.c_str(), "rw+");

        if (fd == NULL)
        {
            printf("File could not be opened for downloading\n");
        }

        printf("\n File opened successfully through fopen()\n");
        int bytesReadFromFile = 0;

        // The first argument is a pointer to buffer used for reading/
        //the data. The data read/written is in the form of ‘nmemb’
        //elements each ‘size’ bytes long.
        //5 elemets each of 1 B stored in buff
        int startOfChunk = fseek(fd, chunkNo * CHUNCK_SIZE, SEEK_SET);
        bytesReadFromFile += fread(buff, sizeof(char), CHUNCK_SIZE, fd);

        //Buffer has the entire chunk now.
        printf("\n The bytes read are [%s]\n", buff);

        //const unsigned char *str = buff;
        //Data to be sent to the downloader
        string data = "";
        for (char c : buff)
        {
            data += c;
        }
        deb(data);

        cout << "Bytes read are: " << bytesRead << endl;
        fclose(fd);

        printf("\n File stream closed through fclose()\n");

        cout << ">";

        ///// PROBLEM FOUND - Space me katt raha hai
        cout << "Sending this data to client: " << data << endl;
        cout << "Copying string to array" << endl;
        memset(&msg, 0, sizeof(msg)); //clear the buffer
        strcpy(msg, data.c_str());
        deb(msg);

        //send the message of data+" "+hashString to client
        bytesWritten += send(newSd, (char *)&msg, strlen(msg), 0);
        cout << "Sent this message: " << msg << "to client" << endl;
        //}
        for (int i = 1; i < numberOfChunks; i++)
        {
            char msg[1500];
            int bytesRead, bytesWritten = 0;
            memset(&msg, 0, sizeof(msg)); //clear the buffer
            //Receive the initiation message from client
            //msg has the string of sourceFile+" "+group_id+" "+to_string(chunkNumber)
            bytesRead += recv(newSd, (char *)&msg, sizeof(msg), 0);

            string request = msg;
            cout << "Got the peer's request as: " << msg << endl;
            vector<string> requestSplit = splitString(request, ' ');
            string sourceFileName = requestSplit[0];
            deb(sourceFileName);
            string group_id = requestSplit[1];
            deb(group_id);
            long long int chunkNo = stoll(requestSplit[2]);
            deb(chunkNo);
            if (IHaveTheseFilesWithChunks[group_id][sourceFileName].chunkVector[chunkNo] != 'C' && IHaveTheseFilesWithChunks[group_id][sourceFileName].chunkVector[chunkNo] != 'U')
            {
                //The peer does not have this chunk
                string data = "NO";
                memset(&msg, 0, sizeof(msg)); //clear the buffer
                strcpy(msg, data.c_str());
                deb(msg);
                //send the message of data+" "+hashString to client
                bytesWritten += send(newSd, (char *)&msg, strlen(msg), 0);
                cout << "Sent this message: " << msg << "to client" << endl;
                pthread_exit(NULL);
            }

            FILE *fd = NULL;

            unsigned char buff[1500]; // will store the data in the chunk requested
            //unsigned char hash[SHA_DIGEST_LENGTH]; // == 20

            memset(buff, 0, sizeof(buff));

            fd = fopen(sourceFileName.c_str(), "rw+");

            if (fd == NULL)
            {
                printf("File could not be opened for downloading\n");
            }

            printf("\n File opened successfully through fopen()\n");
            int bytesReadFromFile = 0;

            // The first argument is a pointer to buffer used for reading/
            //the data. The data read/written is in the form of ‘nmemb’
            //elements each ‘size’ bytes long.
            //5 elemets each of 1 B stored in buff
            int startOfChunk = fseek(fd, chunkNo * CHUNCK_SIZE, SEEK_SET);
            bytesReadFromFile += fread(buff, sizeof(char), CHUNCK_SIZE, fd);

            //Buffer has the entire chunk now.
            printf("\n The bytes read are [%s]\n", buff);

            //const unsigned char *str = buff;
            //Data to be sent to the downloader
            string data = "";
            for (char c : buff)
            {
                data += c;
            }
            deb(data);

            cout << "Bytes read are: " << bytesRead << endl;
            fclose(fd);

            printf("\n File stream closed through fclose()\n");

            cout << ">";

            ///// PROBLEM FOUND - Space me katt raha hai
            cout << "Sending this data to client: " << data << endl;
            cout << "Copying string to array" << endl;
            memset(&msg, 0, sizeof(msg)); //clear the buffer
            strcpy(msg, data.c_str());
            deb(msg);

            //send the message of data+" "+hashString to client
            bytesWritten += send(newSd, (char *)&msg, strlen(msg), 0);
            cout << "Sent this message: " << msg << "to client" << endl;
            //}
        }
    }

    pthread_exit(NULL);
}

int connectToServer(int port)
{
    cout << "Connecting to " << port << endl;
    sockaddr_in sendSockAddr;
    bzero((char *)&sendSockAddr, sizeof(sendSockAddr));
    sendSockAddr.sin_family = AF_INET;
    //sendSockAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
    //    inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
    string serverIp = "127.0.0.1";
    sendSockAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, serverIp.c_str(), &sendSockAddr.sin_addr) < 0)
    {
        cout << "Invalid address" << endl;
    }
    int clientSd = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSd == -1)
    {
        cout << "Error doing socket thingy" << endl;
    }
    //try to connect...
    //Try connecting to various trackers
    int status = connect(clientSd,
                         (sockaddr *)&sendSockAddr, sizeof(sendSockAddr));
    if (status < 0)
    {
        cout << "Error connecting to socket!" << endl;
    }
    else
    {
        cout << "Connected to the peer." << endl;
    }
    return clientSd;
}

void *startDownloading(void *args)
{

    cout << "Startdownloading function started..." << endl;
    string downloadInfo = (char *)args;
    cout << "Started downloading file with information: " << downloadInfo << endl;
    vector<string> dinfo = splitString(downloadInfo, ' ');
    int bytesRead = 0, bytesWritten = 0;
    char msg[1500];
    memset(&msg, 0, sizeof(msg)); //clear the buffer
    if (dinfo[0] == "ASK")
    {
        int socketno = stoi(dinfo[4]);
        memset(&msg, 0, sizeof(msg));
        strcpy(msg, downloadInfo.c_str());
        string serverport = dinfo[1];
        bytesWritten += send(socketno, (char *)&msg, strlen(msg), 0);
        memset(&msg, 0, sizeof(msg));
        bytesRead = recv(socketno, (char *)&msg, sizeof(msg), 0);
        string reply = msg;
        cout << reply << endl;
        vector<string> stringChunks = splitString(reply, ' ');
        vector<long long int> chunkNumbers;
        for (int i = 0; i < stringChunks.size(); i++)
        {
            chunkNumbers.push_back(stoll(stringChunks[i]));
        }
        pair<string, vector<long long int>> myPair = make_pair(serverport, chunkNumbers);
        AllPeersChunkInfo.push_back(myPair);
    }

    else
    {
        string sourceFile = dinfo[0];
        deb(sourceFile);
        string group_id = dinfo[1];
        deb(group_id);
        string destinationFileName = dinfo[2];
        deb(destinationFileName);
        int clientSd = stoi(dinfo[3]);
        deb(clientSd);
        long long int numberOfChunks = dinfo.size() - 4;
        for (int i = 4; i < dinfo.size(); i++)
        {
            int chunkNumberToGet = stoi(dinfo[i]);
            deb(chunkNumberToGet);
            IHaveTheseFilesWithChunks[group_id][sourceFile].chunkVector[chunkNumberToGet] = 'D';
            memset(&msg, 0, sizeof(msg));
            string requestForDownload = sourceFile + " " + group_id + " " + to_string(chunkNumberToGet) + " " + to_string(numberOfChunks);
            strcpy(msg, requestForDownload.c_str());
            bytesWritten += send(clientSd, (char *)&msg, strlen(msg), 0);
            cout << "Sent data " << requestForDownload << " to server" << endl;
            memset(&msg, 0, sizeof(msg)); //clear the buffer
            bytesRead = recv(clientSd, (char *)&msg, sizeof(msg), 0);
            //Get the data and the hashString in msg

            string reply = msg;
            cout << "Received reply: " << reply << " from server" << endl;
            deb(reply);
            if (reply == "NO")
            {
                cout << "Peer did not have the chunk" << endl;
                pthread_exit(NULL);
            }
            string data = reply;
            unsigned char buff[1500];
            //w+ creates file if already not present
            //Changing mode to rb+
            long long int fileSize = getFileSize(sourceFile);
            //Create the file if the file is not present

            //Added mutex
            //   sem_wait(&mutex);
            /*
            FILE *fd1 = NULL;
            fd1 = fopen(destinationFileName.c_str(), "r");
            if (fd1 == NULL)
            {
                FILE *fd2 = fopen(destinationFileName.c_str(), "w");
                //Make offset 0
                if (fallocate(fileno(fd2), 0, 0, fileSize) != 0)
                {
                    cout << "Could not allocate file" << endl;
                    pthread_exit(NULL);
                }
                fclose(fd2);
            }
            fclose(fd1);
            */

            FILE *fd3 = fopen(destinationFileName.c_str(), "a");

            if (fd3 == NULL)
            {
                printf("\n fopen() Error!!!\n");
                pthread_exit(NULL);
            }
            else
            {
                printf("\n File created successfully through fopen()\n");
            }

            memset(buff, 0, sizeof(buff));
            strcpy((char *)buff, data.c_str());

            int positionSet = fseek(fd3, chunkNumberToGet * CHUNCK_SIZE, SEEK_SET);

            if (positionSet != 0)
            {
                cout << "FSEEK failed" << endl;
                pthread_exit(NULL);
            }
            else
            {
                printf("\n fseek() successful\n");
            }
            int bytesWrittenToFile = fwrite(buff, sizeof(char), bytesRead, fd3);

            IHaveTheseFilesWithChunks[group_id][sourceFile].chunkVector[chunkNumberToGet] = 'C';
            // {
            //     printf("\n fwrite() failed\n");
            //     return 1;
            // }
            std::string dataWrittenToFile(buff, buff + sizeof(buff) / sizeof(buff[0]));
            //printf("\n fwrite() successful, %d bytes written to text file: %s\n", bytesWrittenToFile);
            cout << "Data written to file: " << dataWrittenToFile << endl;
            memset(buff, 0, sizeof(buff));
            fclose(fd3);
            //   sem_post(&mutex);
            printf("\n File stream closed through fclose()\n");
        }
        //Iss peer ka kaam khatam hua
        close(clientSd);
    }

    pthread_exit(NULL);
}

void *startServer(void *port)
{
    cout << "StartServer started" << endl;
    int portNumber = *((int *)port);
    cout << "Port " << portNumber << "is acting as a server now." << endl;
    pthread_t uploadID[15];
    //Yeh abhi server ka kaam karega
    int uploadThreadCount = 0;
    sockaddr_in servAddr;
    bzero((char *)&servAddr, sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    //servAddr.sin_addr.s_addr = inet_addr(ipAddressOfClient.c_str());
    servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servAddr.sin_port = htons(portNumber);
    int serverSd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSd < 0)
    {
        cerr << "Error establishing the server socket" << endl;
        exit(0);
    }
    int bindStatus = bind(serverSd, (struct sockaddr *)&servAddr,
                          sizeof(servAddr));
    if (bindStatus < 0)
    {
        cout << "Error binding socket to local address" << endl;
    }
    cout << "Waiting for a client to connect..." << endl;
    //listen for up to 5 requests at a time
    listen(serverSd, 5);
    //receive a request from client using accept
    //we need a new address to connect with the client
    sockaddr_in newSockAddr;
    socklen_t newSockAddrSize = sizeof(newSockAddr);
    //accept, create a new socket descriptor to
    //handle the new connection with client
    int newSd;
    cout << "Server is connected. Waiting for client" << endl;
    while (newSd = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize))
    {
        cout << "Aceppt has come from client" << endl;
        if (newSd < 0)
        {
            cerr << "Error accepting request from client!" << endl;
            exit(1);
        }
        else
        {
            if (pthread_create(&uploadID[uploadThreadCount++], NULL, startUploading, &newSd) != 0)
            {
                cout << "Thread creation failed\n";
            }
        }
    }
    close(newSd);
    close(serverSd);
    cout << "Server closed... Tata Bye Bye" << endl;
    pthread_exit(NULL);
}

bool cmp(const pair<string, vector<long long int>> &a, const pair<string, vector<long long int>> &b)
{
    if (a.second.size() < b.second.size())
        return true;
    else
        return false;
}

int main(int argc, char *argv[])
{
    sem_init(&mutex, 0, 1); //For file writing
    string IP_PortOfClient; //to function as a server this port ip is used
    string trackerFilePath;
    string portOfClient;
    string ipAddressOfClient;
    // Take command line arguments -- CHANGE THIS
    if (argc < 3)
    {
        cout << "Give the <IP>:<PORT> as first argument and tracker path as second arg" << endl;
        return 0;
    }
    else
    {
        IP_PortOfClient = argv[1];
        int colonPos = IP_PortOfClient.find(':');
        portOfClient = IP_PortOfClient.substr(colonPos + 1);
        ipAddressOfClient = IP_PortOfClient.substr(0, colonPos - 1);
        trackerFilePath = argv[2];
    }
    // Read contents of tracker
    // start the serverThread
    pthread_t startServerId;
    cout << "Starting server" << endl;
    int portToStartServer = stoi(portOfClient);
    if (pthread_create(&startServerId, NULL, startServer, (void *)&portToStartServer) != 0)
    {
        cout << "Server could not be started\n";
    }
    char *buffer = (char *)malloc(30);
    int pos;
    FILE *file = fopen(trackerFilePath.c_str(), "r");
    if (!file)
    {
        cout << "Could not open the tracker_info.txt" << endl;
    }
    else
    {
        char c;
        do
        {
            pos = 0;

            do
            {
                if ((c = fgetc(file)) != EOF)
                {
                    buffer[pos++] = (char)c;
                }

            } while (c != EOF && c != '\n');
            buffer[pos] = 0;
            // Buffer contains the entire line
            string line = buffer;
            vector<string> temp = splitString(line, ' ');

            struct trackerInfo t;
            t.IPAddress = temp[1];
            t.portNumber = temp[2];
            trackers[temp[0]] = t;
        } while (c != EOF);
        fclose(file);
        free(buffer);
    }
    cout << "Information of all trackers:" << endl;
    for (auto it = trackers.begin(); it != trackers.end(); it++)
    {
        cout << "ID: " << it->first << " Address: " << it->second.IPAddress << " Port: " << it->second.portNumber << endl;
    }

    int status = -1;
    char msg[1500];
    string serverIp;
    int port;
    int clientSd;
    auto it = trackers.begin();
    //Checking the live tracker
    while (status < 0 && it != trackers.end())
    {

        serverIp = it->second.IPAddress;    //--CHANGE THIS TAKE FROM USER
        port = stoi(it->second.portNumber); //-- CHANGE THIS TAKE FROM USER
                                            //create a message buffer

        //setup a socket and connection tools
        // struct hostent* host = gethostbyname(serverIp);
        sockaddr_in sendSockAddr;
        bzero((char *)&sendSockAddr, sizeof(sendSockAddr));
        sendSockAddr.sin_family = AF_INET;
        sendSockAddr.sin_addr.s_addr = inet_addr(serverIp.c_str());
        //    inet_addr(inet_ntoa(*(struct in_addr*)*host->h_addr_list));
        sendSockAddr.sin_port = htons(port);
        clientSd = socket(AF_INET, SOCK_STREAM, 0);
        //try to connect...
        //Try connecting to various trackers
        status = connect(clientSd,
                         (sockaddr *)&sendSockAddr, sizeof(sendSockAddr));
        if (status < 0)
        {
            cout << "Error connecting to socket!" << endl;
        }
        //  cout << "Connected to the server!" << endl;
        it++;
    }
    if (status < 0)
    {
        cout << "No working tracker found" << endl;
    }
    cout << "Successfully connected to: " << serverIp << ":" << port << endl;
    int bytesRead, bytesWritten = 0;
    string clientInformation = "PortNumber " + portOfClient;
    memset(&msg, 0, sizeof(msg)); //clear the buffer
    strcpy(msg, clientInformation.c_str());
    bytesWritten += send(clientSd, (char *)&msg, strlen(msg), 0);
    cout << "Awaiting server response..." << endl;
    memset(&msg, 0, sizeof(msg)); //clear the buffer
    bytesRead += recv(clientSd, (char *)&msg, sizeof(msg), 0);
    cout << "Received user message" << endl;
    cout << "Reply from server: " << msg << endl;
    while (1)
    {
        cout << ">";
        string command;
        getline(cin, command);

        vector<string> commandVector = splitString(command, ' ');

        //In case of upload file, the client will calculate the SHA of each chunk and store in vector

        if (commandVector[0] == "upload_file")
        {
            if (commandVector.size() != 3)
            {
                cout << "Give the filepath and the group both" << endl;
            }
            else
            {
                string file_path = commandVector[1];

                long long int fSize = getFileSize(file_path);
                deb(file_path);
                string group_id = commandVector[2];
                deb(group_id);
                string permission = "CAN I UPLOAD? " + portOfClient + " " + file_path + " " + group_id + " " + to_string(fSize);
                //Checking if the client is eligible to upload
                memset(&msg, 0, sizeof(msg)); //clear the buffer
                strcpy(msg, permission.c_str());
                bytesWritten += send(clientSd, (char *)&msg, strlen(msg), 0);
                //    cout<<"Sending the user_id and password to server "<<msg<<endl;
                cout << "Awaiting server response..." << endl;
                memset(&msg, 0, sizeof(msg)); //clear the buffer
                bytesRead += recv(clientSd, (char *)&msg, sizeof(msg), 0);
                cout << "Received tracker message" << endl;
                string reply = msg;
                cout << "Reply from tracker is: " << reply << endl;
                if (reply == "OK")
                {
                    cout << "I am logged in" << endl;
                    //The peer can start uploading
                    logged = true;
                    uploading = true;

                    //512 KB = 524288 B
                    long long int numberOfChunks = ceil((float)fSize / CHUNCK_SIZE);
                    deb(numberOfChunks);
                    //'U' stands for the client is the seeder of the file
                    // vector<string> shaOfChunksCall = getShaOfChunks(file_path, numberOfChunks, fSize);
                    vector<char> bitVector(numberOfChunks, 'U');

                    IHaveTheseFilesWithChunks[group_id][file_path].chunkVector = bitVector;
                    //IHaveTheseFilesWithChunks[group_id][file_path].shaOfChunks = shaOfChunksCall;
                    IHaveTheseFilesWithChunks[group_id][file_path].fileSize = fSize;
                    IHaveTheseFilesWithChunks[group_id][file_path].numberOfChunks = numberOfChunks;

                    cout << "Stopped uploading the file " << file_path << endl;
                }
            }
        }

        else if (commandVector[0] == "logout")
        {
            //Give user a error message if he is uploading
            if (uploading == true)
            {
                cout << "You are uploading a file. File will stop uploading if your log out." << endl;
                cout << "Are you sure you want to logout? Press Y/N" << endl;
                char c;
                cin >> c;
                if (c == 'Y')
                {
                    logged = false;
                    uploading = false;
                }
                if (c == 'N')
                {
                    cout << "Good. Stay Here." << endl;
                }
            }
            else
            {
                logged = false;
            }
        }
        else if (commandVector[0] == "stop_share")
        {
            string group_id = commandVector[1];
            string filename = commandVector[2];
            IHaveTheseFilesWithChunks[group_id][filename].share = false;
        }
        else if (commandVector[0] == "show_downloads")
        {
            for (auto it = IHaveTheseFilesWithChunks.begin(); it != IHaveTheseFilesWithChunks.end(); it++)
            {
                string group_id = it->first;

                unordered_map<string, chunkInfo> ci = it->second;
                for (auto it2 = ci.begin(); it2 != ci.end(); it++)
                {
                    string filename = it2->first;
                    struct chunkInfo seeStatus = it2->second;
                    if (find(seeStatus.chunkVector.begin(), seeStatus.chunkVector.end(), 'D') != seeStatus.chunkVector.end())
                    {
                        cout << "[C] [" << group_id << "] " << filename << endl;
                    }
                    else
                    {
                        cout << "[D] [" << group_id << "] " << filename << endl;
                    }
                }
            }
        }

        else if (commandVector[0] == "download_file")
        {
            if (commandVector.size() != 4)
            {
                cout << "Give parameters properly" << endl;
            }
            else
            {
                cout << "Download request got" << endl;
                string group_id = commandVector[1];
                string filename = commandVector[2];
                string destinationPathForDownload = commandVector[3];
                //Ask the tracker for the peer
                command += " " + portOfClient;
                memset(&msg, 0, sizeof(msg)); //clear the buffer
                strcpy(msg, command.c_str());
                bytesWritten += send(clientSd, (char *)&msg, strlen(msg), 0);
                //    cout<<"Sending the user_id and password to server "<<msg<<endl;
                cout << "Awaiting server response..." << endl;
                memset(&msg, 0, sizeof(msg)); //clear the buffer
                bytesRead += recv(clientSd, (char *)&msg, sizeof(msg), 0);
                cout << "Received tracker message" << endl;

                pthread_t downloadThreadID[15];
                int downloadThreadCount = 0;
                string replyFromTracker = msg;
                vector<string> serverReply = splitString(replyFromTracker, ' ');
                cout << "Reply from tracker: " << replyFromTracker << endl;
                if (serverReply[0] == "Connect" && serverReply[1] == "to" && serverReply[2] == "port")
                {
                    long long int fs = stoll(serverReply[3]);
                    IHaveTheseFilesWithChunks[group_id][filename].fileSize = fs;
                    long long int nOfChunks = ceil((float)fs / CHUNCK_SIZE);
                    IHaveTheseFilesWithChunks[group_id][filename].numberOfChunks = nOfChunks;
                    //Started downloading this file
                    vector<char> bv(nOfChunks, 'S');
                    IHaveTheseFilesWithChunks[group_id][filename].chunkVector = bv;
                    int numberofportsThatHaveFile = serverReply.size() - 4;
                    unordered_map<string, int> SocketForEachServer;
                    //Create a thread for every port connection
                    for (int i = 4; i < serverReply.size(); i++)
                    {
                        //connect to port
                        string peerport = serverReply[i];
                        int socketno = connectToServer(stoi(peerport));
                        SocketForEachServer[peerport] = socketno;
                        string downloadInfo = "ASK " + peerport + " " + filename + " " + group_id + " " + to_string(socketno);
                        char dlInfo[1500];
                        memset(&dlInfo, 0, sizeof(dlInfo)); //clear the buffer
                        strcpy(dlInfo, downloadInfo.c_str());
                        deb(downloadInfo);
                        if (pthread_create(&downloadThreadID[downloadThreadCount++], NULL, startDownloading, (void *)&dlInfo) != 0)
                        {
                            cout << "Thread creation failed\n";
                        }
                    }
                    for (int i = 0; i < numberofportsThatHaveFile; i++)
                    {

                        pthread_join(downloadThreadID[i], NULL);
                    }
                    cout << " PRINTING PEER INFO ----------------------" << endl;
                    for (pair<string, vector<long long int>> p : AllPeersChunkInfo)
                    {
                        cout << "Peer: " << p.first << " ";
                        cout << "Chunks: ";
                        for (long long int lli : p.second)
                        {
                            cout << lli << " ";
                        }
                    }
                    //For load balancing
                    for (auto it = SocketForEachServer.begin(); it != SocketForEachServer.end(); it++)
                    {
                        close(it->second);
                    }

                    sort(AllPeersChunkInfo.begin(), AllPeersChunkInfo.end(), cmp);
                    for (pair<string, vector<long long int>> p : AllPeersChunkInfo)
                    {

                        int socketToConnect = connectToServer(stoi(p.first));
                        deb(socketToConnect);
                        string chunksneeded = "";
                        for (long long int lli : p.second)
                        {
                            long long int cn = lli;
                            if (IHaveTheseFilesWithChunks[group_id][filename].chunkVector[lli] == 'S')
                            {
                                chunksneeded += to_string(lli) + " ";
                            }
                        }
                        string downloadInfo = filename + " " + group_id + " " + destinationPathForDownload + " " + to_string(socketToConnect) + " " + chunksneeded;
                        char dlInfo[1500];
                        memset(&dlInfo, 0, sizeof(dlInfo)); //clear the buffer
                        strcpy(dlInfo, downloadInfo.c_str());
                        deb(downloadInfo);
                        if (pthread_create(&downloadThreadID[downloadThreadCount++], NULL, startDownloading, (void *)&dlInfo) != 0)
                        {
                            cout << "Thread creation failed\n";
                        }
                    }

                    /*
             vector<pair<string,vector<long long int>>> PeersInfoAboutChunks;

        string concatInfo = (char *)reply;
      vector<string> splitChunkNumbers = splitString(concatInfo, ' ');
      vector<long long int> chunkNumbers;
      for(long long i =4;i<serverReply.size();i++)
      {
          chunkNumbers.push_back(stoll(splitChunkNumbers[i]));
      }

      pair<string, vector<long long int>> temp = make_pair(serverReply[i],chunkNumbers);
    PeersInfoAboutChunks.push_back(temp);
             }



cout<<"Printing peer information";
             for(int i=0;i<PeersInfoAboutChunks.size();i++){
                 pair<string,vector<long long int>> temp1 = PeersInfoAboutChunks[i];
                 cout<<"Peer: "<<temp1.first<<endl;
                 cout<<"Chunks it has: "<<endl;
                 for(long long int i: temp1.second){
                     cout<<i<<" ";
                 }
             }
*/
                    //Got the chunk information from all the peers
                    /*
   while (find(IHaveTheseFilesWithChunks[group_id][filename].chunkVector.begin(), IHaveTheseFilesWithChunks[group_id][filename].chunkVector.end(), 'S') != IHaveTheseFilesWithChunks[group_id][filename].chunkVector.end())
                    {

                        int chunkNumberToAsk = (rand() % nOfChunks); //generating random chunk number
                                                                     // if the chunk is not already downloaded or is in the process of being downloaded by other peer
                        if (IHaveTheseFilesWithChunks[group_id][filename].chunkVector[chunkNumberToAsk] == 'S')
                        {
                            string downloadInfo = peerport + " " + filename + " " + group_id + " " + to_string(chunkNumberToAsk) + " " + destinationPathForDownload;
                            char dlInfo[1500];
                            memset(&dlInfo, 0, sizeof(dlInfo)); //clear the buffer
                            strcpy(dlInfo, downloadInfo.c_str());
                            deb(downloadInfo);
                            cout << "Connecting to port " << peerport << " for chunk number " << chunkNumberToAsk << endl;
                            IHaveTheseFilesWithChunks[group_id][filename].chunkVector[chunkNumberToAsk] = 'D';
                            if (pthread_create(&downloadThreadID[downloadThreadCount++], NULL, startDownloading, (void *)&dlInfo) != 0)
                            {
                                cout << "Thread creation failed\n";
                            }
                        }
                    }
                    */
                }

                else
                {
                    cout << "Could not get peer port" << endl;
                }
            }
        }
        // MAJOR CHANGE FOR TODAY download_file to this
        if (commandVector[0] != "upload_file" && commandVector[0] != "download_file" && commandVector[0] != "show_downloads")
        {
            command += " " + portOfClient;

            memset(&msg, 0, sizeof(msg)); //clear the buffer
            strcpy(msg, command.c_str());

            bytesWritten += send(clientSd, (char *)&msg, strlen(msg), 0);
            //    cout<<"Sending the user_id and password to server "<<msg<<endl;
            cout << "Awaiting server response..." << endl;
            memset(&msg, 0, sizeof(msg)); //clear the buffer
            bytesRead += recv(clientSd, (char *)&msg, sizeof(msg), 0);
            cout << "Received user message" << endl;

            string replyFromServer = msg;
            vector<string> serverReply = splitString(replyFromServer, ' ');

            cout << "Reply from tracker: " << replyFromServer << endl;
        }
    }
    close(clientSd);
    cout << "********Session********" << endl;

    return 0;
}
