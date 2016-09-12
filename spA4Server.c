/*|=====================================================================================|*/
/*| FILE :          spA3Server.c                                                        |*/
/*|                                                                                     |*/
/*| PROJECT :       PROG1970 - SP Assignment #4                                    	|*/
/*|                                                                                     |*/
/*| PROGRAMMER :    Geun Young Gil  (STD Num. 6944920)					|*/ 
/*|                                                                                     |*/
/*| FIRST VERSION : Apr. 17. 2015                                                       |*/
/*|                                                                                     |*/
/*| DESCRIPTION :   This program is Central Server to connect among clents for chatting.|*/ 
/*|		    It has a special thread for word game. and make threads for all clients*/
/*|		    									|*/
/*|=====================================================================================|*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>	// struct hostent, gethostbyname()
#include <netinet/in.h>	// struct sockaddr_in
#include <arpa/inet.h>	// htons()
#include <sys/socket.h>	// socket(), connect()
#include <sys/types.h>	
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>	// wait3()
#include <signal.h>
#include <pthread.h>	// pthread
#include <unistd.h>

#define PORT 5000
#define MAX_CLIENTS  10
#define MAX_INET_ADDR_LEN 16
#define MAX_Q_LEN 45
#define MAX_ANS_LEN 12
#define MAX_ANS_NUM 55

// client Info
typedef struct clientInfo{
	int socket;		// client socket number
	struct in_addr inetAddr;// ithernet address
	pthread_t tid;
	int score;
} clientInfo;

// prototype
void alarmHandler(int sigNum);
void watchKids (int n);
int findClient(int clientSocket);
void* clientThread(void* data);
void clearCRLF(char* str);
void clearSPACE(char* str);
void *wordGameThread(void* data);
void broadCastMSG(char* msg, int mode);

// Global var
static int nClients = 0;	// the clients connected to server
static int nNoConnect = 0;	// the time with no connection (1 for 1 min)
clientInfo clientList[MAX_CLIENTS];
FILE* ifp = NULL;
int playTime = 120;		// default play time is 2 min
int gameStatus = -1;		// -1: game wait client, 0: game started, 1: game prepare for next game, -2: exit
char question[MAX_Q_LEN] = {0};	// question
char answer[MAX_ANS_NUM][MAX_ANS_LEN] = {0};	// answers
int isFound[MAX_ANS_NUM] = {0};	// whether the answer is found or not 

int main(int argc, char* argv[])
{
	int serverSocket = -1, clientSocket = -1;
	int clientLen = 0;
	int clientInTable = 0;
	int retFork = 0;
	int cnt = 0;
	struct sockaddr_in serverAddr, clientAddr;
	pthread_t tid;

	// check command argument
	if ((argc == 2) || (argc == 3))
	{
		if ( (ifp = fopen(argv[1],"r")) == NULL )
		{
			printf("Error!!: File not openable.\n");
			return 0;
		}

		if (argc == 3)
		{
			playTime = atoi(argv[2]);
		}
	}
	else
	{
		printf("Error!!: USAGE: ./spA4Server <filename> <play time>\n");
		return 0;	
	}


	// inicialize clientsList
	for (cnt = 0; cnt < MAX_CLIENTS; ++cnt)
	{
		clientList[cnt].socket = -1;
		clientList[cnt].inetAddr.s_addr = 0;
		clientList[cnt].tid = 0;
		clientList[cnt].score = 0;
	}

	// install signal hadlers
	printf("[SERVER] Installing signal handler for ZOMBIE processes and WATCHDOG...... ");
	fflush(stdout);
	signal (SIGALRM, alarmHandler);
	alarm (30);
	printf("done.\n");
	fflush(stdout);

	// create socket for server
	printf("[SERVER] Getting socket for Server...... ");
	fflush(stdout);
	if ((serverSocket = socket (AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("Failed!!\n");
		return 1;
	}
	printf("done.\n");
	fflush(stdout);

	// put server info
	memset (&serverAddr, 0, sizeof (serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = htonl (INADDR_ANY);	//all interface
	serverAddr.sin_port = htons (PORT);

	// socket binding
	printf("[SERVER] Binding socket for Server...... ");
	fflush(stdout);
	if (bind (serverSocket, (struct sockaddr*)&serverAddr, sizeof (serverAddr)) < 0)
	{
		
		printf("Failed!!\n");
		close (serverSocket);
		return 2;
	}
	printf("done.\n");
	fflush(stdout);

	// listen the sockets
	printf("[SERVER] Prepareing to listen sockets for Server...... ");
	fflush(stdout);
	if (listen (serverSocket, 5) < 0)
	{
		printf("Failed!!\n");
		close (serverSocket);
		return 3;	
	}
	printf("done.\n");
	fflush(stdout);	

	// looping for accepting clients -> this part only is used by parent until meeting fork()
	while(1)
	{
		// accept client packets 
		printf("[SERVER] Accepting Clients...... ");
		fflush(stdout);
		clientLen = sizeof (clientAddr);
		memset (&clientAddr, 0, clientLen);
		if ( (clientSocket = accept (serverSocket, (struct sockaddr*)&clientAddr, &clientLen)) < 0)
		{
			printf("Failed!!\n");
			close (serverSocket);
			return 4;
		}
		printf("done.\n");
		fflush(stdout);	

		// Add new client in client list
		printf("[SERVER] Adding new Client...... ");
		fflush(stdout);	
		clientInTable = findClient(-1);	// find empty entity in list
		// Server is full
		if (clientInTable == -1)
		{
			printf("Failed. Server is full.\n");
			fflush(stdout);	
			close (clientSocket);
		}
		// Server is not full
		else
		{	
			// creat new thread for new client
			if ( pthread_create(&tid, NULL, clientThread, NULL) != 0)
			{
				close (clientSocket);
				printf("Failed. Can't make new Thread.\n");
				fflush(stdout);
			}
			else
			{
				clientList[clientInTable].socket = clientSocket;
				clientList[clientInTable].inetAddr.s_addr = clientAddr.sin_addr.s_addr;
				clientList[clientInTable].tid = tid;
				++nClients;
				printf("Done.\n");
				fflush(stdout);
			}		
		} // end if-else - server is full or not
		// print client list
		printf("\n< CLIENT TABLE >\n");
		fflush(stdout);
		printf("%-9s %-16s \n","Sock Num.","IP addr");
		fflush(stdout);
		
		for (cnt = 0; cnt < MAX_CLIENTS; ++cnt)
		{
			if (clientList[cnt].socket >= 0)
			{
				printf("%-9d %-16s \n", clientList[cnt].socket, inet_ntoa(clientList[cnt].inetAddr));
			}
		}
		printf("\n< END >\n\n");
		fflush(stdout);

	} // end while - accept clients

	return 0;
} 

/*|=====================================================================================|*/
/*|   FUNCTION   :  clientThread                                                   	|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  thread to get message from only one specify client and broadcast 	|*/
/*|		    that message to other clients including me.				|*/
/*|		    If the message is request to server, server process that message	|*/
/*|		    and send back to only source client.				|*/				
/*|                                                                                     |*/
/*|   PARAMETERS:   void*        	nothing            				|*/
/*|											|*/
/*|   RETURN    :   void* 		NULL						|*/
/*|=====================================================================================|*/
void* clientThread(void* data)
{
	char buffer[BUFSIZ] = {0};
	char msgME[BUFSIZ] = {0};
	char msgOther[BUFSIZ] = {0};
	int cnt = 0;
	int myIndex = -1; 	// my index in clientList 
	int mySocket = -1;
	struct in_addr myInetAddr;
	pthread_t myTid = pthread_self();	// get my tid
	
	// find my index in clientList (wait the time to update clientList)
	for (;cnt < 3; ++cnt)
	{
		sleep(1);	
		if ( (myIndex = findTID(myTid) ) != -1)
		{
			mySocket = clientList[myIndex].socket;
			myInetAddr = clientList[myIndex].inetAddr;
			break;
		}
	}
	
	// fail to find clientList
	if (myIndex == -1)
	{
		printf("[SERVER-new Thread] Faild to get thread info from client list...... Terminated!!!\n");
		return NULL;
	}
	
	printf("[SERVER-Sock#%d] Start new thread for new Client.\n",mySocket);
	fflush(stdout);	
	
	while(1)
	{
		// read packet
		memset(buffer, 0, BUFSIZ);
		read(mySocket, buffer, BUFSIZ);
		printf("[SERVER-Sock#%d] Received message.\n",mySocket);
		fflush(stdout);	
					
		// special command
		if( *buffer == '\\')
		{
			if ( strcmp (buffer + 1 ,"exit") == 0 )
			{
				printf("[SERVER-Sock#%d] A client is disconnected.\n",mySocket);
				fflush(stdout);	
				break;
			}
			else if (strcmp(buffer + 1,"score") == 0)
			{
				broadCastMSG(NULL, mySocket);
			}
			else if (strcmp(buffer + 1,"question") == 0)
			{
				memset (buffer, 0, BUFSIZ);
				strcpy (buffer, question);
				strcat (buffer, "\n");
				write(mySocket, buffer, BUFSIZ);
			}
			else
			{
				memset (buffer, 0, BUFSIZ);
				strcpy (buffer, "[SERVER MESSAGE] No information for the request or Wrong Command!\n");
				write(mySocket, buffer, BUFSIZ);
			}
		}
		// get chatting message
		else
		{
			// game is started
			if (gameStatus == 0)
			{
				int i = 0;
				int isAnswer = 0;
				while ( i < MAX_ANS_NUM )
				{
					if (strcmp(answer[i],buffer) == 0)
					{
						isAnswer = 1;
						memset (buffer, 0, BUFSIZ);

						if (isFound[i] == 0)
						{
							isFound[i] = 1;	
							// calculate score	
							clientList[myIndex].score += strlen (answer[i]);				
							strcpy (buffer, "[SERVER MESSAGE] You got it.\n");
						}
						else
						{
							strcpy (buffer, "[SERVER MESSAGE] Already got it.\n");
						}
						
						write(mySocket, buffer, BUFSIZ);
						break;
					}
					else if (answer[i] == NULL)
					{
						break;
					}

					++i;
				} 

				if (isAnswer == 0)
				{
					memset (buffer, 0, BUFSIZ);
					strcpy (buffer, "[SERVER MESSAGE] Wrong Answer.\n");
					write(mySocket, buffer, BUFSIZ);		
				}
			}
			// next game is being prepared.
			else if (gameStatus == 1)
			{
				memset (buffer, 0, BUFSIZ);
				strcpy (buffer, "[SERVER MESSAGE] Next game is being prepared next game.\n");
				write(mySocket, buffer, BUFSIZ);
			}
			// Tornament is ended.
			else
			{
				memset (buffer, 0, BUFSIZ);
				strcpy (buffer, "[SERVER MESSAGE] All games are ended. please disconnect from server.\n");
				write(mySocket, buffer, BUFSIZ);
			}
		}
	} // end while in child

	// delete a disconnected client
	printf("[SERVER-Sock#%d] Closing disconnected socket and deleting from client list...... ",mySocket);
	fflush(stdout);
	clientList[myIndex].socket = -1;
	clientList[myIndex].inetAddr.s_addr = 0;
	clientList[myIndex].tid = 0;
	close (mySocket);
	--nClients;
	printf("Done.\n");
	fflush(stdout);
				
	return NULL;
}

