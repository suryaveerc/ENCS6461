
char* getmessage(char *);
#pragma comment( linker, "/defaultlib:ws2_32.lib" )
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winsock.h>
#include <stdio.h>
#include <iostream>
#include <string>
#include <windows.h>
#include <fstream>
#include <time.h>
#include <sstream>
#include <iomanip>
using namespace std;
//user defined port number
#define REQUEST_PORT 7000
#define TIMEOUT_USEC 900000
#define TIMEOUT_SEC 5
#define MAX_RETRIES 3000			
#define MAX_PACKET_SEQ 20
int port=REQUEST_PORT;
//socket data types
SOCKET clientSocket;
SOCKADDR_IN clientSocketAddr; // filled by bind
SOCKADDR_IN serverSocketAddr; // fill with server info, IP, port
//buffer data types
char szbuffer[1024];
//char *buffer;
int socketlen;
int ibufferlen=0;
int ibytessent;
int ibytesrecv=0;
int clientpktseq = 0;
int serverpktseq = 0;


//host data types
HOSTENT *hp;
HOSTENT *rp;
int r,infds=1, outfds=0;
fd_set readfds;

char localhost[21],
	remotehost[21];
//other
char clientPWD[MAX_PATH];
char *serverPWD;

string *commandLineTokens;
string rcvdacknowledgementmsg;
string sentacknowledgementmsg;
char *endpacketmarker= "FIN";
char *SEND_FAILED_MSG = "Error sending to server.";
char *RECV_FAILED_MSG = "Error receving from server.";
const int LOG_BUFFER_MAX_SIZE = 5;
string logBuffer[LOG_BUFFER_MAX_SIZE];
int logbufferincrement = 0;
const int packetLengthInBytes = 1024;
bool threewayhandshakecomplete = false;
bool firstlegcomplete = false;
bool correctpktrcvd =  false;
void parseCommand(string , string);
string trim(string );
void ftpLIST(string);
void ftpPWD();
void ftpGET(string);
void ftpPUT(string);
void ftpLCD(string);
void ftpCD(string);
void ftpDELETE(string);
void logEvents(string, string);
void cleanUp();
void ftpQUIT();
void receiveResponse();
bool checkSequence(int,int);
int sendRequest(SOCKET , SOCKADDR_IN , char *, int , int, int );
bool receiveAck(SOCKET , SOCKADDR_IN , int , int );
void sendAck(SOCKET , SOCKADDR_IN , int , int );
int main(void){

	logEvents("CLIENT", "Initializing WSAData");
	WSADATA wsadata;

	try {

		if (WSAStartup(0x0202,&wsadata)!=0){
			cout<<"Error in starting WSAStartup()" << endl;
			logEvents("CLIENT", "Error in starting WSAStartup()");
			throw "Error in starting WSAStartup()";
		} else {
			logEvents("CLIENT", "WSAStartup was successful");
		}
		//Display name of local host.
		gethostname(localhost,20);
		cout<<"You are connected as  \"" << localhost << "\"" << endl;
		logEvents("CLIENT", "You are connected as "+ string(localhost) );

		if((hp=gethostbyname(localhost)) == NULL)
			throw "gethostbyname failed\n";
		while(true)
		{
			cout << "please enter your remote server name :" << flush ;
			cin >> remotehost ;
			if(strcmp(remotehost,"Q")==0)
				exit(1);
			//	logEvents("CLIENT", "Connected to "+ string(remotehost) );
			if((rp=gethostbyname(remotehost)) == NULL)
			{
				cout<<"Unable to find the remote host : "<<remotehost<<endl;
				cout<<"Please retry or type 'Q' to exit"<<endl;
				logEvents("CLIENT","Unable to find the remote host : " +string(remotehost));
			}
			else
				break;
		}

		//Create the socket
		if((clientSocket = socket(AF_INET,SOCK_DGRAM,0))==INVALID_SOCKET) 
			throw "Socket creation failed\n";


		memset(&clientSocketAddr,0,sizeof(clientSocketAddr));
		memcpy(&clientSocketAddr.sin_addr,rp->h_addr,rp->h_length);
		clientSocketAddr.sin_family = AF_INET;
		clientSocketAddr.sin_port = htons(5000);
		clientSocketAddr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (::bind(clientSocket,(LPSOCKADDR)&clientSocketAddr,sizeof(clientSocketAddr)) == SOCKET_ERROR)
			throw "can't bind the socket";
		/* For UDP protocol replace SOCK_STREAM with SOCK_DGRAM */
		//Specify server address for client to connect to server.

		memset(&serverSocketAddr,0,sizeof(serverSocketAddr));
		memcpy(&serverSocketAddr.sin_addr,rp->h_addr,rp->h_length);
		serverSocketAddr.sin_family = rp->h_addrtype;
		serverSocketAddr.sin_port = htons(port);

		//Display the host machine internet address
		cout << "Connecting to remote host: ";
		cout << inet_ntoa(serverSocketAddr.sin_addr) << endl;
		logEvents("CLIENT", "Initiating connection to host: "+ string(inet_ntoa(serverSocketAddr.sin_addr)) );
		//Threeway handshake

		int retrycount = 0;
		socketlen = sizeof(serverSocketAddr);
		int randomnumber = 0;
		//bool threewayhandshakecomplete = false;


		//srand(time(NULL));
		randomnumber = (rand()%255)+1;
		int randlen = to_string(randomnumber).length();
		cout<<"Generated random number = "<<randomnumber<<endl;
	//	cout<<(randomnumber & 1)<<endl;
		memset(szbuffer,'\0',1024);

		sprintf(szbuffer,to_string(randomnumber).c_str());


		//threeway handshake start
		if(sendRequest(clientSocket, serverSocketAddr, szbuffer, randomnumber, socketlen, strlen(szbuffer)) == SOCKET_ERROR)
			throw "Connection failed...\n";

		serverpktseq = (randomnumber & 1);
		clientpktseq = (atoi(rcvdacknowledgementmsg.c_str()) & 1);
		logEvents("CLIENT", "Connected to host: "+ string(inet_ntoa(serverSocketAddr.sin_addr)) );

		bool continueConnection = true;
		cout<<"Enter your choice: \n"
			<<"1. GET\n ** Works on current directory ** ex: get [filename]\n"
			<<"2. PUT\n ** Works on current directory ** ex: put [filename]\n"
			<<"3. CD ex: cd [directory]\n"
			<<"4. LIST ex: list\n"
			<<"5. PWD ex: pwd\n"
			<<"6. LCD ex: lcd or lcd [directory]\n"
			<<"7. DELETE ex: delete [filname]\n"
			<<"8. QUIT ex: quit"
			<<endl;

		while(continueConnection)
		{
			memset(szbuffer,'\0',packetLengthInBytes);
			string userCommand = "";

			//cin.ignore(10000,'\n');
			cout<<"Client initial pkt sequence: "<<clientpktseq<<endl;
			cout<<"Server initial pkt sequence: "<<serverpktseq<<endl;
			logEvents("Client","Client initial pkt sequence: " +to_string(clientpktseq));
			logEvents("Client","Server initial pkt sequence: " +to_string(serverpktseq));

			cout<<"----------------------------------------------------------"<<endl;
			logEvents("Client","----------------------------------------------------------");
			cout<<"Enter user command: ";
			cin.sync();

			while(userCommand.length()<=0)
				getline(cin,userCommand);
			parseCommand(userCommand, " ");

			string command = commandLineTokens[0];
			string argument = commandLineTokens[1];
			string commandToServer = command + " " + argument;

			sprintf(szbuffer,commandToServer.c_str(),0);

			if(strcmpi(command.c_str(),"GET")==0)
			{
				logEvents("GET", "Request from user --" + userCommand);
				while(strlen(argument.c_str())==0)
				{
					cout<<"Please provide a filename: ";
					cin>>argument;
					argument = trim(argument);
				}
				commandToServer = command+" "+argument;
				sprintf(szbuffer,commandToServer.c_str(),0);
				ftpGET(argument);
			}
			else if(strcmpi(command.c_str(),"PUT")==0)
			{
				logEvents("PUT", "Request from user --" + userCommand);
				while(strlen(argument.c_str())==0)
				{
					cout<<"Please provide a filename: ";
					cin>>argument;
					argument = trim(argument);
				}
				commandToServer = command+" "+argument;
				sprintf(szbuffer,commandToServer.c_str(),0);
				ftpPUT(argument);
			}
			else if(strcmpi(command.c_str(),"PWD")==0)
			{
				logEvents("PWD", "Request from user --" + userCommand);
				ftpPWD();
			}
			else if(strcmpi(command.c_str(),"LIST")==0)
			{
				logEvents("LIST", "Request from user --" + userCommand);
				ftpLIST(commandToServer);
			}
			else if(strcmpi(command.c_str(),"LCD")==0)
			{
				logEvents("LCD", "Request from user --" + userCommand);
				ftpLCD(argument);
			}
			else if(strcmpi(command.c_str(),"CD")==0)
			{
				logEvents("CD", "Request from user --" + userCommand);
				while(strlen(argument.c_str())==0)
				{
					cout<<"Please provide directory"<<endl;
					cin>>argument;
					argument = trim(argument);
				}
				commandToServer = command+" "+argument;
				sprintf(szbuffer,commandToServer.c_str(),0);
				ftpCD(commandToServer);
			}
			else if(strcmpi(command.c_str(),"DELETE")==0)
			{
				logEvents("DELETE", "Request from user --" + userCommand);
				while(strlen(argument.c_str())==0)
				{
					cout<<"Please provide a filename: ";
					cin>>argument;
					argument = trim(argument);
				}
				commandToServer = command+" "+argument;
				sprintf(szbuffer,commandToServer.c_str(),0);
				ftpDELETE(argument);
			}
			else if(strcmpi(command.c_str(),"QUIT")==0)
			{
				logEvents("QUIT", "Request from user --" + userCommand);
				ftpQUIT();
				continueConnection = false;
			}
			else
			{
				cout<<"Invalid command."<<endl;
				logEvents("INVALID", "Request from user --" + userCommand);
			}
		}
	} // try loop
	//Display any needed error response.
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
	return 0;
}

