#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/select.h>
#include <time.h>
#include <stdbool.h>
#include <poll.h>
#include <time.h>
#include <ctype.h>

#include "trie.c"

#define QLEN 512 /* size of request queue */


struct client {
	int sd;
	int participant; //bool value for if participant
	int observer_sd; //sd to close if participant closes
	int active; //bool value for if they are intialized
	char username[11]; //username assocaited with the client
	time_t lastContact;  //time left to submit a username
	char message[1000];
	int bytesRead;
	int messageLength;
};

int pClientNum = 0; /* number of participants */
int oClientNum = 0; /* number of observers */

void make_socket(int *sd, int port);
void maintain_select(fd_set *readfds, int pSocket, int oSocket, int *max_sd, struct client *clients[]);

int recv_input(int sd, char *message, int *length, struct client *tempClient); //
int recv_length(int sd, int *length);
int recv_message(int sd, char *message, int length, struct client *tempClient);
void send_input(int sd, char *message, int length);
void send_length(int sd, int length);
void send_message(int sd, char *message, int length);
void get_participant_username(int sd, char *message, struct client *clients[], struct TrieNode *usernames);
void get_observer_username(int sd, char *message, struct client *clients[], struct TrieNode *usernames);
int time_out(struct client *clients[], int i);
void close_participant(struct client *clients[], struct TrieNode *usernames, int k, int length, char *message);
char *prepend_message(struct client *clients[], char *message, int length, int k, int offset, char messageType);

