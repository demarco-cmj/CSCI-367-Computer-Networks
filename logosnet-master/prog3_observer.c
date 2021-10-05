/* demo_client.c - code for example client program that uses TCP */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>

void make_username(int sd);
void run_observer(int sd);
int recv_input(int sd, char *message, int *length);
int recv_length(int sd, int *length);
int recv_message(int sd, char *message, int length);
void send_input(int sd, char *message, int length);
void send_length(int sd, int length);
void send_message(int sd, char *message, int length);

int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	int port; /* protocol port number */
	char *host; /* pointer to host name */


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
		sad.sin_port = htons((u_short)port);
	} else {
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

	/* TODO: Connect the socket to the specified server. You have to pass correct parameters to the connect function.*/
	if (connect(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}
	
	int n; /* number of characters read */
	char message[1000]; /* buffer for data from the server */
	int length;
	
	//recv Y/N
	recv_input(sd, message, &length);
	//check if we are good or exit
	if(message[0] == 'Y'){
		make_username(sd);
		run_observer(sd);
	} else if(message[0] == 'N') {
		printf("Too many clients are connected to this server.\n");
		close(sd);
		exit(0);
	} else {
		fprintf(stderr, "Error. Disconnecting.\n");
		close(sd);
		exit(1);
	}

	exit(EXIT_SUCCESS);
}

//Client tries to make valid username, only given 10 seconds by server
void make_username(int sd) {
	char message[13];
	int again = 0;
	int len;
	int returnval;
	int tempChar;
	fd_set readfds;
	message[0] = 'T';
	while (message[0] == 'T' || message[0] == 'I') {
		do{
			again = 0;
			dprintf(1, "Enter a username: ");
			FD_ZERO(&readfds); //reset readfds
			FD_SET(0, &readfds); //add stdin
			FD_SET(sd, &readfds); //add sd
			returnval = select(sd + 1, &readfds, NULL, NULL, NULL);
			if (returnval < 0) {
				perror("Select");
				exit(1);
			}
			if (FD_ISSET(sd, &readfds)) {
				if (recv_input(sd, message, &len)) {
					close(sd);
					printf("\n");
					exit(0);
				}
			}
			if (FD_ISSET(0, &readfds)) {
				fgets(message, sizeof(message), stdin);
				len = strlen(message) - 1;
			}
			//Check username length 1-10
			if(len > 10 || len == 0) {
				if (message[11] != '\n' && len != 0) {
					while ((tempChar = getchar()) != '\n' && tempChar != EOF) {}
				}
				again = 1;
			}
		} while(again);
		send_input(sd, message, len);
		recv_input(sd, message, &len);
	}
}

void run_observer(int sd) {
	int returnval;
	int length;
	fd_set readfds;
	char buf[1016];
    char quit[] = "/quit";
	int printNewLine;
	while (1) {
		printNewLine = 0;
		//Clear sockets
		FD_ZERO(&readfds); //reset readfds
		FD_SET(0, &readfds); //add stdin
		FD_SET(sd, &readfds); //add sd
		
		returnval = select(sd + 1, &readfds, NULL, NULL, NULL);	//Wait for socket activity
		if (returnval < 0) {
			perror("Select");
			exit(1);
		}
		if (FD_ISSET(0, &readfds)) {
			//check for \quit
			fgets(buf, sizeof(buf), stdin);
            buf[5] = 0;
            if (!strcmp(buf, quit)) {
                close(sd);
				exit(0);
            }
		} 

		if (FD_ISSET(sd, &readfds)) {
			//read and append \n and print
			if (recv_input(sd, buf, &length)) {
				close(sd);
				exit(0);
			}
			if (buf[length-1] != '\n') {
				buf[length] = '\n';
				buf[length + 1] = 0;
			} else {
				buf[length] = 0;
			}
			
			printf("%s", buf);
		}
	}

}

int recv_input(int sd, char *message, int *length){
	if (recv_length(sd, length)) {
		return 1;
	}
	if (recv_message(sd, message, *length)) {
		return 1;
	}
	
}

int recv_length(int sd, int *length) { 
	uint16_t ret;
	char *message = (char*)&ret;
	int templen = sizeof(ret);
	int i;
	int n = 0;
	while (n != templen) {
		i = recv(sd, message + n, templen - n, 0);
		
		if (i == 0) {
			return 1;
		}
		
		if (i < 0) {
			perror("recv");
            close(sd);
            exit(EXIT_FAILURE);
		}
		n += i;
	}
	*length = ntohs(ret);
	return 0;
}

int recv_message(int sd, char *message, int length) {
	int i;
	int n = 0;
	while (n != length) {
		i = recv(sd, message + n, length - n, 0);
		
		if (i == 0) {
			return 1;
		}
		
		if (i < 0) {
			perror("recv");            
            close(sd);
            exit(EXIT_FAILURE);
		}
		n += i;
	}
	return 0;
}

//Sends message to server
void send_input(int sd, char *message, int length) {
	send_length(sd, length);
	send_message(sd, message, length);
}

void send_length(int sd, int length) {
	uint16_t conv = htons(length);
	char *message = (char*)&conv;
	length = sizeof(conv);
    int i;
    int n = 0;
    while(n != length) {
        i = send(sd, message + n, length - n, MSG_NOSIGNAL);
        if (i < 0) {
			perror("send");
			close(sd);
			exit(1);
        }
        n += i;
    }
}

void send_message(int sd, char *message, int length) {
    int i;
    int n = 0;
    while(n != length) {
        i = send(sd, message + n, length - n, 0);
        if (i < 0) {
			perror("send");
			close(sd);
			exit(1);
        }
        n += i;
    }
}