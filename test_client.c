#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#include "ioctl_cmd.h" // IOCTL commands

#define DEVICE "/dev/test_dev"
#define MAILSLOT_STORAGE 8
#define MAXIMUM_MESSAGE_SIZE 512
#define VERSION "1.0"


int main() {

	int i, result, pid;
	char string4[] = "the";
	char string5[] = "this";
	char string6[] = "hello";
	char* buffer4 = malloc(4*sizeof(char)); memset(buffer4, 0, 4*sizeof(char));
	char* buffer5 = malloc(5*sizeof(char)); memset(buffer5, 0, 5*sizeof(char));
	char* buffer6 = malloc(6*sizeof(char)); memset(buffer6, 0, 6*sizeof(char));

	system("reset");
	setbuf(stdout, NULL);
	printf("**  TEST SUITE FOR LINUX MAILSLOT V. %s **\n\n", VERSION);

	int file_descriptor = open(DEVICE, O_RDWR);


	/* SETTING VARIOUS MAXIMUM_MSG_SIZE */
	
	printf("Setting the new maximum message size to -10... [it should fail]\n");
	result = ioctl(file_descriptor, SET_MAXIMUM_MSG_SIZE, -10); 
	result < 0 ? printf("[failed] You can't set the maximum message size to -10!\n\n") : printf("Something went wrong 1\n\n");

	printf("Setting the new maximum message size to 0... [it should fail]\n");
	result = ioctl(file_descriptor, SET_MAXIMUM_MSG_SIZE, 0); 
	result < 0 ? printf("[failed] You can't set the maximum message size to 0!\n\n") : printf("Something went wrong 2\n\n");

	printf("Setting the new maximum message size to 64... [it should be ok]\n");
	result = ioctl(file_descriptor, SET_MAXIMUM_MSG_SIZE, 64); 
	result < 0 && MAXIMUM_MESSAGE_SIZE >= 64 ? printf("Something went wrong 3\n\n") : printf("[ok]\n\n");

	printf("Setting the new maximum message size to %d... [it should be ok]\n", MAXIMUM_MESSAGE_SIZE);
	result = ioctl(file_descriptor, SET_MAXIMUM_MSG_SIZE, MAXIMUM_MESSAGE_SIZE);	
	result < 0 ? printf("Something went wrong 4\n\n") : printf("[ok]\n\n");

	printf("Setting the new maximum message size to 2000... [it should fail]\n");
	result = ioctl(file_descriptor, SET_MAXIMUM_MSG_SIZE, 2000);
	result < 0 ? printf("[failed] You can't set the maximum message size to 2000!\n\n") : printf("Something went wrong 5\n\n");


	/* SETTING NON-BLOCKING AND BLOCKING POLICY */	

	printf("Setting non-blocking policy... [it should be ok]\n");
	result = ioctl(file_descriptor, SET_NONBLOCKING); result < 0 ? printf("Something went wrong 6\n\n") : printf("[ok]\n\n"); 

	printf("Setting blocking policy... [it should be ok]\n");
	result = ioctl(file_descriptor, SET_BLOCKING); result < 0 ? printf("Something went wrong 7\n\n") : printf("[ok]\n\n");


	/* SENDING A NON REGISTERED IOCTL COMMAND */
	
	printf("Sending a non-registered IOCTL command... [it should fail]\n");
	result = ioctl(file_descriptor, 9);
	result < 0 ? printf("[failed] You can't send an unknown IOCTL command!\n\n") : printf("Something went wrong 8\n\n");

 
	/* WRITE A SIMPLE STRING AND READ IT TO VIEW IF IT IS CORRECTLY RETRIEVED */

	printf("Write a simple string and read it to view if it is correctly retrieved... [it should be ok]\n");
	
	result = write(file_descriptor, &string5, sizeof(string5)); // write string5
	if (result == -1) printf("Something went wrong 9\n");

	result = read(file_descriptor, buffer5, sizeof(buffer5)); // read string5
	if (result == -1) printf("Something went wrong 10\n\n");
	else { 
		printf("String sent: \"%s\" String received: \"%s\" ", string5, buffer5); 
		if( strncmp(string5, buffer5, sizeof(string5)) == 0 ) printf("--> Match!\n[ok]\n\n");
	}
	
	/* READ AND WRITE WITH NON CONVENTIONAL PARAMETERS */		
	
	result = write(file_descriptor, &string5, sizeof(string5)); if (result == -1) printf("Something went wrong 11\n"); // write string5	
	
	// read 0 bytes
	printf("Call read syscall with length setted to 0 bytes... [it should fail]\n");
	result = read(file_descriptor, &string5, 0); // read string5	
	result == -1 ? printf("[failed] You can't read a 0 byte message!\n\n") : printf("Something went wrong 12\n\n");
	
	// read with NULL parameter (bad address)
	printf("Call read syscall with buffer setted to NULL... [it should fail]\n");
	result = read(file_descriptor, NULL, sizeof(buffer5)); // read string5
	result == -1 ? printf("[failed] You can't copy the message on this address!\n\n") : printf("Something went wrong 13\n\n");
	// clean..
	result = read(file_descriptor, buffer4, sizeof(buffer4)); if(result == -1) printf("Something went wrong 14\n"); // delete string4	
		
	// write 0 bytes
	printf("Call write syscall with length setted to 0 bytes... [it should fail]\n");
	result = write(file_descriptor, &string5, 0); // write string5
	result == -1 ? printf("[failed] You can't write a 0 byte message!\n\n") : printf("Something went wrong 15\n\n");	
	
	// write with NULL parameter (bad address)
	printf("Call write syscall with buffer setted to NULL... [it should fail]\n");
	result = write(file_descriptor, NULL, sizeof(string5)); // write string5	
	result == -1 ? printf("[failed] You can't write a message to a NULL buffer!\n\n") : printf("Something went wrong 16\n\n");


	/* BOUNDARY VALUE ANALYSIS ON WRITING */

	printf("Boundary Value Analysis on writing...\n");

	result = ioctl(file_descriptor, SET_MAXIMUM_MSG_SIZE, 5); if (result < 0) printf("Something went wrong 17\n");

	result = write(file_descriptor, &string4, sizeof(string4)); // write string4	
	result == -1 ? printf("\tSomething went wrong 18\n") : printf("\t[ok]\n");

	result = write(file_descriptor, &string5, sizeof(string5)); // write string5
	result == -1 ? printf("\tSomething went wrong 19\n") : printf("\t[ok]\n");

	result = write(file_descriptor, &string6, sizeof(string6)); // not writable
	result == -1 ? printf("\t[failed] You can't write 6 bytes on a maximum of 5!\n") : printf("\tSomething went wront 20\n");

	// clean..
	result = read(file_descriptor, buffer4, sizeof(buffer4)); if(result == -1) printf("Something went wrong 21\n"); // delete string4
	result = read(file_descriptor, buffer5, sizeof(buffer5)); if(result == -1) printf("Something went wrong 22\n"); // delete string5


	/* BOUNDARY VALUE ANALYSIS ON READING (BUFFER LIMITS) */
	
	printf("\nBoundary Value Analysis on reading...\n");

	result = ioctl(file_descriptor, SET_MAXIMUM_MSG_SIZE, 10); if (result < 0) printf("Something went wrong 23\n");
	
	result = write(file_descriptor, &string5, sizeof(string5)); if (result == -1) printf("Something went wrong 24\n"); // write string5

	result = write(file_descriptor, &string5, sizeof(string5)); if (result == -1) printf("Something went wrong 25\n"); // write string5

	result = write(file_descriptor, &string5, sizeof(string5)); if (result == -1) printf("Something went wrong 26\n"); // write string5

	result = read(file_descriptor, buffer4, 4); // not readable	
	result == -1 ? printf("\t[failed] You can't copy 5 bytes on a buffer of 4!\n") : printf("\tSomething went wront 27\n");

	result = read(file_descriptor, buffer5, 5);	result == -1 ? printf("\tSomething went wrong 28\n") : printf("\t[ok]\n"); // read string5

	result = read(file_descriptor, buffer6, 6);	result == -1 ? printf("\tSomething went wrong 29\n") : printf("\t[ok]\n"); // read string5
	
	result = read(file_descriptor, buffer6, sizeof(buffer6)); if(result == -1) printf("\tSomething went wrong 30\n"); // clean
	

	/* OVER-FILL THE MAILSLOT WITH A NON-BLOCKING POLICY */

	result = ioctl(file_descriptor, SET_NONBLOCKING); if(result < 0) printf("Something went wrong 31\n");

	printf("\nFill the mailslot..\n\tPolicy: non-blocking\n\tLimit: %d   [only the last should fail]\n\n", MAILSLOT_STORAGE);
 
	for( i = 0; i <= MAILSLOT_STORAGE; i++ )
		write(file_descriptor, &string5, sizeof(string5)) != -1 ? printf("\twrite #%d [ok]\n", i+1) : printf("\twrite #%d [failed]\n", i+1); // write string5;


	/* OVER-EMPTY THE MAILSLOT WITH A NON-BLOCKING POLICY */

	result = ioctl(file_descriptor, SET_NONBLOCKING); if(result < 0) printf("Something went wrong 32\n"); 

	printf("\nEmpty the mailslot..\n\tPolicy: non-blocking\n\tLimit: %d   [only the last should fail]\n\n", MAILSLOT_STORAGE);

	for( i = 0; i <= MAILSLOT_STORAGE; i++ )
		read(file_descriptor, buffer5, sizeof(buffer5)) != -1 ? printf("\tread #%d [ok]\n", i+1) : printf("\tread #%d [failed]\n", i+1); // read string5;


	/* FILL THE MAILSLOT WITH A BLOCKING POLICY */ 
	// with blocking policy I can test only the correctness of writing/reading in this mode, but I can't overfill or over-empty because I will get stuck
	result = ioctl(file_descriptor, SET_BLOCKING); if(result < 0) printf("Something went wrong 33\n");

	printf("\nFill the mailslot..\n\tPolicy: blocking\n\tLimit: %d   [should be everything ok]\n\n", MAILSLOT_STORAGE);
 
	for( i = 0; i < MAILSLOT_STORAGE; i++ )
		write(file_descriptor, &string5, sizeof(string5)) != -1 ? printf("\twrite #%d [ok]\n", i+1) : printf("\twrite #%d [failed]\n", i+1); // write string5;


	/* EMPTY THE MAILSLOT WITH A BLOCKING POLICY */
	// with blocking policy I can test only the correctness of writing/reading in this mode, but I can't overfill or over-empty because I will get stuck
	
	printf("\nEmpty the mailslot..\n\tPolicy: blocking\n\tLimit: %d   [should be everything ok]\n\n", MAILSLOT_STORAGE);

	for( i = 0; i < MAILSLOT_STORAGE; i++ )
		read(file_descriptor, buffer5, sizeof(buffer5)) != -1 ? printf("\tread #%d [ok]\n", i+1) : printf("\tread #%d [failed]\n", i+1); // read string5;
		

	/* TEST READ/WRITE ALTERNATE IN BLOCKING MODE WITH THE HELP OF THE FORK SYSCALL */
	
	result = ioctl(file_descriptor, SET_BLOCKING);	if(result < 0)	printf("Something went wrong 34\n"); 

	printf("\nWrite and read the mailslot concurrently (2 processes)..\n\tPolicy: blocking [they have to run in interleaving mode else the program get stuck]\n\n");

	pid = fork();

	if(pid > 0) { // father process
		for(i=0; i<2*MAILSLOT_STORAGE; i++)
			write(file_descriptor, &string6, sizeof(string6)) != -1 ? printf("\twrite #%d [ok]\n", i+1) : printf("\tSomething went wrong 35\n"); // write string6
		printf("\t*** The writer has finisced ***\n");
	}
	else if(pid == 0) { // child process  
		sleep(2);
		for(i=0; i<2*MAILSLOT_STORAGE; i++)
			read(file_descriptor, buffer6, sizeof(buffer6)) != -1 ? printf("\tread #%d [ok]\n", i+1) : printf("\tSomething went wrong 36\n"); // read string6
		printf("\t*** The reader has finished ***\n");
	}
	else printf("\tSomething went wrong 37\n");


	free(buffer4); free(buffer5); free(buffer6);
	close(file_descriptor);
	return 0;

}
