#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

// Usage: your_docker.sh run <image> <command> <arg1> <arg2> ...

char * find_binary(const char *binary) {
  char * path_env = getenv("PATH"); // returns colon separated string like /user/local/bin:/usr/bin:/bin
  if (!path_env) return NULL;
  
  char * path_copy = strdup(path_env); // get a copy of the path
  char *dir = strtok(path_copy, ":");

  while (dir != NULL) { // iterate through each directory token
    int path_size = strlen(dir) + strlen(binary) + 2;
    char * full_path = malloc(path_size * sizeof(char));

    snprintf(full_path, path_size, "%s/%s", dir, binary); // assemble the potential path to binary
    
    if (access(full_path, X_OK) == 0) { // file exists and is executable
      free(path_copy);
      return full_path;
    }
    free(full_path);
    dir = strtok(NULL, ":");
  }
  free(path_copy);
  return NULL;

}


int copy_file(const char *src_path, const char *dest_path)
{
  FILE * src = fopen(src_path, "rb");
  FILE * dest = fopen(dest_path, "wb");
  if (!src) {
    printf("Source file: %s\n", src_path);
    perror("Failed to open source file");
    return 1;
  }
  if (!dest) {
    perror("Failed to open destination file.");
    return 1;
  }
  
  char buffer[4096];
  size_t bytesRead;
  while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0) {
    if (fwrite(buffer, sizeof(char), bytesRead, dest) != bytesRead) {
      perror("Error while writing to destination file.");
      fclose(src);
      fclose(dest);
      unlink(dest_path); // get rid of the incomplete file
      return 1;
    }
  }

  if (chmod(dest_path, 0755) != 0) {
    perror("Failed to set file permissions");
    return 1;
  } // make sure the copied file is executable

  fclose(src);
  fclose(dest);
  return 0;
}



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
  char full_cmd[1024] = "/";
  strcat(full_cmd, argv[3]);
  for (int i = 4; i < argc; i++) {
    strcat(full_cmd, " ");
    strcat(full_cmd, argv[i]);
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

    // create temporary directory
    char dir_path[128] = "/tmp/container-XXXXXX";
    if (mkdtemp(dir_path) == NULL) { // failed to create new temporary directory
      return 1;
    }
    char temp_binary_path[128] = {0}; // path of the binary inside the temporary directory (dir_path + binary)
    strcpy(temp_binary_path, dir_path);
    temp_binary_path[strlen(temp_binary_path)] = '/';
    strncat(temp_binary_path, argv[3], sizeof(temp_binary_path) - 1); // concatenate the binary to the end

    // find and copy the binaries to temporary directory
    char * bin_path = find_binary(argv[3]);
    
    if (copy_file(bin_path, temp_binary_path) != 0) {
      printf("Failed to copy file.");
      return 1;
    }
    free(bin_path);
    char *direct_args[] = {temp_binary_path, "TEST", NULL};
    execv(temp_binary_path, direct_args);

    // call chroot to change root to temporary directory
    if (chroot(dir_path) != 0) {
      printf("chroot failed.\n");
      return 1;
    }
    
    // call chdir to set working directory inside chroot
    if (chdir("/") != 0) {
      printf("chdir failed.\n");
      return 1;
    }

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

