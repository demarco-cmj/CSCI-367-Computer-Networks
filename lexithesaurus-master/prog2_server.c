/*  LexiThesaurus
    Koby Brackebusch
    Connor Demarco
    CSCI 367
    Assignment Start Date: 7/7/21
*/

#include <netinet/in.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h> 
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define QLEN 51 /* size of request queue */

//trie.c prototypes
struct TrieNode *getNode(void);
void insert(struct TrieNode *root, const char *key);
bool search(struct TrieNode *root, const char *key);   

void run_game(int p1, int p2, uint8_t boardLen, uint8_t timeLimit, struct TrieNode *dict);
void run_turn(int active, int other, int *correct, struct TrieNode *dict, struct TrieNode *usedWords, uint8_t boardLen, const char *board, int characters[]);
void set_game(int sd, uint8_t boardLen, uint8_t timeLimit, uint8_t player);
void create_board(uint8_t boardLen, char *board);
void recv_input(int sd, char *message);
void send_input(int sd, char *message, int length, int otherSD);


int main(int argc, char **argv) {
    struct protoent *ptrp; /* pointer to a protocol table entry */
    struct sockaddr_in sad; /* structure to hold server's address */
    struct sockaddr_in cad; /* structure to hold client's address */
    int sd, sd2, sd3; /* socket descriptors */
    int port; /* protocol port number */
    int alen; /* length of address */
    int optval = 1; /* boolean value when we set socket option */
    int returnval; //used to wait on children processes
    pid_t cpid; 
    
    if (argc != 5) {
        fprintf(stderr,"Error: Wrong number of arguments\n");
        fprintf(stderr,"usage:\n");
        fprintf(stderr,"./server server_port board_size round_time file\n");
        exit(EXIT_FAILURE);
    }
    
    memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */

    //Set socket family to AF_INET
    sad.sin_family = AF_INET;

    //Set local IP address to listen to all IP addresses this server can assume. You can do it by using INADDR_ANY
    sad.sin_addr.s_addr = INADDR_ANY;
        
    port = atoi(argv[1]); /* convert argument to binary */
    if (port > 0) { /* test for illegal value */
        //set port number. The data type is u_short
        sad.sin_port = htons(port);
    } else { /* print error message and exit */
        fprintf(stderr,"Error: Bad port number %s\n",argv[1]);
        exit(EXIT_FAILURE);
    }

    /* Map TCP transport protocol name to protocol number */
    if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
        fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
        exit(EXIT_FAILURE);
    }

    /*Create a socket with AF_INET as domain, protocol type as SOCK_STREAM, and protocol as ptrp->p_proto. This call returns a socket descriptor named sd. */

    sd = socket(AF_INET, SOCK_STREAM, ptrp->p_proto);
    if (sd < 0) {
        fprintf(stderr, "Error: Socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    /* Allow reuse of port - avoid "Bind failed" issues */
    if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
        fprintf(stderr, "Error Setting socket option failed\n");
        exit(EXIT_FAILURE);
    }

    /* Bind a local address to the socket. For this, you need to pass correct parameters to the bind function. */
    if (bind(sd, (struct sockaddr*) &sad, sizeof(sad)) < 0) {
        fprintf(stderr,"Error: Bind failed\n");
        exit(EXIT_FAILURE);
    }

    /* Specify size of request queue. Listen take 2 parameters -- socket descriptor and QLEN, which has been set at the top of this code. */
    if (listen(sd, QLEN) < 0) {
        fprintf(stderr,"Error: Listen failed\n");
        exit(EXIT_FAILURE);
    }

    //Local Variables
    uint8_t boardLen = atoi(argv[2]);
    uint8_t timeLimit = atoi(argv[3]);
    int maxFileLineLength = 128;
    char line[128];
    struct TrieNode *dict = getNode();

    //Open Dictonary
    FILE *file = fopen(argv[4], "r");
    if (file == 0){
        printf("Cannot open file. \n");
        exit(1);
    }
    //Create Trie
    while(fgets(line, maxFileLineLength, file) != NULL) {
        line[strlen(line) - 1] = 0;
        insert(dict, line);
    }
    

    /* Main server loop - accept and handle requests */
    while (1) {
        //Wait for two players to connect
        alen = sizeof(cad);
        if ((sd2 = accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
            fprintf(stderr, "Error: Accept failed\n");
            exit(EXIT_FAILURE);
        }
        set_game(sd2, boardLen, timeLimit, 1);
        if ((sd3 = accept(sd, (struct sockaddr *)&cad, &alen)) < 0) {
            fprintf(stderr, "Error: Accept failed\n");
            exit(EXIT_FAILURE);
        }
        set_game(sd3, boardLen, timeLimit, 2);
        //two people connected, let's start a game.
        cpid = fork();
        if (cpid == 0) {
            run_game(sd2, sd3, boardLen, timeLimit, dict);
        }

        
        while ((returnval = waitpid(-1, NULL, WNOHANG)) > 0) {}
    }
    exit(EXIT_SUCCESS);
}


void set_game(int sd, uint8_t boardLen, uint8_t timeLimit, uint8_t player) {
    char message[2];
    message[0] = 1; //message length
    //send to p1
    message[1] = player;
    send_input(sd, message, message[0] + 1, sd);
    message[1] = boardLen;
    send_input(sd, message,message[0] + 1, sd);
    message[1] = timeLimit;
    send_input(sd, message, message[0] + 1, sd);
}

void run_game(int p1, int p2, uint8_t boardLen, uint8_t timeLimit, struct TrieNode *dict) {
    
    //Local Variables
    int roundNum = 1;
    int pOneScore = 0;
    int pTwoScore = 0;
    int correct = 1;
    char message[1024];
    int playerTurn;
    int characters[255];
    

    //Start Rounds
    while(1) {
        memset(characters, 0, 255*sizeof(int));
        struct TrieNode *usedWords = getNode();
        //R.1 - send scores
        message[0] = 2;
        message[1] = pOneScore;
        message[2] = pTwoScore;
        send_input(p1, message, message[0] + 1, p2);
        send_input(p2, message, message[0] + 1, p1);
        
        if (pOneScore == 3 || pTwoScore == 3) {
            break;
        }

        //R.2 - send round number
        message[0] = 1;
        message[1] = roundNum;
        send_input(p1, message, message[0] + 1, p2);
        send_input(p2, message, message[0] + 1, p1);
        
        //R.3 & R.4- create/send board
        create_board(boardLen, message);
        for (int i = 1; i < boardLen + 1; i++) {
            characters[(uint8_t) message[i]] = characters[(uint8_t) message[i]] + 1;
        }

        send_input(p1, message, boardLen + 1, p2);
        send_input(p2, message, boardLen + 1, p1);

        //R.5 - Start guessing
        playerTurn = roundNum % 2;
        while(correct) {
            //decide starting player
            if(playerTurn % 2){
                //Player 1 guesses
                run_turn(p1, p2, &correct, dict, usedWords, boardLen, message, characters);
            } else {
                //Player 2 guesses
                run_turn(p2, p1, &correct, dict, usedWords, boardLen, message, characters);
            }
            playerTurn++;
        }
        //Increment Score
        if ((playerTurn - 1) % 2) {
            pTwoScore++;
        } else {
            pOneScore++;
        }

        correct = 1;
        roundNum++;

        free(usedWords);
    }
    close(p1);
    close(p2);
    exit(EXIT_SUCCESS);

}


void run_turn(int active, int other, int *correct, struct TrieNode *dict, struct TrieNode *usedWords, uint8_t boardLen, const char *board, int characters[]) {
    int n;
    int err;
    char message[1024];
    char guess[1024];
    char tempBoard[1024];
    int tempchars[255];
    memset(tempchars, 0, 255*sizeof(int));
   
    message[0] = 1;
    //Send players Y/N according to their turn
    message[1] = 'Y';
    send_input(active, message, message[0] + 1, other);
    message[1] = 'N';
    send_input(other, message, message[0] + 1, active);

    //receive guess
    n = 0;
    while(1) {
        err = recv(active, message, 1024, 0);
        if (err < 0) {
            perror("recv: ");
            message[0] = 1;
            message[1] = 255;
            send_input(active, message, message[0]+1, other);
            send_input(other, message, message[0]+1, active);
            close(active);
            close(other);
            exit(EXIT_FAILURE);
        }
        n += err; 
        if (n >= message[0] - 1) {
            break;
        }
    }

    if ((uint8_t) message[1] == 255) { //our own error protocol
        send_input(other, message, message[0]+1, active);
        close(active);
        close(other);
        exit(EXIT_FAILURE);
    }
    

    //check guess

    message[message[0] + 1] = 0;
    for (int i = 0; i < message[0] + 2; i++) {
        guess[i] = message[i];
    }

    int boardPass;
    if (message[1] == 0) {
        //guesser ran out of time (our own protocol)
        boardPass = 0;
    } else {
        boardPass = 1;
        for (int i = 1; i < message[0] + 1; i++) { //looping through word
            tempchars[(uint8_t) message[i]] = tempchars[(uint8_t) message[i]] + 1;
            if (tempchars[(uint8_t) message[i]] > characters[(uint8_t) message[i]]) {
                boardPass = 0;
                break;
            }
        }
    }

    if (boardPass && search(dict, message + 1) && !search(usedWords, message + 1)){
        insert(usedWords, message + 1); //so it is no longer a valid word
        message[0] = 1;
        message[1] = 1;
        send_input(active, message, message[0] + 1, other);
        send_input(other, guess, guess[0] + 1, other); 

    } else { //if incorrect
        *correct = 0;
        message[0] = 1;
        message[1] = 0;
        send_input(other, message, message[0] + 1, active);
        send_input(active, message, message[0] + 1, other);
    }   
}

//returns a char array of length boardLen containing
void create_board(uint8_t boardLen, char *board) {
    char vowels[] = {'a', 'e', 'i', 'o', 'u'};
    int noVowel = 1;
    srandom(time(NULL));
    board[0] = boardLen;
    for(int i = 0; i < boardLen + 1; i++){
        board[i + 1] = 'a' + (random() % 26);
        if (board[i] == 'a' || board[i] == 'e' || board[i] == 'i' || board[i] == 'o' || board[i] == 'u') {
            noVowel = 0;
        }
    }
    if(noVowel) {
        board[boardLen - 1] = vowels[random() % 5];
    }
}

void send_input(int sd, char *message, int length, int otherSD) {
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
            if (sd != otherSD) {
                send(otherSD, message, message[0] + 1, 0);
                close(otherSD);
            }
            exit(EXIT_FAILURE);
        }
        n += i;
    }
}

