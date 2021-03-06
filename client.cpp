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
#include <filesystem>
#include <semaphore.h>

#define CHUNK_SIZE 512 * 1024

using namespace std;

bool isCreated;

int sock1 = 0, sock2 = 0;
struct sockaddr_in serv_addr1;
struct sockaddr_in serv_addr2;
string loggedInUuid = "";
pair<string, int> clientIpAddress;

class Downloads {
	string downloadId;
	string groupId;
	string filename;
	int status;
	string uuid;
	string seederUuid;
public:
	Downloads(string downloadId, string groupId, string filename, int status, string uuid, 
				string seederUuid) {
		this->downloadId = downloadId;
		this->groupId = groupId;
		this->filename = filename;
		this->status = status;
		this->uuid = uuid;
		this->seederUuid = seederUuid;
	}

	string getGroupId() {
		return this->groupId;
	}

	string getFilename() {
		return this->filename;
	}

	int getStatus() {
		return this->status;
	}

	string getUuid() {
		return this->uuid;
	}

	string getSeederUuid() {
		return this->seederUuid;
	}

	string getDownloadId() {
		return this->downloadId;
	}

	void setGroupId(string groupId) {
		this->groupId = groupId;
	}

	void setFilename(string filename) {
		this->filename = filename;
	}

	void setStatus(int status) {
		this->status = status;
	}

	void setUuid(string uuid) {
		this->uuid = uuid;
	}

	void setSeederUuid(string uuid) {
		this->uuid = uuid;
	}

	void setDownloadId(string downloadId) {
		this->downloadId = downloadId;
	}
};

class Shares {
	string filename;
	string groupId;
	string seederUuid;
	int status;
	long long int chunksAlreadySent;
public:
	Shares(string filename, string groupId, string seederUuid, int status) {
		this->filename = filename;
		this->groupId = groupId;
		this->seederUuid = seederUuid;
		this->status = status;
		this->chunksAlreadySent = 0;
	}

	void setSeederUuid(string seederUuid) {
		this->seederUuid = seederUuid;
	}

	void setGroupId(string groupId) {
		this->groupId = groupId;
	}

	void setFilename(string filename) {
		this->filename = filename;
	}

	void setStatus(int status) {
		this->status = status;
	}

	void setChunksAlreadySent(long long int chunksAlreadySent) {
		this->chunksAlreadySent = chunksAlreadySent;
	}

	string getGroupId() {
		return this->groupId;
	}

	string getFilename() {
		return this->filename;
	}

	int getStatus() {
		return this->status;
	}

	string getSeederUuid() {
		return this->seederUuid;
	}

	long long int getChunksAlreadySent() {
		return this->chunksAlreadySent;
	}
};

vector<Downloads> downloadsListLeecher;
sem_t shareListSeederMutex;
map<string, Shares> shareListSeeder;
map<string, pair<sem_t, bool>> sharesMutex;

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

unsigned int random_char() {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);
    return dis(gen);
}

string genDownloadId(const unsigned int len) {
    std::stringstream ss;
    for (auto i = 0; i < len; i++) {
        const auto rc = random_char();
        std::stringstream hexstream;
        hexstream << std::hex << rc;
        auto hex = hexstream.str();
        ss << (hex.length() < 2 ? '0' + hex : hex);
    }
    return ss.str();
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

string genFileHash(string filepath) {
	ifstream srcFd(filepath, ifstream::binary);
	string fileHash = "";
    if (!srcFd) {
        cout << "FILE DOES NOT EXITST : " << string(filepath) << endl;
        return "false";
    }

    struct stat fileStat;
    stat(filepath.c_str(), &fileStat);

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
    }

    return fileHash;
}

bool createMTorrentFile(string filename, string path) {
	string filePath = path + filename;
	string mTorrentFilename = filename + ".mtorrent";
	string mTorrentFilePath = path + mTorrentFilename;

	struct stat seedFileStat;
    if (stat(filePath.c_str(), &seedFileStat) == -1) {
        cout << "FILE NOT FOUND" << filePath << endl;
        return false;
    }

    ofstream mTorrentFile(mTorrentFilePath);

    string fileHash = genFileHash(filePath);
    mTorrentFile << fileHash << endl;
    mTorrentFile.close();

    return true;
}

