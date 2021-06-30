

// g++ siriTracker.cpp -o siriTracker -lpthread
//./siriTracker tracker_info.txt 1

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
#include <sstream>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <fstream>
#include <map>

#define deb(x) cout << #x << " :" << x << endl
using namespace std;

//Key is port number as a user can have multiple ports, but a port can have only one userID
unordered_map<string, string> portNumberUserID;

//Key is userID and value is the boolean : True if he is logged in else false
// If a user logs out once and is logged in on another terminal, he will be logged out
unordered_map<string, bool> userIdLoggedIn;

//Key is userID and value is password
unordered_map<string, string> userIDpassword;

//Placing NULL initially in group[group_id] to make sure that group exists
//Owner of the group will be group[group_id][1]
// The value is the vector of user_ids that are in the group
//Example: Map will be {{G1,[siri,sirisha,sai]},{G2,[sai,siri]}}
unordered_map<string, vector<string>> group;
unordered_map<string, vector<string>> pendingGroupRequests;

//If siri first uploads file1.txt in Group G1, the map entry will be like:
// listOfFilesSeeder[G1] = {file1.txt, siri}
// Then afterwards if

struct fileInformation
{
    // Get the port number
    vector<string> port_no;
    long long int fileSize;
    vector<bool> share; //corresponding to each peer
};

//Key is group ID and value is vector of files
// GroupID filename - peer1 peer2 peer3, 26
unordered_map<string, unordered_map<string, fileInformation>> listOfFiles;

string currentPortNumber;

string currUser_ID;
vector<string> splitString(string command)
{
    vector<string> vecCommand;
    stringstream ss(command);
    char ch = ' ';
    string a;
    while (ss >> a)
    {
        vecCommand.push_back(a);
    }
    return vecCommand;
}

