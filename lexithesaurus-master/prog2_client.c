/*  LexiThesaurus
    Koby Brackebusch
    Connor Demarco
    CSCI 367
    Assignment Start Date: 7/7/21
*/

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <poll.h>

void recv_input(int sd, char *message);
void send_input(int sd, char *message, int length);

void run_game(int sd);
void run_turn(int sd, int seconds, int *again);
void your_turn(int sd, int seconds, int *again);
void opp_turn(int sd, int *again);




int main(int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number *///HELLO
	char *host; /* pointer to host name */
	int n; /* number of characters read */
	char buf[1000]; /* buffer for data from the server */
	char stdinLine[255];

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./client server_address server_port\n");
		exit(EXIT_FAILURE);
	}

	port = atoi(argv[2]); /* convert to binary */
	if (port > 0) { /* test for legal value */
		sad.sin_port = htons( (u_short) port);
	}
	else {
		fprintf(stderr,"Error: bad port number %s\n",argv[2]);
		exit(EXIT_FAILURE);
	}

	host = argv[1]; /* if host argument specified */

	/* Convert host name to equivalent IP address and copy to sad. */
	ptrh = gethostbyname(host);
	if ( ptrh == NULL ) {
		fprintf(stderr,"Error: Invalid host: %s\n", host);
		exit(EXIT_FAILURE);
	}

	memcpy(&sad.sin_addr, ptrh->h_addr, ptrh->h_length);

	/* Map TCP transport protocol name to protocol number. */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/* Create a socket. */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Connect the socket to the specified server. You have 
	to pass correct parameters to the connect function.*/
	if (connect(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}

	run_game(sd);
	
	
}

void run_game(int sd) {
	char message[1024];
	int myPlayerNum;
	int oppPlayerNum;
	int roundNum;
	int myScore = 0;
	int oppScore = 0;
	int seconds;
	
	//Recieve Player number
	recv_input(sd, message);
	myPlayerNum = (uint8_t) message[1];
	if (myPlayerNum == 1) {
		oppPlayerNum = 2;
		printf("You are Player 1... the game will begin when PLayer 2 joins...\n");
	} else {
		oppPlayerNum = 1;
		printf("You are player 2...\n");
	}

	//Receive board size
	recv_input(sd, message);
	printf("Board size: %d\n", (uint8_t) message[1]);

	//Receive time limit
	recv_input(sd, message);
	seconds = (uint8_t) message[1];
	printf("Seconds per turn: %d\n", (uint8_t) message[1]);

	//Round start
	while(1) {
		//R.1 - recv scores
		recv_input(sd, message);
		myScore = message[myPlayerNum];
		oppScore = message[oppPlayerNum];
		if (myScore == 3) {
			printf("You won!\n");
			close(sd);
			exit(EXIT_SUCCESS);
		}
		if (oppScore == 3) {
			printf("You lost!\n");
			close(sd);
			exit(EXIT_SUCCESS);
		}
		
		//R.2 - receive roundNum
		recv_input(sd, message);
		roundNum = (uint8_t) message[1];

		//R.3 - receive board
		recv_input(sd, message);
		message[message[0] + 1] = 0;

		//Print round info
		printf("\nRound %d...\n", roundNum);
		printf("Score is %d-%d\n", myScore, oppScore);
		dprintf(1, "Board:");
		for(int i = 0; i < message[0]; i++) {
			dprintf(1, " %c", message[i+1]);
		}
		dprintf(1, "\n");

		//Start Turn Loop
		int again = 1;
		while(again) {
			run_turn(sd, seconds, &again);
		}
	}
}

void run_turn(int sd, int seconds, int *again) {
	//Local variables
	char message[1024];
	recv_input(sd, message);
	
	//Decide who's turn
	if(message[1] == 'Y'){
		your_turn(sd, seconds, again);
	} else {
		opp_turn(sd, again);
	}
}

void your_turn(int sd, int seconds, int *again) {
	char message[1024];
	int outOfTime = 0;
	struct pollfd fds = {0, POLLIN|POLLPRI};
	fflush(0); //so previous attemps are removed
	dprintf(1, "Your turn, enter word: ");
	if (poll(&fds, 1, seconds*1000)) {
		scanf("%s", message + 1);
		message[0] = strlen(message + 1);
	} else {
		//ran out of time
		message[0] = 1;
		message[1] = 0; //to tell server
		outOfTime = 1;
	}
	send(sd, message, sizeof(message), 0);

	//check if valid word
	recv_input(sd, message);
	if ((uint8_t) message[1] == 1){
		printf("Valid word!\n");
	} else if (outOfTime) {
		printf("\nOut of time!\n");
		*again = 0;
	} else {
		printf("Invalid word!\n");
		*again = 0;
	}

}

void opp_turn(int sd, int *again) {
	//Local variables
	char message[1024];
	
	printf("Please wait for opponent to enter word...\n");
	recv_input(sd, message);
	if (message[1] == 0) {
		printf("Opponent lost the round!\n");
		*again = 0;
	} else {
		message[message[0] + 1] = 0;
		printf("Opponent entered \"%s\"\n", message + 1);
	}
}

//Gets message from server
void recv_input(int sd, char *message){
	int i;
	message[0] = 0;
	int n = recv(sd, message, 1, 0); //to establish buffer length
	if (n < 0) { //Error catching protocol
		perror("recv");
		message[0] = 1;
		message[1] = 255;
		send(sd, message, message[0]+1, 0);
		close(sd);
		exit(EXIT_FAILURE);
	}
	while(n != message[0] + 1) {
		i = recv(sd, message + n, message[0] + 1 - n, 0);
		if (i < 0) {
			perror("recv");
			close(sd);
			exit(EXIT_FAILURE);
		}
		n += i;
	}

	if ((uint8_t) message[1] == 255) { //our own error protocol
        close(sd);
        exit(EXIT_FAILURE);
    }

}

void send_input(int sd, char *message, int length) {
    int i;
    int n = 0;
    while(n != message[0] + 1) {
        i = send(sd, message + n, length - n, 0);
        if (i < 0) {
            perror("send");
            message[0] = 1;
            message[1] = 255;
            send(sd, message, message[0] + 1, 0);
            close(sd);
            exit(EXIT_FAILURE);
        }
        n += i;
    }
}