#include "stdafx.h"
#include "AuthServer.h"
#include "DataManager.h"
#include "Cryptography.h"
#include "CurlWebRequest.h"

SOCKET AuthSocket;
#if WIN32
WSADATA AuthWsaData;
#endif
bool AuthServerReady = false;
struct sockaddr_in AuthServerSockAddrIn;

unsigned char Hypervisor[0x40000];
unsigned char ChallengeResponse[0x200];
int TitanXexSize;
unsigned char *TitanXex;

char* ServerPath = "167.114.64.26:8089";

//xblatlas
//char* Path = "xblatlas.php";
char* Path = "titan3.php";

#if WIN32
DWORD WINAPI OnClientConnect(LPVOID lpParam);
#else
void OnClientConnect ( void *ptr );
#endif


DWORD COD_BLACK_OPS_2	= 0x415608C3;
DWORD COD_GHOSTS		= 0x415608fc;
DWORD xOsc				= 0xFFFFFFD1;
DWORD COD_AW 			= 0x41560914;


#if WIN32
	char* XBLH		= "Data\\XBLHammer.xex";
	char* chall		= "Data\\chall_resp.bin";
	char* HV		= "Data\\HV.bin";
	char* x0sc		= "Data\\Hacks\\x0sc.xex";

	//Game Hacks
	char* GHOSTS	= "Data\\Hacks\\GHOSTS.xex";
	char* BLACKOPS2	= "Data\\Hacks\\BLACKOPS2.xex";
	char* AW		= "Data\\Hacks\\AW.xex";
#else
	char* XBLH		= "Data/XBLHammer.xex";
	char* chall		= "Data/chall_resp.bin";
	char* HV		= "Data/HV.bin";
	char* x0sc		= "Data/Hacks/x0sc.xex";

	//Game Hacks
	char* GHOSTS	= "Data/Hacks/GHOSTS.xex";
	char* BLACKOPS2	= "Data/Hacks/BLACKOPS2.xex";
	char* AW		= "Data/Hacks/AW.xex";	
#endif

void DumpKeyVault(unsigned char *Data);
void DoAuth(SOCKET ClientSocket, unsigned char *CpuKey, unsigned char *ModuleHash);
void SendChallengeResponse(SOCKET ClientSocket, unsigned char *Token, unsigned char *Salt);
void UpdatePresence(SOCKET ClientSocket, unsigned char *Token, char *Gamertag, DWORD TitleId);
void SendSuccessResponse(SOCKET ClientSocket);
void SendFailedResponse(SOCKET ClientSocket);
void LoadHack(SOCKET ClientSocket, DWORD TitleId);
void DoLogger(unsigned char *CpuKey, unsigned char *MSG);
void SendMSG(SOCKET ClientSocket, unsigned char *Token);

void printBytes(PBYTE bytes, DWORD len){

	for(int i=0; i<(int)len; i++){
		if(i%16==0 && i!=0) printf("\n");
		printf("%02X", bytes[i]);
	}
	printf("\n");
}

bool LoadXexData() {
	FILE *fXex;
	
	fXex = fopen(XBLH, "rb");
	if (!fXex) {
		printf("Failed to open titan xex.\n");
		fclose(fXex);
		return false;
	}

	// Seek to the end of the files
	fseek(fXex , 0, SEEK_END);

	// Set size and allocate xex data
	TitanXexSize = ftell(fXex);
	TitanXex = (unsigned char*)malloc(TitanXexSize);

	// Rewind our pointers
	rewind(fXex);

	// Read our data
	fread(TitanXex         , 1, TitanXexSize, fXex);

	// Close our files
	fclose(fXex);

	return true;
}

bool LoadServerData() {
	FILE *fHv;
	FILE *fResp;
	FILE *fXex;

	// Open our files
	fHv = fopen(HV, "rb");
	if (!fHv) {
		printf("Failed to open hypervisor.\n");
		return false;
	}
	
	fResp = fopen(chall, "rb");
	if (!fResp) {
		printf("Failed to open challenge response.\n");
		fclose(fHv);
		return false;
	}

	// Seek to the end of the files
	fseek(fHv  , 0, SEEK_END);
	fseek(fResp, 0, SEEK_END);

	// Check sizes
	if (ftell(fHv) != 0x40000) {
		printf("Hypervisor size is wrong.\n");
		fclose(fHv);
		fclose(fResp);
		return false;
	}

	if (ftell(fResp) != 0x200) {
		printf("Challenge response size is wrong.\n");
		fclose(fHv);
		fclose(fResp);
		return false;
	}

	// Rewind our pointers
	rewind(fHv);
	rewind(fResp);

	// Read our data
	fread(Hypervisor       , 1, 0x40000     , fHv);
	fread(ChallengeResponse, 1, 0x200       , fResp);

	// Close our files
	fclose(fHv);
	fclose(fResp);

	return true;
}

