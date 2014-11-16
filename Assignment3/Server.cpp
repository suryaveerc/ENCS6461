#pragma comment( linker, "/defaultlib:ws2_32.lib" )
#pragma once

#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <process.h>
#include <winsock.h>
#include <iostream>
#include <windows.h>
#include <fstream>
#include <string>
#include <stdio.h>
#include <time.h>
#include <thread>
#include <sstream>
#include <iomanip>

using namespace std;
//port data types

#define REQUEST_PORT 7001
#define TIMEOUT_USEC 900000	
#define TIMEOUT_SEC 5
#define MAX_RETRIES 3000
#define MAX_PACKET_SEQ 20
#define WINDOW_SIZE 4
int port=REQUEST_PORT;
//socket data types
SOCKET serverSocket;
SOCKET cs;
SOCKADDR_IN serverSocketAddr; 
SOCKADDR_IN clientSocketAddr; 
int senderAddrSize = sizeof (clientSocketAddr);
//buffer data types

//char *buffer;
int ibufferlen;
int ibytesrecv;
int ibytessent;
int clientpktseq;
int serverpktseq;
//host data types
char localhost[21];
HOSTENT *hp;
//wait variables
int nsa1;
int r,infds=1, outfds=0;
fd_set readfds;
//others
char pwd[MAX_PATH];
string sentacknowledgementmsg;
string rcvdacknowledgementmsg;
const char *endpacketmarker= "FIN";
const char *SEND_FAILED_MSG = "Error sending to client";
const char *RECV_FAILED_MSG = "Error receving from client";
const int LOG_BUFFER_MAX_SIZE = 5;
const int packetLengthInBytes = 1024;
string logBuffer[LOG_BUFFER_MAX_SIZE];
int logbufferincrement = 0;
char* packetwindow[WINDOW_SIZE];
int send_base;
int packetcounter;

int seqlength = 1;
string *commandLineArguments;
const int num_threads = 5;
bool threewayhandshakecomplete = false;
bool handshakerequest = true;
bool correctpktrcvd = false;
bool ignoretheack = false ; 
// defined methods
string trim(string );
void initializeSockets();
void acceptUserConnections();
void handleUserConnection(SOCKET);
void parseCommand(string);
void ftpLIST(string,SOCKET);
void ftpPWD(string,SOCKET);
void ftpGET(string, string,SOCKET);
void ftpPUT(string,unsigned int, string,SOCKET);
string ftpCD(string, SOCKET);
void logEvents(string , string);
void ftpQUIT(SOCKET);
void cleanUp();
void ftpDelete(string, string, SOCKET);
void receiveResponse();
void sendAck(SOCKET , SOCKADDR_IN ,  int , int );
bool receiveAck(SOCKET , SOCKADDR_IN, int , int );
bool checkSequence(int,int);
int sendRequest(SOCKET , SOCKADDR_IN , char *, int , int, int );

