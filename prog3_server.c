#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>

#define QLEN 2 /* size of request queue */
#define DIM 9
#define MAX_NAME 31
#define MES_SIZE 512
#define MES_SMAL 64
#define MAX_LEAD 1024
#define MOVE_OK 1
#define LEAD_MES 128
#define SEND_LEAD 1
#define SEND_BOARD 1
/*------------------------------------------------------------------------
* Program: Server for sudoku game with select
*
* Purpose: Play sudoku game with multiple clients making moves in any order
*
* Syntax: ./server port_observers port_participants maxObservers maxParticipants secondsRound secondsPause
*
* Author: Ian Hammerstrom
* Date: 11/24/15
*
*------------------------------------------------------------------------
*/

typedef struct Participant{
	int socket;
	int points;
	char name[MAX_NAME];
}Participant;

void game (int particSocket, int observSocket, int maxParticipants, int maxObservers, int secondsPause, int secondsRound);
void sendObservers(int sendBoard, char board[DIM][DIM], char messBuf[], int observers[], int maxObservers, int sendLead, Participant leaderBoard[]);
void updateLeaderBoard(Participant leaderBoard[], Participant partic);
int getMaxSocket(Participant participants[], int observers[], int maxParticipants, int maxObservers, int particSocket, int observSocket);
int getSocketFromPort(uint32_t port);
Participant getBlankParticipant();
void handleParticDisconnect(Participant participants[], int i, fd_set sockSet);
void handleObservDisconnect(int observers[], int i, fd_set sockSet);
int setNameParticipant(Participant participants[], int i, Participant leaderBoard[]);
int checkLegal(char board[DIM][DIM], char place[3]);
void newParticpant(Participant participants[], int maxParticipants, int particSocket, int *maxSocket);
void newObserver(int observers[], int maxObservers, int observSocket, int *maxSocket);
void resetFDSET(int maxParticipants, int maxObservers, fd_set *sockSet, int particSocket, int observSocket, Participant participants[], int observers[]);
void handleIO(int movesAllowed, int maxParticipants, int maxObservers, fd_set *sockSet, Participant participants[], int observers[], int *maxSocket, int particSocket, int observSocket, char board[DIM][DIM], Participant leaderBoard[]);

int main(int argc, char **argv) {

	int observSocket;
	int particSocket;
	u_short maxObservers;
	u_short maxParticipants;
	u_short secondsRound;
	u_short secondsPause;


	if( argc != 7 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./server port_observers port_participants maxObservers maxParticipants secondsRound secondsPause\n");
		exit(EXIT_FAILURE);
	}

	observSocket = getSocketFromPort((u_short)atoi(argv[1]));
	particSocket = getSocketFromPort((u_short)atoi(argv[2]));
	maxObservers = (u_short)atoi(argv[3]);
	maxParticipants = (u_short)atoi(argv[4]);
	secondsRound = (u_short)atoi(argv[5]);
	secondsPause = (u_short)atoi(argv[6]);

	game(particSocket, observSocket, maxParticipants, maxObservers, secondsPause, secondsRound);
}