#ifndef WIN32
void error(const char *msg)
{
    perror(msg);
    exit(1);
}
#endif

bool AuthServerStartup(short Port) {
	int Result;

	if (!LoadServerData()) {
		printf("Failed to load server data.\n");
		return false;
	}

#if WIN32
	Result = WSAStartup(MAKEWORD(2, 2), &AuthWsaData);

	if (Result != 0) {
		printf(" %08X\n", WSAGetLastError());
		return false;
	}
#endif

	AuthSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (AuthSocket == INVALID_SOCKET) {
#if WIN32
		printf("Invalid Socket %08X\n", WSAGetLastError());
#endif
		return false;
	}
	
#if WIN32
	AuthServerSockAddrIn.sin_addr.S_un.S_addr = INADDR_ANY;
	AuthServerSockAddrIn.sin_family = AF_INET;
	AuthServerSockAddrIn.sin_port = htons(Port);
#else
	AuthServerSockAddrIn.sin_family = AF_INET;
	AuthServerSockAddrIn.sin_addr.s_addr = INADDR_ANY;
	AuthServerSockAddrIn.sin_port = htons(Port);
	
#endif

	 Result = bind(AuthSocket, (struct sockaddr*)&AuthServerSockAddrIn, sizeof(AuthServerSockAddrIn));

	if (Result == INVALID_SOCKET) {
#if WIN32
		printf("Failed to bind %08X\n", WSAGetLastError());
#endif
		return false;
	}

	AuthServerReady = true;

	printf("Auth Server is ready.\n");

	return true;
}

void AuthServerListen() {
	if (!AuthServerReady) {
		printf("Server is not ready to listen\n");
		return;
	}

	listen(AuthSocket, SOMAXCONN);

	while (true) {
		struct sockaddr_in ClientAddr;
		int ClientLength = sizeof(ClientAddr);
		SOCKET ClientSocket = accept(AuthSocket, (struct sockaddr*)&ClientAddr, &ClientLength);

		if (ClientSocket == INVALID_SOCKET)
		{
			printf("Accept client failed!\n");
			continue;
		}

#if WIN32
		printf("Client connected %d.%d.%d.%d\n", ClientAddr.sin_addr.S_un.S_un_b.s_b1, ClientAddr.sin_addr.S_un.S_un_b.s_b2, ClientAddr.sin_addr.S_un.S_un_b.s_b3, ClientAddr.sin_addr.S_un.S_un_b.s_b4);
		HANDLE handle = CreateThread(NULL, NULL, OnClientConnect, (LPVOID)ClientSocket, NULL, NULL);
		CloseHandle(handle);
#else
		pthread_t thread1;
		pthread_create (&thread1, NULL, (void *) &OnClientConnect, (void *)ClientSocket);
		pthread_join(thread1, NULL);
#endif

	}
}