void *acceptRequest(void *arg)
{
    int newSd = *((int *)arg);
    while (true)
    {
        char msg[1500];
        cout << "Connected with client!" << endl;
        int bytesRead, bytesWritten = 0;
        //receive a message from the client (listen)
        cout << "Awaiting client response..." << endl;
        memset(&msg, 0, sizeof(msg)); //clear the buffer
        bytesRead += recv(newSd, (char *)&msg, sizeof(msg), 0);
        //Message contains the command that the client has sent

        // if (!strcmp(msg, "exit"))
        // {
        //     cout << "Client has quit the session" << endl;
        //     break;
        // }
        string commandFromClient = msg;

        cout << "Received the message " << commandFromClient << " from the client" << endl;
        string reply;
        string portNumberOfClient;
        vector<string> commandVector = splitString(msg);
        if (commandVector[0] == "PortNumber")
        {
            portNumberOfClient = commandVector[1];
            reply = "Your port number is stored in the tracker as " + portNumberOfClient;
        }

        else if (commandVector[0] == "create_user")
        {
            currentPortNumber = commandVector[3];
            //Take user_id and password and store it in the map
            currUser_ID = commandVector[1];

            string password = commandVector[2];
            cout << "Received " << currUser_ID << " " << password << " from client" << endl;

            if (userIDpassword.find(currUser_ID) == userIDpassword.end())
            {
                userIDpassword[currUser_ID] = password;
                reply = "Created user successfully\n";
                portNumberUserID[portNumberOfClient] = currUser_ID;
                //reply+= "User "+currUser_ID+" connected to port number "+portNumberOfClient;
                cout << "Checking: " << endl;
                cout << "Port Number " << portNumberOfClient << " is attached to user " << portNumberUserID[portNumberOfClient] << endl;
            }
            else
            {
                reply = "User already exists.";
            }
        }
        else if (commandVector[0] == "login")
        {
            currentPortNumber = commandVector[3];
            currUser_ID = commandVector[1];
            string password = commandVector[2];
            if (userIDpassword.find(currUser_ID) == userIDpassword.end())
            {
                reply = "User not found";
            }
            else if (userIDpassword[currUser_ID] != password)
            {
                reply = "Incorrect password";
            }
            else
            {
                portNumberUserID[currentPortNumber] = currUser_ID;
                userIdLoggedIn[currUser_ID] = true;
                reply = "Successfully logged in...";
            }
        }
        else if (commandVector[0] == "create_group")
        {

            currentPortNumber = commandVector[2];
            cout << "Current port number is: " << currentPortNumber << endl;
            currUser_ID = portNumberUserID[currentPortNumber];
            cout << "Current user ID: " << currUser_ID << endl;
            cout << "Create group request received from user " << currUser_ID << endl;
            if (userIdLoggedIn[currUser_ID] == true)
            {
                cout << "User logged in..." << endl;
                cout << "Recieved the create_group request from client" << endl;
                string group_id = commandVector[1];
                group[group_id].push_back(currUser_ID); //Dummy string to note that group has been created
                reply = "Successfully created your group. You are now a member";
            }
            else
            {
                cout << "User is not logged in" << endl;
                reply = "User not logged in...";
            }
        }
        else if (commandVector[0] == "join_group")
        {
            currentPortNumber = commandVector[2];
            currUser_ID = portNumberUserID[currentPortNumber];
            string group_id = commandVector[1];
            vector<string> temp = pendingGroupRequests[group_id];
            vector<string> temp2 = group[group_id];
            if (group.find(group_id) != group.end())
            {
                if (find(temp.begin(), temp.end(), currUser_ID) != temp.end())
                {
                    reply = "Your request to join the group is already pending...";
                }

                else if (find(temp2.begin(), temp2.end(), currUser_ID) != temp2.end())
                {
                    reply = "You are already the part of this group...";
                }

                else
                {
                    if (userIdLoggedIn[currUser_ID] == true)
                    {

                        pendingGroupRequests[group_id].push_back(currUser_ID);
                        reply = "Your request to join group " + group_id + " is pending";
                    }
                    else
                    {
                        reply = "User not logged in";
                    }
                }
            }
            else
            {
                reply = "Group does not exist";
            }
        }

        else if (commandVector[0] == "requests" && commandVector[1] == "list_requests")
        {
            currentPortNumber = commandVector[3];
            currUser_ID = portNumberUserID[currentPortNumber];

            if (userIdLoggedIn[currUser_ID] == true)
            {
                string group_id = commandVector[2];
                if (group.find(group_id) == group.end())
                {
                    reply = "Group does not exist";
                }
                else
                {
                    reply = "The pending requests are:\n";
                    for (string pendingUser : pendingGroupRequests[group_id])
                    {
                        reply += pendingUser + "\n";
                    }
                }
            }
            else
            {
                reply = "User not logged in";
            }
        }

        else if (commandVector[0] == "accept_request")
        {
            currentPortNumber = commandVector[3];
            currUser_ID = portNumberUserID[currentPortNumber];

            if (userIdLoggedIn[currUser_ID] == true)
            {
                string group_id = commandVector[1];
                //Checking if the person is the owner of the group
                if (group.find(group_id) == group.end())
                {
                    reply = "Group does not exist";
                }
                else
                {
                    if (currUser_ID == group[group_id][0])
                    {

                        string user_id = commandVector[2];
                        group[group_id].push_back(user_id);
                        reply = user_id + " has joined the group " + group_id + " successfully...";
                    }
                    else
                    {
                        reply = "You are not the owner of the group";
                    }
                }
            }
            else
            {
                reply = "User not logged in";
            }
        }

        else if (commandVector[0] == "stop_share")
        {

            string group_id = commandVector[1];
            string filename = commandVector[2];
            currentPortNumber = commandVector[3];
            currUser_ID = portNumberUserID[currentPortNumber];
            auto it = find(listOfFiles[group_id][filename].port_no.begin(), listOfFiles[group_id][filename].port_no.end(), currentPortNumber);
            if (it != listOfFiles[group_id][filename].port_no.end())
            {
                int index = it - listOfFiles[group_id][filename].port_no.begin();
                listOfFiles[group_id][filename].share[index] = false;
                reply = "Stopped sharing "+filename;
            }
            else{
                reply = "It was not your file anyway";
            }

        }

            else if (commandVector[0] == "list_files")
            {
                string group_id = commandVector[1];
                currentPortNumber = commandVector[2];
                currUser_ID = portNumberUserID[currentPortNumber];
                vector<string> groupmembers = group[group_id];
                reply = "";
                if (find(groupmembers.begin(), groupmembers.end(), currUser_ID) != groupmembers.end())
                {
                    currUser_ID = portNumberUserID[currentPortNumber];
                    unordered_map<string, fileInformation> ffi = listOfFiles[group_id];
                    for (auto it = ffi.begin(); it != ffi.end(); it++)
                    {
                        string file = it->first;
                        fileInformation fi = it->second;
                        if (find(fi.share.begin(), fi.share.end(), true) != fi.share.end())
                        {
                            reply += file + " ";
                        }
                    }
                    if (reply == "")
                    {
                        reply = "No file found";
                    }
                }
                else
                {
                    reply = "Not a part of group. Cannot see files.";
                }
            }

            /*
        else if (commandVector[0] == "join_group")
        {
            string group_id = commandVector[1];
            if (group.find(group_id) != group.end())
            {
                group[group_id].push_back(currUser_ID);
                reply = "You have joined the group " + group_id + " successfully...";
            }
            else
            {
                reply = "GroupID does not exist. Please try again...";
            }
        }
*/
            else if (commandVector[0] == "leave_group")
            {
                currentPortNumber = commandVector[2];
                currUser_ID = portNumberUserID[currentPortNumber];
                if (userIdLoggedIn[currUser_ID] == true)
                {
                    cout << "Recieved the leave_group request from the user" << endl;
                    string group_id = commandVector[1];
                    vector<string> temp = group[group_id];
                    auto it = find(temp.begin(), temp.end(), currUser_ID);
                    if (it == temp.end())
                    {
                        reply = "You were not a member of this group anyway.";
                    }
                    else
                    {

                        group[group_id].erase(it);
                        reply = "Succesfully removed " + currUser_ID + " from the group " + group_id;
                        if (group[group_id].size() == 0)
                        {

                            //There is no one in this group anymore. So, dissolve the group.
                            group.erase(group_id);
                            reply += " Dissolved group";
                        }
                    }
                }
                else
                {
                    reply = "User not logged in";
                }
            }

            else if (commandVector[0] == "list_groups")
            {
                currentPortNumber = commandVector[1];
                currUser_ID = portNumberUserID[currentPortNumber];
                if (userIdLoggedIn[currUser_ID] == true)
                {
                    cout << "Received the list_groups request from the user" << endl;
                    reply = "Printing the list of groups: \n";
                    for (auto it = group.begin(); it != group.end(); it++)
                    {
                        reply += it->first + "\n";
                    }
                }
                else
                {
                    reply = "User not logged in...";
                }
            }
            else if (commandVector[0] == "logout")
            {
                currentPortNumber = commandVector[1];
                currUser_ID = portNumberUserID[currentPortNumber];
                userIdLoggedIn[currUser_ID] = false;
            }
            else if (commandVector[0] == "upload_file")
            {
                reply = "Processing upload request...";
            }

            else if (commandVector[0] == "CAN" && commandVector[1] == "I" && commandVector[2] == "UPLOAD?")
            {
                cout << "Checking if you can upload" << endl;
                currentPortNumber = commandVector[3];
                currUser_ID = portNumberUserID[currentPortNumber];

                if (userIdLoggedIn[currUser_ID] == true)
                {
                    reply = "OK";
                    string filePath = commandVector[4];
                    string group_id = commandVector[5];
                    long long int fileSize = stoll(commandVector[6]);
                    //Tracker knows what file is with what user
                    listOfFiles[group_id][filePath].port_no.push_back(currentPortNumber);
                    listOfFiles[group_id][filePath].fileSize = fileSize;

                    cout << currUser_ID << " of Group " << group_id << " has added file " << filePath << "with file size" << fileSize;
                }
                else
                {
                    reply = "NOT LOGGED IN";
                }
            }

            else if (commandVector[0] == "download_file")
            {
                currentPortNumber = commandVector[4];

                cout << "Received download request from " << currentPortNumber << endl;
                currUser_ID = portNumberUserID[currentPortNumber];
                string group_id = commandVector[1];
                vector<string> temp = group[group_id];
                if (find(temp.begin(), temp.end(), currUser_ID) != temp.end())
                {
                    //The user is the part of the group and can download the file
                    string filenameFromClient = commandVector[2];
                    string destinationPath = commandVector[3];
                    vector<string> peersToSend = listOfFiles[group_id][filenameFromClient].port_no;
                    long long int fileSize = listOfFiles[group_id][filenameFromClient].fileSize;
                    //Get the port to which the file is present for
                    reply = "Connect to port " + to_string(fileSize);
                    for (int i = 0; i < peersToSend.size(); i++)
                    {

                        reply += " " + peersToSend[i];
                    }
                       listOfFiles[group_id][filenameFromClient].port_no.push_back(currentPortNumber);

                    cout << "Sending peer list to client" << reply << endl;

                }
                else
                {
                    reply = "You are not the member of the group. You cannot download the file.";
                }
            }

            else
            {
                reply = "Kya command dia hai bhai";
            }
            //cout << "Client: " << msg << endl;
            cout << ">";

            memset(&msg, 0, sizeof(msg)); //clear the buffer
            strcpy(msg, reply.c_str());

            //send the message to client
            bytesWritten += send(newSd, (char *)&msg, strlen(msg), 0);
        }
        pthread_exit(NULL);
    }

    struct trackerInfo
    {
        string portNumber;
        string IPAddress;
    };

    //Read the port number and the ip from the file later -- CHANGE THIS
    // The command line input is in the form: ./tracker​ tracker_info.txt ​ tracker_no
    int main(int argc, char *argv[])
    {


        //Taking in command line arguments

        //Contains information of all trackers - key is ID and trackerinfo contains ipAddress and port
        map<string, trackerInfo> trackers;
        string trackerInfoFilePath;
        string tracker_no;
        cout << "Arg[0] " << argv[0] << endl;
        cout << "Arg[1] " << argv[1] << endl;
        cout << "Arg[2] " << argv[2] << endl;

        if (argc < 3)
        {
            cout << "Please give the tracker_info file path as the first argument and tracker number as the second argument" << endl;
        }
        else
        {
            trackerInfoFilePath = argv[1];
            tracker_no = argv[2];
            deb(trackerInfoFilePath);
            deb(tracker_no);
        }

        //Reading the data from tracker_info.txt

        //Buffer to store each line
        char *buffer = (char *)malloc(30);
        int pos;
        FILE *file = fopen(trackerInfoFilePath.c_str(), "r");
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
                vector<string> temp = splitString(line);

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

        string currPortNumber = trackers[tracker_no].portNumber;
        deb(currPortNumber);
        string currIP = trackers[tracker_no].IPAddress;
        deb(currIP);

        int newSd;
        int serverSd;
        pthread_t tid[15];
        int i = 0;
        // Take the port number from the tracker file later -- CHANGE THIS
        //int portNumber = 9734;
        int portNumber = stoi(currPortNumber);
        //buffer to send and receive messages with
        char msg[1500];
        sockaddr_in servAddr;
        bzero((char *)&servAddr, sizeof(servAddr));
        servAddr.sin_family = AF_INET;
        servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
        // inet_addr kabhi chalta hai kabhi nai INADDR_ANY mast chalta hai, wahi rakh
        //   servAddr.sin_addr.s_addr = inet_addr(currIP.c_str());
        servAddr.sin_port = htons(portNumber);
        //open stream oriented socket with internet address
        //also keep track of the socket descriptor
        serverSd = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSd < 0)
        {
            cerr << "Error establishing the server socket" << endl;
            exit(0);
        }
        //bind the socket to its local address
        int bindStatus = bind(serverSd, (struct sockaddr *)&servAddr,
                              sizeof(servAddr));
        if (bindStatus < 0)
        {
            cerr << "Error binding socket to local address" << endl;
            exit(0);
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

        while (newSd = accept(serverSd, (sockaddr *)&newSockAddr, &newSockAddrSize))
        {
            if (newSd < 0)
            {
                cerr << "Error accepting request from client!" << endl;
                exit(1);
            }
            else
            {
                if (pthread_create(&tid[i++], NULL, acceptRequest, &newSd) != 0)
                {
                    cout << "Thread creation failed\n";
                }
            }
        }
        close(newSd);
        close(serverSd);
        cout << "Connection closed..." << endl;

        return 0;
    }