void initializeSockets()
{
	try
	{
		logEvents("SERVER", "Initializing the server");
		WSADATA wsadata;
		if (WSAStartup(0x0202,&wsadata)!=0)
		{
			cout<<"Error in starting WSAStartup()\n";
			logEvents("SERVER", "Error in starting WSAStartup()");
			throw "Error in starting WSAStartup()";
		}
		else
		{
			//buffer="WSAStartup was suuccessful\n";
			logEvents("SERVER", "WSAStartup was suuccessful");
		}
		//Display info of local host
		gethostname(localhost,20);
		cout<<"hostname: "<<localhost<< endl;
		if((hp=gethostbyname(localhost)) == NULL)
		{
			cout << "Cannot get local host info."
				<< WSAGetLastError() << endl;
			logEvents("SERVER", "Cannot get local host info. Exiting....");
			exit(1);
		}

		//Create the server socket
		if((serverSocket = socket(AF_INET,SOCK_DGRAM,0))==INVALID_SOCKET) 
			throw "can't initialize socket";

		// For UDP protocol replace SOCK_STREAM with SOCK_DGRAM
		//Fill-in Server Port and Address info.
		serverSocketAddr.sin_family = AF_INET;
		serverSocketAddr.sin_port = htons(5001);
		serverSocketAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		//Bind the server port
		if (::bind(serverSocket,(LPSOCKADDR)&serverSocketAddr,sizeof(serverSocketAddr)) == SOCKET_ERROR)
			throw "can't bind the socket";
		cout << "Bind was successful" << endl;
		logEvents("SERVER", "Socket bound successfully.");

		acceptUserConnections();

	}
	catch(char* str)
	{ 
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
	cleanUp();
}
void acceptUserConnections()
{
		int retrycount = 0;
	int randomnumber = 0;
	char szbuffer[packetLengthInBytes];

	while(1)
	{
		try{
			cout<<"Accepting user connections: "<<endl;
			logEvents("SERVER", "Waiting for users to connect...");
			bool threewayhandshakecomplete = false;
			memset(szbuffer,'\0',1024);
			cout<<"Step1: Waiting for handshake msg from client: "<<szbuffer<<endl;
			if(recvfrom(serverSocket,szbuffer,sizeof(szbuffer),0,(SOCKADDR *)&clientSocketAddr, &senderAddrSize)==SOCKET_ERROR)
				throw "Error";
			string userAddress(inet_ntoa(clientSocketAddr.sin_addr));
			cout<<"connection from: "<<userAddress<<endl;
			srand ( time(NULL) );
			randomnumber = (rand()%255)+1;
			cout<<"Generated random number = "<<randomnumber<<endl;
			cout<<"Received from client : "<<szbuffer<<endl;
			clientpktseq = (randomnumber & 1);
			serverpktseq = (atoi(szbuffer) & 1);
			strcat(szbuffer,to_string(randomnumber).c_str());
			cout<<"Step2: Sending handshake msg to client: "<<szbuffer<<endl;
			logEvents("Server", "Step2: Sending handshake msg to client: "+string(szbuffer));
			sendRequest(serverSocket,clientSocketAddr,szbuffer,randomnumber,senderAddrSize,strlen(szbuffer));

			handleUserConnection(serverSocket);
		}
		catch(char* str)
		{
			LPTSTR Error = 0;
			if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
			{
				cout<<str<<endl;
				logEvents("ERROR", str +string(" Failed to format error message"));
			}
			else
			{
				cerr<<Error<<endl;
				logEvents("ERROR", Error);
			}
			LocalFree(Error);
		}
	}
}

void handleUserConnection(SOCKET clientSocket)
{
	try
	{
		char localbuffer[packetLengthInBytes];
		string userAddress(inet_ntoa(clientSocketAddr.sin_addr));
		string port(to_string(clientSocketAddr.sin_port));
		char presentworkingdirectory[MAX_PATH] = {};
		string userrequest="\0";
		//cout<<"accepted connection from "<<userAddress<<":"<<port<<endl;

		logEvents("SERVER", "Accepted connection from user @"+ userAddress +":"+port);
		bool clientConnectionActive = true;
		GetCurrentDirectory(MAX_PATH,presentworkingdirectory);
		while(clientConnectionActive)
		{
			cout<<"Client initial pkt sequence: "<<clientpktseq<<endl;
			cout<<"Server initial pkt sequence: "<<serverpktseq<<endl;
			logEvents("Server","Client initial pkt sequence: " +to_string(clientpktseq));
			logEvents("Server","Server initial pkt sequence: " +to_string(serverpktseq));

			cout<<"----------------------------------------------------------"<<endl;
			logEvents("Server","----------------------------------------------------------");
			int recvdpktnum = -1;
			memset(localbuffer,'\0',packetLengthInBytes);
			if(!handshakerequest)
			{
				cout<<"Waiting for client request:"<<endl;
				ibytesrecv = recvfrom(clientSocket,localbuffer,packetLengthInBytes,0,(LPSOCKADDR)&clientSocketAddr,&senderAddrSize);
				if(ibytesrecv == SOCKET_ERROR)
					throw "Receive error in server program. Possible reason may be conncetion closed by client.\n";
				cout<<localbuffer<<endl;
				recvdpktnum = string(localbuffer).at(1) - 48;
				cout<<recvdpktnum ;
				//cout<<"recvdpktnum "<<recvdpktnum<<endl;
				userrequest = string(localbuffer).erase(0,1); 
			}
			if(!handshakerequest && !checkSequence(clientpktseq, recvdpktnum))
			{
				cout<<"Discarded wrong packet"<<endl;
				logEvents("Server", "Discarded wrong packet");
			}
			else
			{
				if(handshakerequest)
				{
					//					ignoretheack = true;
					userrequest = rcvdacknowledgementmsg;
				}
				cout << "This is message from client: " << userrequest << endl;
				logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);

				parseCommand(userrequest);

				string command = commandLineArguments[0];
				string argument1 = commandLineArguments[1].length() >0 ?  commandLineArguments[1] :"" ;
				string argument2 = commandLineArguments[2].length() >0 ?  commandLineArguments[2] :"" ;

				if(strcmpi(command.c_str(),"GET")==0)
				{
					cout<<"GET request from user @"<<userAddress<<":"<<port<<endl;
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);
					ftpGET(argument1, presentworkingdirectory,clientSocket);
				}
				else if (strcmpi(command.c_str(),"PUT")==0)
				{
					cout<<"PUT request from user @"<<userAddress<<":"<<port<<endl;
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);
					ftpPUT(argument1, atol(argument2.c_str()), presentworkingdirectory, clientSocket);

				}
				else if (strcmpi(command.c_str(),"PWD")==0)
				{
					cout<<"PWD request from user @"<<userAddress<<":"<<port<<endl;
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);
					ftpPWD(presentworkingdirectory,clientSocket);

				}
				else if (strcmpi(command.c_str(),"CD")==0)
				{
					cout<<"CD request from user @"<<userAddress<<":"<<port<<endl;
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);
					if(argument1.find("..")==0) //check for relative path
						strcat(presentworkingdirectory,argument1.c_str());
					string dir = ftpCD(argument1,clientSocket);
					if(dir.length()>0)
						strcpy(presentworkingdirectory, dir.c_str());

				}
				else if (strcmpi(command.c_str(),"LIST")==0)
				{
					cout<<"LIST request from user @"<<userAddress<<":"<<port<<endl;
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);

					string listDir = argument1.length()==0 ? presentworkingdirectory : argument1;
					ftpLIST(listDir, clientSocket);
				}
				else if (strcmpi(command.c_str(),"QUIT")==0)
				{
					cout<<"QUIT request from user @"<<userAddress<<":"<<port<<endl;
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);
					ftpQUIT(clientSocket);
					clientConnectionActive =false;
					cout<<"User @"<<userAddress+":"+port +" disconnected."<<endl;
				}
				else if (strcmpi(command.c_str(),"DELETE")==0)
				{
					cout<<"DELETE request from user @"<<userAddress<<":"<<port<<endl;
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);
					ftpDelete(argument1,presentworkingdirectory, clientSocket);
				}
				else
				{
					cout<<"This is INVALID Request\n";
					logEvents("SERVER", "Request from user @"+userAddress+":"+port +" --" +userrequest);
				}
			}
		}//while 
	}
	catch(char* str)
	{

		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
		return;
	}
}
void parseCommand(string userInput)
{
	string delimiter = " ";
	commandLineArguments = new string[MAX_PATH];
	int count = 0;
	int pos = 0;
	userInput = trim(userInput);
	pos = userInput.find(delimiter);
	while(pos>=0)
	{
		userInput = trim(userInput);
		pos = userInput.find(delimiter);
		string command = userInput.substr(0, pos);
		commandLineArguments[count]=command;
		userInput = userInput.erase(0,pos+1);
		userInput = trim(userInput);
		pos=userInput.find(delimiter);
		count++;
	}
	if(strlen(userInput.c_str()) > 0 && pos <= 0)
		commandLineArguments[count]=userInput;
}