void createMTorrentFileOfTempFile(string tempFilename) {
	createMTorrentFile(tempFilename, "");
}

void sendFileContent(string filename, string groupId, string shareId, void *new_socket) {
	//cout << "intializing file transfer process" << endl;
	
	string filePath = "uploads/" + filename;
	int seederSocket = *(int *)new_socket;
	
	struct stat seedFileStat;
    if (stat(filePath.c_str(), &seedFileStat) == -1) {
        cout << "FILE NOT FOUND" << endl;
        return;
    }

    char *fpath = new char[filePath.length() + 1];
    strcpy(fpath, filePath.c_str());

    ifstream seederFile(fpath, ifstream::binary);

	long int totalSize = seedFileStat.st_size;
    long int currChunkSize;
    long int chunksAlreadySent = 0;
    char *chunkData;
    while(totalSize > 0) {

    	sem_wait(&shareListSeederMutex);
    	//cout << "taken mutex on share list" << endl;
    	auto shareEntityIter = shareListSeeder.find(shareId);

		auto mutexIter = sharesMutex.find(shareId);

		if(mutexIter == sharesMutex.end()) {
			break;
		}

		pair<sem_t, bool> seederMutex = mutexIter->second;
    	sem_wait(&seederMutex.first);
    	//cout << "taking mutex on seederMutex" << endl;
    	if(seederMutex.second) {	
    		//cout << "mutex boolean is true" << endl;
    		sem_post(&seederMutex.first);
    		sem_post(&shareListSeederMutex);
    		break;
    	}
		sem_post(&seederMutex.first);
		//cout << "releasing mutex on seedermutex" << endl;
    	sem_post(&shareListSeederMutex);
    	//cout << "releasing mutex on share list" << endl;

    	currChunkSize = CHUNK_SIZE;
    	if(totalSize < CHUNK_SIZE) {
    		currChunkSize = totalSize;    		
    	}
		chunkData =(char *) malloc( sizeof(char) * ( currChunkSize + 1 ) );;
		seederFile.read(chunkData, currChunkSize);

		send(seederSocket, chunkData, currChunkSize, 0);
		
		totalSize = totalSize - currChunkSize;

		chunksAlreadySent++;

		free(chunkData);
    }

    sem_wait(&shareListSeederMutex);
    auto shareEntityIter = shareListSeeder.find(shareId);
    Shares shareEntity = shareEntityIter->second;
    shareEntity.setChunksAlreadySent(chunksAlreadySent);
    if(totalSize <= 0) {
    	cout << "Download complete" << endl;
    	
    	string completeDownload = "$cOmPlEtE$";
    	char *completeDownloadSignal = new char[completeDownload.length() + 1];
		strcpy(completeDownloadSignal, completeDownload.c_str());
		cout << "sending complete download signal" << endl;
    	send(seederSocket, completeDownloadSignal, strlen(completeDownloadSignal), 0);
    	
    	shareEntity.setStatus(1);
    	shareEntityIter->second = shareEntity;
    	sem_post(&shareListSeederMutex);
    	seederFile.close();
    	return;
    }

    shareEntityIter->second = shareEntity;
    sem_post(&shareListSeederMutex);

    cout << "pause download" << endl;
    string pauseDownload = "$PaUsE$";
	char *pauseDownloadSignal = new char[CHUNK_SIZE + 1];
	strcpy(pauseDownloadSignal, pauseDownload.c_str());
	send(seederSocket, pauseDownloadSignal, CHUNK_SIZE, 0);

    cout << "total Size is not 0, download stopped" << endl;
    cout << "stopping download" << endl;
    seederFile.close();
}