void parseCommand(string userInput, string delimiter)
{
	commandLineTokens = new string[MAX_PATH];
	int count = 0;
	int pos = 0;
	userInput = trim(userInput);
	pos = userInput.find(delimiter);
	while(pos>=0)
	{
		userInput = trim(userInput);
		pos = userInput.find(delimiter);
		string command = userInput.substr(0, pos);
		commandLineTokens[count]=command;
		userInput = userInput.erase(0,pos+1);
		userInput = trim(userInput);
		pos=userInput.find(delimiter);
		//		cout<<"Command: "<<commandLineTokens[count]<<endl;
		count++;
	}
	if(strlen(userInput.c_str()) > 0 && pos <= 0)
		commandLineTokens[count]=userInput;
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

void ftpGET(string argument)
{

	int last_index_of_slash =argument.rfind('\\');
	argument = argument.substr(last_index_of_slash+1, argument.length());
	int ackrcvdforpkt = 0;
	int retrycount = 0;

	bool acknowledged = false;

	try
	{
		ofstream file(argument, ios::out | ios::trunc | ios::binary);

		if(file)
		{

			if(!threewayhandshakecomplete)
			{
				stringstream paddedseq;
				paddedseq <<setfill('0')<<setw(2)<<rcvdacknowledgementmsg;
				string seq = paddedseq.str();
				paddedseq.str("");
				sprintf(szbuffer,string(szbuffer).insert(0,seq).c_str());
			}
			else
			{
				stringstream paddedseq;
				paddedseq <<setfill('0')<<setw(2)<<clientpktseq;
				string seq = paddedseq.str();
				paddedseq.str("");
				sprintf(szbuffer,string(szbuffer).insert(0,seq).c_str());
			}

			if(!threewayhandshakecomplete)
			{
				threewayhandshakecomplete = true;
				cout<<"Connection Established"<<endl;
				logEvents("CLIENT", "Threeway handshake with server complete.");
			}


			bool ack = false;
			bool transfercomplete = false;
			int rcvdpktnumberforclient = -1;
			int rcvdpktnumberforserver = -1 ;
			int writeCounter = 0;
			int extrabytes = 4;
			char *recvbuff = new char[packetLengthInBytes];
			time_t start = time(0);
			int totalBytes = 0;
			while(!(ack && transfercomplete))
			{
				cout<<szbuffer<<endl;
				if(!ack)
				{
					if(sendto(clientSocket,szbuffer,strlen(szbuffer),0,(LPSOCKADDR)&serverSocketAddr,socketlen) ==SOCKET_ERROR)
						throw SEND_FAILED_MSG;
					logEvents("GET","Sent request to server: "+string(szbuffer));
				}

				recvbuff = new char[packetLengthInBytes+extrabytes];
				memset(recvbuff,'\0',packetLengthInBytes+extrabytes);
				correctpktrcvd = false;
				while(!correctpktrcvd)
				{
					struct timeval *timeout = new timeval;
					timeout->tv_sec=TIMEOUT_SEC;
					timeout->tv_usec=0;
					FD_ZERO(&readfds);
					FD_SET(clientSocket,&readfds);
					int i = 1;
					if(!ack)
						i = select(1,&readfds,NULL,NULL,timeout);
					if(i)
					{
						if((ibytesrecv = recvfrom(clientSocket,recvbuff,packetLengthInBytes+extrabytes,0,(LPSOCKADDR)&serverSocketAddr,&socketlen))==SOCKET_ERROR)
							throw RECV_FAILED_MSG;
						logEvents("GET","Data received from server \n"+ string(recvbuff));
						rcvdpktnumberforclient = stoi(string(recvbuff).substr(0,1).c_str());
						rcvdpktnumberforserver = stoi(string(recvbuff).substr(2,3).c_str());
						logEvents("GET","rcvdpktnumberforclient: "+ to_string(rcvdpktnumberforclient));
						logEvents("GET","rcvdpktnumberforserver:"+ to_string(rcvdpktnumberforserver));
						if(checkSequence(clientpktseq,rcvdpktnumberforclient))
						{
							ack = true;
							cout<<"Received packet sequence from server"<<rcvdpktnumberforclient<<endl;
							logEvents("GET","Received packet sequence from server" +to_string(rcvdpktnumberforclient));
							bool expectedserverpkt = checkSequence(serverpktseq,rcvdpktnumberforserver);
							sendAck(clientSocket,serverSocketAddr,rcvdpktnumberforserver,socketlen);
							if(expectedserverpkt)
							{
								++writeCounter;
								cout<<"Total packets received "<<writeCounter<<endl;
								logEvents("Get","Total packets received "+to_string(writeCounter));
								char *writebuff = recvbuff+extrabytes; //this will ignore the first byte
								totalBytes = totalBytes + ibytesrecv;
								  logEvents("Break ","---------------------------------------------------------------------");
									logEvents("GET", "Bytes received from server in "+ to_string(writeCounter) +string("request: ") +  to_string(ibytesrecv));
								if(string(writebuff).find("FIN")==0)
								{
									time_t end = time(0);
									cout<<"Received "<<to_string(writeCounter)<<" packets for "<<totalBytes<< " bytes in "<<difftime(end,start)<<" seconds"<<endl;
									logEvents("GET","Transfer complete. Received "+to_string(writeCounter) +" packets for " + to_string(totalBytes) +" bytes in " + to_string(difftime(end,start)) +" seconds");
									transfercomplete = true;
									clientpktseq = (clientpktseq + 1) % MAX_PACKET_SEQ;
								}
								else
								{
									file.write(writebuff, ibytesrecv-extrabytes);
								}
							}
						}
						else
						{
							cout<<"Discarding the wrong ACK"<<endl;
							cout<<"Expecting ACK for packet "<<clientpktseq<<endl;
							cout<<"Received ACK for packet "<<rcvdpktnumberforclient<<endl;
							if(ack)
								sendAck(clientSocket,serverSocketAddr,rcvdpktnumberforserver,socketlen);
							else
								if(sendto(clientSocket,szbuffer,strlen(szbuffer),0,(LPSOCKADDR)&serverSocketAddr,socketlen) ==SOCKET_ERROR)
									throw SEND_FAILED_MSG;
							logEvents("Client","Discarding the wrong ACK.\nExpecting ACK for packet:"+to_string(clientpktseq)+"\nReceived ACK for packet: "+to_string(rcvdpktnumberforclient));
						}
					}
					else
					{
						cout<<"Request timed out waiting for ACK for packet " + to_string(clientpktseq)<<endl;
						logEvents("GET", "Request timed out waiting for ACK for packet " + to_string(clientpktseq));
						if(ack)
							sendAck(clientSocket,serverSocketAddr,rcvdpktnumberforserver,socketlen);
						else
							if(sendto(clientSocket,szbuffer,strlen(szbuffer),0,(LPSOCKADDR)&serverSocketAddr,socketlen) ==SOCKET_ERROR)
								throw SEND_FAILED_MSG;

					}
				}


			}
		}
		else
		{
			cout<<"Error occured at client: "<<strerror(errno)<<endl;
			logEvents("GET","Error occured at client: "+string(strerror(errno)));
		}
		file.close();
	}
	catch(char* str)
	{
		LPTSTR Error = 0;
		rcvdacknowledgementmsg = '\0';
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() ,0,(LPTSTR)&Error,0,NULL) == 0)
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
void ftpPUT(string argument)
{
	try
	{
		int bytesRead = 0;
		long int remainingBytesToSend = 0;
		long int fsize = 0;

		logEvents("PUT", "Opening filestream ...");

		ifstream ifs(argument, ios::in | ios::binary);
		if(ifs)
		{
			ifs.seekg (0, ifs.end);
			fsize = ifs.tellg();
			ifs.seekg (0, ifs.beg);
			char size[15]={};

			if(!threewayhandshakecomplete)
			{

				string commandToServer = rcvdacknowledgementmsg+string("PUT") + " "+argument+" "+ltoa(fsize,size,10);
				sprintf(szbuffer,commandToServer.c_str());
			}
			else		
			{
				string commandToServer = to_string(clientpktseq)+string("PUT") + " "+argument+" "+ltoa(fsize,size,10);
				sprintf(szbuffer,commandToServer.c_str());
			}

			sendRequest(clientSocket, serverSocketAddr, szbuffer, clientpktseq, socketlen, strlen(szbuffer));

			if(!threewayhandshakecomplete)
			{
				threewayhandshakecomplete = true;

				cout<<"Connection Established"<<endl;
				logEvents("CLIENT", "Threeway handshake with server complete.");
			}
			if(rcvdacknowledgementmsg.find("1")==0)
			{
				remainingBytesToSend = fsize;
				time_t start = time(0);
				cout<<"Uploading file to server..."<<endl;
				logEvents("PUT", "Uploading file to server...");
				int readCounter = 0;
				char *sendbuff;
				int totalBytes = 0;
				while(remainingBytesToSend > 0)
				{
					int bytesToSend = packetLengthInBytes < remainingBytesToSend ? packetLengthInBytes :remainingBytesToSend;
					sendbuff = new char[bytesToSend];
					memset(sendbuff,'\0',bytesToSend);
					ifs.read(sendbuff, bytesToSend);	
					char *tempbuff = new char[bytesToSend+1];
					memset(tempbuff,'\0',bytesToSend+1);
					itoa(clientpktseq,tempbuff,10);
					memcpy(tempbuff+1,sendbuff,bytesToSend);
					logEvents("Debug",tempbuff);
					logEvents("Break ","---------------------------------------------------------------------");

					ibytessent = sendRequest(clientSocket,serverSocketAddr,tempbuff,clientpktseq,socketlen, bytesToSend+1);

					++readCounter;
					remainingBytesToSend = remainingBytesToSend - bytesToSend;
					totalBytes = totalBytes + ibytessent;
				}
				time_t end = time(0);

				cout<<"Sent "<<to_string(readCounter)<<" packets for "<<fsize<< " bytes in "<<difftime(end,start)<<" seconds"<<endl;
			logEvents("GET","Transfer complete. Sent "+to_string(readCounter) +" packets for " + to_string(fsize) +" bytes in " + to_string(difftime(end,start)) +" seconds");
	
			}
			else
			{
				cout<<"Error from server: "<<rcvdacknowledgementmsg.erase(0,1)<<endl;
				logEvents("ERROR", "Error from server: "+rcvdacknowledgementmsg.erase(0,1));
			}
		}
		else
		{
			cout<<strerror(errno)<<endl;
			logEvents("ERROR", strerror(errno) + commandLineTokens[1]);
		}
		ifs.close();
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
void ftpLIST(string commandToServer)
{
	try
	{
		const int maxByteToRead = 1024;
		char *buffer = new char[maxByteToRead+1];
		memset(buffer,'\0',maxByteToRead+1);

		if(!threewayhandshakecomplete)
			sprintf(buffer,commandToServer.insert(0,rcvdacknowledgementmsg).c_str());
		else
		{
			commandToServer = to_string(clientpktseq)+commandToServer;
			sprintf(buffer,commandToServer.c_str());
		}

		if(!threewayhandshakecomplete)
		{
			threewayhandshakecomplete = true;
			cout<<"Connection Established"<<endl;
			logEvents("CLIENT", "Threeway handshake with server complete.");
		}
		else
			logEvents("LIST","Sending request.");
		bool ack = false;

		bool transfercomplete = false;
		int rcvdpktnumberforclient = -1;
		int rcvdpktnumberforserver = -1 ;
		while(!(ack && transfercomplete))
		{
			if(!ack)
			{
				if(sendto(clientSocket,buffer,strlen(buffer),0,(LPSOCKADDR)&serverSocketAddr,socketlen) ==SOCKET_ERROR)
					throw SEND_FAILED_MSG;
				logEvents("List","Sent request to server: "+string(buffer));
			}

			correctpktrcvd  = false;
			while(!correctpktrcvd)
			{
				//				ack = false;
				struct timeval *timeout = new timeval;
				timeout->tv_sec=TIMEOUT_SEC;
				timeout->tv_usec=0;
				FD_ZERO(&readfds);
				FD_SET(clientSocket,&readfds);
				int i = 1;
				if(!ack)
					i = select(1,&readfds,NULL,NULL,timeout);
				if(i)
				{
					logEvents("List","Waiting to recv data from server.");
					memset(buffer,'\0',packetLengthInBytes);
					if(recvfrom(clientSocket,buffer,packetLengthInBytes,0,(LPSOCKADDR)&serverSocketAddr,&socketlen)==SOCKET_ERROR)
						throw RECV_FAILED_MSG;
					logEvents("LIST","Data received from server \n"+ string(buffer));
					rcvdpktnumberforclient = string(buffer).at(0) - 48;
					rcvdpktnumberforserver = string(buffer).at(1) - 48;
		//			logEvents("LIST","rcvdpktnumberforclient: "+ to_string(rcvdpktnumberforclient));
		//			logEvents("LIST","rcvdpktnumberforserver:"+ to_string(rcvdpktnumberforserver));
					if(checkSequence(clientpktseq,rcvdpktnumberforclient))
					{
						ack = true;
						cout<<"Received packet sequence from server"<<rcvdpktnumberforclient<<endl;
						logEvents("List","Received packet sequence from server" +to_string(rcvdpktnumberforclient));
						bool expectedserverpkt = checkSequence(serverpktseq,rcvdpktnumberforserver);

						sendAck(clientSocket,serverSocketAddr,rcvdpktnumberforserver,socketlen);

						if(expectedserverpkt)
						{
							if(string(buffer).find("FIN")==2)
							{
								logEvents("List","LIST request complete.");
								transfercomplete = true;
								clientpktseq = (clientpktseq + 1) % MAX_PACKET_SEQ;
							}
							else if(string(buffer).erase(0,2).find("-1") == 0)
							{
								logEvents("List",string(buffer).erase(0,4));
								cout<<string(buffer).erase(0,4)<<endl;
								logEvents("List",string(buffer).erase(0,4));
								transfercomplete = true;
								clientpktseq = (clientpktseq + 1) % MAX_PACKET_SEQ;
							}
							else
							{
								cout<<string(buffer).erase(0,2);
							}
						}
					}
					else
					{
						cout<<"Discarding the wrong ACK"<<endl;
						cout<<"Expecting ACK for packet "<<clientpktseq<<endl;
						cout<<"Received ACK for packet "<<rcvdpktnumberforclient<<endl;
						if(ack)
							sendAck(clientSocket,serverSocketAddr,rcvdpktnumberforserver,socketlen);
						else
							if(sendto(clientSocket,buffer,strlen(buffer),0,(LPSOCKADDR)&serverSocketAddr,socketlen) ==SOCKET_ERROR)
								throw SEND_FAILED_MSG;
						logEvents("List","Discarding the wrong ACK.\nExpecting ACK for packet:"+to_string(clientpktseq)+"\nReceived ACK for packet: "+to_string(rcvdpktnumberforclient));
					}
				}
				else
				{
					cout<<"Request timed out waiting for ACK for packet " + to_string(clientpktseq)<<endl;
					logEvents("List", "Request timed out waiting for ACK for packet " + to_string(clientpktseq));
					if(ack)
						sendAck(clientSocket,serverSocketAddr,rcvdpktnumberforserver,socketlen);
					else
						if(sendto(clientSocket,buffer,strlen(buffer),0,(LPSOCKADDR)&serverSocketAddr,socketlen) ==SOCKET_ERROR)
							throw SEND_FAILED_MSG;

				}
			}
		}
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
void ftpPWD()
{
	try
	{
		if((ibytessent=send(clientSocket,szbuffer, packetLengthInBytes,0)) == SOCKET_ERROR)
			throw SEND_FAILED_MSG;
		memset(szbuffer,'\0',packetLengthInBytes);
		if(recv(clientSocket,szbuffer,packetLengthInBytes,0)== SOCKET_ERROR)
			throw RECV_FAILED_MSG;
		if(string(szbuffer).find(1)==0)
			cout<<"Current working directory is: "<<string(szbuffer).erase(0,1)<<endl;
		else
			cout<<string(szbuffer).erase(0,1)<<endl;
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
void ftpLCD(string directory)
{
	try
	{
		char pwd[MAX_PATH];
		GetCurrentDirectory(MAX_PATH, pwd);
		if(strlen(directory.c_str()) == 0)
		{
			cout<<"Current directory is : "<<endl<<pwd<<endl;
			logEvents("CD", "Current directory is : "+ string(pwd));
		}
		else 
		{
			if(!SetCurrentDirectory(directory.c_str()))
			{
				cout<<"System cannot find the specified directory."<<endl;
				logEvents("CD", "System cannot find the specified directory.");
			}
			else
			{
				cout<<"Directory changed to :"<<endl<<directory<<endl;
				logEvents("CD", "Directory changed to : " + directory);
			}
		}
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
void ftpCD(string commandToServer)
{
	try
	{
		char *buffer = new char[packetLengthInBytes];
		memset(buffer,'\0',packetLengthInBytes);
		cout<<commandToServer<<endl;
		commandToServer = to_string(clientpktseq)+commandToServer;
		cout<<commandToServer<<endl;
		sprintf(buffer,commandToServer.c_str());

		if(sendRequest(clientSocket,serverSocketAddr,buffer,clientpktseq,socketlen,strlen(buffer)) ==SOCKET_ERROR)
			throw SEND_FAILED_MSG;

		if(rcvdacknowledgementmsg.find("1")==0)
		{
			cout<<rcvdacknowledgementmsg.erase(0,1)<<endl;
			logEvents("CD", rcvdacknowledgementmsg.erase(0,1));
		}
		else
		{
			cout<<rcvdacknowledgementmsg.erase(0,2)<<endl;
			logEvents("CD", rcvdacknowledgementmsg.erase(0,1));
		}
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
	rcvdacknowledgementmsg = '\0';
}
void ftpDELETE(string file)
{
	try
	{
		if(!threewayhandshakecomplete)
		{
			string commandToServer = rcvdacknowledgementmsg+string(szbuffer);
			sprintf(szbuffer,commandToServer.c_str());
		}
		else
		{
			string commandToServer = to_string(clientpktseq)+string(szbuffer);
			sprintf(szbuffer,commandToServer.c_str());
		}

		if(!threewayhandshakecomplete)
		{
			threewayhandshakecomplete = true;
			cout<<"Connection Established"<<endl;
			logEvents("CLIENT", "Threeway handshake with server complete.");
		}

		if(sendRequest(clientSocket, serverSocketAddr, szbuffer, clientpktseq, socketlen, strlen(szbuffer)) == SOCKET_ERROR)
			throw SEND_FAILED_MSG;
		
		cout<<rcvdacknowledgementmsg.erase(0,1)<<endl;
		logEvents("Client",rcvdacknowledgementmsg.erase(0,1));
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
void ftpQUIT()
{
	try
	{
		if(send(clientSocket,szbuffer,128,0) == SOCKET_ERROR)
			throw SEND_FAILED_MSG;
		logEvents("QUIT", "Bye.... ");
		cleanUp();
	}
	catch(char* str)
	{
		LPTSTR Error = 0;
		if(FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,NULL,WSAGetLastError() | GetLastError(),0,(LPTSTR)&Error,0,NULL) == 0)
		{
			//cout<<str<<endl;
			logEvents("ERROR", str +string(" Failed to format error message"));
			logEvents("QUIT", "Bye.... ");
		}
		else
		{
			cerr<<Error<<endl;
			logEvents("ERROR", Error);
			logEvents("QUIT", "Bye.... ");
		}
		LocalFree(Error);
		cleanUp();
	}

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
		logFile.open("c:\\logs\\clientlog.txt",ios::out | ios::app);
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
			logEvents("ERROR", " Failed to format error message.");
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
	logEvents("Client","Waiting for sequence "+to_string(serverpktseq));
	logEvents("Client","Received sequence "+to_string(pktnumberrcvd));
	correctpktrcvd = checkSequence(serverpktseq,pktnumberrcvd);
	//itoa((correctpktrcvd ? pktnumberrcvd :serverpktseq),sendbuffer,10);
	itoa(pktnumberrcvd ,sendbuffer,10);
	strcat(sendbuffer,sentacknowledgementmsg.c_str());

	if(sendto(socket,sendbuffer,packetLengthInBytes,0,(LPSOCKADDR)&socketaddr,socketlength) ==  SOCKET_ERROR)
		throw SEND_FAILED_MSG;
	cout<<"Sent ACK for packet sequence to server: "<<sendbuffer<<endl;
	logEvents("Client","ACK Data sent to server \n"+ string(sendbuffer));
	if(correctpktrcvd)
	{
		logEvents("Client","Sending ACK for packet "+ to_string(pktnumberrcvd));
		serverpktseq = (serverpktseq+1) % MAX_PACKET_SEQ;
	}
	else
		logEvents("Client","Received wrong packet."+ to_string(pktnumberrcvd));
}
bool checkSequence(int previouspktnumber, int ackrcvdforpkt)
{
	return previouspktnumber == ackrcvdforpkt ? true :false;
}

int sendRequest(SOCKET socket , SOCKADDR_IN socketaddr, char *sendbuffer, int packetseq, int socketlength, int bytestosend)
{
	int retrycount = 0;
	bool acknowledged = false;

	while(!acknowledged)
	{
		if(sendto(socket,sendbuffer, bytestosend,0,(LPSOCKADDR)&socketaddr,socketlength) == SOCKET_ERROR)
			throw SEND_FAILED_MSG;
		cout<<"Data sent to server: "<<sendbuffer<<endl;
		cout<<"Sent request packet sequence to server: "<<packetseq<<endl;
		logEvents("Client","Data sent to server \n"+ string(sendbuffer));
		logEvents("Client","Sent request packet sequence to server:"+ to_string(packetseq));
		if(receiveAck( socket , socketaddr, packetseq, socketlength))
		{
			cout<<"Received ACK for packet "+to_string(packetseq)<<endl;
			logEvents("Client", "Received ACK for packet "+to_string(packetseq));
			acknowledged = true;
		}
		else if(retrycount++ == MAX_RETRIES)
		{
			cout<<"No of retries exchausted"<<endl;
			throw "No of retries exchausted.";
		} 
	}
	return ibytessent;
}
bool receiveAck(SOCKET socket , SOCKADDR_IN socketaddr, int packetseq, int socketlength)
{
	char recvbuffer[packetLengthInBytes]; 
	int ackrcvdforpkt = 0;
	memset(recvbuffer,'\0',packetLengthInBytes);

	struct timeval *timeout = new timeval;
	timeout->tv_sec=TIMEOUT_SEC;
	timeout->tv_usec=0;
	FD_ZERO(&readfds);
	FD_SET(socket,&readfds);

	int i = select(1,&readfds,NULL,NULL,timeout);
	if(i)
	{
		if(recvfrom(socket,recvbuffer, packetLengthInBytes,0,(LPSOCKADDR)&socketaddr,&socketlength) == SOCKET_ERROR)
			throw RECV_FAILED_MSG;
		logEvents("Client","ACK Data received from server \n"+ string(recvbuffer));	
		if(!firstlegcomplete)
			ackrcvdforpkt = atoi(string(recvbuffer).substr(0,to_string(packetseq).length()).c_str());
		else
			ackrcvdforpkt = string(recvbuffer).at(0) - 48;

		cout<<"Received ACK for packet sequence from server: "<<ackrcvdforpkt<<endl;
		if(checkSequence(packetseq,ackrcvdforpkt))
		{
			if(strlen(recvbuffer)>=1)
			{
				if(!firstlegcomplete)
				{
					rcvdacknowledgementmsg = string(recvbuffer).substr(to_string(packetseq).length(),strlen(recvbuffer)).c_str();
					firstlegcomplete = true;
				}
				else
				{
					rcvdacknowledgementmsg = string(recvbuffer).erase(0,1);
					clientpktseq = (clientpktseq +1) % MAX_PACKET_SEQ;
				}
			}
			return true;
		}
		else
		{
			cout<<"Discarding the wrong ACK"<<endl;
			cout<<"Expecting ACK for packet "<<packetseq<<endl;
			cout<<"Received ACK for packet "<<ackrcvdforpkt<<endl;
			logEvents("Client","Discarding the wrong ACK.\nExpecting ACK for packet:"+to_string(packetseq)+"\nReceived ACK for packet: "+to_string(ackrcvdforpkt));
			return false;
		}
	}
	else
	{
		cout<<"Request timed out waiting for ACK for packet " + to_string(packetseq)<<endl;
		logEvents("Client", "Request timed out waiting for ACK for packet " + to_string(packetseq));
		return false;
	}
}

void cleanUp()
{
	logEvents("CLIENT", "Closing connection");
	closesocket(clientSocket);
	logEvents("CLIENT", "Connection closed. Performing cleanup.");
	/* When done uninstall winsock.dll (WSACleanup()) and exit */
	WSACleanup();
	logEvents("CLIENT", "Cleanup done. Exiting...");
}