/*|=====================================================================================|*/
/*|   FUNCTION   :  wordGameThread                                                   	|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  thread to start word game. manage the game schedule.		|*/				
/*|                                                                                     |*/
/*|   PARAMETERS:   void*        	nothing            				|*/
/*|											|*/
/*|   RETURN    :   void* 		NULL						|*/
/*|=====================================================================================|*/
void *wordGameThread(void* data)
{
	// loop until the end of file
	while (!feof(ifp))
	{
		memset(question, 0, MAX_Q_LEN);
		fgets(question,MAX_Q_LEN, ifp);		// read question
		
		// read answers		
		int i = 0;
		int isEndAns = 0;
		for (;i < MAX_ANS_NUM; ++i)
		{
			memset(answer[i], 0, MAX_ANS_LEN);
			isFound[i] = 0;
			
			if (isEndAns == 0)
			{
				fgets(answer[i], MAX_ANS_LEN, ifp);
				clearCRLF(answer[i]);
				clearSPACE(answer[i]);
				
				if ( (strcmp(answer[i],"") == 0) || feof(ifp) )
				{
					isEndAns = 1;
				}
			}			
		}
		for (i = 0; i < MAX_CLIENTS; ++i)
		{
			clientList[i].score = 0;
		}

		sleep(5);
		gameStatus = 0;
		broadCastMSG("[Server] Word Game is started!! \n", -1);
		broadCastMSG(question,-1);
		broadCastMSG("\n", -1);
		sleep(playTime);
		gameStatus = 1;
		broadCastMSG("[Server] Word Game is ended!! \n", -1);
		broadCastMSG(NULL, 0);
		sleep(2);
	}
	
	gameStatus = -2;	// exit
	broadCastMSG("[Server] Word Tournament is ended.\n", -1);
	exit(-1);
}

