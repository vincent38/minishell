/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <string.h>
#include "readcmd.h"
#include "csapp.h"

/* Background jobs double-chained list (for better navigation) */

struct background_jobs {
	int id;
	pid_t pid;
	struct background_jobs* previous;
	struct background_jobs* next;
};

// First element of the list
struct background_jobs* jobs_list = NULL;

// Appends a job to the list and prints an info line
void append (pid_t pid){
	struct background_jobs* build = malloc(sizeof(struct background_jobs));
	build->id = 1;
	build->pid = pid;
	build->previous = NULL;
	build->next = NULL;

	if (jobs_list == NULL){
		jobs_list = build;
	} else {
		(build->id)++;
		struct background_jobs* current = jobs_list;
		while(current->next != NULL){
			(build->id)++;
			current = current->next;
		}
		build->previous = current;
		current->next = build;
	}

	printf("[bg-%d]\tP_STARTED\t\t%d\n", build->id, pid);
}

// Crushes a finished task (called when untracked child is signalling itself)
void crush(int sig){
	int status;
	pid_t pid;
	pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
	struct background_jobs* current = jobs_list;
	while(current != NULL && current->pid != pid){
		current = current->next;
	}

	if (current != NULL && current->pid == pid){
		// Gotcha, child is really in background
		if (current->previous == NULL){
			// First in list, remove and replace
			jobs_list = current->next;
			if (current->next != NULL){
				// There is a next, edit its previous field
				current->next->previous = NULL;
			}
		} else {
			// Link our next to the previous' next element
			current->previous->next = current->next;
			if (current->next != NULL) {
				// There is a next, do the link the other way
				current->next->previous = current->previous;
			}
		}

		// Take back it's position and break it
		int id = current->id;
		free(current);

		// Prints it's status
		printf("\n[bg-%d]+\t", id);
		if (status){
			printf("P_ERROR\t\t\t");
		} else {
			printf("P_DONE\t\t\t");
		}
		printf("%d\n", pid);
	}
} 

// Prints the job list (called through cmd jobs)
void jobs(){
	struct background_jobs* current = jobs_list;
	printf("----- Current jobs on session -----\n");
	if (current == NULL){
		printf("No active job for this session...\n");
	} else {
		while(current != NULL){
			printf("[%d]\t%d\n", current->id, current->pid);
			current = current->next;
		}
	}
}

// Main procedure, handles the whole logic behind the shell
int main()
{
	// Disables the default handlers for SIGINT and SIGTSTP
	signal(SIGINT, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	while (1) {
		struct cmdline *l;
		int i;

		printf("shell> ");
		l = readcmd();

		/* If input stream closed, normal termination */
		if (!l) {
			printf("exit\n");
			exit(0);
		}

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		int seqNb = 0;
		/* Counts how many commands we have */
		for (i=0; l->seq[i]!=0; i++) {
			seqNb++;
		}

		// cpipe and ppipe keeps track of interprocess pipes
		int cpipe[2];
		int ppipe[2];

		// What's my pid ?
		pid_t process;

		// Loop through the commands
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];

			if(l->seq[i+1] != 0){
				// There is another command coming up, open a new pipe
				if(pipe(cpipe) < 0){
					perror("cannot create pipe.");
				}
			}

			// Cmd launcher (internal and external)
			if (strcmp(cmd[0],"quit") == 0 && seqNb == 1){
				// Internal command - Quit shell
				printf("exit\n");
				exit(0);
			} else if (strcmp(cmd[0],"jobs") == 0){
				// Internal command - Running jobs
				jobs();
			} else {
				// External command
				
				// Fork process
				process = fork();

				// processus status;
				if (process < -1){
					// Error
					printf("error while forking the processus.");
					exit(1);
				} else if (process == 0){
					// Open in file on stdin for first processus
					if (l->in != 0 && i == 0) {
						int fin = Open(l->in, O_RDONLY, 0);
						if (fin < 0){
							int stId = errno;
							if (stId == EACCES){
								printf("%s: Permission denied.", l->in);
							} else if (stId == ENOENT){
								printf("%s: No such file or directory.", l->in);
							} else {
								printf("%s: Unknown error.", l->in);
							}
						} else {
							Dup2(fin, 0);
							Close(fin);
						}
					}

					// If anything else than the first process
					if (i != 0){
						close(ppipe[1]);
						Dup2(ppipe[0],STDIN_FILENO);
					}

					// If anything else than the last process
					if(l->seq[i+1] != 0){
						close(cpipe[0]);
						Dup2(cpipe[1],STDOUT_FILENO);
					}

					// Open out file on stdout for last processus
					if (l->out != 0 && l->seq[i+1] == 0) {
						int fout = Open(l->out, O_WRONLY | O_TRUNC | O_CREAT, 0644);
						if (fout < 0){
							int stId = errno;
							if (stId == EACCES){
								printf("%s: Permission denied.", l->out);
							} else if (stId == ENOENT){
								printf("%s: No such file or directory.", l->out);
							} else {
								printf("%s: Unknown error.", l->out);
							}						
						} else {
							Dup2(fout, 1);
							Close(fout);
						}
					}

					// If process is not in background, give it the default handlers
					if (!l->bg) {
						signal(SIGINT, SIG_DFL);
						signal(SIGTSTP, SIG_DFL);
					}

					// Launch the process
					if (execvp(cmd[0], cmd) == -1){
						printf("%s: command not found\n", cmd[0]);
						exit(-1);
					}
					// Exit in case of
					exit(0);
				} else {
					// Father is awaiting

					// Close unused pipes
					if (i > 0) {
						close(ppipe[0]);
					}

					if(l->seq[i+1] != 0){
						close(cpipe[1]);
					}

					// If process not in background, await
					// Else, append the process to the jobs table and handle it's termination
					if (!l->bg) {
						int status;
						waitpid(process, &status, WUNTRACED);
					} else {
						append(process);
						signal(SIGCHLD, crush);
					}

					// Warp to the next command
					ppipe[0] = cpipe[0];
					ppipe[1] = cpipe[1];
				}
			}
		}
	}
}
