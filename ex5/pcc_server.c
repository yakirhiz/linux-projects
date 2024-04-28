#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

#include <signal.h>

#define FAILURE 1

struct {
	uint32_t cc[95];
} pcc_total;

int exit_flag = 0;
int connfd    = -1;

void print_total_characters_count();

void signal_handler(int signalNum) {
	if(connfd < 0) {
		print_total_characters_count();
		exit(0);
    } else { 
        exit_flag = 1;
    }
}

int main(int argc, char** argv) {
	
	if (argc != 2) {
		perror("Not enough arguments.");
		exit(FAILURE);
	}
	
	/* Register the handler for SIGUSR1 */
	struct sigaction sigusr1;
	sigusr1.sa_handler = &signal_handler;
	sigusr1.sa_flags = SA_RESTART;
	if (sigaction(SIGUSR1, &sigusr1, NULL) == -1) {
		perror("Error while executing sigaction()");
		exit(1);
	}

	int listenfd  = -1;

	struct sockaddr_in serv_addr;
	struct sockaddr_in peer_addr;
	socklen_t addrsize = sizeof(struct sockaddr_in);
	
	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("\n Error : Could not create socket \n");
		exit(FAILURE);
	}
	
	int yes = 1;
	if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        fprintf(stderr, "setsockopt Error: %s\n", strerror(errno));
        exit(FAILURE);
    }
	
	memset(&serv_addr, 0, addrsize);

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[1]));
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY); // INADDR_ANY = any local machine address
	
	if(bind(listenfd, (struct sockaddr*) &serv_addr, addrsize) != 0) {
		perror("Error : Bind Failed.");
		exit(FAILURE);
	}

	if(listen(listenfd, 10) != 0) {
		printf("Error : Listen Failed.\n");
		exit(FAILURE);
	}
	
	while (1) {
		// Can use NULL in 2nd and 3rd arguments
		// but we want to print the client socket details
		connfd = accept(listenfd, (struct sockaddr*) &peer_addr, &addrsize);

		if (connfd < 0) {
			printf("Error : Accept Failed.\n");
			exit(FAILURE);
		}
		
		// Receive N
		uint32_t N_network;
		void *N_recv = (void*) &N_network;
		int bytes_received = 0;
		
		while(bytes_received < 4) {
			int bytes = recv(connfd, N_recv + bytes_received, 4 - bytes_received, 0);
			
			if (bytes == -1 && (errno == ETIMEDOUT ||
				errno == ECONNRESET || errno == EPIPE)) {
				perror("Server: Error while fetching N");
				
                close(connfd);
                connfd = -1;
				continue;
			} else if (bytes == -1) {
				perror("Server: Critical error");
				exit(FAILURE);
			}
			
			if (bytes == 0) {
				perror("Server: Unexpected connection close");
				
                close(connfd);
                connfd = -1;
				continue;
			}
			
			bytes_received += bytes;
		}
		
		uint32_t N = ntohl(N_network);
		
		// Receive content
		char *buff = malloc(sizeof(char) * (N + 1));
		bytes_received = 0;
		while(bytes_received < N) {
			int bytes = recv(connfd, buff + bytes_received, N - bytes_received, 0);
			
			if (bytes == -1 && (errno == ETIMEDOUT ||
				errno == ECONNRESET || errno == EPIPE)) {
				perror("Server: Error while fetching N");
				
                close(connfd);
                connfd = -1;
				free(buff);
				continue;
			} else if (bytes == -1) {
				perror("Server: Critical error");
				exit(FAILURE);
			}
			
			if (bytes == 0) {
				perror("Server: Unexpected connection close");
				
                close(connfd);
                connfd = -1;
				free(buff);
				continue;
			}
			
			bytes_received += bytes;
		};
		buff[N] = '\0';
		
		// Send C
		uint32_t C = 0;
		uint32_t pcc_tmp[95] = {0};
		
		for (int i = 0; i < N; ++i) {
			char ch = *(buff + i);
			
			if (ch >= 32 && ch <= 126) {
				pcc_tmp[ch - ' ']++;
				C++;
			}
		}
		
		uint32_t C_network = (htonl(C));
		void *C_sent = (void*) &C_network;
		int bytes_sent = 0;
		
		while(bytes_sent < 4) {
			int bytes = send(connfd, C_sent + bytes_sent, 4 - bytes_sent, 0);
			
			if (bytes == -1 && (errno == ETIMEDOUT ||
				errno == ECONNRESET || errno == EPIPE)) {
				perror("Server: Error while fetching N");
				
                close(connfd);
                connfd = -1;
				continue;
			} else if (bytes == -1) {
				perror("Server: Critical error");
				exit(FAILURE);
			}
			
			bytes_sent += bytes;
		}

		for (int i = 0; i < 95; i++) {
			pcc_total.cc[i] += pcc_tmp[i];
		}
		
		close(connfd); // Close connection
		connfd    = -1;
		
		if (exit_flag) {
            break;
        }
	}
	
	print_total_characters_count();
	
	return 0;
}

// Print the total amount of each character sent to the server
void print_total_characters_count() {
	for (int i = 0; i < 95; i++)
		printf("char '%c' : %u times\n", (i + ' '), pcc_total.cc[i]);
}