/*|=====================================================================================|*/
/*|   FUNCTION   :  broadCastMSG                                                    	|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  broadcast message to all clients. - mode -1				|*/
/*|		    broadcast score board to all clients - mode 0			|*/
/*|		    broadcast score board to specific socket mode > 0			|*/
/*|                                                                                     |*/
/*|   PARAMETERS:   const char* msg     : only used for mode -1        			|*/
/*| 		    int         mode 	: -1: broadcast message, 0: broadcast score	|*/
/*|					: socket number to send score			|*/
/*|											|*/
/*|   RETURN    :   void								|*/
/*|=====================================================================================|*/
void broadCastMSG(char* msg, int mode)
{		
	int cnt = 0;	
	char scoreBoard[200] = {0};

	if (mode > -1)
	{
		sprintf(scoreBoard, "\n<SCORE BOARD>\n\n");
	}
		
	for(; cnt < MAX_CLIENTS; ++cnt)
	{
		// find connected client
		if (clientList[cnt].socket >= 0)
		{
			// broadcast message mode
			if (mode == -1)
			{
				write (clientList[cnt].socket, msg, strlen (msg));
			}
			// score mode, append scores
			else if(mode > -1)
			{
				sprintf(scoreBoard + strlen (scoreBoard), "%s]\t%d\n",inet_ntoa (clientList[cnt].inetAddr),clientList[cnt].score);
			}
			
		}
	}
	
	if (mode > -1)
	{
		sprintf(scoreBoard + strlen (scoreBoard), "\n<End of Board>\n");
		if (mode == 0)
		{
			for(cnt = 0; cnt < MAX_CLIENTS; ++cnt)
			{
				if (clientList[cnt].socket >= 0)
				{
					write (clientList[cnt].socket, scoreBoard, strlen (scoreBoard));
				}
			}
		}
		else
		{
			write(mode, scoreBoard,strlen (scoreBoard));
		}
	}

}