string trim(string arg)
{
	const string whitespace = " \t\f\v\n\r";
	int start = arg.find_first_not_of(whitespace);
	int	end = arg.find_last_not_of(whitespace);
	arg.erase(0,start);
	arg.erase((end - start) + 1);
	return arg;
}

void ftpLIST(string directory, SOCKET clientSocket)
{
	try
	{
		if(handshakerequest)
					handshakerequest = false;
		logEvents("LIST","Executing system DIR command");
		if(access(directory.c_str(),0) ==0 )
		{
				system(string("dir \"" + directory + "\" /b /o:gn > c:\\logs\\list.txt").c_str());
				
				logEvents("LIST","Opening file to read the dir contents.");
				const int maxByteToSent = 1021;
				//array to send to client. +1 to add sentinel character.
				char *buff= new char[maxByteToSent + 1];
				memset(buff,'\0',maxByteToSent+1);

				ifstream ifs("c:\\logs\\list.txt",ios::in);
				ifs.seekg(0,ifs.end);
				int length =  ifs.tellg();
				ifs.seekg(0,ifs.beg);
				char *listBuffer = new char[length];
				memset(listBuffer,'\0',sizeof(listBuffer));
				ifs.read(listBuffer,length);
				int bytesRead = ifs.gcount();
				ifs.close();
				remove("c:\\temp\\list.txt");
				logEvents("LIST","File closed and removed.");
				int totalbytessent = 0;
				while(bytesRead >= totalbytessent)
				{
					int remainingBytes = bytesRead - totalbytessent;
					int bytesToSend= remainingBytes > maxByteToSent ? maxByteToSent : remainingBytes ;
					memset(buff,'\0',maxByteToSent+3);
					strcpy(buff,(to_string(clientpktseq)+to_string(serverpktseq)).c_str());
					memcpy(buff+2, listBuffer+totalbytessent,bytesToSend); //copy the number of bytes to be sent to buff.
					if((ibytessent = sendRequest(clientSocket,clientSocketAddr,buff,serverpktseq,senderAddrSize,maxByteToSent+2)) != SOCKET_ERROR) // send to client.
						totalbytessent =totalbytessent + (ibytessent -1);
					else
						throw SEND_FAILED_MSG;
				}
				strcpy(buff,(to_string(clientpktseq)+to_string(serverpktseq)).c_str());
				memcpy(buff+2, endpacketmarker,6);// To indicate transfer finish.
				if(sendRequest(clientSocket,clientSocketAddr,buff,serverpktseq,senderAddrSize,strlen(endpacketmarker)+2) == SOCKET_ERROR)
					throw SEND_FAILED_MSG;
				else
					logEvents("LIST","Request completed successfully");
		}
		else
		{
			cout<<strerror(errno)<<endl;
			sentacknowledgementmsg = string("-1") +string(strerror(errno));
			sendAck(clientSocket,clientSocketAddr,clientpktseq,senderAddrSize);
			logEvents("LIST",strerror(errno) + directory);
			return;
		}

	}
	catch (char *str)
	{ 
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
	clientpktseq = (clientpktseq + 1) % MAX_PACKET_SEQ;
}

void ftpGET(string sourceFile, string directory, SOCKET clientSocket)
{
	try
	{
		if(handshakerequest)
					handshakerequest = false;
		int last_index_of_slash =sourceFile.rfind('\\');
		//string fileName = sourceFile.substr(last_index_of_slash+1, sourceFile.length());
		if(last_index_of_slash <=0)
			directory.append("\\"+sourceFile);
		else
			directory = sourceFile;
		ifstream ifs (directory, ios::in | ios::binary);	
		char *sendbuff = new char[packetLengthInBytes];

		if(ifs)
		{
			int bytesRead = 0;
			int remainingBytesToSend = 0;
			long fsize = 0;
			time_t start = time(0);
			ifs.seekg (0, ifs.end);
			fsize = ifs.tellg();
			ifs.seekg (0, ifs.beg);
			remainingBytesToSend = fsize;
//			sentacknowledgementmsg = to_string(fsize);
//			sendAck(clientSocket, clientSocketAddr,clientpktseq,senderAddrSize);
//			cout<<"Sent ack for control packet "<<clientpktseq <<endl;
			cout<<"Sending file " + directory +" to client "<<fsize<<endl;
			logEvents("GET", "Sending file " + directory +" to client");
			int readCounter = 0;
			int totalBytes = 0;
			int extrabytes = 4;
			stringstream paddedseq;
			while(remainingBytesToSend > 0)
			{
				int bytesToSend = packetLengthInBytes < remainingBytesToSend ? packetLengthInBytes :remainingBytesToSend;
				sendbuff = new char[bytesToSend];
				memset(sendbuff,'\0',bytesToSend);
				ifs.read(sendbuff, bytesToSend);	
				char *tempbuff = new char[bytesToSend+extrabytes];
				memset(tempbuff,'\0',bytesToSend+extrabytes);
				
				paddedseq <<setfill('0')<<setw(2)<<clientpktseq;
				string seqc = paddedseq.str();
				paddedseq.str("");
				paddedseq <<setfill('0')<<setw(2)<<serverpktseq;
				string seqs = paddedseq.str();
				paddedseq.str("");
				strcpy(tempbuff,(seqc+seqs+to_string(fsize)).c_str());
				memcpy(tempbuff+extrabytes,sendbuff,bytesToSend);
				logEvents("Debug",tempbuff);
				ibytessent = sendRequest(clientSocket,clientSocketAddr,tempbuff,serverpktseq,senderAddrSize, bytesToSend+extrabytes);
				++readCounter;
				remainingBytesToSend = remainingBytesToSend - bytesToSend;
				totalBytes = totalBytes + ibytessent;
			}
			paddedseq <<setfill('0')<<setw(2)<<clientpktseq;
				string seqc = paddedseq.str();
				paddedseq.str("");
				paddedseq <<setfill('0')<<setw(2)<<serverpktseq;
				string seqs = paddedseq.str();
				paddedseq.str("");
			char *tempbuff = new char[strlen(endpacketmarker)+extrabytes];
			strcpy(tempbuff,(seqc+seqs).c_str());
				memcpy(tempbuff+extrabytes, endpacketmarker,6);// To indicate transfer finish.
				if((ibytessent=sendRequest(clientSocket,clientSocketAddr,tempbuff,serverpktseq,senderAddrSize, strlen(endpacketmarker)+extrabytes)) == SOCKET_ERROR)
					throw SEND_FAILED_MSG;
				else
					logEvents("Get","Request completed successfully");
				totalBytes = totalBytes + ibytessent;
			time_t end = time(0);
	
			cout<<"Sent "<<to_string(readCounter+1)<<" packets for "<<totalBytes<< " bytes in "<<difftime(end,start)<<" seconds"<<endl;
			logEvents("GET","Transfer complete. Sent "+to_string(readCounter+1) +" packets for " + to_string(fsize) +" bytes in " + to_string(difftime(end,start)) +" seconds");
		}
		else
		{
			cout<<strerror(errno)<<endl;
			logEvents("GET", directory +" : " +strerror(errno));
			sentacknowledgementmsg = string("-1")+strerror(errno); 
			//sprintf(sendbuff,(to_string(clientpktseq)+string("-1")+strerror(errno)).c_str());
			sendAck(clientSocket,clientSocketAddr,clientpktseq,senderAddrSize);
			//if(sendto(clientSocket,sendbuff,strlen(sendbuff),0)== SOCKET_ERROR)
			//throw SEND_FAILED_MSG;
		}
		ifs.close();
	}
	catch (char *str)
	{ 
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
		clientpktseq = (clientpktseq + 1) % MAX_PACKET_SEQ;
}
void ftpPUT(string sourceFile,unsigned int fsize, string directory, SOCKET clientSocket)
{
	try
	{
		if(handshakerequest)
				handshakerequest = false;
		char localbuffer[packetLengthInBytes];
		bool transferComplete = false;
		char recvbuff[packetLengthInBytes];
		long int remainingBytesToRead = fsize;
		memset(localbuffer,'\0',1024);
		int last_index_of_slash =sourceFile.rfind('\\');
		string fileName = sourceFile.substr(last_index_of_slash+1, sourceFile.length());
		//if(last_index_of_slash <=0)
		directory.append("\\"+fileName);
		//else
		//directory = sourceFile;
		ofstream ofs(directory, ios::out | ios::trunc | ios::binary);
		time_t start = time(0);
		if(ofs)
		{
			sentacknowledgementmsg = string("1"); 
			sendAck(clientSocket, clientSocketAddr,clientpktseq,senderAddrSize);
			int writeCounter = 0;
			int remainingBytesToRead = fsize;
			char *recvbuff;
			time_t start = time(0);
			int totalBytes = 0;
			int rcvdpktnumber = -1;

			while(remainingBytesToRead > 0)
			{
				int bytesToRecv = packetLengthInBytes < remainingBytesToRead ? packetLengthInBytes :remainingBytesToRead;
				recvbuff = new char[bytesToRecv+1];
				memset(recvbuff,'\0',bytesToRecv+1);
				correctpktrcvd = false;
				while(!correctpktrcvd)
				{
					ibytesrecv = recvfrom(clientSocket,recvbuff,bytesToRecv+1,0,(LPSOCKADDR)&clientSocketAddr,&senderAddrSize);
					rcvdpktnumber = string(recvbuff).at(0) - 48;
					sendAck(clientSocket,clientSocketAddr,rcvdpktnumber,senderAddrSize);
				}
				if(ibytesrecv !=SOCKET_ERROR)
				{	
					++writeCounter;
					cout<<"Total packets received "<<writeCounter<<endl;
					remainingBytesToRead = remainingBytesToRead - bytesToRecv;
					char *writebuff = recvbuff+1; 
					totalBytes = totalBytes + ibytesrecv;
					logEvents("Break ","---------------------------------------------------------------------");
					logEvents("GET", "Bytes received from server in "+ to_string(writeCounter) +string(" request: ") +  to_string(ibytesrecv));
					logEvents("GET", "remainingBytesToRead "+ to_string(remainingBytesToRead));
					logEvents("Debug:recvbuff ",recvbuff);
					ofs.write(writebuff, bytesToRecv);
				}
				else
				{
					cout<<RECV_FAILED_MSG<<endl;
					logEvents("ERROR", RECV_FAILED_MSG);
					throw RECV_FAILED_MSG;
				}
			}
			time_t end = time(0);
			cout<<"Received "<<to_string(writeCounter)<<" packets for "<<fsize<< " bytes in "<<difftime(end,start)<<" seconds"<<endl;
			logEvents("Put","Transfer complete. Sent "+to_string(writeCounter) +" packets for " + to_string(fsize) +" bytes in " + to_string(difftime(end,start)) +" seconds");
		}
		else
		{
			sentacknowledgementmsg = string("-1")+strerror(errno); 
			sendAck(clientSocket, clientSocketAddr,clientpktseq,senderAddrSize);
			cout<<strerror(errno)<<endl;
			logEvents("PUT",strerror(errno));
		}
		ofs.close();
	}
	catch (char *str)
	{ 
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
}
string ftpCD(string directory, SOCKET clientSocket)
{
	try
	{

		char localbuffer[packetLengthInBytes];
		memset(localbuffer,'\0',packetLengthInBytes);
		if(access( directory.c_str(), 0 ) != 0)
		{
			cout<<strerror(errno)<<endl;
			directory = "\0";
			sentacknowledgementmsg = "-1System cannot find the specified directory."; 
			sendAck(clientSocket, clientSocketAddr,clientpktseq,senderAddrSize);
			logEvents("CD", "System cannot find the specified directory : "+ directory );
		}
		else
		{
			struct stat status;
			stat( directory.c_str(), &status );
			if (status.st_mode & S_IFDIR )
			{
				sentacknowledgementmsg = "1Directory changed successfully."; 
				sendAck(clientSocket, clientSocketAddr,clientpktseq,senderAddrSize);
				logEvents("CD", "Directory changed successfully to : "+ directory );
			}
			else
			{
				sentacknowledgementmsg = "-1Cannot set file as directory."; 
				sendAck(clientSocket, clientSocketAddr,clientpktseq,senderAddrSize);
				logEvents("CD", "Cannot set file as directory. : "+ directory );
				directory = "\0";
			}

		}
	}
	catch (char *str)
	{ 
		directory = "\0";
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);

	}
	return directory;
}
void ftpPWD(string directory,SOCKET clientSocket)
{

	try {
		char presentDir[MAX_PATH]={};
		char localbuffer[packetLengthInBytes];
		memset(localbuffer,'\0',packetLengthInBytes);
		//int nBufferLength = 0;
		if(directory.length()==0)
			GetCurrentDirectory(MAX_PATH, presentDir);
		else
			strcpy(presentDir,directory.c_str());

		if(!strlen(presentDir))
		{
			sprintf(localbuffer,"0Failed to get current directory");
			if(send(clientSocket,localbuffer,packetLengthInBytes,0) == SOCKET_ERROR )
				throw SEND_FAILED_MSG;
			logEvents("PWD", "Failed to get current directory");
		}
		else
		{
			sprintf(localbuffer,(string("1")+string(presentDir)).c_str());
			if(send(clientSocket,localbuffer,packetLengthInBytes,0) == SOCKET_ERROR )
				throw SEND_FAILED_MSG;
			logEvents("PWD", "Request completed. Present working directory is: "+ string(presentDir));
		}
	}
	catch (char *str)
	{
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
}
void ftpQUIT(SOCKET clientSocket)
{
	try
	{
		char localbuffer[packetLengthInBytes];
		memset(localbuffer,'\0',packetLengthInBytes);
		if(send(clientSocket,localbuffer,128,0) == SOCKET_ERROR)
			throw SEND_FAILED_MSG;
		logEvents("SERVER", "Closing client socket...");
		closesocket(clientSocket);
		logEvents("QUIT", "Bye.... ");
	}
	catch (char *str)
	{ 
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
		logEvents("SERVER", "Closing client socket...");
		closesocket(clientSocket);
		logEvents("QUIT", "Bye.... ");

	}

}
void ftpDelete(string file, string directory, SOCKET clientSocket)
{
	try
	{
		if(handshakerequest)
			handshakerequest = false;
		char localbuffer[packetLengthInBytes];
		memset(localbuffer,'\0',packetLengthInBytes);
		int last_index_of_slash =file.rfind('\\');
		//string fileName = file.substr(last_index_of_slash+1, file.length());
		if(last_index_of_slash <=0)
			directory.append("\\"+file);
		else
			directory = file;

		if (remove(directory.c_str())!=0)
		{
			cout<<strerror(errno)<<endl;
			sentacknowledgementmsg = string("0")+string(strerror(errno));
			logEvents("DELETE", strerror(errno));
			sendAck(clientSocket,clientSocketAddr,clientpktseq,senderAddrSize);
				
		}
		else
		{
			cout<<"File deleted."<<endl;
			logEvents("DELETE", "File deleted.");
			sentacknowledgementmsg = "1File deleted.";
			sendAck(clientSocket,clientSocketAddr,clientpktseq,senderAddrSize);
		}
	}
	catch (char *str)
	{ 
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
}
void cleanUp()
{
	logEvents("SERVER", "Closing client socket...");
	closesocket(cs);
	logEvents("SERVER", "Closed. \n Closing server socket...");
	closesocket(serverSocket);
	logEvents("SERVER", "Closed. Performing cleanup...");
	WSACleanup();
}

void logEvents(string tag, string eventDescription)
{
	try{
		ofstream logFile;
		time_t now = time(0);
		char* dt = ctime(&now);
		string dateTime(dt);
		dateTime = dateTime.substr(0,strlen(dt)-1);
		string logLine = dateTime +" : "+ tag +":: "+ eventDescription;
		//logBuffer[logbufferincrement++] = logLine;
		logFile.open("c:\\logs\\serverlog.txt",ios::in | ios::out | ios::app);
		logFile<< logLine<<endl;
		logFile.close();
		/*	if(logbufferincrement == LOG_BUFFER_MAX_SIZE)
		{
		int counter = 0;
		logFile.open("c:\\temp\\log.txt",ios::in | ios::out | ios::app);
		while(counter < logbufferincrement)
		{
		logFile<< logBuffer[counter++]<<endl;
		logbufferincrement--;
		}
		logFile.close();
		}
		*/
	}
	catch(...)
	{
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			logEvents("ERROR", " Failed to format error message");
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
}
void sendAck(SOCKET socket , SOCKADDR_IN socketaddr, int pktnumberrcvd,  int socketlength)
{
	char sendbuffer[packetLengthInBytes];
	correctpktrcvd = checkSequence(clientpktseq,pktnumberrcvd);

//	itoa((correctpktrcvd ? pktnumberrcvd :serverpktseq),sendbuffer,10);
	itoa(pktnumberrcvd,sendbuffer,10);
	strcat(sendbuffer,sentacknowledgementmsg.c_str());


	
	if(sendto(socket,sendbuffer,packetLengthInBytes,0,(LPSOCKADDR)&socketaddr,socketlength) ==  SOCKET_ERROR)
		throw SEND_FAILED_MSG;
	cout<<"ACK data sent to client: "<<sendbuffer<<endl;
	cout<<"Sent ACK for packet sequence to client: "<<pktnumberrcvd<<endl;
	logEvents("Server","Sent ACK for packet sequence to client: "+pktnumberrcvd);
	logEvents("Server","ACK data sent to client:  \n"+string(sendbuffer));
	if(correctpktrcvd)
	{
		cout<<"Sending ACK for packet "+ to_string(pktnumberrcvd)<<endl;
		logEvents("Server","Sending ACK for packet "+ to_string(pktnumberrcvd));
		clientpktseq = (clientpktseq+1) % MAX_PACKET_SEQ;
	}
	else
	{
		logEvents("Server","Received wrong packet."+ to_string(pktnumberrcvd));
		cout<<"Received wrong packet "+ to_string(pktnumberrcvd)<<endl;
	}
	sentacknowledgementmsg = '\0';
}

bool checkSequence(int previouspktnumber, int ackrcvdforpkt)
{
	return previouspktnumber == ackrcvdforpkt ? true :false;
}

int current_window_size = 0;
int sendRequest(SOCKET socket , SOCKADDR_IN socketaddr, char *sendbuffer, int packetseq, int socketlength, int bytesToSend)
{
	int retrycount = 0;
	bool acknowledged = false;

	if(current_window_size != WINDOW_SIZE)
	{
		packetwindow[packetcounter] = sendbuffer;
		packetcounter = (packetcounter+1) % WINDOW_SIZE;
		return bytesToSend;
	}
	else
	{
		for(int i=0; i<WINDOW_SIZE; i = (i+1) % WINDOW_SIZE)
		{
			if((ibytessent = sendto(socket,packetwindow[i], bytesToSend,0,(LPSOCKADDR)&socketaddr,socketlength)) == SOCKET_ERROR)
				throw SEND_FAILED_MSG;
			cout<<"Sent request packet sequence to client: "<<packetseq<<endl;
	//		cout<<"Request data sent to client: "<<sendbuffer<<endl;
			logEvents("Server","Data sent to client \n"+ string(sendbuffer));
			logEvents("Server","Sent request packet sequence to client: "+ packetseq);

		}
	}

	while(!acknowledged)
	{
		if(receiveAck( socket , socketaddr, packetseq, socketlength))
		{
			logEvents("Server", "Received ACK for packet "+to_string(packetseq));
			cout<<"Received ACK for packet "+to_string(packetseq)<<endl;
			acknowledged = true;
		}
		else
		{
			if(retrycount++ == MAX_RETRIES)
			{
				cout<<"No of retries exchausted"<<endl;
				throw "No of retries exchausted.";
			}
		}
	}
	return ibytessent;
}

bool receiveAck(SOCKET socket , SOCKADDR_IN socketaddr,int packetseq, int socketlength)
{
	try
	{
		char recvbuffer[packetLengthInBytes]; 
		int ackrcvdforpkt = 0;
		memset(recvbuffer,'\0',packetLengthInBytes);
		struct timeval *timeout = new timeval;
		timeout->tv_sec=TIMEOUT_SEC;
		timeout->tv_usec=0;
		FD_ZERO(&readfds);
		 FD_SET(socket,&readfds);

		if(select(1,&readfds,NULL,NULL,timeout))
		{
			memset(recvbuffer,'\0',packetLengthInBytes);
			if(recvfrom(socket,recvbuffer, packetLengthInBytes,0,(LPSOCKADDR)&socketaddr,&socketlength) == SOCKET_ERROR)
				throw RECV_FAILED_MSG;

			if(!threewayhandshakecomplete)
				ackrcvdforpkt = stoi(string(recvbuffer).substr(0,to_string(packetseq).length()).c_str());
			else
				ackrcvdforpkt = stoi(string(recvbuffer).substr(0,1).c_str());

			cout<<"Received ACK for packet sequence from client: "<<ackrcvdforpkt<<endl;
			logEvents("Server","ACK Data received from client \n"+ string(recvbuffer));	

			//check Ack/NAK
			if(ackrcvdforpkt >=0)
			{
				send_base = (ackrcvdforpkt+1) % WINDOW_SIZE;
			}
			else
			{
				//its a NAK
			}
			if(checkSequence(packetseq,ackrcvdforpkt))
			{
				if(!threewayhandshakecomplete)
				{
					rcvdacknowledgementmsg = string(recvbuffer).substr(to_string(packetseq).length(),strlen(recvbuffer)).c_str();
					threewayhandshakecomplete = true;
					cout<<"Connection Established"<<endl;
				}
				else
				{
					serverpktseq = (serverpktseq+1) % MAX_PACKET_SEQ;		
				}
				return true; 
			}
			else
			{
				cout<<"Discarding the wrong ACK"<<endl;
				cout<<"Expecting ACK for packet "<<packetseq<<endl;
				cout<<"Received ACK for packet "<<ackrcvdforpkt<<endl;
				logEvents("Server","Discarding the wrong ACK.\nExpecting ACK for packet:"+to_string(packetseq)+"\nReceived ACK for packet: "+to_string(ackrcvdforpkt));
				return false;
			}
		}
		else
		{
			cout<<"Request timed out waiting for ACK for packet " + to_string(packetseq)<<endl;
			logEvents("Server", "Request timed out waiting for ACK for packet " + to_string(packetseq));
			return false;
		}
	}
	catch(...)
	{
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			logEvents("ERROR", " Failed to format error message");
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
		return false;
	}
}
int main(void)
{
	try
	{
		initializeSockets();
	}
	catch(char* str)
	{
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
		}
		LocalFree(Error);
	}
	return 0;
}
