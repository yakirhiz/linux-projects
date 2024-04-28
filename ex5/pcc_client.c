#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#define FAILURE 1

int main(int argc, char** argv) {
	
	if (argc != 4) {
		perror("Not enough arguments.\n");
		exit(FAILURE);
	}
	
	char* path = argv[3];
	
	FILE *file = fopen(path, "r");
	
	if (file == NULL) {
		perror("Client [Error]: Connot open the file");
		exit(FAILURE);
	}
	
	fseek(file, 0L, SEEK_END);
	uint32_t N = (uint32_t) ftell(file);
	fseek(file, 0L, SEEK_SET);
	
	char *buff = malloc(sizeof(char) * N);
	
	if (buff == NULL) {
		perror("Client [Error]: Memory allocation failed");
		exit(FAILURE);
	}
	
	if (fread(buff, sizeof(char), N, file) != N) {
		perror("Client [Error]: Error while reading");
		exit(FAILURE);
	}
	
	fclose(file);
	
	int  sockfd = -1;

	struct sockaddr_in serv_addr; // where we want to get to

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("Client [Error]: Could not create socket");
		exit(FAILURE);
	}

	memset(&serv_addr, 0, sizeof(serv_addr));
	
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[2])); // Note: htons for endiannes
	//serv_addr.sin_addr.s_addr = inet_addr(argv[1]);
	inet_pton(AF_INET, argv[1], &serv_addr.sin_addr.s_addr);

	// Note: what about the client port number?
	
	if (connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("Client [Error]: Connection failed");
		exit(FAILURE);
	}
	
	// Send N
	uint32_t N_network = (htonl(N));
	void *N_sent = (void*) &N_network;
	int bytes_sent = 0;
	
	while (bytes_sent < 4) {
		int bytes = send(sockfd, N_sent + bytes_sent, 4 - bytes_sent, 0);
		
		if (bytes < 0) {
			perror(" Error while sending N");
        	exit(FAILURE);
		}
		
		bytes_sent += bytes;
	}
	
	// Send content
	bytes_sent = 0;
	
	while (bytes_sent < N){
		int bytes = send(sockfd, buff + bytes_sent, N - bytes_sent, 0);
		
		if (bytes < 0) {
			perror(" Error while sending content");
        	exit(FAILURE);
		}
		
		bytes_sent += bytes;
	}
	
	uint32_t C_network;
	void *C_recv = (void*) &C_network;
	int bytes_received = 0;
	
	while(bytes_received < 4) {
		int bytes = recv(sockfd, C_recv + bytes_received, 4 - bytes_received, 0);
		
		if (bytes < 0) {
			perror(" Error while fetching C");
        	exit(FAILURE);
		}
		
		bytes_received += bytes;
	}
	
	uint32_t C = ntohl(C_network);
	
	printf("# of printable characters: %u\n", C);
	
	return 0;
}