void reSendFileContent(string filename, string groupId, string shareId, void *new_socket) {
	cout << "re-intializing file transfer process" << endl;
	string filePath = "uploads/" + filename;
	string mTorrentFilename = filename + ".mtorrent";
	int seederSocket = *(int *)new_socket;
	
	struct stat seedFileStat;
    if (stat(filePath.c_str(), &seedFileStat) == -1) {
        cout << "FILE NOT FOUND" << endl;
        return;
    }

    char *fpath = new char[filePath.length() + 1];
    strcpy(fpath, filePath.c_str());

    ifstream seederFile(fpath, ifstream::binary);

	long int totalSize = seedFileStat.st_size;
    long int currChunkSize;

    sem_wait(&shareListSeederMutex);
    auto shareEntityIter = shareListSeeder.find(shareId);
    Shares shareEntity = shareEntityIter->second;
    long long int chunksAlreadySent = shareEntity.getChunksAlreadySent();
    cout << "chunks already sent : " << chunksAlreadySent << endl;
    sem_post(&shareListSeederMutex);
    long long int currChunkNum = 0;
    while(totalSize > 0) {
    	sem_wait(&shareListSeederMutex);
    	//cout << "taken mutex on share list" << endl;
    	auto shareEntityIter = shareListSeeder.find(shareId);

		auto mutexIter = sharesMutex.find(shareId);

		if(mutexIter == sharesMutex.end()) {
			break;
		}

		pair<sem_t, bool> seederMutex = mutexIter->second;
    	sem_wait(&seederMutex.first);
    	//cout << "taking mutex on seederMutex" << endl;
    	if(seederMutex.second) {	
    		//cout << "mutex boolean is true" << endl;
    		sem_post(&seederMutex.first);
    		sem_post(&shareListSeederMutex);

    		string pauseDownload = "$PaUsE$";
			char *pauseDownloadSignal = new char[pauseDownload.length() + 1];
			strcpy(pauseDownloadSignal, pauseDownload.c_str());
			send(seederSocket, pauseDownloadSignal, strlen(pauseDownloadSignal), 0);

    		break;
    	}
		sem_post(&seederMutex.first);
		//cout << "releasing mutex on seedermutex" << endl;
    	sem_post(&shareListSeederMutex);
    	//cout << "releasing mutex on share list" << endl;

    	currChunkSize = CHUNK_SIZE;
    	if(totalSize < CHUNK_SIZE) {
    		currChunkSize = totalSize;    		
    	}
		char *chunkData = new char[currChunkSize];
		if(currChunkNum < chunksAlreadySent) {
			seederFile.read(chunkData, currChunkSize);
			currChunkNum++;
    		continue;
    	}

		send(seederSocket, chunkData, currChunkSize, 0);
		
		totalSize = totalSize - currChunkSize;

		currChunkNum++;
		chunksAlreadySent++;
    }

    sem_wait(&shareListSeederMutex);
    shareEntityIter = shareListSeeder.find(shareId);
    shareEntity = shareEntityIter->second;
    shareEntity.setChunksAlreadySent(chunksAlreadySent);
    if(totalSize <= 0) {
    	cout << "Download complete" << endl;

    	string completeDownload = "$cOmPlEtE$";
		char *completeDownloadSignal = new char[CHUNK_SIZE + 1];
		strcpy(completeDownloadSignal, completeDownload.c_str());
		send(seederSocket, completeDownloadSignal, strlen(completeDownloadSignal), 0);
    	shareEntity.setStatus(1);
    	seederFile.close();
    	return;
    }

    shareEntityIter->second = shareEntity;
    sem_post(&shareListSeederMutex);

    cout << "total Size is not 0, download stopped" << endl;
    cout << "stopping download" << endl;
    seederFile.close();
}

void sendHashFileData(string filename, int new_socket) {
	cout << "sending hash of file" << endl;

	string mTorrentFilename = "uploads/" + filename + ".mtorrent";

	struct stat torrentFileStat;
    if (stat(mTorrentFilename.c_str(), &torrentFileStat) == -1) {
        cout << "FILE NOT FOUND" << endl;
        return;
    }

    char *fpath = new char[mTorrentFilename.length() + 1];
    strcpy(fpath, mTorrentFilename.c_str());

    ifstream mTorrentFile(fpath, ifstream::binary);

    string line;
    long long int hashNo = 0;
    string fileHash = "";

    long long int totalSize = torrentFileStat.st_size;
    long long int currChunkSize;
    char *chunkData;

    while(totalSize > 0) {
    	currChunkSize = CHUNK_SIZE;
    	if(totalSize < CHUNK_SIZE) {
    		currChunkSize = totalSize;
    	}

    	chunkData = (char *)malloc( sizeof(char) * (currChunkSize));
		mTorrentFile.read(chunkData, currChunkSize);

		send(new_socket, chunkData, currChunkSize, 0);
		
		totalSize = totalSize - currChunkSize;
		hashNo++;
		cout << "hash for chunk no " << hashNo << endl;

		free(chunkData);
    }

	mTorrentFile.close();
}