//continues the game loop
void game (int particSocket, int observSocket, int maxParticipants, int maxObservers, int secondsPause, int secondsRound){
	int i=0;

	Participant participants[maxParticipants];
	for (i=0; i<maxParticipants; i++){
		participants[i] = getBlankParticipant();
	}
	int observers[maxObservers];
	for (i=0; i<maxObservers; i++){
		observers[i] = -1;
	}

	fd_set sockSet;
	//time_t endTime = time(NULL) + secondsRound;
	char board[DIM][DIM];
	memset(&board, '0', sizeof(board));

	char moveBuf[3];
	int maxSocket=0, newSock;
	struct sockaddr_in cad;//////////////
	int alen = sizeof(cad);

	Participant leaderBoard[MAX_LEAD];
	for (i=0; i< MAX_LEAD; i++){
		leaderBoard[i] = getBlankParticipant();
	}

	int tempSock= -1;
	struct timeval secondsToEnd;
	bzero(&secondsToEnd, sizeof(secondsToEnd));
	secondsToEnd.tv_sec = secondsRound;
	char messBuf[MES_SMAL];
	bzero(&messBuf, sizeof(messBuf));
	
	

	while (1){// Main Loop
		printf("Beginning new round, will end in %d seconds.\n", secondsRound); 
		
		while(secondsToEnd.tv_sec > 0){ //game loop

			resetFDSET(maxParticipants, maxObservers, &sockSet, particSocket, observSocket, participants, observers);
			maxSocket = getMaxSocket(participants, observers, maxParticipants, maxObservers, particSocket, observSocket);

			if (select(maxSocket + 1, &sockSet, NULL, NULL, &secondsToEnd) < 0){ 
				fprintf(stderr, "Error: Select failed\n" );
			}else{
				if(FD_ISSET(particSocket, &sockSet)){ //new participant

					newParticpant(participants, maxParticipants, particSocket, &maxSocket);

				}else if (FD_ISSET(observSocket, &sockSet)){ //new observer
						
					newObserver(observers, maxObservers, observSocket, &maxSocket);

				}else{//some other IO operation

					handleIO(MOVE_OK, maxParticipants, maxObservers, &sockSet, participants, observers, &maxSocket, particSocket, observSocket, board, leaderBoard);
				}
			}
		} 
		//Waiting for new round
		printf("Round Over! Starting next round in %d seconds\n", secondsPause);

		//send with leaderboard
		sendObservers(!SEND_BOARD, NULL, messBuf, observers, maxObservers, SEND_LEAD, leaderBoard);

		secondsToEnd.tv_sec = secondsPause;
		memset(&board, '0', sizeof(board));


		while(secondsToEnd.tv_sec > 0){ //waiting loop
			resetFDSET(maxParticipants, maxObservers, &sockSet, particSocket, observSocket, participants, observers);

			printf("Pause will end in %d seconds.\n", (int)secondsToEnd.tv_sec);
			if (select(maxSocket + 1, &sockSet, NULL, NULL, &secondsToEnd) < 0){ 
				fprintf(stderr, "Error: Select failed\n" );
			}
			if(FD_ISSET(particSocket, &sockSet)){ //new participant

				newParticpant(participants, maxParticipants, particSocket, &maxSocket);

			}else if (FD_ISSET(observSocket, &sockSet)){ //new observer		

				newObserver(observers, maxObservers, observSocket, &maxSocket);

			}else{

				handleIO(!MOVE_OK, maxParticipants, maxObservers, &sockSet, participants, observers, &maxSocket, particSocket, observSocket, board, leaderBoard);
			
			}
		}
		secondsToEnd.tv_sec = secondsRound;
	
	}
}



//handles IO with the observers and participants
void handleIO(int movesAllowed, int maxParticipants, int maxObservers, fd_set *sockSet, Participant participants[], int observers[], int *maxSocket, int particSocket, int observSocket, char board[DIM][DIM], Participant leaderBoard[]){
	char moveBuf[4], messBuf[MES_SMAL];
	int i=0;

	for(i=0; i< maxParticipants;i++){
		if(FD_ISSET(participants[i].socket, sockSet)){ //if participant

			if (participants[i].name[0]=='!'){ //they are chosing a name
				if(setNameParticipant(participants, i, leaderBoard)){ 
					handleParticDisconnect(participants, i, *sockSet); //disconnected while choosing name
				}

			}else if(recv(participants[i].socket, &moveBuf, sizeof(moveBuf), 0) == 0){ //participant disconnected
				handleParticDisconnect(participants, i, *sockSet);
				*maxSocket = getMaxSocket(participants, observers, maxParticipants, maxObservers, particSocket, observSocket);

			}else if (movesAllowed){ //they have a name and moves are allowed, make move
				moveBuf[sizeof(moveBuf)-1] = '\0';
				if(!checkLegal(board, moveBuf)){//legal
					
					participants[i].points++;
					//printf("%s was a legal move, %s's points are now: %d\n",moveBuf, participants[i].name, participants[i].points);
					snprintf(messBuf, sizeof(messBuf), "%s made valid move %s.\n", participants[i].name, moveBuf);

					sendObservers(SEND_BOARD, board, messBuf, observers, maxObservers, !SEND_LEAD, NULL);
				}else{//illegal
					participants[i].points--;
					//printf("%s was an illegal move, %s's points are now: %d\n",moveBuf, participants[i].name, participants[i].points);
					snprintf(messBuf, sizeof(messBuf), "%s attempted to make an invalid move.\n", participants[i].name);
				
					sendObservers(!SEND_BOARD, NULL, messBuf, observers, maxObservers, !SEND_LEAD, NULL);
				}
				updateLeaderBoard(leaderBoard, participants[i]);

			}

			bzero(&moveBuf, (sizeof(moveBuf)));

		}else if(FD_ISSET(observers[i], sockSet)){ //observer disconnected
			handleObservDisconnect(observers, i, *sockSet);
			*maxSocket = getMaxSocket(participants, observers, maxParticipants, maxObservers, particSocket, observSocket);

		}
	}
}

