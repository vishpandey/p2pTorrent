#include <iostream>
#include <future>
#include <thread>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <bits/stdc++.h>
#include <fstream>
#include <openssl/sha.h>
#include <sys/types.h> 
#include <sys/stat.h>

#define CHUNK_SIZE 512 * 1024

using namespace std;


bool isCreated;

int sock1 = 0, sock2 = 0;
struct sockaddr_in serv_addr1;
struct sockaddr_in serv_addr2;
string loggedInUuid = "";

pair<string, int> processAddressString(string ipAddress) {
	string token = "";
	pair<string, int> ipPortPair;

	for(int i = 0; i < ipAddress.length(); i++) {
		if(ipAddress.at(i) == ':') {
			ipPortPair.first = token;
			token = "";
			continue;
		}

		token += ipAddress.at(i);
	}

	//cout << "client hostname: " << ipPortPair.first << endl;
	//cout << "client port:" << token << endl;
	ipPortPair.second = stoi(token);

	return ipPortPair;
}

vector<pair<string, int>> readTrackerConfigFile(string trackerConfigFile) {
	ifstream file(trackerConfigFile);
	vector<pair<string, int>> trackerConfig;
	if(file.is_open()) {
		string line;
		while(getline(file, line)) {
			pair<string, int> tempPair;
			string token = "";

			for(int i = 0; i < line.length(); i++) {
				if(line.at(i) == ' ') {
					tempPair.first = token;
					token = "";
					continue;
				}

				token += line.at(i);
			}

			tempPair.second = stoi(token);

			trackerConfig.push_back(tempPair);
		}
	}

	return trackerConfig;
}

vector<string> processString(string cmd, char delimiter) {
	vector<string> tokens;
	string token = "";

	for(int i = 0; i < cmd.length(); i++) {
		if(cmd.at(i) == delimiter) {
			tokens.push_back(token);
			token = "";
			continue;
		}

		token += cmd.at(i);
	}

	tokens.push_back(token);

	return tokens;
}

string createStubFromUserInput(vector<string> command) {
	string stub = command[0];
	for(int i = 1; i < command.size(); i++) {
		stub += "$" + command[i];
	}

	return stub;
}

string chunkHash(char* chunkData, long int chunkSize) {
	unsigned char obuf[40];
	char buf[80];
	string hash = "";
	SHA1((unsigned char *)chunkData, chunkSize, obuf);

	for (int i = 0; i < 40; i++)
        sprintf((char *)&(buf[i * 2]), "%02x", obuf[i]);

    for (int i = 0; i < SHA_DIGEST_LENGTH * 2; i++) {
        hash += buf[i];
    }

    return hash;
}

string genFileHash(string filename) {
	ifstream srcFd(filename, ifstream::binary);
	string fileHash = "";
    if (!srcFd) {
        cout << "FILE DOES NOT EXITST : " << string(filename) << endl;
        return "false";
    }

    struct stat fileStat;
    stat(filename.c_str(), &fileStat);

    long int totalSize = fileStat.st_size;
    long int currChunkSize;

    while(totalSize > 0) {
    	currChunkSize = CHUNK_SIZE;
    	if(totalSize < CHUNK_SIZE) {
    		currChunkSize = totalSize;
    		
    	}
		char *chunkData = new char[currChunkSize];
		srcFd.read(chunkData, currChunkSize);
		totalSize = totalSize - currChunkSize;
		string cHash = chunkHash(chunkData, currChunkSize);
		fileHash += cHash;
		cout << "chunk hash : " << cHash << endl;
    }

    return fileHash;
}

bool createMTorrentFile(vector<string> command) {
	string mTorrentFilename = command[1] + ".mtorrent";

	struct stat seedFileStat;
    if (stat(command[1].c_str(), &seedFileStat) == -1) {
        cout << "FILE NOT FOUND" << endl;
        return false;
    }

    ofstream mTorrentFile(mTorrentFilename);

    string fileHash = genFileHash(command[1]);
    cout << "generated File Hash : " << fileHash << endl;
    mTorrentFile << command[1] << endl;
    mTorrentFile << fileHash << endl;
    mTorrentFile.close();

    return true;
}