void sendSeederOfflineMessageToClient(int new_socket) {
	string response = "Failed, seeder is not online";
	char *responseStub = new char[response.length() + 1];

	strcpy(responseStub, response.c_str());
	send(new_socket, responseStub, strlen(responseStub), 0);
}

void seederService(pair<string, int> myIpAddress) {
	int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1; 
    int addrlen = sizeof(address);
	int readStatus;
	char *payload;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { 
        perror("socket failed"); 
        exit(EXIT_FAILURE); 
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) { 
        perror("setsockopt"); 
        exit(EXIT_FAILURE); 
    }

    address.sin_family = AF_INET; 
    address.sin_addr.s_addr = inet_addr(myIpAddress.first.c_str()); 
    address.sin_port = htons(myIpAddress.second);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    }

    cout << "seeder service initiated" << endl;
    cout << "seeder service ipAddress : " << myIpAddress.first << endl;
    cout << "seeder service port : " << myIpAddress.second << endl;

    while(true) {
	    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
	                       (socklen_t*)&addrlen)) < 0) {
	        cout << server_fd << endl; 
	        perror("accept"); 
	        exit(EXIT_FAILURE);
	    }

	    char buffer[1024] = {0};
	    readStatus = read(new_socket, buffer, 1024);

	    string clientMessage = string(buffer);
 		vector<string> tokens = processString(clientMessage, '$');

 		if(tokens[0] == "stat") {
 			if(tokens[4] != loggedInUuid) {
 				sendSeederOfflineMessageToClient(new_socket);
 				continue;
 			}

 			string seederFilePath = "uploads/" + tokens[1];

			struct stat seederFileStat;
		    if (stat(seederFilePath.c_str(), &seederFileStat) == -1) {
		        cout << "FILE NOT FOUND" << endl;
		        return;
		    }

		    long int fileSize = seederFileStat.st_size;
		    long int numOfChunksToReceive = 0;
		    if(fileSize % (CHUNK_SIZE) == 0) {
		    	numOfChunksToReceive = fileSize / (CHUNK_SIZE);
		    } else {
		    	numOfChunksToReceive = (fileSize / (CHUNK_SIZE)) + 1;
		    }

		    string response = to_string(fileSize) + "$" + to_string(numOfChunksToReceive);

		    char *responseStub = new char[response.length() + 1];
			strcpy(responseStub, response.c_str());
			send(new_socket, responseStub, strlen(responseStub), 0);
 		} else if (tokens[0] == "ping") {
 			string response = "True";
 			
 			if(tokens[1] != loggedInUuid) {
 				response = "False";
 			}

 			char *responseStub = new char[response.length() + 1];
			strcpy(responseStub, response.c_str());
			send(new_socket, responseStub, strlen(responseStub), 0);

 		} else if(tokens[0] == "new_download") {
 			cout << "got download request from one of the clients" << endl;
 			
 			string filename = tokens[1];
 			string groupId = tokens[2];
 			string shareId = tokens[3];
 			string seederUuid = tokens[4];

 			if(seederUuid != loggedInUuid) {
 				sendSeederOfflineMessageToClient(new_socket);
 				continue;
 			}
 			
 			sem_wait(&shareListSeederMutex);

 			sem_t seederDownloadMutex;
			sem_init(&seederDownloadMutex, 0, 1);
 			
			shareListSeeder.insert({shareId, Shares(filename, groupId, 
									loggedInUuid, 0)});
 			sharesMutex.insert({shareId, 
 								make_pair(seederDownloadMutex, false)});

 			sem_post(&shareListSeederMutex);

 			thread sendFileContentThread(&sendFileContent, filename, groupId,
 										shareId,
 										(void *)&new_socket);
 			
 			sendFileContentThread.detach();
 		} else if (tokens[0] == "new_file_hash") {
 			if(tokens[4] != loggedInUuid) {
 				sendSeederOfflineMessageToClient(new_socket);
 				continue;
 			}

 			sendHashFileData(tokens[1], new_socket);
 		} else if (tokens[0] == "resume_download") {
 			cout << "got request to resume download" << endl;

 			string filename = tokens[1];
 			string groupId = tokens[2];
 			string shareId = tokens[3];
 			string seederUuid = tokens[4];

 			if(seederUuid != loggedInUuid) {
 				sendSeederOfflineMessageToClient(new_socket);
 				continue;
 			}
 			
 			sem_wait(&shareListSeederMutex);

 			auto shareMutexIter = sharesMutex.find(shareId);

 			if(shareMutexIter == sharesMutex.end()) {
 				cout << "download could not be resumed" << endl;
 				continue;
 			}

 			pair<sem_t, bool> seederDownloadMutex = shareMutexIter->second;
 			
 			sem_wait(&seederDownloadMutex.first);

 			seederDownloadMutex.second = false;

 			sem_post(&seederDownloadMutex.first);

 			shareMutexIter->second = seederDownloadMutex;

 			sem_post(&shareListSeederMutex);

 			thread reSendFileContentThread(&reSendFileContent, filename, groupId,
 										shareId,
 										(void *)&new_socket);
 			
 			reSendFileContentThread.detach();
 		}
	}

	close(new_socket);
}