//resets the fd_set
void resetFDSET(int maxParticipants, int maxObservers, fd_set *sockSet, int particSocket, int observSocket, Participant participants[], int observers[]){
	int tempSock =0;
	int i=0;

	FD_ZERO(sockSet);
	FD_SET(particSocket, sockSet);
	FD_SET(observSocket, sockSet);
	//adds participants 
	for (i=0; i < maxParticipants; i++){
		tempSock = participants[i].socket;
		if(tempSock > 0){
			FD_SET(tempSock, sockSet);
		}
	}
	//adds observers
	for (i=0; i< maxObservers; i++){
		tempSock = observers[i];
		if(tempSock > 0){
			FD_SET(tempSock, sockSet);
		}
	}
}


//sends observers information about the game
void sendObservers(int sendBoard, char board[DIM][DIM], char messBuf[], int observers[], int maxObservers, int sendLead, Participant leaderBoard[]){
	char letter = 'A';
	int i=0,j=0;
	char fullMessage[MES_SIZE];
	bzero(&fullMessage, (sizeof(fullMessage)));
	//char tempMessage[32];
	char *target = fullMessage;
	if(sendBoard){
		target += sprintf(target, " | 1 2 3 | 4 5 6 | 7 8 9 |\n");
		target += sprintf(target, "-+-------+-------+-------+\n");
		for (i=0; i<DIM; i++){
			target += sprintf(target, "%c| ", letter);
			for(j=0; j<DIM; j++){
				if(board[i][j]!='0'){
					target += sprintf(target, "%c ", board[i][j]);
				}else{
					target += sprintf(target, "  ");
				}

				if((j+1)%3==0){
					target += sprintf(target, "| ");
				}
			}
			target += sprintf(target, "\n");
			letter++;
			if((i+1)%3==0){
				target += sprintf(target, "-+-------+-------+-------+\n");
			}
		}
	}

	if(sendLead){
		target += sprintf(target, "Leaderboard:\n");
		i=0;
		while(leaderBoard[i].name[0] != '!' && i<5){
			target += sprintf(target, "%d) %s has %d points\n", i+1, leaderBoard[i].name, leaderBoard[i].points);
			i++;
		}
	}


	strcat(fullMessage, messBuf);



	//printf("%s", fullMessage);



	for (i=0; i< maxObservers; i++){
		if(observers[i] != -1){
			send(observers[i], fullMessage, MES_SIZE, 0);
		}
	}

}

//connects new participant
void newParticpant(Participant participants[], int maxParticipants, int particSocket, int *maxSocket){
	int newSock = -1, i=0;
	struct sockaddr_in cad;
	int alen = sizeof(cad);

	if ( (newSock = accept(particSocket, (struct sockaddr *)&cad, &alen)) < 0) {
		fprintf(stderr, "Error: Participant accept failed\n");
		exit(EXIT_FAILURE); //In industry this would be a terrible idea.
	}

	for (i=0; i< maxParticipants; i++){ //adding new socket to participants
		if(participants[i].socket == -1){ //empty space
			participants[i].socket = newSock;

			if(newSock > *maxSocket){
				*maxSocket = newSock;//set new max socket
			}
			return;
		}
	}
	close(newSock);

}

