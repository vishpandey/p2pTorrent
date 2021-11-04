#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <bits/stdc++.h>
#include <thread>
#include <chrono>
#include <sstream>
#include <random>
#include <string>
#include <semaphore.h>

using namespace std;

unordered_map<string, int> peerStatus;
unordered_map<string, pair<string, string>> usersMappingWithCredentials;
unordered_map<string, pair<string, string>> usersMappingWithIpAddress;
set <string> currentlyLoggedInUsers;
vector<pair<string, int>> secondaryTrackers;
bool isSyncUpPending = false;
string syncUpCategory = "";
string syncUpData = "";

sem_t syncUpMutex;

class GroupDetails {
	string owner;
	string groupId;
	set<string> members;
	map<string, set<string>> shareableFiles; 

public:
	GroupDetails(string owner, string groupId) {
		this->owner = owner;
		this->groupId = groupId;
	}

	string getOwner() {
		return this->owner;
	}

	string getGroupId() {
		return this->groupId;
	}

	void setOwner(string owner) {
		this->owner = owner;
	}

	void setGroupId(string grouId) {
		this->groupId = groupId;
	}

	void setAllMembers(set<string> members) {
		this->members = members;
	}

	void addMember(string uuid) {
		this->members.insert(uuid);
	}

	bool isMember(string uuid) {
		if(this->members.find(uuid) != this->members.end()) {
			return true;
		}

		return false;
	}

	set<string> getAllMembers() {
		return this->members;
	}

	vector<string> getShareableFiles() {
		vector<string> fileList;
		for(auto itr = this->shareableFiles.begin(); itr != this->shareableFiles.end(); itr++) {
			cout << itr->first << endl;
			
			fileList.push_back(itr->first);
		}

		return fileList;
	}

	void addShareableFiles(string filename, string uuid) {
		auto fileIterator = this->shareableFiles.find(filename);
		set<string> peersList;
		if(fileIterator == this->shareableFiles.end()) {
			peersList.insert(uuid);
			this->shareableFiles.insert(pair<string, set<string>>(filename, peersList));
		} else  {
			peersList = fileIterator->second;
			peersList.insert(uuid);
			fileIterator->second = peersList;
		}
	}
};

map<string, GroupDetails> groupList;
map<string, string> ownerToGroupMapping;
map<string, set<string>> pendingJoinGroupRequests;

vector<string> preprocess(string input) {
	vector<string> tokens;
	string token = "";
	for(int i = 0; i < input.length(); i++) {
		if(input.at(i) == '$') {
			tokens.push_back(token);
			token = "";
			continue;
		}

		token = token + input.at(i);
	}

	if(token.length() > 0) {
		tokens.push_back(token);
	}

	return tokens;
}