void sendRequestToTracker(vector<string> command, 
						vector<pair<string, int>> trackerAddress,
						int sock, struct sockaddr_in serv_addr) {

	cout << "sending user request to tracker" << endl;
	int responseStatus;
	string stub = createStubFromUserInput(command);
	char *requestStub = new char[stub.length() + 1];
	strcpy(requestStub, stub.c_str());

	char buffer[1024] = {0};
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return;
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(trackerAddress[0].second);
    
    if(inet_pton(AF_INET, trackerAddress[0].first.c_str(), &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return;
    } 

	send(sock, requestStub, strlen(requestStub), 0);

	char responseStub[1024] = {0};
	responseStatus = recv(sock , responseStub, 1024, 0);

	string serverMessage = string(responseStub);
 	vector<string> tokens = processString(serverMessage, '$');

 	if(command[0] == "login") {
 		loggedInUuid = tokens[1];
 		cout << "loggein user token is: " << loggedInUuid << endl;
 	}
 	cout << "server returned " << responseStub << endl;

 	close(sock);
 	return;
}

int main(int argc, char *argv[]) {
	if(argc != 3) {
		cout << "Invalid Arguments. Expected Arguments 3." << endl;
		return 0;
	}

	pair<string, int> clientIpAddress = processAddressString(string(argv[1]));
	vector<pair<string, int>> trackerAddress = readTrackerConfigFile(string(argv[2]));

    // establishConnectionWithTracker(sock1, serv_addr1, trackerAddress[0]);
    // establishConnectionWithTracker(sock2, serv_addr2, trackerAddress[1]);

	// promise<bool> boolResponse;
	// auto f = boolResponse.get_future();
	while(1) {
		string cmd;
		getline(cin >> ws, cmd);

		vector<string> command = processString(cmd, ' ');

		if(command[0] == "create_user") {


			thread sendCreateUserThread(&sendRequestToTracker, command, 
										trackerAddress,
										sock1, serv_addr1);
			sendCreateUserThread.join();
		} else if (command[0] == "login") {
			if(loggedInUuid != "") {
				cout << "Already logged in with another user" << endl;
				continue;
			}

			command.push_back(clientIpAddress.first);
			command.push_back(to_string(clientIpAddress.second));
			thread sendLoginRequestThread(&sendRequestToTracker, command, 
										trackerAddress,
										sock1, serv_addr1);
			sendLoginRequestThread.join();
		} else if (command[0] == "create_group") {
			command.push_back(loggedInUuid);
			thread sendCreateGroupRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock1, serv_addr1);
			sendCreateGroupRequestThread.join();
		} else if (command[0] == "list_groups") {
			command.push_back(loggedInUuid);
			thread sendListGroupsRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendListGroupsRequestThread.join();
		} else if (command[0] == "join_group") {
			command.push_back(loggedInUuid);
			thread sendJoinGroupRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendJoinGroupRequestThread.join();
		} else if (command[0] == "logout") {
			command.push_back(loggedInUuid);
			thread sendLogoutRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendLogoutRequestThread.join();
		} else if (command[0] == "requests" && command[1] == "list_requests") {
			command.push_back(loggedInUuid);
			thread sendListPendingRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendListPendingRequestThread.join();
		} else if (command[0] == "accept_request") {
			command.push_back(loggedInUuid);
			thread sendAcceptRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendAcceptRequestThread.join();
		} else if (command[0] == "leave_group") {
			command.push_back(loggedInUuid);
			thread sendLeaveGroupRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendLeaveGroupRequestThread.join();
		} else if (command[0] == "upload_file") {
			command.push_back(loggedInUuid);
			createMTorrentFile(command);

			thread sendUploadFileRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendUploadFileRequestThread.join();
		} else if (command[0] == "list_files") {
			command.push_back(loggedInUuid);
			thread sendListFilesRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendListFilesRequestThread.join();
		} else if (command[0] == "download_file") {
			command.push_back(loggedInUuid);
			thread sendDownloadFileRequestThread(&sendRequestToTracker, command,
												trackerAddress,
												sock2, serv_addr2);
			sendDownloadFileRequestThread.join();
		}
	}
}