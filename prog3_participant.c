#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define DIM 9
#define MAX_NAME 31
#define MAX_LINE 1024

/*------------------------------------------------------------------------
* Program: Participant for sudoku game
*
* Purpose: Plays sudoku game
*
* Syntax: ./Participant server_address participant_port
*
* Author: Ian Hammerstrom
* Date: 11/24/15
*
*------------------------------------------------------------------------
*/


void printBoard(char board[DIM][DIM]);

int main( int argc, char **argv) {
	struct hostent *ptrh; /* pointer to a host table entry */
	struct protoent *ptrp; /* pointer to a protocol table entry */
	struct sockaddr_in sad; /* structure to hold an IP address */
	int sd; /* socket descriptor */
	u_short port; /* protocol port number */
	char *host; /* pointer to host name */
	int n=1; /* number of characters read */



	memset((char *)&sad,0,sizeof(sad)); /* clear sockaddr structure */
	sad.sin_family = AF_INET; /* set family to Internet */

	if( argc != 3 ) {
		fprintf(stderr,"Error: Wrong number of arguments\n");
		fprintf(stderr,"usage:\n");
		fprintf(stderr,"./Participant server_address participant_port\n");
		exit(EXIT_FAILURE);
	}

	port = (u_short)atoi(argv[2]); /* convert to binary */
	if (port > 0) /* test for legal value */
		sad.sin_port = htons((u_short)port);
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

	/* Connect the socket to the specified server. */
	if (connect(sd, (struct sockaddr *)&sad, sizeof(sad)) < 0) {
		fprintf(stderr,"connect failed\n");
		exit(EXIT_FAILURE);
	}


	char nameBuf[MAX_NAME];
	char moveBuf[5];
	char resName;

	
	printf("Welcome to Sudoku.\n");	
	//name selection loop
	do{
		printf("Please enter a name of up to 30 alphanumeric characters\n");
		fgets(nameBuf, MAX_LINE, stdin);
		nameBuf[strlen(nameBuf)-1] = '\0';

		send(sd, &nameBuf, sizeof(nameBuf)-1, 0);
		if(recv(sd, &resName, sizeof(resName), 0)==0){
			printf("Recv Fail\n");
			exit(EXIT_FAILURE);
		}
	}while (resName != 'Y');

	//move loop
	do{
		printf("Please enter your move.\n");
		//scanf("%s", &moveBuf);
		fgets(moveBuf, sizeof(moveBuf), stdin);
		moveBuf[strlen(moveBuf) -1] = '\0';
		printf("moveBuf is %s\n", moveBuf);

	}while(send(sd, &moveBuf, 3, 0) > 0);
	
	close(sd);
	exit(EXIT_SUCCESS);
}


//prints the board
void printBoard(char board[DIM][DIM]){
	char letter = 'A';
	int i=0,j=0;

	printf(" | 1 2 3 | 4 5 6 | 7 8 9 |\n");
	printf("-+-------+-------+-------+\n");
	for (i=0; i<DIM; i++){
		printf("%c| ", letter);

		for(j=0; j<DIM; j++){
			if(board[i][j]!='0'){
				printf("%c ", board[i][j]);
			}else{
				printf("  ");
			}

			if((j+1)%3==0){
				printf("| ");
			}
		}
		putchar('\n');
		letter++;
		if((i+1)%3==0){
			printf("-+-------+-------+-------+\n");
		}
	}


}