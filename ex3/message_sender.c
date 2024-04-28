#include "message_slot.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv)
{
	if (argc != 4) {
		perror("Not enough arguments");
		exit(1);
	}
	
	char* file_path = argv[1];
	int channel_id = atoi(argv[2]);
	char* message = argv[3];
	int msglen = strlen(message);
	
	int file_desc = open(file_path, O_WRONLY);
	if(file_desc < 0) 
	{
		// printf("Can't open device file: %s\n", DEVICE_FILE_NAME);
		perror("open() failed");
		exit(1);
	}

	if (ioctl(file_desc, MSG_SLOT_CHANNEL, channel_id) == -1) {
		perror("ioctl() failed");
		exit(1);
	}
	
	if (write(file_desc, message, msglen) != msglen) {
		perror("write() failed or partial writing");
		exit(1);
	}
	
	close(file_desc);
	
	exit(0);
}
