#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>

static void error(); // Print errors to stderr.
static int regular(char **arglist); // Execture regular command.
static int background(int count, char **arglist); // Execture background command.
static int pipe_process(int count, int separator, char **arglist); // Execture commands with pipe.
static int redirect(int count, int separator, char **arglist); // Execture command with redirect of output.
void restore_default_handler(); // Restore the SIGINT handler to the default.
void handler_chld(int pid); // Handler for SIGCHLD signal in the main shell.


void handler_chld(int pid) {
	int ret;
	
	// Wait for all the processes that was finished.
	while ((ret = waitpid(-1, NULL, WNOHANG)) > 0) {
		
	}
	
	if(ret == -1 && errno != ECHILD) {
		error("Error while executing waitpid(). 1");
		exit(1);
	}
}

void restore_default_handler() {
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		error("Error while executing sigaction().");
		exit(1);
	}
}

int prepare(void) {
	// Ignore from SIGINT (CTRL+C).
	struct sigaction ign;
	ign.sa_handler = SIG_IGN;
	if (sigaction(SIGINT, &ign, NULL) == -1) {
		error("Error while executing sigaction().");
		return 1;
	}
	
	// Make handler for the SIGCHLD signal.
	struct sigaction chld;
	chld.sa_handler = &handler_chld;
	chld.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &chld, NULL) == -1) {
		error("Error while executing sigaction().");
		return 1;
	}
	
	return 0;
}

int finalize(void) {
	return 0;
}

int process_arglist(int count, char **arglist) {
	// Background process case.
	if (!strcmp(arglist[count - 1], "&")) {
		arglist[count - 1] = NULL;
		return background(count - 1, arglist);
	}
	
	// piped processes case.
	for (int i = 0; i < count; i++) {
		if (!strcmp(arglist[i], "|")) {
			arglist[i] = NULL;
			return pipe_process(count, i, arglist);
		}
	}
	
	// Process output redirection case.
	for (int i = 0; i < count; i++) {
		if (!strcmp(arglist[i], ">")) {
			arglist[i] = NULL;
			return redirect(count, i, arglist);
		}
	}

	// Regular process invocation.
	return regular(arglist);
}

static int redirect(int count, int separator, char **arglist) {
	int pid = fork();
	
	if (pid < 0) {
		error("Error while executing fork().");
		return 0;
	}
	else if (pid == 0) {	
		restore_default_handler();
		
		// Open the file (where the output goes) for writing and truncate it, create if needed
		int fd = open(arglist[count - 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if(fd == -1) {
			exit(1); 
		}
		
		if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1) {
			error("Error while executing dup2().");
			exit(1);
		}
		close(fd);
		
		if(execvp(arglist[0], arglist) == -1) {
			error("Error while execute execvp().");
			exit(1);
		}
	}
	
	// Wait fot process to finish
	if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD) {
		error("While trying to wait().");
		return 0;
	}
	
	return 1;
}

static int pipe_process(int count, int separator, char **arglist) {
	int pfds[2];
	
	// Create two-sided pipe side per process.
	if (pipe(pfds) == -1) {
		error("Error while executing pipe().");
		return 1;
	}
	
	int reader = pfds[0];
	int writer = pfds[1];
	
	int pid1 = fork();
	
	if (pid1 < 0) {
		error("Error while executing fork().");
		return 0;
	} else if (pid1 == 0) {
		restore_default_handler();
		
		// Redirect output of process one (right to the pipe) to the pipe writing side.
		if (dup2(writer, STDOUT_FILENO) == -1) {
			error("Error while executing dup2().");
			exit(1);
		}

		close(reader);
		close(writer);
		
		if (execvp(arglist[0], arglist) == -1) {
			error("Error while executeing execvp().");
			exit(1);
		}
	}
	
	int pid2 = fork();
	
	if (pid2 < 0) {
		error("Error while executing fork().");
		return 0;
	} else if (pid2 == 0) {
		restore_default_handler();
		
		// Redirect input of process two (left to the pipe) to the pipe reading side.
		if(dup2(reader, STDIN_FILENO) == -1) {
			error("Error while executing dup2().");
			exit(1);
		}
		
		close(reader);
		close(writer);
		
		if(execvp(arglist[separator + 1], &arglist[separator + 1]) == -1) {
			error("Error while execute execvp().");
			exit(1);
		}
	}
	
	// Close pipes at parent.
	close(reader);
	close(writer);
	
	// Wait for process to finish
	if(waitpid(pid1, NULL, 0) == -1 && errno != ECHILD) {
		error("Error while executing waitpid(). 2");
		exit(1);
	}
	
	// Wait for process to finish
	if(waitpid(pid2, NULL, 0) == -1 && errno != ECHILD) {
		error("Error while executing waitpid(). 3");
		exit(1);
	}
	
	return 1;
}

static int background(int count, char **arglist) {
	int pid = fork();
	
	if (pid < 0) {
		error("Error while executing fork().");
		return 0;
	}	
	else if (pid == 0) {
		if(execvp(arglist[0], arglist) == -1) {
			error("Error while executing execvp().");
			exit(1);
		}
	}
	
	return 1;
}

static int regular(char **arglist) {
	int pid = fork();
	
	if (pid < 0) {
		error("Error while executing fork().");
		return 0;
	}
	else if (pid == 0) {
		restore_default_handler();
		
		if(execvp(arglist[0], arglist) == -1) {
			error("Error while executing execvp().");
			exit(1);
		}
	}
	
	// Wait fot process to finish
	if (waitpid(pid, NULL, 0) == -1 && errno != ECHILD) {
		error("Error while executing waitpid().");
		return 0;
	}
	
	return 1;
}

static void error(char* msg) {
	fprintf(stderr, "ERROR OCCURRED! %s\n", msg);
}