#if WIN32
DWORD WINAPI OnClientConnect(LPVOID lpParam) {
#else
void OnClientConnect ( void *lpParam ) {
#endif

	unsigned char CpuKey[16];
	unsigned char ModuleHash[20];
	unsigned char Token[16];
	unsigned char Salt[16];
	unsigned char MSG[512];
	SOCKET ClientSocket = (SOCKET)lpParam;
	unsigned char *RecievedData;
	int RecievedDataSize;
	int Id = 0;
	char *Gamertag;
	DWORD TitleId;

	printf("OnClientConnect Started.\n");
	bool Continue = true;
	while (Continue) {
		RecievedData = (unsigned char*)RecieveData(ClientSocket, &RecievedDataSize, &Id);

		if (!RecievedData) {
			printf("Failed to recieve data.\n");
			break;
		}

		switch (Id) {
		case AUTH_NULL:
			printf("AUTH_NULL\n");
			break;
		case AUTH_LOGIN:
			printf("AUTH_LOGIN\n");

			// Copy our auth data
			memcpy(CpuKey    , RecievedData     , 16);
			memcpy(ModuleHash, RecievedData + 16, 20);

			DoAuth(ClientSocket, CpuKey, ModuleHash);

			Continue = false;
			break;
		case AUTH_CHALLENGE:
			printf("AUTH_CHALLENGE\n");

			memcpy(Token , RecievedData     , 16);
			memcpy(Salt  , RecievedData + 16, 16);

			SendChallengeResponse(ClientSocket, Token, Salt);

			Continue = false;
			break;
		case AUTH_UPDATEPRESENCE:
			printf("AUTH_UPDATEPRESENCE\n");

			memcpy(Token, RecievedData, 16);
			Gamertag = (char*)(RecievedData + 16);
			memcpy(&TitleId		, RecievedData + 32, 4);

			UpdatePresence(ClientSocket, Token, Gamertag, TitleId);

			Continue = false;
			break;
		case 7:
			printf("AUTH_GAMEHACK\n");

			memcpy(CpuKey		, RecievedData     , 16);
			memcpy(&TitleId		, RecievedData + 16, 4);
			TitleId = htonl(TitleId);
			
			LoadHack(ClientSocket, TitleId);

			Continue = false;
			break;
		case 8:
			printf("AUTH_MSG\n");
			memcpy(Token, RecievedData, 16);

			SendMSG(ClientSocket, Token);

			Continue = false;
			break;
		case 9:
			printf("AUTH_X\n");
			memcpy(Token, RecievedData, 16);

			SendMSG(ClientSocket, Token);
			Continue = false;
			break;
		default:
			// They gave us a bad packet?
			printf("Unknown packet Id %08X\n", Id);
			Continue = false;
			break;
		}
		free(RecievedData);
	}
	printf("OnClientConnect Ended.\n");
	closesocket(ClientSocket);

#if WIN32
	return 0;
#endif
}

void LoadHack(SOCKET ClientSocket, DWORD TitleId) {
	FILE *fHack;
	char xex[512];
	unsigned char Packet[1024];

	if(TitleId != NULL) {
		if(TitleId == COD_BLACK_OPS_2){
			sprintf(xex, BLACKOPS2);
			printf("loaded COD_BLACK_OPS_2 Gamehack.\n");
		}else if(TitleId == COD_GHOSTS){
			sprintf(xex, GHOSTS);
			printf("loaded COD_GHOSTS Gamehack.\n");
		}else if(TitleId == COD_AW){
			sprintf(xex, AW);
			printf("loaded COD_AW Gamehack.\n");
		}else if(TitleId == xOsc){
			sprintf(xex, x0sc);
			printf("loaded xOsc.\n");
		}else{
#if WIN32
			sprintf(xex, "Data\\Hacks\\%08X.xex", TitleId);
#else
			sprintf(xex, "Data/Hacks/%08X.xex", TitleId);
#endif
		}
		// Open our files
		fHack = fopen(xex, "rb");
		if (!fHack) {
			printf("Failed to open Gamehack.\n");
			SendFailedResponse(ClientSocket);
			return;
		}

		// Seek to the end of the files
		fseek(fHack  , 0, SEEK_END);

		// Set size and allocate xex data
		DWORD XexSize = ftell(fHack);
		unsigned char* Xex = (unsigned char*)malloc(XexSize);

		// Rewind our pointers
		rewind(fHack);

		// Read our data
		fread(Xex         , 1, XexSize, fHack);

		SendData(ClientSocket, Xex, XexSize, PACKET_UPDATE);
		printf("SendData %s..\n", xex);
		// Close our files
		memset(Xex, 0, XexSize);
		fclose(fHack);
	}
}

void DoAuth(SOCKET ClientSocket, unsigned char *CpuKey, unsigned char *ModuleHash) {
	unsigned char Packet[1024];
	char Format[512];
	int Token[4];
	int *CpuKeyCast = (int*)CpuKey; // This will be four ints
	char *DelimiterContext;

	// Format our request string
	sprintf(Format, "http://%s/%s?x=Login&cpukey=%08X%08X%08X%08X", 
		ServerPath,
		Path,
		htonl(CpuKeyCast[0]), 
		htonl(CpuKeyCast[1]), 
		htonl(CpuKeyCast[2]), 
		htonl(CpuKeyCast[3]));

	char *WebRequest = DoWebRequest(Format);
	// We failed our web request (Server not up?)
	if (!WebRequest) {
		printf("DoAuth: Failed web request.\n");
		SendFailedResponse(ClientSocket);
		return;
	}

	char *Message = strtok(WebRequest, ":");
	char *Level = strtok(NULL, ":");

	// We failed to tokenize our data
	if (!Message || !Level) {
		printf("DoAuth: Failed to tokenize web response.\n");
		SendFailedResponse(ClientSocket);
		free(WebRequest);
		return;
	}

	// Check our login string
	if (strcmp(WebRequest, "SUCCESS") != 0) {
		// We did not succeed our login
		printf("DoAuth: User failed authorization.\n");
		SendFailedResponse(ClientSocket);
		free(WebRequest);
		return;
	}

	// Convert our user level (big endian)
	int UserLevel = htonl(strtoul(Level, 0, 0));
	int UserLevelLe = strtoul(Level, 0, 0);

	//int UserLevel = 2;
	//int UserLevelLe = 2;


	// Set our token
	GetRandomBytes(Token, 16);

	// Clear our packet
	memset(Packet, 0, 1024);

	// Send our user level and token
	memcpy(Packet   , &UserLevel, 4);
	memcpy(Packet + 4, Token    , 16);

	if (UserLevelLe == AUTH_ADMIN) {
		printf("DoAuth: Skipped xex check for admin.\n");
	} else {
		if (LoadXexData()){
			if (!CheckXexChallenge(CpuKey, ModuleHash)) {
				printf("DoAuth: User failed xex check, sending update.\n");
				SendData(ClientSocket, TitanXex, TitanXexSize, PACKET_UPDATE);
				free(WebRequest);
				return;
			}
			memset(TitanXex,  0, TitanXexSize);
		}
	}

	// Send our result
	if (!SendData(ClientSocket, Packet, 1024, PACKET_SUCCESS)) {
		printf("DoAuth: Failed to send auth success packet.\n");
		free(WebRequest);
		return;
	}

	sprintf(Format, "http://%s/%s?x=UpdateToken&cpukey=%08X%08X%08X%08X&token=%08X%08X%08X%08X", 
		ServerPath,
		Path,
		htonl(CpuKeyCast[0]), 
		htonl(CpuKeyCast[1]), 
		htonl(CpuKeyCast[2]), 
		htonl(CpuKeyCast[3]), 
		htonl(Token[0]), 
		htonl(Token[1]), 
		htonl(Token[2]), 
		htonl(Token[3]));

	DoWebRequest(Format);

	free(WebRequest);
}

void SendMSG(SOCKET ClientSocket, unsigned char *Token) {
	unsigned char Packet[1028];
	char Format[256];
	int *TokenCast = (int*)Token;
	char *DelimiterContext;

	sprintf(Format, "http://%s/%s?x=MSG&token=%08X%08X%08X%08X", 
		ServerPath,
		Path,
		htonl(TokenCast[0]), 
		htonl(TokenCast[1]), 
		htonl(TokenCast[2]), 
		htonl(TokenCast[3]));

	char *WebRequest = DoWebRequest(Format);

	// We failed our web request (Server not up?)
	if (!WebRequest) {
		printf("SendChallengeResponse: Failed web request.\n");
		SendFailedResponse(ClientSocket);
		return;
	}

	char *Message = strtok_s(WebRequest, ":", &DelimiterContext);

	// Clear our packet
	memset(Packet, 0, 1028);

	// Copy our data
	memcpy(Packet     , Message   , 1028);

	// Send our result
	if (!SendData(ClientSocket, Packet, 1028, PACKET_SUCCESS)) {
		printf("SendChallengeResponse: Failed to send challenge packet.\n");
		free(WebRequest);
		return;
	}

	free(WebRequest);
}

void SendChallengeResponse(SOCKET ClientSocket, unsigned char *Token, unsigned char *Salt) {
	unsigned char Packet[1024];
	char Format[512];
	unsigned char ResponseDigest[20];
	int *TokenCast = (int*)Token;
	char *DelimiterContext;

	//sprintf(Format, "http://%s/%s?x=CheckToken&token=%08X%08X%08X%08X", 
	//	ServerPath,
	//	Path,
	//	htonl(TokenCast[0]), 
	//	htonl(TokenCast[1]), 
	//	htonl(TokenCast[2]), 
	//	htonl(TokenCast[3]));

	//char *WebRequest = DoWebRequest(Format);

	//// We failed our web request (Server not up?)
	//if (!WebRequest) {
	//	printf("SendChallengeResponse: Failed web request.\n");
	//	SendFailedResponse(ClientSocket);
	//	return;
	//}

	//char *Message = strtok(WebRequest, ":");
	//char *Level = strtok(NULL, ":");

	//// We failed to tokenize our data
	//if (!Message || !Level) {
	//	printf("SendChallengeResponse: Failed to tokenize web response.\n");
	//	SendFailedResponse(ClientSocket);
	//	free(WebRequest);
	//	return;
	//}

	//// Check our login string
	//if (strcmp(WebRequest, "SUCCESS") != 0) {
	//	// We did not succeed our login
	//	printf("SendChallengeResponse: User failed authorization.\n");
	//	SendFailedResponse(ClientSocket);
	//	free(WebRequest);
	//	return;
	//}

	DoCreateChallengeResponse(ResponseDigest, Salt);

	// Clear our packet
	memset(Packet, 0, 1024);

	// Copy our data
	memcpy(Packet     , ResponseDigest   , 20);
	memcpy(Packet + 20, ChallengeResponse, 0x200);

	// Send our result
	if (!SendData(ClientSocket, Packet, 1024, PACKET_SUCCESS)) {
		printf("SendChallengeResponse: Failed to send challenge packet.\n");
		//free(WebRequest);
		return;
	}

	printf("SendChallengeResponse: Sent challenge to client.\n");
	//free(WebRequest);
}

void UpdatePresence(SOCKET ClientSocket, unsigned char *Token, char *Gamertag, DWORD TitleId) {
	char Format[256];
	int *TitleIdCast = (int*)TitleId;
	int *TokenCast = (int*)Token;
	char *DelimiterContext;
	char *GamertagTemp = Gamertag;
	char FinalGamertag[64];

	memset(FinalGamertag, 0, 64);

	int i = 0;
	while (*GamertagTemp) {
		if (!isalnum(*GamertagTemp) && *GamertagTemp != ' ') {
			FinalGamertag[i++] = 'X';
		} else if (*GamertagTemp == ' ') {
			FinalGamertag[i++] = '%';
			FinalGamertag[i++] = '2';
			FinalGamertag[i++] = '0';
		} else {
			FinalGamertag[i++] = *GamertagTemp;
		}

		GamertagTemp++;
	}

	sprintf(Format, "http://%s/%s?x=UpdatePresence&token=%08X%08X%08X%08X&gamertag=%s&titleid=%08X", 
		ServerPath,
		Path,
		htonl(TokenCast[0]), 
		htonl(TokenCast[1]), 
		htonl(TokenCast[2]), 
		htonl(TokenCast[3]),
		FinalGamertag,
		TitleId);

	char *WebRequest = DoWebRequest(Format);

	// We failed our web request (Server not up?)
	if (!WebRequest) {
		printf("UpdatePresence: Failed web request.\n");
		SendFailedResponse(ClientSocket);
		return;
	}

	char *Message = strtok_s(WebRequest, ":", &DelimiterContext);
	char *Level = strtok_s(NULL, ":", &DelimiterContext);

	// We failed to tokenize our data
	if (!Message || !Level) {
		printf("UpdatePresence: Failed to tokenize web response.\n");
		SendFailedResponse(ClientSocket);
		free(WebRequest);
		return;
	}

	// Check our login string
	if (strcmp(WebRequest, "SUCCESS") != 0) {
		// We did not succeed our login
		printf("UpdatePresence: User failed authorization.\n");
		SendFailedResponse(ClientSocket);
		free(WebRequest);
		return;
	}

	SendSuccessResponse(ClientSocket);

	free(WebRequest);
}

void SendSuccessResponse(SOCKET ClientSocket) {
	char Packet[1024];

	// Clear our packet
	memset(Packet, 0, 1024);

	// Send it
	SendData(ClientSocket, Packet, 1024, PACKET_SUCCESS);
}

void SendFailedResponse(SOCKET ClientSocket) {
	char Packet[1024];

	// Clear our packet
	memset(Packet, 0xFF, 1024);

	// Send it
	SendData(ClientSocket, Packet, 1024, PACKET_FAILED);
}