bool fetchHashValueFromSeeder(string ipAddress, string port, string request, 
								string tempFilePath) {
	int sock;
	struct sockaddr_in serv_addr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    printf("\n Socket creation error \n");
        return false;
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(stoi(port));
    
    if(inet_pton(AF_INET, ipAddress.c_str(), &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        close(sock);
        return false;
    }

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        close(sock);
        return false;
    }
	
	char *requestStub = new char[request.length() + 1];
	strcpy(requestStub, request.c_str());
	
	send(sock, requestStub, strlen(requestStub), 0);

	string seederFileHashFromServer = "", tempTorrentFileHash = "";
	int responseStatus;
	
	ifstream srcFd(tempFilePath, ifstream::binary);
	string fileHash = "";
    if (!srcFd) {
        cout << "FILE DOES NOT EXITST : " << string(tempFilePath) << endl;
        close(sock);
        return false;
    }

    // struct stat fileStat;
    // stat(filepath.c_str(), &fileStat);
    long long int hashno = 0;

    tempFilePath += ".mtorrent"; 
    ifstream mTorrentTempFile(tempFilePath.c_str(), ifstream::binary);

    char *responseStub, *tempHashChunkData;

	do {
		responseStub = (char *)malloc( sizeof(char) * (CHUNK_SIZE));
		
		responseStatus = read(sock , responseStub, CHUNK_SIZE);
		
		hashno++;
		cout << "received hash no " << hashno << endl;
		
		seederFileHashFromServer = string(responseStub);

		tempHashChunkData = (char *)malloc( sizeof(char) * (CHUNK_SIZE / 8));
		mTorrentTempFile.read(tempHashChunkData, CHUNK_SIZE / 8);

		tempTorrentFileHash = string(tempHashChunkData);

		if(tempTorrentFileHash != seederFileHashFromServer) {
			//cout << "actual hash: " << tempTorrentFileHash << endl;
			//cout << "expected hash : " << seederFileHashFromServer << endl;
			cout << "hash of chunks " << hashno <<  " did not match" << endl;
			srcFd.close();
			close(sock);
			return false;
		}

		free(responseStub);
		free(tempHashChunkData);

	} while (responseStatus > 0);

	srcFd.close();
	close(sock);

	return true;
}

string fetchFileDetailsFromSeeder(string ipAddress, string port, string request) {
	int sock;
	struct sockaddr_in serv_addr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    printf("\n Socket creation error \n");
        return "False";
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(stoi(port));
    
    if(inet_pton(AF_INET, ipAddress.c_str(), &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return "False";
    }

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return "False";
    }
	
	string fileStatDetails = "";
	char *requestStub = new char[request.length() + 1];
	strcpy(requestStub, request.c_str());
	
	send(sock, requestStub, strlen(requestStub), 0);

	char responseStub[CHUNK_SIZE] = {0};
	int responseStatus = read(sock , responseStub, CHUNK_SIZE);

	fileStatDetails += string(responseStub);

	close(sock);

	return fileStatDetails;
}