void peerService() {
	cout << "peer service initiating" << endl;
	struct sockaddr_in address; 
    int sock = 0;
    struct sockaddr_in serv_addr;
    char *ping = "ping";
    int healthCheck, status;
    string key;

	while(1) {
		for(int i = 0; i < secondaryTrackers.size(); i++) {
			if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) { 
        		perror("Socket creation error"); 
        		continue;
    		} 

    		memset(&serv_addr, '0', sizeof(serv_addr)); 

    		serv_addr.sin_family = AF_INET;  
    		serv_addr.sin_port = htons(secondaryTrackers[i].second);
    		if(inet_pton(AF_INET, secondaryTrackers[i].first.c_str(), &serv_addr.sin_addr)<=0) {
		        printf("\nInvalid address/ Address not supported \n");
		        continue;
		    }


    		if(connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr))<0) {
		        printf("Connection to tracker failed\n");
		        key = secondaryTrackers[i].first + to_string(secondaryTrackers[i].second);
		        peerStatus[key] = 0;
		        continue;
		    }
		    
		    cout << "sent ping to tracker" << endl;
		    send(sock , ping, strlen(ping) , 0);

		    char payload[1024] = {0};
		    status = recv( sock , payload, 1024, 0);
    		if(status < 0) {
    			perror("Tracker down");
    			key = secondaryTrackers[i].first + to_string(secondaryTrackers[i].second);
    			peerStatus[key] = 0;
    		}
 			
 			string serverMessage = string(payload);
 			vector<string> tokens = preprocess(serverMessage);

 			if(tokens[0] == "pong") {
 				cout << "Tracker active" << endl;
		    	key = tokens[1] + ":" + tokens[2];
		    	peerStatus[key] = 1;
 			}

 			if(isSyncUpPending) {
 				cout << "initiating syncup" << endl;
 				sem_wait(&syncUpMutex);
 				if(syncUpCategory == "new_user") {
 					cout << "syncing up new users" << endl;
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				} else if (syncUpCategory == "new_login") {
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				} else if (syncUpCategory == "new_group") {
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				} else if (syncUpCategory == "new_join_group") {
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				} else if (syncUpCategory == "new_logout") {
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				} else if (syncUpCategory == "new_accept_request") {
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				} else if (syncUpCategory == "new_leave_request") {
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				} else if (syncUpCategory == "new_upload") {
 					payload[1024] = {0};
 					strcpy(payload, syncUpData.c_str());
 					send(sock , payload, strlen(payload) , 0);
 				}
 				isSyncUpPending = false;
 				syncUpCategory = "";
 				syncUpData = "";
 				sem_post(&syncUpMutex);
 			}

 			close(sock);
		}
		
		sleep(5);

	}
}

void sync_up(string category, string data) {
	cout << "syncup flags set" << endl;
	isSyncUpPending = true;
	sem_wait(&syncUpMutex);
	cout << "taking mutex" << endl;
	syncUpCategory = category;
	syncUpData = syncUpCategory + "$" + data;
	sem_post(&syncUpMutex);
	cout << "releasing mutex" << endl;
}

unsigned int random_char() {
    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<> dis(0, 255);
    return dis(gen);
}

