/*
 * Copyright (C) 2002, Simon Nieuviarts
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "readcmd.h"
#include "csapp.h"


int main()
{
	while (1) {
		struct cmdline *l;
		int i, j;

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

		if (l->in) {
			printf("in: %s\n", l->in);
		}

		if (l->out) {
			printf("out: %s\n", l->out);
		}

		/* Display each command of the pipe */
		for (i=0; l->seq[i]!=0; i++) {
			char **cmd = l->seq[i];
			printf("seq[%d]: ", i);
			for (j=0; cmd[j]!=0; j++) {
				printf("%s ", cmd[j]);
			}
			printf("\n");
			// Quit shell
			if (strcmp(cmd[0],"quit") == 0){
				printf("exit\n");
				exit(0);
			} else {
				// Fork process
				pid_t process = fork();
				int status;
				if (process == -1){
					// Error
					printf("error while forking the processus.");
					exit(1);
				} else if (process == 0){
					// Child
					if (l->in) {
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

					if (l->out) {
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
					if (execvp(cmd[0], cmd) == -1){
						printf("%s: command not found\n", cmd[0]);
						exit(-1);
					}
				} else {
					// Father is awaiting
					if (waitpid(process, &status, 0) == -1){
						printf("error while catching the closed processus.");
						exit(1);
					}
				}
			}
		}
	}
}