int main(int argc, char **argv) {
	struct sockaddr_in *pCad; 
	struct sockaddr_in *oCad;
	int alen; /* address length */
	int pSocket; /* participant socket descriptor */
	int oSocket; /* observer socket descriptor */
	int pPort; /* participant port number */
	int oPort; /* observer port number */
	struct client *clients[512] = {NULL}; /* sd array for all clients */
	
	int max_sd; /* largest sd number */
	int new_sd; /* temporary sd for accepting clients */
	struct client *tempClient; /* temporary client for looping through clients[] */
	time_t temptime;
	time_t ogtime;
	int length; /* length to pass to recv() */

	struct TrieNode *usernames = getNode();

	uint16_t messageLength;
	char *message;
	char *tempChar = malloc(1);

	struct timeval waitTime; /* timeout value for select() */
	waitTime.tv_sec = 1;
	fd_set readfds; /* fd_set struct for select() */
	int returnval; /* return value for select() */

	int isPrivate;
	char clientUsername[11];
	int target_sd;

	if(argc != 3) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server participant_port oberserver_port\n");
		exit(EXIT_FAILURE);
	}

	pPort = atoi(argv[1]);
	oPort = atoi(argv[2]);	
	make_socket(&pSocket, pPort); //create participant socket
	make_socket(&oSocket, oPort); //create observer socket

	/* Main server loop - accept and handle requests */
	while (1) {
		isPrivate = 0;
		maintain_select(&readfds, pSocket, oSocket, &max_sd, clients); 		//Clear sockets sets/prep for messages
		returnval = select(max_sd + 1, &readfds, NULL, NULL, &waitTime);	//Wait for socket activity (for 1 second)
		if (returnval < 0) {
			perror("Select");
			exit(1);
		}
		if (FD_ISSET(pSocket, &readfds)) { //New participant found
			alen = sizeof(pCad);
			if ((new_sd = accept(pSocket, (struct sockaddr *)&pCad, &alen)) < 0) {
				fprintf(stderr, "Error: Accept failed\n");
				exit(EXIT_FAILURE);
			}

			if(pClientNum >= 255){ //if users are at capacity
				//Send 'N', sorry, we can't add you
				tempChar[0] = 'N';
				send_input(new_sd, tempChar, 1);
				close(new_sd);
			} else {
				//we have space, confirm to client
				tempChar[0] = 'Y';
				pClientNum = pClientNum + 1; //increment total client amount
				send_input(new_sd, tempChar, 1);
			}
			clients[new_sd] = (struct client *) malloc(sizeof(struct client));
			if (clients[new_sd] == NULL) {
				fprintf(stderr, "Malloc failed.\n");
				exit(1);
			}
			clients[new_sd]->sd = new_sd;
			clients[new_sd]->active = 0;
			clients[new_sd]->lastContact = time(NULL);
			clients[new_sd]->participant = 1;
			clients[new_sd]->observer_sd = -1;
			clients[new_sd]->messageLength = 0;
			clients[new_sd]->bytesRead = 0;
		}
		if (FD_ISSET(oSocket, &readfds)) { //New observer found
			alen = sizeof(oCad);
			if ((new_sd = accept(oSocket, (struct sockaddr *)&oCad, &alen)) < 0) {
				fprintf(stderr, "Error: Accept failed\n");
				exit(EXIT_FAILURE);
			}
			if(oClientNum >= 255){ //if users are at capacity
				//Send 'N', sorry, we can't add you
				tempChar[0] = 'N';
				send_input(new_sd, tempChar, 1);
				close(new_sd);
			} else {
				//we have space, confirm to client
				tempChar[0] = 'Y';
				oClientNum = oClientNum + 1; //increment total client amount
				send_input(new_sd, tempChar, 1);
			}
			clients[new_sd] = (struct client *) malloc(sizeof(struct client));
			if (clients[new_sd] == NULL) {
				fprintf(stderr, "Malloc failed.\n");
				exit(1);
			}
			clients[new_sd]->sd = new_sd;
			clients[new_sd]->active = 0;
			clients[new_sd]->lastContact = time(NULL);
			clients[new_sd]->participant = 0;
			clients[new_sd]->messageLength = 0;
			clients[new_sd]->bytesRead = 0;
		}
		//loop through clients and check if they are readable
		for(int i = 0; i < 512; i++) { 
			tempClient = clients[i];
			if (tempClient == NULL) {
				continue;
			}

			if (FD_ISSET(tempClient->sd, &readfds) && tempClient->participant && tempClient->active) {
				returnval = recv_input(tempClient->sd, message, &length, tempClient); 
				if (returnval == 1) {
					close_participant(clients, usernames, i, length, message);
					continue;
				}
				if (returnval == 2) {
					continue;
				}
				message = tempClient->message;
				int usernameLength;
				//check if message is private
				if(message[0] ==  '@'){
					isPrivate = 1;
					int j = 1;
					while(j){
						if (j == 12) {
							close_participant(clients, usernames, i, length, message);
						}
						if(isspace(message[j])){ //if space, stop looping
							clientUsername[j - 1] = 0;
							j = 0;
						}
						else { //else record target user
							clientUsername[j - 1] = message[j];
							j++;
						}
					}
				}

				if(!isPrivate){ //if not private, send to everyone
					char *finalMessage = prepend_message(clients, message, length, i, 0, '>');
					for (int i = 0; i < 512; i++) {
						tempClient = clients[i];
						if (tempClient == NULL) {
							continue;
						}
						if (!tempClient->participant && tempClient->active) {
							send_input(tempClient->sd, finalMessage, length + 14);
						}
					}
					free(finalMessage);

				} else { //else send to specific user
					target_sd = return_sd(usernames, clientUsername);
					if(target_sd >= 0) { //if participant exists
						int usernameLength = strlen(clientUsername);
						char *finalMessage = prepend_message(clients, message, length, i, 2 + usernameLength, '-');
						target_sd = clients[target_sd]->observer_sd;
						if (target_sd >= 0) { //if not flying blind	
							send_input(target_sd, finalMessage, ((length - usernameLength) - 2) + 14);
						}
						target_sd = clients[i]->observer_sd;
						if (target_sd >= 0) {
							send_input(target_sd, finalMessage, ((length - usernameLength) - 2) + 14); 
						}
						free(finalMessage);
						
					} else { //else, target user doesnt exist, send warning 
						int tempLength;
						char *warn = malloc(17);
						if (warn == NULL) {
							fprintf(stderr, "Malloc failed.\n");
							exit(1);
						}
						strncpy(warn, "Warning: user ", 14);
						// warn = "Warning: user ";
						for (int i = 0; i < 14; i++) {
							message[i] = warn[i];
						}
						tempLength = 14 + strlen(clientUsername);
						for (int i = 14; i < tempLength; i++) {
							message[i] = clientUsername[i-14];
						}
						strncpy(warn, " doesn't exist...", 17);
						// warn = " doesn't exist...";
						length = tempLength + 17;
						for (int i = tempLength; i < length; i++) {
							message[i] = warn[i-tempLength];
						}		
						target_sd = tempClient->observer_sd;
						if (target_sd >= 0) {
							send_input(target_sd, message, length); 
						}	
						free(warn);
					}

				}

			} else if (FD_ISSET(tempClient->sd, &readfds) && !tempClient->participant && tempClient->active) {
				if (recv_input(tempClient->sd, message, &length, tempClient)) {
					//set participant's observer_sd to -1
					clients[return_sd(usernames, tempClient->username)]->observer_sd = -1;
					close(tempClient->sd);
					oClientNum = oClientNum - 1;
					free(clients[i]);
					clients[i] = NULL;
				}
			} else if (FD_ISSET(tempClient->sd, &readfds) && !tempClient->participant && !tempClient->active) {
				get_observer_username(tempClient->sd, message, clients, usernames);
			} else if (FD_ISSET(tempClient->sd, &readfds) && tempClient->participant && !tempClient->active) {
				get_participant_username(tempClient->sd, message, clients, usernames);
			} else if (!tempClient->active) {
				time_out(clients, i);
			}
		}
	}
}