/*|=====================================================================================|*/
/*|   FUNCTION   :  findClient                                                    	|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  find the client from clientList by using socket and return index num|*/
/*|                                                                                     |*/
/*|   PARAMETERS:   int        	clientSocket            				|*/
/*|											|*/
/*|   RETURN    :   int 	the index num of clientList or -1 (no client)		|*/
/*|=====================================================================================|*/
int findClient(int clientSocket)
{
	int retCode = -1;
	int i = 0;

	for(; i < MAX_CLIENTS;	++i)
	{
		if (clientList[i].socket == clientSocket)
		{
			retCode = i;
			break;
		} 
	}

	return retCode;
}
/*|=====================================================================================|*/
/*|   FUNCTION   :  findTID                                                    		|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  find the client from clientList by using tid and return index num	|*/
/*|                                                                                     |*/
/*|   PARAMETERS:   pthread_t 	tid            						|*/
/*|											|*/
/*|   RETURN    :   int 	the index num of clientList or -1 (no client)		|*/
/*|=====================================================================================|*/
int findTID(pthread_t tid)
{
	int retCode = -1;
	int i = 0;

	for(; i < MAX_CLIENTS;	++i)
	{
		if (pthread_equal(tid, clientList[i].tid) != 0)
		{
			retCode = i;
			break;
		} 
	}

	return retCode;
}