string generate_hex(const unsigned int len) {
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

string createNewUser(vector<string> tokens) {
	bool alreadyExists = false;
	string uuid = "";
	for (auto user = usersMappingWithCredentials.begin(); user != usersMappingWithCredentials.end(); user++) {
		pair<string, string> alreadyCreatedUsers = user->second;
		if(tokens[1] == alreadyCreatedUsers.first) {
			alreadyExists = true;
		}
	}

	if(alreadyExists) {
		return "";
	}

	uuid = generate_hex(5);
	usersMappingWithCredentials[uuid] = make_pair(tokens[1], tokens[2]);

	return uuid;
}

string loginUser(vector<string> tokens) {
	string uuid = "";
	string ipAddress = tokens[3];
	string port = tokens[4];
	for (auto user = usersMappingWithCredentials.begin(); user != usersMappingWithCredentials.end(); user++) {
		pair<string, string> alreadyCreatedUsers = user->second;

		if(alreadyCreatedUsers.first == tokens[1] && alreadyCreatedUsers.second == tokens[2]) {
			uuid = user->first;
			currentlyLoggedInUsers.insert(uuid);
			usersMappingWithIpAddress[uuid] = make_pair(ipAddress, port);
			break;
		}
	}

	if(uuid == "") {
		return "false$" + uuid; 
	}

	return "true$" + uuid;
}

bool authenticateUser(string uuid) {
	if(currentlyLoggedInUsers.find(uuid) == currentlyLoggedInUsers.end()) {
		return false;
	}

	return true;
}

string createGroup(string groupId, string uuid) {
	if(!authenticateUser(uuid)) {
		cout << "error while creating group. User not loggedIn" << endl;
		string error = "false";
		return error;
	}

	auto owner = ownerToGroupMapping.find(groupId);
	if(owner != ownerToGroupMapping.end()) {
		return "false";
	}

	ownerToGroupMapping[groupId] = uuid;
	groupList.emplace(groupId, GroupDetails(uuid, groupId));

	return "true";
}

string generateGroupsList() {
	string groupIdPayload = "";
	for(auto const& group: groupList) {
		if(groupIdPayload.length() == 0) {
			groupIdPayload += group.first;
			continue;
		}

		groupIdPayload += "$" + group.first;
	}

	return groupIdPayload;
}

string joinGroupRequest(string groupId, string uuid) {
	cout << "group id : " << groupId << " uuid: " << uuid << endl;
	auto groupDetail = groupList.find(groupId);  
	if(groupDetail != groupList.end()) {
		GroupDetails temp = groupDetail->second;
		if(uuid == temp.getOwner()) {
			return "true$Owner cannot send join request";
		}

		if(temp.isMember(uuid)) {
			return "true$Already a member";
		}
		auto pendingRequest = pendingJoinGroupRequests.find(groupId);

		if(pendingRequest == pendingJoinGroupRequests.end()) {
			set<string> tempList;
			tempList.insert(uuid);
			pendingJoinGroupRequests[groupId] = tempList;

			return "true$Sent request to join the group to owner.";
		}

		
		cout << "pending request for group found" << endl;
		set<string> uuidList = pendingRequest->second;
		if(uuidList.find(uuid) == uuidList.end()) {
			cout << "pending request for uuid not found" << endl;
			uuidList.insert(uuid);

			pendingRequest->second = uuidList;

			return "true$Sent request to join the group to owner.";
		}

		return "true$Request to join group already sent";
	}

	return "false";
}

string listPendingRequest(string groupId, string uuid) {
	auto groupOwner = ownerToGroupMapping.find(groupId);

	// cout << "current uuid is : " << uuid << endl;
	// cout << "owner of the group is " << groupOwner->second << endl;

	if(groupOwner == ownerToGroupMapping.end()) {
		return "error: group doesnt exist";
	}

	if(groupOwner->second != uuid) {
		return "error: only owner can see request list";
	}
	
	auto pendingRequests = pendingJoinGroupRequests.find(groupId);
	
	set<string> requests = pendingRequests->second;
	
	set<string>::iterator itr;

	string response = "";
	
	for (itr = requests.begin(); itr != requests.end(); itr++) {
        if(response == "") {
        	response = *itr;
        	continue;
        }

        response += "$" + *itr;
    }

    cout << "pending requests are " << response << endl;
    return response;
}

string acceptRequest(string groupId, string uuid, string clientUuid) {
	auto groupOwner = ownerToGroupMapping.find(groupId);
	bool entryFound = false;
	// cout << "current uuid is : " << uuid << endl;
	// cout << "owner of the group is " << groupOwner->second << endl;

	if(groupOwner == ownerToGroupMapping.end()) {
		return "error: group doesnt exist";
	}

	if(groupOwner->second != clientUuid) {
		return "error: only owner can accept request";
	}
	
	auto pendingRequests = pendingJoinGroupRequests.find(groupId);
	
	set<string> requests = pendingRequests->second;
	
	set<string>::iterator itr;

	string response = "";

	for (itr = requests.begin(); itr != requests.end(); itr++) {
        if(*itr == uuid) {
        	auto group = groupList.find(groupId);

        	GroupDetails temp = group->second;

        	entryFound = true;
        	temp.addMember(uuid);

        	group->second = temp;

        	break;
        }
    }

    if(entryFound) {
	    requests.erase(uuid);
		pendingRequests->second = requests;
		return "true";
    }

	return "false";
}

string leaveGroup(string groupId, string uuid) {
	auto groupOwner = ownerToGroupMapping.find(groupId);
	bool entryFound = false;

	if(groupOwner == ownerToGroupMapping.end()) {
		return "error: group doesnt exist";
	}

	if(groupOwner->second == uuid) {
		return "error: owner cannot leave the group";
	}

	auto group = groupList.find(groupId);

	GroupDetails temp = group->second;
	set<string> allMembers = temp.getAllMembers();

	allMembers.erase(uuid);

	temp.setAllMembers(allMembers);
	group->second = temp;

	return "true";
}

string uploadFile(string filePath, string groupId, string uuid) {
	auto group = groupList.find(groupId);
	if(group == groupList.end()) {
		return "error: group doesnt exist";
	}

	GroupDetails groupDetail = group->second;

	if(groupDetail.getOwner() != uuid && !groupDetail.isMember(uuid)) {
		return "error: user not a part of the group";
	}

	auto clientAddress = usersMappingWithIpAddress.find(uuid);
	if(clientAddress == usersMappingWithIpAddress.end()) {
		return "error: client ip address info not available";
	}

	groupDetail.addShareableFiles(filePath, uuid);
	group->second = groupDetail;

	return "true";
}

string listFiles(string groupId, string uuid) {
	auto group = groupList.find(groupId);
	if(group == groupList.end()) {
		return "error: group doesnt exist";
	}
	GroupDetails groupDetail = group->second;

	if(groupDetail.getOwner() != uuid && !groupDetail.isMember(uuid)) {
		return "error: user not a part of the group";
	}

	vector<string> fileList = groupDetail.getShareableFiles();

	string fileListString = "";
	for(int i = 0; i < fileList.size(); i++) {
		if(fileListString == "") {
			fileListString = fileList[i];
			continue;
		}

		fileListString += "$" + fileList[i];
	}
	

	return fileListString;
}

char* createResponse(string response) {
	char *responseStub = new char[response.length() + 1];
	strcpy(responseStub, response.c_str());

	return responseStub;
}


void trackerService(string trackerIp, int trackerPort) {
	cout << "tracker service initiating" << endl;
	int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1; 
    int addrlen = sizeof(address);
	int readStatus;
	string pong = "pong";
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
    address.sin_addr.s_addr = inet_addr(trackerIp.c_str()); 
    address.sin_port = htons(trackerPort);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) { 
        perror("bind failed"); 
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) { 
        perror("listen"); 
        exit(EXIT_FAILURE); 
    }

    cout << "Tracker service initiated" << endl;

    while(true) {

	    if ((new_socket = accept(server_fd, (struct sockaddr *)&address,  
	                       (socklen_t*)&addrlen)) < 0) {
	        cout << server_fd << endl; 
	        perror("accept"); 
	        exit(EXIT_FAILURE);
	    }


    	char buffer[1024] = {0};
	    readStatus = read(new_socket , buffer, 1024);

	    string clientMessage = string(buffer);
 		vector<string> tokens = preprocess(clientMessage);

 		//cout << "received message: " << buffer << endl;
 		cout << "received command: " << tokens[0] << endl;
 		
 		if(tokens[0] == "ping") {
 			string pingResponse = pong + "$" + trackerIp + "$" + to_string(trackerPort);
 			
 			payload = createResponse(pingResponse);
 			
 			send(new_socket, payload, strlen(payload) , 0);

 			//cout << "sent reply : " << pingResponse << endl;
 		} else if(tokens[0] == "create_user") {
 			//cout << "received create user request" << endl;

 			string serverResponse = "false";
 			string uuid = createNewUser(tokens);
 			if(uuid != "") {
 				serverResponse = "true";
 			}
 			
 			payload = createResponse(serverResponse);

 			send(new_socket, payload, strlen(payload), 0);

 			if(serverResponse != "false") {
 				sync_up("new_user",uuid + "$" + tokens[1] + "$" + tokens[2]);	
 			}

 		} else if (tokens[0] == "new_user") {
 			
 			usersMappingWithCredentials[tokens[1]] = make_pair(tokens[2], tokens[3]);
 		
 		} else if (tokens[0] == "login") {
 			
 			string uuid = loginUser(tokens);
 			
 			payload = createResponse(uuid);

 			send(new_socket, payload, strlen(payload), 0);

 			//cout << "sent login response to client" << endl;

 			if(uuid != "") {
 				sync_up("new_login", uuid);
 			}

 		} else if (tokens[0] == "new_login") {
 			
 			currentlyLoggedInUsers.insert(tokens[1]);
 		
 		} else if (tokens[0] == "create_group") {
 			
 			string createGroupResponse = createGroup(tokens[1], tokens[2]);

 			payload = createResponse(createGroupResponse);

 			send(new_socket, payload, strlen(payload), 0);

 			if(createGroupResponse != "false") {
 				sync_up("new_group", tokens[1] + "$" + tokens[2]);
 			}

 		} else if (tokens[0] == "new_group") {
 		
 			string grpId = tokens[1];
 			string grpOwner = tokens[2];
 			string createGroupResponse = createGroup(grpId, grpOwner);
 			//groupList.emplace(grpId, GroupDetails(grpOwner, grpId)); 			
 		
 		} else if (tokens[0] == "list_groups") {
 			
 			if(!authenticateUser(tokens[1])) {
 				string errorMessage = "Failed, user not authenticated";
 				payload = createResponse(errorMessage);
 				send(new_socket, payload, strlen(payload), 0);
 				continue;
 			}

 			string groupIdPayload = generateGroupsList();
 			
 			payload = createResponse(groupIdPayload);

 			send(new_socket, payload, strlen(payload), 0);

 		} else if (tokens[0] == "join_group") {

 			if(!authenticateUser(tokens[2])) {
 				string errorMessage = "Failed, user not authenticated";
 				payload = createResponse(errorMessage);
 				send(new_socket, payload, strlen(payload), 0);
 				continue;
 			}

 			string joinGroupResponse = joinGroupRequest(tokens[1], tokens[2]);

 			payload = createResponse(joinGroupResponse);

 			send(new_socket, payload, strlen(payload), 0);

 			if(joinGroupResponse != "false") {
 				sync_up("new_join_group", tokens[1] + "$" + tokens[2]);
 			}

 		} else if (tokens[0] == "new_join_group") {
 			
 			joinGroupRequest(tokens[1], tokens[2]);
 		
 		} else if (tokens[0] == "logout") {
 			
 			auto userId = currentlyLoggedInUsers.find(tokens[1]);
 			if(userId == currentlyLoggedInUsers.end()) {
 				string logoutResponse = "already logged in";
	 			payload = createResponse(logoutResponse);

	 			send(new_socket, payload, strlen(payload), 0);

	 			continue;
 			}

 			currentlyLoggedInUsers.erase(userId, currentlyLoggedInUsers.end());
 			usersMappingWithIpAddress.erase(tokens[1]);

 			string logoutResponse = "true";
 			payload = createResponse(logoutResponse);

 			send(new_socket, payload, strlen(payload), 0);

 			sync_up("new_logout", tokens[1]);	
 		
 		} else if (tokens[0] == "new_logout") {
 			
 			auto userId = currentlyLoggedInUsers.find(tokens[1]);
 			currentlyLoggedInUsers.erase(userId, currentlyLoggedInUsers.end());

 		} else if (tokens[0] == "requests" && tokens[1] == "list_requests") {
 			if(!authenticateUser(tokens[3])) {
 				string errorMessage = "Failed, user not authenticated";
 				payload = createResponse(errorMessage);
 				send(new_socket, payload, strlen(payload), 0);
 				continue;
 			}

 			string listPendingRequestsResponse = listPendingRequest(tokens[2], tokens[3]);
 			payload = createResponse(listPendingRequestsResponse);

 			send(new_socket, payload, strlen(payload), 0);
 		} else if (tokens[0] == "accept_request") {
 			if(!authenticateUser(tokens[3])) {
 				string errorMessage = "Failed, user not authenticated";
 				payload = createResponse(errorMessage);
 				send(new_socket, payload, strlen(payload), 0);
 				continue;
 			}

 			string acceptRequestResponse = acceptRequest(tokens[1], tokens[2], tokens[3]);

 			payload = createResponse(acceptRequestResponse);

 			send(new_socket, payload, strlen(payload), 0);

 			if(acceptRequestResponse != "false") {
 				sync_up("new_accept_request", tokens[1] + "$" + tokens[2] + "$" + tokens[3]);
 			}
 		} else if (tokens[0] == "new_accept_request") {
 			
 			string response = acceptRequest(tokens[1], tokens[2], tokens[3]);
 		
 		} else if (tokens[0] == "leave_group") {
 			
 			if(!authenticateUser(tokens[2])) {
 				string errorMessage = "Failed, user not authenticated";
 				payload = createResponse(errorMessage);
 				send(new_socket, payload, strlen(payload), 0);
 				continue;
 			}

 			string leaveGroupResponse = leaveGroup(tokens[1], tokens[2]);

 			payload = createResponse(leaveGroupResponse);

 			send(new_socket, payload, strlen(payload), 0);

 			if(leaveGroupResponse == "true") {
 				sync_up("new_leave_request", tokens[1] + "$" + tokens[2]);
 			}
 		} else if (tokens[0] == "new_leave_request") {

 			string leaveGroupResponse = leaveGroup(tokens[1], tokens[2]);
 		
 		} else if (tokens[0] == "upload_file") {
 			if(!authenticateUser(tokens[3])) {
 				string errorMessage = "Failed, user not authenticated";
 				payload = createResponse(errorMessage);
 				send(new_socket, payload, strlen(payload), 0);
 				continue;
 			}

 			string filePath = tokens[1];
 			string groupId = tokens[2];
 			string uuid = tokens[3];

 			string uploadFileResponse = uploadFile(filePath, groupId, uuid);

 			payload = createResponse(uploadFileResponse);

 			send(new_socket, payload, strlen(payload), 0);

 			if(uploadFileResponse == "true") {
 				sync_up("new_upload", tokens[1] + "$" + tokens[2] + "$" + tokens[3]);
 			}
 		} else if (tokens[0] == "new_upload") {
 			string filePath = tokens[1];
 			string groupId = tokens[2];
 			string uuid = tokens[3];

 			uploadFile(filePath, groupId, uuid);
 		} else if (tokens[0] == "list_files") {
 			if(!authenticateUser(tokens[2])) {
 				string errorMessage = "Failed, user not authenticated";
 				payload = createResponse(errorMessage);
 				send(new_socket, payload, strlen(payload), 0);
 				continue;
 			}

 			string listFilesResponse = listFiles(tokens[1], tokens[2]);

 			payload = createResponse(listFilesResponse);

 			send(new_socket, payload, strlen(payload), 0);
 		}
    }
}