char *prepend_message(struct client *clients[], char *message, int length, int k, int offset, char messageType) { 
	struct client *tempClient = clients[k];
	int usernameLength = strlen(tempClient->username);
					
	char *prepend = malloc(14);
	if (prepend == NULL) {
		fprintf(stderr, "Malloc failed.\n");
		exit(1);
	}
	prepend[0] = messageType;
	for (int i = 1; i <= 11-usernameLength; i++) {
		prepend[i] = ' ';
	}
	for (int i = 12 - usernameLength; i < 12; i++) {
		prepend[i] = tempClient->username[i - (12 - usernameLength)];
	}
	prepend[12] = ':';
	prepend[13] = ' ';

	char *finalMessage = malloc(length + 14);
	if (finalMessage == NULL) {
		fprintf(stderr, "Malloc failed.\n");
		exit(1);
	}
	for (int i = 0; i < 14; i++) {
		finalMessage[i] = prepend[i];
	} 

	for (int i = 14; i < length + 14; i++) {
		finalMessage[i] = message[(i-14) + offset];
	}
	free(prepend);
	return finalMessage;
}

void close_participant(struct client *clients[], struct TrieNode *usernames, int k, int length, char *message) {
	struct client *tempClient = tempClient = clients[k];
	if (tempClient->observer_sd >= 0) {
		close(tempClient->observer_sd);
		oClientNum = oClientNum - 1;
	}
	close(tempClient->sd);
	pClientNum = pClientNum - 1;
	removeTrieNode(usernames, tempClient->username, 0);
	length = strlen(tempClient->username);
	for(int i = 0; i < length; i++){
		message[i] = tempClient->username[i];
	}

	char suffix[] = " has left";
	for (int i = length; i < length + 9; i++){
		message[i] = suffix[i - length];
	}

	free(clients[tempClient->observer_sd]);
	clients[tempClient->observer_sd] = NULL;

	for (int i = 0; i < 512; i++) {
		tempClient = clients[i];
		if (tempClient == NULL) {
			continue;
		}

		if (!tempClient->participant && tempClient->active) {
			send_input(tempClient->sd, message, length + 9);	
		}
		
	}
	
	free(clients[k]);
	clients[k] = NULL;
}

