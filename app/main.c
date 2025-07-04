#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

// Usage: your_docker.sh run <image> <command> <arg1> <arg2> ...
int main(int argc, char *argv[]) {
	int fd[2];
  if (pipe(fd) == -1) {
    // pipe failed
    return 1;
  }
  

  setbuf(stdout, NULL);
 	setbuf(stderr, NULL);
	char *command = argv[3];
	int child_pid = fork();
	if (child_pid == -1) {
	    printf("Error forking!");
	    return 1;
	}
	if (child_pid == 0) {
		// Replace current program with calling program
	  close(fd[0]); // close read end
    dup2(fd[1], STDOUT_FILENO); // make stdout point to the write end of the pipe
    dup2(fd[1], STDERR_FILENO);
    execv(command, &argv[3]);
    
	} else {
		   // We're in parent
       close(fd[1]); // close write end
       dup2(fd[0], STDIN_FILENO); // make STDIN point to the same thing as the pipes read end 
		   wait(NULL);
		   printf("Child terminated");
	}
	return 0;
}