int main(int argc, char *argv[]) {
	if(argc != 3) {
		cout << "Invalid Arguments. Expected Arguments 3." << endl;
	}

	string secondaryTrackerIp;
	int secondaryTrackerPort;
	string trackerIp;
	int trackerPort;

	string trackerConfigFile = argv[1];
	int trackerNo = stoi(argv[2]);

	sem_init(&syncUpMutex, 0, 1);

	ifstream file(trackerConfigFile);
	int lineNo  = 0;
	if(file.is_open()) {
		string line;
		while(getline(file, line)) {
			lineNo++;
			string tokens = "";

			if(lineNo == trackerNo) {
				for(auto chars:line) {
		            if(chars == ' ') {
		            	trackerIp = tokens;
		                tokens.erase();
		            } else {
		            	tokens = tokens + chars;
		            }
		        }

		        trackerPort = stoi(tokens);
			} else {
				for(auto chars:line) {
		            if(chars == ' ') {
		            	secondaryTrackerIp = tokens;
		                tokens.erase();
		            } else {
		            	tokens = tokens + chars;
		            }
		        }

		        secondaryTrackerPort = stoi(tokens);
		        secondaryTrackers.push_back(make_pair(secondaryTrackerIp, secondaryTrackerPort));
			}
		}
	}

	file.close();

	cout << "primary tracker details" << endl;
	cout << trackerIp << endl;
	cout << trackerPort << endl;

	cout << "secondary tracker details" << endl;
	for(int i = 0; i < secondaryTrackers.size(); i++) {
		cout << secondaryTrackers[i].first << endl;
		cout << secondaryTrackers[i].second << endl;
	}

	thread peerConnectionThread(peerService);
	thread clientConnectionThread(trackerService, trackerIp, trackerPort);

	peerConnectionThread.join();
	clientConnectionThread.join();

	return 0;
}