int time_out(struct client *clients[], int i) {
	struct client *tempClient = clients[i];
	if (time(NULL) - tempClient->lastContact >= 10) {
		if (tempClient->participant) {
			pClientNum = pClientNum - 1;
		} else {
			oClientNum = oClientNum - 1;
		}
		close(tempClient->sd);
		free(clients[i]);
		clients[i] = NULL;
		return 1;
	} 
	return 0;
}

//username is negotiated in the server
void get_participant_username(int sd, char *message, struct client *clients[], struct TrieNode *usernames) {
	//see if they have timed out
	if (time_out(clients, sd)) { 
		return;
	}
	struct client *tempClient = clients[sd];
	int length;
	int returnval;
	returnval = recv_input(sd, message, &length, tempClient);
	if (returnval == 1) {
		close(sd);
		free(clients[sd]);
		clients[sd] = NULL;
		return;
	}
	if (returnval == 2) {
		return;
	}
	message = tempClient->message;
	//Check each char for criteria
	for (int i = 0; i < length; i++) {
		if(!isalpha(message[i]) && !isdigit(message[i]) && !((int) message[i] == 95)){
			//if doesnt pass, send 'I', then try again
			length = 1;
			message[0] = 'I';
			send_input(sd, message, length);
			return;
		}
	}
	
	
	message[length] = 0;

	//it's a used username
	if (search(usernames, message)) {
		length = 1;
		message[0] = 'T';
		send_input(sd, message, length);
		tempClient->lastContact = time(NULL);
		return;
	}

	//it's a good usernamae
	int globalMessageLength = length + 16;
	int i;
	char globalMessagePreix[] = "User ";
	char globalMessageSuffix[] = " has joined";
	
	//copy username into message
	for (i = 0; i <= length; i++) {
		tempClient->username[i] = message[i];
	}

	char globalMessage[globalMessageLength];

	for (i = 0; i < 5; i++) {
		globalMessage[i] = globalMessagePreix[i];
	}

	length = length + 5;

	for (i = 5; i < length; i++) {
		globalMessage[i] = message[i-5];
	}
	for (i = length; i < globalMessageLength; i++) {
		globalMessage[i] = globalMessageSuffix[i - length];
	}

	length = 1;
	insert(usernames, message, sd); //into Trie
	message[0] = 'Y';
	send_input(sd, message, length); //to client we accepted
	tempClient->active = 1;

	for (i = 0; i < 512; i++) {
		tempClient = clients[i];
		if (tempClient == NULL) {
			continue;
		}
		if (!tempClient->participant && tempClient->active) {
			send_input(tempClient->sd, globalMessage, globalMessageLength);
		}
	}
}

void get_observer_username(int sd, char *message, struct client *clients[], struct TrieNode *usernames) {
	//see if they have timed out
	if (time_out(clients, sd)) { 
		return;
	}
	int length;
	struct client *tempClient = clients[sd];
	int i;
	int participant_sd;
	int returnval;
	returnval = recv_input(sd, message, &length, clients[sd]);
	if (returnval == 1) {
		close(sd);
		free(clients[sd]);
		clients[sd] = NULL;
		return;
	}
	if (returnval == 2) {
		return;
	}
	message = tempClient->message;

	//Check each char for criteria
	for (int i = 0; i < length; i++) {
		if(!isalpha(message[i]) && !isdigit(message[i]) && !((int) message[i] == 95)){
			//if doesnt pass, send 'I', then try again
			length = 1;
			message[0] = 'I';
			send_input(sd, message, length);
			return;
		}
}
	message[length] = 0;
	participant_sd = return_sd(usernames, message);
	
	if (participant_sd < 0) {
		message[0] = 'N';
		send_input(sd, message, 1);
		oClientNum = oClientNum - 1;
		close(sd);
		clients[sd] = NULL;
		return;
	}
	tempClient = clients[participant_sd];

	if (tempClient->observer_sd < 0) {
		for (int i = 0; i <= length; i++) {
			clients[sd]->username[i] = message[i];
		}

		message[0] = 'Y';
		send_input(sd, message, 1);

		tempClient->observer_sd = sd;
		clients[sd]->active = 1;
		char welcomeMessage[] = "A new observer has joined";
		for (i = 0; i < 512; i++) {
			tempClient = clients[i];
			if (tempClient == NULL) {
				continue;
			}
			if (!tempClient->participant && tempClient->active) {
				send_input(tempClient->sd, welcomeMessage, 25);
			}
		}
		return;
	}

	message[0] = 'T';
	send_input(sd, message, 1);
	clients[sd]->lastContact = time(NULL);
}

