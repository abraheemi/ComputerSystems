#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>


void parseCommand(char cmd[], char** arg, int *argc){
  char *token;
  char delim[2] = " ";
  int i = 0;

	token = strtok(cmd, delim);

	while(token != NULL){
		arg[i] = token;

		token = strtok(NULL, delim);
		i++;
		(*argc)++;	
	}
	arg[i] = NULL;
	
}


static void handleSignal(int signum) {
  if(signum == 2)
  	printf("\nSignal %d (SIGINT) has been recieved\n", signum);

  if(signum == 20)
  	printf("\nSignal %d (SIGTSTP) has been recieved\n", signum);
}


int main(){
  pid_t pid;
  int i, flag=0, fd;
  char** arg;
  int argc = 0;
  char cmd[513];
  char *fileName;

  if( signal(SIGINT, handleSignal) == SIG_ERR)
		printf("signal error\n");

  if( signal(SIGTSTP, handleSignal) == SIG_ERR)
		printf("signal error\n");

  arg = (char**)malloc(sizeof(char*)*10);

while(1){
	printf("bash$ ");
	fgets(cmd, 512, stdin);	
	int len = strlen(cmd);
	cmd[len-1] = '\0';

	argc = 0;
	parseCommand(cmd, arg, &argc);
	
	flag = 0;
	for(i=0; i<argc; i++){
		// OUT
		if( strcmp(arg[i], ">") == 0 ){
			flag = 1;
			fileName = arg[i+1];
			break;
		}
		// APPEND
		if( strcmp(arg[i], ">>") == 0 ){
			flag = 2;
			fileName = arg[i+1];
			break;
		}	
		// IN
		if( strcmp(arg[i], "<") == 0 ){
			flag = 3;
			fileName = arg[i+1];
			break;
		}	

	}

	if( !strcmp("exit", cmd)){
		printf("Exited shell\n");
		exit(0);
	}

	pid = fork();
	if(pid != 0)
		printf("Child PID is: %d\n\n", pid);

	if( pid == 0 ){ // Child process

		if(flag == 1){
			arg[i] = NULL;
  		fd = open(fileName, O_WRONLY | O_CREAT | O_TRUNC, 0644);
			if(fd < 0){
      	printf("Unable to open %s\n", fileName);
			}
			else{
  			dup2(fd, 1);
  			close(fd);
			}
		}

		if(flag == 2){
			arg[i] = NULL;
  		fd = open(fileName, O_WRONLY | O_APPEND, 0644);
      if( fd < 0 ) {
      	printf("Unable to open %s\n", fileName);
      }
			else{
  			dup2(fd, 1);
  			close(fd);
			}
		}

		if(flag == 3){
			arg[i] = NULL;
  		fd = open(fileName, O_RDONLY, 0644);
      if( fd < 0 ) {
      	printf("Unable to open %s\n", fileName);
      }
			else{
  			dup2(fd, 0);
  			close(fd);
			}
		}

		execvp(arg[0], arg);
		//printf("Child exited\n");
		exit(0);
	}

	int status;
	wait(&status);
}


	return 0;
}


