#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>

#define TEMP_DIR "/tmp/temp/"
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


// int copy_file(const char *src_path, const char *dest_path)
// {
//   FILE * src = fopen(src_path, "rb");
//   FILE * dest = fopen(dest_path, "wb");
//   if (!src) {
//     printf("Source file: %s\n", src_path);
//     perror("Failed to open source file");
//     return 1;
//   }
//   if (!dest) {
//     perror("Failed to open destination file.");
//     return 1;
//   }
  
//   char buffer[4096];
//   size_t bytesRead;
//   while ((bytesRead = fread(buffer, 1, sizeof(buffer), src)) > 0) {
//     if (fwrite(buffer, sizeof(char), bytesRead, dest) != bytesRead) {
//       perror("Error while writing to destination file.");
//       fclose(src);
//       fclose(dest);
//       unlink(dest_path); // get rid of the incomplete file
//       return 1;
//     }
//   }

//   if (chmod(dest_path, 0755) != 0) {
//     perror("Failed to set file permissions");
//     return 1;
//   } // make sure the copied file is executable

//   fclose(src);
//   fclose(dest);
//   return 0;
// }
int copy_file(const char *src_path, const char *dest_path)
{
  int dest = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
  if (dest == -1) {
    perror("Couldn't open dest file.");
    printf("Cant open dest file.\n");
    return -1;
  }
  int src = open(src_path, O_RDONLY);
  if (src == -1) {
     printf("Can't open src file.\n");
     perror("Couldn't open src file.");
     close(dest);
     return -1;
  }
  char buffer[4096];
  ssize_t bytesRead;
  while ((bytesRead = read(src, buffer, sizeof(buffer))) > 0) {
    write(dest, buffer, bytesRead);
  }
 chmod("/tmp/temp/busybox", 0755);
  //fsync(dest);
  close(src);
  close(dest);
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
// create temporary directory
    mkdir(TEMP_DIR, 0755);
    //char * bin_path = find_binary(argv[3]);
    copy_file("/bin/busybox", "/tmp/temp/busybox");
    //free(bin_path);


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

    
    // call chroot to change root to temporary directory
    if (chroot(TEMP_DIR) != 0) {
      printf("chroot failed.\n");
      return 1;
    }
    // call chdir to set working directory inside chroot
    if (chdir("/") != 0) {
      printf("chdir failed.\n");
      return 1;
    }
    
    // Right after chdir("/"), add:
    char * args[argc - 2];
    args[0] = "busybox";
    for (int i = 1; i < argc - 3; i++) {
      args[i] = argv[i + 3];
    }
    args[argc - 3] = NULL;
	/*
    for (int i = 0; i < argc - 2; i++) {
	    printf("args[%d]: %s\n", i, args[i]);
    }*/
    //char * simple_args[] = {"busybox", "--help", NULL};
    execv("/busybox", args);
    perror("execv");
    printf("execv() failed.\n");
    exit(1);

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