int recv_input(int sd, char *message, int *length, struct client *tempClient){
	if (tempClient->messageLength == 0) {
		if (recv_length(sd, length)) {
			return 1;
		}
		tempClient->messageLength = *length;
	}
	return recv_message(sd, message, tempClient->messageLength, tempClient);
	
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
	if (*length > 1000) {  //Message length limit
		return 1;
	}
	return 0;
}

int recv_message(int sd, char *message, int length, struct client *tempClient) {
	int n;
	//bytesread
	n = recv(sd, tempClient->message + tempClient->bytesRead, length - tempClient->bytesRead, 0);
	
	if (n == 0) {
		return 1;
	}
	
	if (n < 0) {
		perror("recv");            
		close(sd);
		exit(EXIT_FAILURE);
	}

	if (n + tempClient->bytesRead == length) {
		tempClient->bytesRead = 0;
		tempClient->messageLength = 0;
		return 0;
	} //else

	tempClient->bytesRead = tempClient->bytesRead + n;
	return 2;
}

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
        i = send(sd, message + n, length - n, 0);
        if (i < 0) {
            perror("send");
            close(sd);
            exit(EXIT_FAILURE);
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
            exit(EXIT_FAILURE);
        }
        n += i;
    }
}

void maintain_select(fd_set *readfds, int pSocket, int oSocket, int *max_sd, struct client *clients[]) {
	int tempsd;
	FD_ZERO(readfds); //reset readfds
	FD_SET(pSocket, readfds); //add pSocket
	FD_SET(oSocket, readfds); //add oSocket
	*max_sd = (pSocket > oSocket) ? pSocket : oSocket; //set maxsd to max of p vs o socket
	// printf("running maintainselect\n");
	for (int i = 0; i < 512; i++) {
		if(clients[i] == NULL) {
			continue;
		}
		// printf("\nclient %d is NOT null\n\n", i);
		tempsd = clients[i]->sd;
		// printf("adding sd to fd_set\n");
		if (tempsd > 0) { //this is a currently used sd
			FD_SET(tempsd, readfds);
		}
		if (tempsd > *max_sd) { //update max_sd for select()
			*max_sd = tempsd;
		}
	}
	// printf("\n");
}

void make_socket(int *sd, int port) {
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	int optval = 1; /* boolean value when we set socket option */

	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */

	sad.sin_family = AF_INET;
	
	//Set local IP address to listen to all IP addresses this server can assume. You can do it by using INADDR_ANY
	sad.sin_addr.s_addr = INADDR_ANY;

	if (port > 0) { /* test for illegal value */
		//set port number. The data type is u_short
		sad.sin_port = htons(port);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port number %d\n", port);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	/*Create a socket with AF_INET as domain, protocol type as SOCK_STREAM, and protocol as ptrp->p_proto. This call returns a socket descriptor named sd. */

	*sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
	if (*sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(*sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/*Bind a local address to the socket. For this, you need to pass correct parameters to the bind function. */
	if (bind(*sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
		fprintf(stderr,"Error: Bind failed\n");
		exit(EXIT_FAILURE);
	}

	/*Specify size of request queue. Listen take 2 parameters -- socket descriptor and QLEN, which has been set at the top of this code. */
	if (listen(*sd, QLEN) < 0) {
		fprintf(stderr,"Error: Listen failed\n");
		exit(EXIT_FAILURE);
	}
}