bool getPingResponse(string ipAddress, string port, string seederUuid) {
	int sock;
	struct sockaddr_in serv_addr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    printf("\n Socket creation error \n");
        return false;
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(stoi(port));
    
    if(inet_pton(AF_INET, ipAddress.c_str(), &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return false;
    }

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return false;
    }

    string request = "ping$" + seederUuid;

	char *requestStub = new char[request.length() + 1];
	strcpy(requestStub, request.c_str());
	
	send(sock, requestStub, strlen(requestStub), 0);

	char *buffer = new char[CHUNK_SIZE];
    int responseStatus = read(sock, buffer, CHUNK_SIZE);

    close(sock);
    if(responseStatus > 0 && string(buffer) == "True") {
    	return true;
    }

    return false;
}

void initiateBlockingPingCall(string ipAddress, string port, string seederUuid) {
	cout << "starting blocking ping call for uuid " << seederUuid << endl;
	while(true) {
		int sock;
		struct sockaddr_in serv_addr;

		if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		    printf("\n Socket creation error \n");
	        continue;
	    }
	   
	    serv_addr.sin_family = AF_INET;
	    serv_addr.sin_port = htons(stoi(port));
	    
	    if(inet_pton(AF_INET, ipAddress.c_str(), &serv_addr.sin_addr)<=0) {
	        printf("\nInvalid address/ Address not supported \n");
	    }
		if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
	        printf("\nConnection Failed \n");
	    }

	    string request = "ping$" + seederUuid;

		char *requestStub = new char[request.length() + 1];
		strcpy(requestStub, request.c_str());
		
		send(sock, requestStub, strlen(requestStub), 0);

		char *buffer = new char[CHUNK_SIZE];
	    int responseStatus = read(sock, buffer, CHUNK_SIZE);

	    if(responseStatus > 0 && string(buffer) == "True") {
	    	break;
	    }

	    close(sock);
	    sleep(10);
	}
}

void writeSeederFileData(string ipAddress, string port, string request, 
							string tempFilename, 
							long long int totalFileSize, 
							long long int numOfChunksToReceive,
							string seederUuid, 
							string filename, 
							string groupId, 
							string downloadId) {
	
	cout << "got " << request << " request" << endl;
	int sock;
	struct sockaddr_in serv_addr;

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
	    printf("\n Socket creation error \n");
        return;
    }
   
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(stoi(port));
    
    if(inet_pton(AF_INET, ipAddress.c_str(), &serv_addr.sin_addr)<=0) {
        printf("\nInvalid address/ Address not supported \n");
        return;
    }

	if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return;
    }
	
	request += "$" + filename + "$" + groupId + "$" + downloadId + "$" + seederUuid;
	char *requestStub = new char[request.length() + 1];
	strcpy(requestStub, request.c_str());
	
	send(sock, requestStub, strlen(requestStub), 0);

    char *destPath = new char[tempFilename.length() + 1];
    strcpy(destPath, tempFilename.c_str());
    ofstream destFile(destPath, ofstream::binary);
    int n;
    long long int numOfChunksReceived = 0, totalSizeReceived = 0;

    do
    {
        char *buffer = new char[CHUNK_SIZE];
        n = read(sock, buffer, CHUNK_SIZE);

        if(strstr(buffer, "$cOmPlEtE$")) { 
        	cout << "received complete signal from seeder" << endl; 
        	break;
        }

     	if(strstr(buffer, "$PaUsE$")) {
     		cout << "received pause signal from seeder" << endl; 
        	break;
     	}

        destFile.write(buffer, n);
        numOfChunksReceived++;
        cout << "chunk no:" << numOfChunksReceived << " received" <<endl; 
    } while (n > 0);

    cout << "numOfChunksReceived : " << numOfChunksReceived << endl;
    cout << "numOfChunksToReceive : " << numOfChunksToReceive << endl;
    if(numOfChunksReceived < numOfChunksToReceive) {
		destFile.close();
		close(sock);
    	if (!getPingResponse(ipAddress, port, seederUuid)) {
    		initiateBlockingPingCall(ipAddress, port, seederUuid);
    		cout << "exiting blocking ping call" << endl;
    		numOfChunksToReceive = numOfChunksToReceive - numOfChunksReceived;
    		request = "resume_download";
    		writeSeederFileData(ipAddress, port, request, tempFilename, totalFileSize,
    			numOfChunksToReceive, seederUuid, filename, groupId, downloadId);

    		return;
    	}
    }

    destFile.close();

	close(sock);

	return;
}

bool verifySeederFileData(string fileHash, string tempFilename, string filename) {
	cout << "verifying seed file hash" << endl;
	string tempFileHash = filename + "\n" + genFileHash(tempFilename) + "\n";
	if(tempFileHash == fileHash) {
		return true;
	}

	cout << "file hash do not match" << endl;
	return false;
}

