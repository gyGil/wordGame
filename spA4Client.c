/*|=====================================================================================|*/
/*| FILE :          spA3Client.c                                                        |*/
/*|                                                                                     |*/
/*| PROJECT :       PROG1970 - SP Assignment #4                                    	|*/
/*|                                                                                     |*/
/*| PROGRAMMER :    Geun Young Gil  (STD Num. 6944920)					|*/ 
/*|                                                                                     |*/
/*| FIRST VERSION : Apr. 17. 2015                                                       |*/
/*|                                                                                     |*/
/*| DESCRIPTION :   This program is Client to connect Serve0r				|*/
/*|		    It use fork to listen and write.					|*/ 
/*|		    It get connection with Server. And send answer to server. and get 	|*/
/*|		    back result. It can use special command				|*/
/*|=====================================================================================|*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>	// struct hostent, gethostbyname()
#include <netinet/in.h>	// struct sockaddr_in
#include <arpa/inet.h>	// htons()
#include <sys/socket.h>	// socket(), connect()
#include <sys/select.h> // select(), FD_ZERO(), FD_SET()
#include <sys/ipc.h>	// semaphore
#include <sys/sem.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>

#define PORT 5000

int inputMode(int serverSocket, char* in_buffer);
char getch(void);

int main (int argc, char *argv[])
{
	/*
	* struct hostent: hosts database
	* 	char* 	h_name: 	official name of host
	*	char** 	h_aliases:	alternative names for the host
	*	int	h_addrtype:	AF_INET or AF_INET6
	*	int 	h_length:	in bytes for address
	*	char**	h_addr_list:	vector of addresses for the host
	*	char*	h_addr:		the first host address (== h_addr_list[0])	
	*/
	struct hostent* server = NULL;
	/*
	* struct sockaddr_in
	*	short		sin_family:	e.g. AF_INET
	*	unsigned short 	sin_port: 	16 bit port num 0-65535  e.g. htons(3490)
	*	struct in_addr	sin_addr:	address struct
	*	char		sin_zero[8]:	for future expansion
	*/
	struct sockaddr_in serverAddr;
	int serverSocket = 0;	// serverSocket
	int cnt = 0;
	int exitSignal = 0;	// indicator whether exit or not (-1: exit, 0: normal)
	fd_set inputSet;	// file descriptor masks for stdin
	fd_set readSet;		// for reading socket
	struct timeval tv;	// time set for select()
	int retSelect = 0;	
	char buffer[BUFSIZ] = {0};
	int semid = 0;		// semaphore id
	struct sembuf acquire_operation = { 0, -1, SEM_UNDO };	// decrement by 1
	struct sembuf release_operation = { 0, 1, SEM_UNDO };	// increment by 1
	unsigned short init_values[1] = { 1 };	// initial semaphore value

	// check command usage
	if (argc != 2)
	{
		printf ("USAGE: spA3Client <server name (= ip address)>\n");
		return 1;
	}

	// get Server info
	printf ("[CLIENT] Finding the Server...... ");
	fflush(stdout);
	if ( (server = gethostbyname(argv[1])) == NULL)
	{
		printf("Failed!!\n");
		return 2;
	}
	printf("Done.\n");
	fflush(stdout);

	// initialize struct sockaddr_in to info which got from gethostbyname()
	memset(&serverAddr, 0 , sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	memcpy(&serverAddr.sin_addr, server->h_addr, server->h_length);
	// convert the unsigned short int hostshort from host byte order to network byte order
        // unit16_t htons(unit16_t hostshort)
        serverAddr.sin_port = htons (PORT);

	// make a socket (creat an unbound socket in a communication domain)
	printf ("[CLIENT] Getting socket for connecting server...... ");
	fflush(stdout);
	if ((serverSocket = socket (AF_INET, SOCK_STREAM, 0)) < 0) // TCP
	{
		printf("Failed!!\n");
		return 3;
	}
	printf("Done.\n");
	fflush(stdout);

	// connect to server
	printf ("[CLIENT] Connecting server...... ");
	fflush(stdout);
	if ( connect (serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
	{
		printf("Failed!!\n");
		close (serverSocket);
		return 4;
	}
	printf("Done.\n");
	fflush(stdout);

	// get semaphore
	printf("[CLIENT] Making semaphore for processing...... ");
   	fflush(stdout);
	semid = semget (IPC_PRIVATE, 1, IPC_CREAT | 0666);	// create semaphore
	if (semid == -1)
	{
		printf("Failed!!\n");
		close (serverSocket);
		return 5;
	}
	// initialize semaphore
	if (semctl (semid, 0, SETALL, init_values) == -1) 
	{
	  	printf("Failed!!\n");
		close (serverSocket);
	 	return 6;
	}
	printf("Done.\n\n");
	fflush(stdout);

	// print intro
	printf("Now Chat Client is starting.\n");
	printf("Please hit \"Enter key\" for input.\n\n");
	fflush(stdout);
	exitSignal = 0;

	// child to keep receiving packets from server
	memset(buffer, 0, BUFSIZ);
	int child = fork();
	if (child == 0)
	{
		while (1)
		{
			read (serverSocket, buffer, BUFSIZ);

			// Child(receiving) try to get semaphore
			if ( semop (semid, &acquire_operation, 1) == 0)
			{
				printf("%s", buffer);
				fflush(stdout);
				memset (buffer, 0, BUFSIZ);
				semop (semid, &release_operation, 1); 
			}
		}

	}
	// parent to standby to get input from user
	else if (child > 0)
	{	
		exitSignal = 0;
		while (exitSignal != -1)
		{
			if ( getch())
			{
				while ( semop (semid, &acquire_operation, 1) != 0 );
				exitSignal = inputMode(serverSocket, buffer);
				semop (semid, &release_operation, 1); 

			}
		}
		
		kill(child,SIGKILL);		// kill child
		semctl (semid, 0, IPC_RMID);	// remove semaphore
		close (serverSocket);
	}
	// fail fork()
	else
	{
		printf("[CLIENT] Failed to fork!!\n");
		semctl (semid, 0, IPC_RMID);	
		close (serverSocket);
		return 7;
	}
	
	return 0;	
}


/*|=====================================================================================|*/
/*|   FUNCTION   :  inputMode                                                    	|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  It's signal handler. It is used to get input from user for chatting |*/
/*|		    The "ctrl-c" keycombination is the signal to trigar chatting mode	|*/
/*|                                                                                     |*/
/*|   PARAMETERS :  char*       in_buffer       :   buffer to write 			|*/
/*|		    int         serverSocket	:   server socket			|*/ 
/*|                                                                                     |*/
/*|   RETURN    :   int         exitSignal	:   indicate whether exit program or not|*/
/*|                                                 -1: exit, 0: not exit               |*/		                
/*|=====================================================================================|*/
int inputMode(int serverSocket, char* in_buffer)
{
	memset (in_buffer,0,BUFSIZ);
	printf ("Input Mode (exit:\"\\exit\", help: \"\\?\", score:\"\\score\", question:\"\\question\")\n");
	printf (">> ");
	fflush (stdout);

	// clear input stdin
	fgets (in_buffer, BUFSIZ, stdin);

	// delete carage return
	if (in_buffer[strlen (in_buffer) - 1] == '\n')
	{
		in_buffer[strlen (in_buffer) - 1] = '\0';
	}

	// special command
	if (in_buffer[0] == '\\')
	{
		if(strcmp (in_buffer + 1, "?") == 0)
		{
			printf("\n");
			printf("\\?:\t help \n");
			printf("\\score:\t list score\n");
			printf("\\exit:\t exit program\n\n");
		}
		else if(strcmp (in_buffer + 1, "exit") == 0)
		{
			write (serverSocket, in_buffer, strlen (in_buffer));
			return -1;
		}
		else if(strcmp (in_buffer + 1, "score") == 0)
		{
			write (serverSocket, in_buffer, strlen (in_buffer));
		}
		else if(strcmp (in_buffer + 1, "question") == 0)
		{
			write (serverSocket, in_buffer, strlen (in_buffer));
		}
		else
		{
			printf("\n No Command!! (help: \"\\?\")\n\n");
		}
	}
	else
	{
		write (serverSocket, in_buffer, strlen (in_buffer)); 
	}
	fflush(stdout);
	

	return 0;
}

/*|=====================================================================================|*/
/*|   FUNCTION   :  getch                                                   		|*/
/*|                                                                                     |*/
/*|   DISCRIPTION:  same as getch() in windows c					|*/
/*|                 resource: http://stackoverflow.com/questions/7469139/what-is-equivalent-to-getch-getche-in-linux                                                                   |*/				
/*|                                                                                     |*/
/*|   PARAMETERS :  void								|*/
/*|                                                                                     |*/
/*|   RETURN    :   void	                					|*/
/*|=====================================================================================|*/
char getch(void){
    char buf=0;
    struct termios old={0};
    fflush(stdout);
    if(tcgetattr(0, &old)<0)
        perror("tcsetattr()");
    old.c_lflag&=~ICANON;
    old.c_lflag&=~ECHO;
    old.c_cc[VMIN]=1;
    old.c_cc[VTIME]=0;
    if(tcsetattr(0, TCSANOW, &old)<0)
        perror("tcsetattr ICANON");
    if(read(0,&buf,1)<0)
        perror("read()");
    old.c_lflag|=ICANON;
    old.c_lflag|=ECHO;
    if(tcsetattr(0, TCSADRAIN, &old)<0)
        perror ("tcsetattr ~ICANON");
    printf("%c\n",buf);
    return buf;
 }




























