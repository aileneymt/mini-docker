#define _GNU_SOURCE
#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <sched.h>


#define TEMP_DIR "/tmp/temp/"
#define IMAGE_ARG 3

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
  chmod("/tmp/temp/bin/busybox", 0755);
  //fsync(dest);
  close(src);
  close(dest);
  return 0;

}



int createDirectories()
{
  char path[512];
  const char* dirs[] = {"bin", "etc", "proc", "dev", NULL};
  
  for (int i = 0; dirs[i] != NULL; i++) {
    snprintf(path, sizeof(path), "%s/%s", TEMP_DIR, dirs[i]);
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
      perror("Could not create directory");
      return -1;
    }

  }

  return 0;

}

void getImageNameAndTag(char * imageArg, char ** imageName, char ** imageTag) {
  *imageName = strtok(imageArg, ":");
  if (*imageName == NULL) {
    perror("Invalid image name argument.");
    exit(1);
  }
  *imageTag = strtok(NULL, ":");
  if (*imageTag == NULL) {
    perror("Invalid image tag argument.");
    exit(1);
  }
}


void getDockerAuthToken(char **token, char * imageName, char * imageTag) {

  char cmd[512];
  snprintf(cmd, sizeof(cmd),
    "curl -s \"https://auth.docker.io/token?service=registry.docker.io&scope=repository:library/%s:pull\"", imageName);
  FILE *fp = popen(cmd, "r"); // opening pipe to read output of the curl commnd

  if (fp == NULL) {
    perror("popen failed.");
    exit(1);
  }
  char buffer[4096];
  size_t bytesRead = fread(buffer, sizeof(char), sizeof(buffer) - 1, fp);
  buffer[bytesRead] = '\0';
  pclose(fp);
  *token = strstr(buffer, "\"token\":\"");
  if (*token == NULL) {
    perror("Cannot find token.");
    exit(1);
  }

  *token += strlen("\"token\":\"");
  char * end = strchr(*token, '\"');
  *end = '\0';

}


void getManifest(char * token, char * imageName, char * imageTag) {
  
  //char cmd[4096] = {0};
  char cmd[4096];
  memset(cmd, 0, sizeof(cmd));
  printf("Token: <%s>\n", token);
  snprintf(cmd, sizeof(cmd),
    "curl -s -H \"Authorization: Bearer %s\" "
    "-H \"Accept: application/vnd.docker.distribution.manifest.v2+json\" "
    "\"https://registry-1.docker.io/v2/library/%s/manifests/%s\"", token, imageName, imageTag);
  
  printf("CMD: <%s>\n", cmd);
  FILE *fp = popen(cmd, "r"); // opening pipe to read output of the curl commnd

  if (fp == NULL) { 
    perror("popen failed.");
    exit(1);
  }
  
  char * buffer = malloc(sizeof(char) * 1024);
  int bufferSize = 1024;
  int responseLen = 0;
  size_t bytesRead = 0; 
  while ((bytesRead = fread(buffer, sizeof(char), bufferSize, fp)) != 0) {
    if (bytesRead == sizeof(buffer)) { // we read to the max, need to expand
      bufferSize *= 2;
      char * temp = malloc(sizeof(char) * bufferSize);
      memcpy(temp, buffer, bytesRead);
      free(buffer);
      buffer = temp;
    }
    responseLen += bytesRead;
  }
  buffer[responseLen] = '\0';
  printf("%s\n", buffer);
  pclose(fp);
  free(buffer);

}



int main(int argc, char *argv[])
{
  
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);
 
  char * imageName;
  char * imageTag;
  char * token;
  getImageNameAndTag(argv[IMAGE_ARG], &imageName, &imageTag);
  getDockerAuthToken(&token, imageName, imageTag);
  printf("Token: <%s>\n", token);
  getManifest(token, imageName, imageTag);
  
  
  char *command = argv[3];
  int fd[2];
  if (pipe(fd) == -1) {
    return 1;
  }
  // create temporary directory
  mkdir(TEMP_DIR, 0755);
  if (createDirectories() == -1) {
    perror("Error while creating directories.");
    return 1;
  }
  //char * bin_path = find_binary(argv[3]);
  copy_file("/bin/busybox", "/tmp/temp/bin/busybox");
  //free(bin_path);


  int child_pid = syscall(SYS_clone, CLONE_NEWPID | SIGCHLD, NULL, NULL, NULL, NULL);
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
    
    mount("proc", "/proc", "proc", 0, NULL);
    // call chdir to set working directory inside chroot
    if (chdir("/") != 0) {
      printf("chdir failed.\n");
      return 1;
    }

    char * args[argc - 2];
    int size = argc - 2;
    //args[0] = "busybox";
    for (int i = 0; i < argc - 3; i++) {
      args[i] = argv[i + 4];
    }
    args[size - 1] = NULL;
    execv("/bin/busybox", args);
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