//connects new observer
void newObserver(int observers[], int maxObservers, int observSocket, int *maxSocket){
	int newSock = -1, i=0;
	struct sockaddr_in cad;
	int alen = sizeof(cad);

	if ( (newSock = accept(observSocket, (struct sockaddr *)&cad, &alen)) < 0) {
		fprintf(stderr, "Error: Observer accept failed\n");
		exit(EXIT_FAILURE);
	}
	for(i=0; i< maxObservers; i++){ //adding new socket to observers
		if(observers[i]== -1){
			observers[i] = newSock;

			if(newSock > *maxSocket){
				*maxSocket = newSock;//set new max socket
			}
			return;
		}
	}
	close(newSock);
}

//updates the leader board with the participant's new points
void updateLeaderBoard(Participant leaderBoard[], Participant partic){
	int i=0;
	Participant temp;

	//find player or empty spot
	for (i=0; i < MAX_LEAD && leaderBoard[i].name[0] != '!' && strcmp(leaderBoard[i].name, partic.name)!=0; i++);

	//no room,doesn't make it into the top MAX_LEAD spots
	if(i==MAX_LEAD){
		//printf("no room\n");
		return;//no room,doesn't make it into the top MAX_LEAD spots
	}

	leaderBoard[i] = partic;

	//if right neighbor is now more, swap with right until this is not the case.
	while(i < MAX_LEAD-1 && leaderBoard[i+1].name[0] != '!' && leaderBoard[i+1].points > leaderBoard[i].points){ 
		//printf("swapping right\n");
		temp = leaderBoard[i+1];
		leaderBoard[i+1] = leaderBoard[i];
		leaderBoard[i] = temp;
		i++;
	}
	//if left neighbor is now less, swap with left until this is not the case.
	while(i > 0 && leaderBoard[i-1].name[0] != '!' && leaderBoard[i-1].points < leaderBoard[i].points){ 
		//printf("swapping left\n");
		temp = leaderBoard[i-1];
		leaderBoard[i-1] = leaderBoard[i];
		leaderBoard[i] = temp;
		i--;
	}


}



//finds the maximum socket
int getMaxSocket(Participant participants[], int observers[], int maxParticipants, int maxObservers, int particSocket, int observSocket){
	int i=0, maxSocket=0;
	if(particSocket > observSocket){ //setting max socket
		maxSocket = particSocket;
	}else{
		maxSocket = observSocket;
	}

	for (i=0; i< maxParticipants; i++){
		if (participants[i].socket > maxSocket){
			maxSocket = participants[i].socket;
		}
	}
	for (i=0; i< maxObservers; i++){
		if (observers[i] > maxSocket){
			maxSocket = observers[i];
		}
	}

	return maxSocket;
}

//returns socket
int getSocketFromPort(uint32_t port){
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold server's address */
	int alen; /* length of address */
	int optval = 1; /* boolean value when we set socket option */
	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */
	sad.sin_addr.s_addr = INADDR_ANY; /* set the local IP address */

	int sd=0; //the socket to be initialized and returned


	if (port > 0) { /* test for illegal value */
		sad.sin_port = htons((u_short)port);
	} else { /* print error message and exit */
		fprintf(stderr,"Error: Bad port number %hu\n",port);
		exit(EXIT_FAILURE);
	}

	/* Map TCP transport protocol name to protocol number */
	if ( ((long int)(ptrp = getprotobyname("tcp"))) == 0) {
		fprintf(stderr, "Error: Cannot map \"tcp\" to protocol number");
		exit(EXIT_FAILURE);
	}

	//struct sockaddr_in cad; /* structure to hold client's address */

	/* Create a socket */
	sd = socket(PF_INET, SOCK_STREAM, ptrp->p_proto);
	if (sd < 0) {
		fprintf(stderr, "Error: Socket creation failed\n");
		exit(EXIT_FAILURE);
	}

	/* Allow reuse of port - avoid "Bind failed" issues */
	if( setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0 ) {
		fprintf(stderr, "Error Setting participant socket option failed\n");
		exit(EXIT_FAILURE);
	}

	/* Bind a local address to the socket */
	if (bind(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"bind participant failure\n");
		exit(EXIT_FAILURE);
	}

	/* Specify size of request queue */
	if (listen(sd, QLEN) < 0) {
		fprintf(stderr,"Error: participant listen failed\n");
		exit(EXIT_FAILURE);
	}

	return sd;
}


