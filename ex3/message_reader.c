#include "message_slot.h"    

#include <fcntl.h>      /* open */ 
#include <unistd.h>     /* exit */
#include <sys/ioctl.h>  /* ioctl */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char** argv)
{
	char buf[BUF_LEN];
	
	if (argc != 3) {
		perror("Not enough arguments");
		exit(1);
	}
	
	char* file_path = argv[1];
	int channel_id = atoi(argv[2]);

	int file_desc = open(file_path, O_RDONLY);
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
	
	int num_of_bytes = read(file_desc, buf, BUF_LEN);
	if (num_of_bytes == -1) {
		perror("read() failed");
		exit(1);
	}
	
	close(file_desc);
	
	if (write(1, buf, num_of_bytes) != num_of_bytes) { // MAYBE: Check if equal to num_of_bytes
		perror("write() failed or partial writing");
		exit(1);
	}
	
	exit(0);
}