void transferDataFromTempToOriginalFile(string tempFile, string destFile) {
	ifstream ini_file {tempFile};
    ofstream out_file {destFile};

    string line;
    if(ini_file && out_file){
 
        while(getline(ini_file,line)){
            out_file << line << "\n";
        }
 
        //cout << "Copy Finished \n";
 
    } else {
        printf("Cannot read File");
    }

    ini_file.close();
    out_file.close();

    return;
}

void deleteTempFile(string tempFilename) {
	remove(tempFilename.c_str());
}

void changeDownloadStatus(string groupId, string filename, int status) {
	for(int i = 0; i < downloadsListLeecher.size(); i++) {
		Downloads temp = downloadsListLeecher[i];

		if(downloadsListLeecher[i].getFilename() == filename && 
			downloadsListLeecher[i].getGroupId() == groupId && 
			downloadsListLeecher[i].getUuid() == loggedInUuid) {
			
			downloadsListLeecher[i].setStatus(status); 
			break;
		}
	}
}

void initializeDownload(string uuid, string ipAddress, string port, 
						string groupId, string filename, string destination) {
	
	cout << "download thread initialized" << endl; cout << "server ip : " << ipAddress << endl; 
	cout << "server port : " << port << endl;

	string downloadId = genDownloadId(10);

	string tempFilenamePrefix = "temp_";
	string tempFilename = tempFilenamePrefix + filename;

	string destinationFilename = destination + filename;

	string request = "stat$" + filename + "$" + groupId + "$" + downloadId + "$" + uuid;
    string fileStatDetails = fetchFileDetailsFromSeeder(ipAddress, port, request);
    cout << "received file details" << endl;

    vector<string> tokens = processString(fileStatDetails, '$');

    int totalFileSize = stoll(tokens[0]);
    int numOfChunksToReceive = stoll(tokens[1]);

    cout << "total filesize " << totalFileSize << endl;
    cout << "num of chunks in total " << numOfChunksToReceive << endl;

    downloadsListLeecher.push_back(Downloads(downloadId, groupId, filename, 0, loggedInUuid, uuid));

    request = "new_download";
	
	writeSeederFileData(ipAddress, port, request, tempFilename, 
						totalFileSize, numOfChunksToReceive, uuid, 
						filename, groupId, downloadId);

	// cout << "creating temp mtorrent file" << endl;
	// createMTorrentFileOfTempFile(tempFilename);
	
	request = "new_file_hash";
	request += "$" + filename + "$" + groupId + "$" + downloadId + "$" + uuid;
	//bool fileHashVerificationStatus = fetchHashValueFromSeeder(ipAddress, port, request, 
	//															tempFilename);
	
	bool fileHashVerificationStatus = true;
	cout << "received file hash details" << endl;

	if (fileHashVerificationStatus) {
		changeDownloadStatus(groupId, filename, 1);
		transferDataFromTempToOriginalFile(tempFilename, destinationFilename);
		createMTorrentFile(filename, destination);
		deleteTempFile(tempFilename);
		cout << "Download complete" << endl;
		return;
	}

	deleteTempFile(tempFilename);
	changeDownloadStatus(groupId, filename, 2);
	cout << "download failed" << endl;
    return;
}

void displayDownloadsList() {
	if(loggedInUuid == "") {
		cout << "Failed to show downloads. Please login to run this command" << endl;
		return;
	}

	for(int i = 0; i < downloadsListLeecher.size(); i++) {
		if(downloadsListLeecher[i].getUuid() == loggedInUuid) {
			if(downloadsListLeecher[i].getStatus() == 0) {
				cout << "[D] ";
			} else if (downloadsListLeecher[i].getStatus() == 1) {
				cout << "[C] ";
			} else if (downloadsListLeecher[i].getStatus() == 2) {
				cout << "[F] ";
			}

			cout << "[" << downloadsListLeecher[i].getGroupId() << "] ";
			cout << downloadsListLeecher[i].getFilename() << endl;
		}
	}
}

