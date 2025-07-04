#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
// Usage: your_docker.sh run <image> <command> <arg1> <arg2> ...
int main(int argc, char *argv[])
{

  setbuf(stdout, NULL);
  setbuf(stderr, NULL);

  char *command = argv[3];
  int fd[2];
  if (pipe(fd) == -1) {
    return 1;
  }

  char * shell_args[4];
  shell_args[0] = "sh";
  shell_args[1] = "-c";
  shell_args[3] = NULL;

  // assemble the full command
  char full_cmd[1024] = {0};
  for (int i = 3; i < argc; i++) {
    strcat(full_cmd, argv[i]);
    if (i != argc - 1) {
      strcat(full_cmd, " ");
    }
  }
  shell_args[2] = full_cmd;

  int child_pid = fork();
  if (child_pid == -1) {
    printf("Error forking!");
    return 1;
  }

  if (child_pid == 0)
  { // CHILD ------------------------------------

    close(fd[0]);               // close read end
    dup2(fd[1], STDOUT_FILENO); // make stdout point to the write end of the pipe
    dup2(fd[1], STDERR_FILENO);
    // printf("FROM CHILD");
    execvp("sh", shell_args);

  }
  else
  { // PARENT -------------------------------------
    close(fd[1]);              // close write end
    dup2(fd[0], STDIN_FILENO); // make stdin point to the same thing as the pipes read end
    char buffer[128] = {};
    int bytesRead = 0;
    while ((bytesRead = read(STDIN_FILENO, buffer, sizeof(buffer) - 1)) > 0) {
      buffer[bytesRead] = 0;
      printf("%s", buffer);
    }
    close(fd[0]);
    
    int status;
    waitpid(child_pid, &status, 0);
    return WEXITSTATUS(status);
  }
  return 0;
}