//returns a blank participant
Participant getBlankParticipant(){
	Participant participant;
	participant.socket = -1;
	participant.points = 0;
	participant.name[0] = '!';
	//strcpy(participant.name, '\0');

	return participant;
}

//handles the participant disconnection
void handleParticDisconnect(Participant participants[], int i, fd_set sockSet){
	printf("Disconnecting %s\n", participants[i].name);
	FD_CLR(participants[i].socket, &sockSet);
	close(participants[i].socket);
	participants[i] = getBlankParticipant();
}

//handles the observer disconnnection
void handleObservDisconnect(int observers[], int i, fd_set sockSet){
	printf("Disconnecting observer %d\n", i);
	FD_CLR(observers[i], &sockSet);
	close(observers[i]);
	observers[i] = -1;
}

//asks the participant at index i for their name
int setNameParticipant(Participant participants[], int i, Participant leaderBoard[]){
	char nameBuf[MAX_NAME];
	nameBuf[0] = '!';
	int j=0;
	char resName = 'N';

	//printf("Getting name\n");
	if(recv(participants[i].socket, &nameBuf, sizeof(nameBuf), 0) <= 0){
		return 1; //participant disconnected while asking for a name
	}
	for(i=0; i< MAX_LEAD; i++){
		if(leaderBoard[i].name[0] == '!'){ //if it's not a name already used
			for(j=0; j< strlen(nameBuf); j++){ //check for non alphanumeric characters
				if(!isalpha(nameBuf[j]) && nameBuf[j]!= '_' && nameBuf[j]!= ' '){
					break;
				}
			}
			resName = 'Y';
			break;

		}else if(strcmp(leaderBoard[i].name, nameBuf)==0){
			resName = 'N';
			break;
		}
	}


	if(resName == 'Y'){
		strcpy(participants[i].name, nameBuf);
	}

	//printf("%c, Name was: %s\n", resName, nameBuf);

	send(participants[i].socket, &resName, sizeof(resName), 0);
	return 0;
}

//returns 0 if move is legal, 1 otherwise
int checkLegal(char board[DIM][DIM], char place[3]){
	int x=0,y=0,num=0;
	int i=0,j=0;
	int lowX=0, lowY=0;


	place[0] = toupper(place[0]);
	y = place[0]-'A';
	x = place[1]-'0'-1;
	num = place[2];

	if(board[y][x] != '0'){ //already something placed here
		return 1;
	}

	if(x>DIM || x<0 || y>DIM || y<0 || num-'0'>DIM || num-'0'<1){ //within bounds of board
		return 1;
	}

	for(i=0; i< DIM; i++){ //isn't in row
		if(board[i][x]== num){
			//printf("Already in row\n");
			return 1;
		}
	}
	for(j=0; j< DIM; j++){ //isn't in column
		if(board[y][j]==num){
			//printf("Already in column\n");
			return 1;
		}
	}

	//below: if isn't in block
	if(x < DIM/3){ 
		lowX=0;
	} else if( x < 2*DIM/3){
		lowX=DIM/3;
	} else{
		lowX=2*DIM/3;
	}

	if(y < DIM/3){
		lowY=0;
	} else if( y < 2*DIM/3){
		lowY=DIM/3;
	} else{
		lowY=2*DIM/3;
	}

	for(i=lowY; i<lowY+DIM/3; i++){
		for(j=lowX; j<lowX+DIM/3; j++){
			if (board[i][j]==num){
				//printf("Already in square \n");
				return 1;
			}
		}
	}
	board[y][x] = num;
	return 0;
}