void stopAllShares() {
	if(loggedInUuid == "") {
		cout << "Failed to stop downloads. Please login to run this command" << endl;
		return;
	}
	for (auto itr = shareListSeeder.begin(); itr != shareListSeeder.end(); itr++) {
		Shares obj = itr->second;

		if(obj.getSeederUuid() == loggedInUuid) {
			auto mutexIter = sharesMutex.find(itr->first);
			pair<sem_t, bool> seederMutex = mutexIter->second;
			sem_wait(&seederMutex.first);
			seederMutex.second = true;
			sem_post(&seederMutex.first);
			mutexIter->second = seederMutex;
		}
	}
}

void stopShare(string groupId, string filename) {
	sem_wait(&shareListSeederMutex);
	for (auto itr = shareListSeeder.begin(); itr != shareListSeeder.end(); itr++) {
		cout << "looking for share entity to stop" << endl;
		Shares obj = itr->second;

		if(obj.getFilename() == filename && obj.getGroupId() == groupId &&
			obj.getSeederUuid() == loggedInUuid) {

			cout << "got share entity" << endl;
			auto mutexIter = sharesMutex.find(itr->first);

			if(mutexIter == sharesMutex.end()) {
				break;
			}

			cout << "got share entity mutex" << endl;
			pair<sem_t, bool> seederMutex = mutexIter->second;
			sem_wait(&seederMutex.first);
			cout << "inside seeder mutex" << endl;
			seederMutex.second = true;
			sem_post(&seederMutex.first);
			mutexIter->second = seederMutex;
		}
	}

	sem_post(&shareListSeederMutex);
}

void sendRequestToTracker(vector<string> command, 
						vector<pair<string, int>> trackerAddress,
						int sock, struct sockaddr_in serv_addr) {

	//cout << "sending user request to tracker" << endl;
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
	//cout << "server returned " << responseStub << endl;


	string serverMessage = string(responseStub);
 	vector<string> tokens = processString(serverMessage, '$');

 	if(command[0] == "login") {
 		loggedInUuid = tokens[1];
 		cout << "loggedin user token is: " << loggedInUuid << endl;
 	} else if (command[0] == "download_file") {
 		if(tokens.size() % 3 != 0) {
 			cout << "error in response received from server" << endl;
 		}

 		string uuid = tokens[0];
 		string ipAddress = tokens[1];
 		string port = tokens[2];

 		thread sendInitializeDownloadThread(&initializeDownload, uuid, 
 											ipAddress, port, command[1], command[2], command[3]);
 		sendInitializeDownloadThread.detach();
 	} else if (command[0] == "create_group" && tokens[0] == "true") {
 		cout << "successfully created group" << endl;
 	} else if (command[0] == "upload_file" && tokens[0] == "true") {
 		cout << "successfully uploaded file" << endl;
 	} else if (command[0] == "list_files") {
 		if (tokens.size() > 0) {
 			for(int i = 0; i < tokens.size(); i++) {
 				cout << tokens[i] << endl;
 			}
 		}
 	} else if (command[1] == "list_requests") {
 		if (tokens.size() > 0) {
 			for(int i = 0; i < tokens.size(); i++) {
 				cout << tokens[i] << endl;
 			}
 		}
 	} else if (command[0] == "join_group") {
 		if (tokens[0] == "true") {
 			cout << tokens[1] << endl;
 		} else {
 			cout << "failed to sent request to owner" << endl;
 		}
 	}

 	close(sock);
 	return;
}

int main(int argc, char *argv[]) {
	if(argc != 3) {
		cout << "Invalid Arguments. Expected Arguments 3." << endl;
		return 0;
	}

	sem_init(&shareListSeederMutex, 0, 1);

	clientIpAddress = processAddressString(string(argv[1]));
	vector<pair<string, int>> trackerAddress = readTrackerConfigFile(string(argv[2]));

	thread initSeederThread(&seederService, clientIpAddress);
	initSeederThread.detach();

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
			stopAllShares();
			loggedInUuid = "";
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
			createMTorrentFile(command[1], "uploads/");

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
		} else if (command[0] == "show_downloads") {
			displayDownloadsList();
		} else if(command[0] == "stop_share") {
			command.push_back(loggedInUuid);
			// thread sendStopShareRequestThread(&sendRequestToTracker, command,
			// 									trackerAddress,
			// 									sock2, serv_addr2);
			// sendStopShareRequestThread.join();

			stopShare(command[1], command[2]);
		}
	}
}