/*|=====================================================================================|*/
/*|   FUNCTION   :  alarmHandler                                                    	|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  It's a watch dog timer, This detect no connections with clients.	|*/
/*|		    If it go over 5 minutes without connection, it terminate server.	|*/
/*|		    And make thread for word game.					|*/
/*|                                                                                     |*/
/*|   PARAMETERS:   int        	signal_number   :   signal number to re-define         	|*/
/*|											|*/
/*|   RETURN    :   void						                |*/
/*|=====================================================================================|*/
void alarmHandler(int sigNum)
{
	printf("[SERVER-Alarm] %d clients exist now.\n",nClients);
	fflush(stdout);	
				
	pthread_t tid;
	alarm(30);	// reset alarm to 30 seconds
	// if there are no connection
	if (nClients == 0)
	{
		if ( (nNoConnect > 0) && ((nNoConnect % 30) == 0) )
		{
			printf ("[Server Watch Dog] No connection with clients for %d minute(s) %d seconds! \n",nNoConnect / 60, nNoConnect % 60);
			fflush(stdout);

		}
		nNoConnect += 30;
	}
	// occur connections
	else
	{
		nNoConnect = 0;
	}
	
	if ( (nNoConnect >= 120) && (nClients == 0) )
	{
		printf("\n");
		printf ("[Server Watch Dog] No connection with clients for %d minutes! \n", nNoConnect / 60);
		printf ("[Server Watch Dog] Automatically terminating program!\n");
		exit (-1);
	}

	// start word game
	if ( (gameStatus == -1) && (nClients > 0))
	{
		// creat new thread for new client
		if ( pthread_create(&tid, NULL, wordGameThread, NULL) != 0)
		{
			printf("[Server] Error!! Can't make new Thread for Word Game.\n");
			fflush(stdout);
		}
		else
		{
			gameStatus = 1;
		}
	} 

	signal(sigNum,alarmHandler);
}

/*|=====================================================================================|*/
/*| FUNCTION    :   void clearCRLF(char *str)                                           |*/
/*|                 It belongs to Carlo Sgro who is a professor of SET                  |*/
/*|                 at Conestoga College.                                               |*/
/*|                                                                                     |*/
/*| DESCRIPTION :   This function will get rid of a \n at the end of a string.          |*/
/*|                 If there isn't one, it doesn't change the string.                   |*/
/*|                                                                                     |*/
/*| PARAMETERS  :   char*   str :   start of the string                                 |*/
/*|                                                                                     |*/
/*| RETRURN     :   nothing                                                             |*/
/*|=====================================================================================|*/
void clearCRLF(char* str)
{
	char *whereIsCRLF = strchr (str, '\n');
	if (whereIsCRLF != NULL)
	{
		*whereIsCRLF = '\0';
	}
}

/*|=====================================================================================|*/
/*| FUNCTION    :   void clearSPACE                                                     |*/
/*|                                                                                     |*/
/*| DESCRIPTION :   This function will get rid of a ' '(SPACES) between the end of words|*/
/*|                 and the end of string                                               |*/
/*|                                                                                     |*/
/*| PARAMETERS  :   char*   str :   start of the string                                 |*/
/*|                                                                                     |*/
/*| RETRURN     :   nothing                                                             |*/
/*|=====================================================================================|*/
void clearSPACE(char *str)
{
    // strrchr: locate last occurrence of char string
    char *whereIsSPACE = strrchr(str, ' ');
 
    // find the end of space in string
    if ((whereIsSPACE != NULL) && (*(whereIsSPACE + 1) == '\0'))
    {
        // delete spaces between the end of words and end of string
        while (*whereIsSPACE == ' ')
        {
            whereIsSPACE--;
        }
        *(whereIsSPACE + 1) = '\0';
    }
 
}















