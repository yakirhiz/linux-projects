#define _GNU_SOURCE // TODO: Check if needed

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
#include <pthread.h>
#include <limits.h>
#include <stdatomic.h>

#define SUCCESS 0
#define FAILURE 1

int is_searchable(char* path);
void search_dir(char* path);
void* searching_thread(void*);
void q_remove_head(char* path);
void q_insert_last(char* path);

typedef struct Directory {
	char path[PATH_MAX];
	struct Directory* next;
} Directory;

struct Queue {
	Directory *head, *tail;
} queue;

char* term;
atomic_int num_threads;
atomic_int ready_threads = 0;
atomic_int count = 0;
atomic_int num_threads_waiting = 0;
atomic_int num_threads_error = 0;

pthread_mutex_t threads_mutex;
pthread_mutex_t queue_mutex;

pthread_cond_t start_threads;
pthread_cond_t main_start_threads;
pthread_cond_t insert;

int main(int argc, char** argv) {
	if (argc != 4) {
		fprintf(stderr, "Not enough arguments\n");
		exit(FAILURE);
	}
	
	char* root = argv[1];
	term = argv[2];
	num_threads = atoi(argv[3]);
	
	if (!is_searchable(root)) { // Check if root is searchable
		perror("Root is not searchable");
		exit(FAILURE);
	}
	
	// Initialize the mutex'es
	pthread_mutex_init(&threads_mutex, NULL);
	pthread_mutex_init(&queue_mutex, NULL);
	
	// Initialize the conditional variables
	pthread_cond_init(&start_threads, NULL);
	pthread_cond_init(&main_start_threads, NULL);
	pthread_cond_init(&insert, NULL);
	
	q_insert_last(root);
	
	pthread_t threads[num_threads];
	
	pthread_mutex_lock(&threads_mutex);
	
	for (long i = 0; i < num_threads; ++i) {
		if (pthread_create(&threads[i], NULL, &searching_thread, (void*) NULL) != 0) {
			perror("Thread creation failed\n");
			exit(FAILURE);
		}
	}
	
	pthread_cond_wait(&main_start_threads, &threads_mutex);
	pthread_cond_broadcast(&start_threads);
	pthread_mutex_unlock(&threads_mutex);
	
	for (long i = 0; i < num_threads; ++i) {
		if (pthread_join(threads[i], (void**) NULL) != 0) {
			perror("pthread_join() failed\n");
			exit(FAILURE);
		}
	}
	
	// Destroy the mutex'es
	pthread_mutex_destroy(&threads_mutex);
	pthread_mutex_destroy(&queue_mutex);
	
	// Destroy the conditional variables
	pthread_cond_destroy(&start_threads);
	pthread_cond_destroy(&main_start_threads);
	pthread_cond_destroy(&insert);
	
	printf("Done searching, found %d files\n", count);
	
	if(num_threads_error > 0) {
		return FAILURE;
	}
	
	return SUCCESS;
}

void* searching_thread(void* arg) {
	
	pthread_mutex_lock(&threads_mutex);
	
	++ready_threads;
	
	if (ready_threads == num_threads) {
		pthread_cond_broadcast(&main_start_threads);
	}
	
	pthread_cond_wait(&start_threads, &threads_mutex);

	pthread_mutex_unlock(&threads_mutex);
	
	char path[PATH_MAX];
	
	for(;;) {
		q_remove_head(path);
		search_dir(path);
	}
	
	return SUCCESS;
}

void search_dir(char* path) {
	DIR* dir = opendir(path); // Open the directory
	
	if (!strcmp(path, ""))
		return;
	
	if (dir == NULL) {
		++num_threads_error;
		perror("Could not open a directory");
		pthread_exit(NULL);
	}
	
	char fullpath[PATH_MAX];
	struct dirent* entry; // Directory entry
	
	while ((entry = readdir(dir)) != NULL) // While directory contains files
	{
		sprintf(fullpath, "%s/%s", path, entry->d_name);
		
		if (entry->d_type == DT_DIR) {
			if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
				
			} else if (!is_searchable(entry->d_name)) {
				printf("Directory %s: Permission denied.\n", fullpath);
			} else {
				q_insert_last(fullpath);
			}
		} else if (strstr(entry->d_name, term) != NULL) { // Name contains search term
				printf("%s\n", fullpath);
				++count;
		}
	}
	
	closedir(dir); // Close the directory
}

void q_insert_last(char* path) {
	Directory* directory = malloc(sizeof(Directory));
	if(directory == NULL){
		++num_threads_error;
		perror("Allocation Failed\n");
		pthread_exit(NULL);
	}
	
	strcpy(directory->path, path);
	directory->next = NULL;
	
	pthread_mutex_lock(&queue_mutex);
	
	if (queue.head == NULL) {
		queue.head = directory;
		queue.tail = directory;
		
		pthread_cond_signal(&insert);
	} else {
		queue.tail->next = directory;
		queue.tail = directory;
	}
	
	pthread_mutex_unlock(&queue_mutex);
}

void q_remove_head(char* path) {
	
	pthread_mutex_lock(&queue_mutex);
	
	while (queue.head == NULL) {
		++num_threads_waiting;
		
		if (num_threads_waiting + num_threads_error == num_threads) {
			
			pthread_cond_broadcast(&insert);
			pthread_mutex_unlock(&queue_mutex);
			pthread_exit(NULL);
		}
		
		pthread_cond_wait(&insert, &queue_mutex);
		
		if (num_threads_waiting + num_threads_error == num_threads) {
			
			pthread_mutex_unlock(&queue_mutex);
			pthread_exit(NULL);
		}
		
		--num_threads_waiting;
	}
	
	if (queue.head != NULL && queue.head->path != NULL) {
		strcpy(path, queue.head->path);
			
		queue.head = queue.head->next;
		
		if(queue.head == NULL) { // Didn't free previous head
			queue.tail = NULL;
		}
	}

	pthread_mutex_unlock(&queue_mutex);
}

int is_searchable(char* path) {
	if (access(path, R_OK | X_OK) == -1 && errno == EACCES)
		return 0;
	else
		return 1;
}