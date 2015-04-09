#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>

/**
 * This program takes as input a shell command
 * Support for <,> and >> operators
 * Support for | and pipelining any number of commands
 * Support for two new operators || and |||
 *
 * The program reads a line from stdin. It then tokenizes on the pipe operation.
 * If it finds one, it forks two new processes for both the commands. In the
 * command, it tokenizes on the redirection operators. If it finds <, stdin is
 * redirected to file. Else, it is the pipe read end. If it finds >(write)
 * or >>(append), stdout is redirect to file. Else, it is the pipe write end.
 * 
 */

#define MAX 255
#define MAXARGS 10
#define OUTPUTMAX 1024

void runCommand(char *command);

int main()
{
  char command[MAX];
  printf("This is the shell. Please enter your command. Press CTRL+D to stop the shell\n");
  printf("$> ");
  while(fgets(command, MAX, stdin) != NULL)
  {
    runCommand(command);
    printf("$> ");
  }
  printf("\n");
}

int findNextPipe(char *command)
{
  char pipe1 = '|';
  const char *ptr = strchr(command, pipe1);
  if(ptr) {
      int index = ptr - command;
      if(command[index+1] == '|')
      {
        if(command[index+2] == '|')
        {
          // Triple pipe
          return 3;
        }
        // Double pipe
        return 2;
      }
      // Single pipe
      return 1;
  }
  else {
    // not found. Last command
    return 0;
  }
}

int findRedirection(char *command)
{
  // 0 for no redirections
  // 1 for write redirection only
  // 2 for append redirection only
  // 3 for read redirection only
  // 4 for write + read redirection
  // 5 for append + read redirection
  char pipe1 = '>';
  int type = 0;
  const char *ptr = strchr(command, pipe1);
  if(ptr) {
    int index = ptr - command;
    if(command[index+1] == '>')
    {
      // Append redirection
      type += 2;
    }
    else
    {
      // Write redirection
      type += 1;
    }
  }
  // No output redirection
  pipe1 = '<';
  ptr = strchr(command, pipe1);
  if(ptr) {
    // Input redirection
    type += 3;
  }
  return type;
}

void getWriteFile(int index, char *command, char *fileName)
{
  index++;
  if(command[index] == ' ')
    index++;
  int i = index;
  int j = 0;
  while(i < strlen(command))
  {
    if(command[i] == ' ' || command[i] == '\n')
      break;
    fileName[j++] = command[i++];
  }
  fileName[j] = '\0';
}

void findCommand(char *command, char *cmd)
{
  int i = 0, j = 0;
  while(command[i] == ' ')
    i++;
  while(i < strlen(command))
  {
    if(command[i] == ' ' || command[i] == '\n')
      break;
    cmd[j++] = command[i++];
  }
  cmd[j] = '\0';
}

void findArgument(char *command, int *j)
{
  while(*j < strlen(command) && (command[*j] == ' ' || command[*j] == '\n'))
    (*j)++;
}

int addArgument(int j, char *command, char **args, int index)
{
  int i = j;
  int k = 0;
  char arg[MAX];
  while(i < strlen(command))
  {
    if(command[i] == ' ' || command[i] == '\n')
      break;
    arg[k++] = command[i++];
  }
  // copy to argument list
  strncpy(args[index], arg, k);
  return i;
}

void getArguments(char *command, char **args, int len)
{
  int i = 1;
  int j = len;
  int k = 1;
  int index1, index2;
  // Number of extra spaces tells us how much extra whitespace to forward j by
  int m = 0;
  while(command[m++] == ' ')
    j++;
  const char *ptr = strchr(command, '>');
  if(ptr)
  {
    index1 = ptr - command;
  }
  else
  {
    index1 = strlen(command);
  }
  ptr = strchr(command, '<');
  if(ptr)
  {
    index2 = ptr - command;
  }
  else
  {
    index2 = strlen(command);
  }
  while(j < strlen(command) && j < index1 && j < index2)
  {
    findArgument(command, &j);
    if(j == strlen(command) || j >= index1 || j >= index2)
      break;
    // Add the argument to the array
    j = addArgument(j, command, args, k++);
  }
  // Point last pointer to NULL
  args[k] = NULL;
}

void callCommand(char *cmd, char **args, int *isPipe, int *fd1, int nostdin, int inputRedirec, char *inFile, int last, int *fd2, int outputRedirec, char *outFile, int flag)
{
  pid_t ret;
  ret = fork();
  if(ret < 0)
    perror("Fork");
  if(ret == 0)
  {
    // Child
    // Check for input pipe
    if(*isPipe){
      // Close write end of this pipe
      close(fd1[1]);
      // There is a pipe to take input from
      close(STDIN_FILENO);
      dup2(fd1[0], STDIN_FILENO);
    }
    if(nostdin){
      // Map stdin to /dev/null
      int fd = open("/dev/null", O_RDONLY);
      close(STDIN_FILENO);
      dup2(fd, STDIN_FILENO);
    }
    if(inputRedirec){
      int fd = open(inFile, O_RDONLY);
      close(STDIN_FILENO);
      dup2(fd, STDIN_FILENO);
    }
    if(!last){
      // Not last. outputRedirect stdout to pipe
      close(STDOUT_FILENO);
      dup2(fd2[1], STDOUT_FILENO);
      close(fd2[0]);
      (*isPipe) = 1;
    }
    if(outputRedirec){
      // outputRedirect to file
      int fd;
      if(flag == 1)
        fd = open(outFile, O_CREAT | O_TRUNC | O_WRONLY, 0666);
      else if(flag == 2)
        fd = open(outFile, O_APPEND | O_WRONLY, 0666);
      close(STDOUT_FILENO);
      dup2(fd, STDOUT_FILENO);
      (*isPipe) = 0;
    }
    // Execute command
    if(execvp(cmd, args) == -1)
    {
      perror("Shell error: ");
      exit(0);
    }
  }
  else
  {
    if(!last){
      // Not last. outputRedirect stdout to pipe
      (*isPipe) = 1;
    }
    if(outputRedirec){
      // Redirect to file
      (*isPipe) = 0;
    }
    // Wait until child is done
    wait(NULL);
    close(fd2[1]);
  }
}

void executeCommand(char *command1, int *redirec, int *isPipe, int *fd1, int nostdin, int last, int *fd2)
{
  // Find any redirection operators
  char *command, file1[MAX], file2[MAX];
  char **arguments = calloc(MAXARGS, sizeof(char*));
  int i = 0;
  for (; i < MAXARGS; ++i){
    arguments[i] = malloc(MAX* sizeof(char));
  }
  int type, index;
  type = findRedirection(command1);
  if(type == 0)
  {
    // No redirection
    *redirec = 0;
    // First argument is the command itself
    char cmd[MAX];
    findCommand(command1, cmd);
    strncpy(arguments[0], cmd, sizeof(cmd));
    // Get the remaining arguments
    int len = strlen(cmd);
    getArguments(command1, arguments, len);
    callCommand(cmd, arguments, isPipe, fd1, nostdin, 0, NULL, last, fd2, 0, NULL, 0);
  }
  else if(type == 1)
  {
    // Write redirection
    *redirec = 1;
    const char *ptr = strchr(command1, '>');
    index = ptr - command1;
    // Assumption: You write the command and its argument before mentioning any of the redirection operators
    getWriteFile(index, command1, file1);
    // First argument is the command itself
    char cmd[MAX];
    findCommand(command1, cmd);
    strncpy(arguments[0], cmd, sizeof(cmd));
    // Get the remaining arguments
    int len = strlen(cmd);
    getArguments(command1, arguments, len);
    callCommand(cmd, arguments, isPipe, fd1, nostdin, 0, NULL, last, fd2, 1, file1, 1);

  }
  else if(type == 2)
  {
    // Append redirection
    *redirec = 1;
    const char *ptr = strchr(command1, '>');
    index = ptr - command1;
    // Assumption: You write the command and its argument before mentioning any of the redirection operators
    // Make sure to give the index of the second >
    getWriteFile(index+1, command1, file1);
    // First argument is the command itself
    char cmd[MAX];
    findCommand(command1, cmd);
    strncpy(arguments[0], cmd, sizeof(cmd));
    // Get the remaining arguments
    int len = strlen(cmd);
    getArguments(command1, arguments, len);
    callCommand(cmd, arguments, isPipe, fd1, nostdin, 0, NULL, last, fd2, 1, file1, 2);
  }
  else if(type == 3)
  {
    // Read redirection
    *redirec = 0;
    const char *ptr = strchr(command1, '<');
    index = ptr - command1;
    // Assumption: You write the command and its argument before mentioning any of the redirection operators
    // Make sure to give the index of the second >
    getWriteFile(index, command1, file1);
    // First argument is the command itself
    char cmd[MAX];
    findCommand(command1, cmd);
    strncpy(arguments[0], cmd, sizeof(cmd));
    // Get the remaining arguments
    int len = strlen(cmd);
    getArguments(command1, arguments, len);
    callCommand(cmd, arguments, isPipe, fd1, nostdin, 1, file1, last, fd2, 0, NULL, 0);
  }
  else if(type == 4)
  {
    // Write + read redirection
    *redirec = 1;
    const char *ptr = strchr(command1, '<');
    index = ptr - command1;
    // Assumption: You write the command and its argument before mentioning any of the redirection operators
    // Make sure to give the index of the second >
    getWriteFile(index, command1, file1);
    // Get Write file
    ptr = strchr(command1, '>');
    index = ptr - command1;
    getWriteFile(index, command1, file2);
    // First argument is the command itself
    char cmd[MAX];
    findCommand(command1, cmd);
    strncpy(arguments[0], cmd, sizeof(cmd));
    // Get the remaining arguments
    int len = strlen(cmd);
    getArguments(command1, arguments, len);
    callCommand(cmd, arguments, isPipe, fd1, nostdin, 1, file1, last, fd2, 1, file2, 1);
  }
  else if(type == 5)
  {
    // Append + read redirection
    *redirec = 1;
    const char *ptr = strchr(command1, '<');
    index = ptr - command1;
    // Assumption: You write the command and its argument before mentioning any of the redirection operators
    // Make sure to give the index of the second >
    getWriteFile(index, command1, file1);
    // Get Append file
    ptr = strchr(command1, '>');
    index = ptr - command1;
    getWriteFile(index+1, command1, file2);
    // First argument is the command itself
    char cmd[MAX];
    findCommand(command1, cmd);
    strncpy(arguments[0], cmd, sizeof(cmd));
    // Get the remaining arguments
    int len = strlen(cmd);
    getArguments(command1, arguments, len);
    callCommand(cmd, arguments, isPipe, fd1, nostdin, 1, file1, last, fd2, 1, file2, 2);
  }
}

void runCommand(char *command)
{
  int type, redirec, isPipe, nostdin;
  redirec = 0;
  isPipe = 0;
  nostdin = 0;
  int fd1[2]; // Pipes
  int fd2[2];
  int fd3[2];
  int fd4[2];
  char test[MAX];
  pipe(fd1);
  pipe(fd2);
  pipe(fd3);
  pipe(fd4);
  type = findNextPipe(command);
  while(type)
  {
    if(type == 1)
    {
      // Single pipe is the next pipe
      char *command1, *command2;
      command1 = strtok(command, "|");
      command2 = strtok(NULL, "");
      if(redirec)
        nostdin = 1;
      else
        nostdin = 0;
      executeCommand(command1, &redirec, &isPipe, fd1, nostdin, 0, fd2);
      command = command2;
      // Close the write end of fd2
      close(fd2[1]);
      // Close fd1 pipe
      close(fd1[0]);
      close(fd1[1]);
      // Copy the File descriptors
      fd1[0] = fd2[0];
      fd1[1] = fd2[1];
      // Create new pipe
      pipe(fd2);
    }
    else if(type == 2)
    {
      // Doube pipe is the next pipe. Assume end of command
      char *command1, *command2, *command3;
      command1 = strtok(command, "|");
      command2 = strtok(NULL, "|,");
      command3 = strtok(NULL, "");
      if(redirec)
        nostdin = 1;
      else
        nostdin = 0;
      executeCommand(command1, &redirec, &isPipe, fd1, nostdin, 0, fd2);
      char result[OUTPUTMAX];
      FILE *fd0 = fdopen(fd2[0], "r");
      fflush(fd0);
      int charsRead;
      charsRead = read(fd2[0], result, OUTPUTMAX);
      // fgets(result, OUTPUTMAX, fd0);
      // Write back into the first pipe
      write(fd3[1], result, charsRead);
      // Write into the next pipe
      write(fd1[1], result, charsRead);
      // Close the write end
      close(fd3[1]);
      close(fd1[1]);
      // Execute the two commands
      pid_t ret;
      ret = fork();
      if(ret < 0)
        perror("fork");
      if(ret == 0)
      {
        close(STDIN_FILENO);
        dup2(fd1[0], STDIN_FILENO);
        char cmd[MAX];
        char **arguments = calloc(MAXARGS, sizeof(char*));
        int i = 0;
        for (; i < MAXARGS; ++i){
          arguments[i] = malloc(MAX* sizeof(char));
        }
        findCommand(command2, cmd);
        strncpy(arguments[0], cmd, sizeof(cmd));
        int len = strlen(cmd);
        getArguments(command2, arguments, len);
        if(execvp(cmd, arguments) == -1)
        {
          perror("Shell error: ");
          exit(0);
        }
      }
      else
      {
        wait(NULL);
        // Parent
        // Run next command
        ret = fork();
        if(ret < 0)
          perror("fork");
        if(ret == 0)
        {
          close(STDIN_FILENO);
          dup2(fd3[0], STDIN_FILENO);
          char cmd[MAX];
          char **arguments = calloc(MAXARGS, sizeof(char*));
          int i = 0;
          for (; i < MAXARGS; ++i){
            arguments[i] = malloc(MAX* sizeof(char));
          }
          findCommand(command3, cmd);
          strncpy(arguments[0], cmd, sizeof(cmd));
          int len = strlen(cmd);
          getArguments(command3, arguments, len);
          if(execvp(cmd, arguments) == -1)
          {
            perror("Shell error: ");
            exit(0);
          }
        }
        else
          wait(NULL);
      }
      // This would be the last command
      return;
    }
    else if(type == 3)
    {
      // Triple pipe is the next pipe. Assume end of command
      char *command1, *command2, *command3, *command4;
      command1 = strtok(command, "|");
      command2 = strtok(NULL, "|,");
      command3 = strtok(NULL, ",");
      command4 = strtok(NULL, ",");
      if(redirec)
        nostdin = 1;
      else
        nostdin = 0;
      executeCommand(command1, &redirec, &isPipe, fd1, nostdin, 0, fd2);
      char result[OUTPUTMAX];
      int charsRead;
      charsRead = read(fd2[0], result, OUTPUTMAX);
      // Write back into the first pipe
      write(fd4[1], result, charsRead);
      // Write into the next pipe
      write(fd1[1], result, charsRead);
      // Write into next pipe
      write(fd3[1], result, charsRead);
      // Close the write end
      close(fd4[1]);
      close(fd1[1]);
      close(fd3[1]);
      // Execute the three commands
      pid_t ret;
      ret = fork();
      if(ret < 0)
        perror("fork");
      if(ret == 0)
      {
        close(STDIN_FILENO);
        dup2(fd1[0], STDIN_FILENO);
        char cmd[MAX];
        char **arguments = calloc(MAXARGS, sizeof(char*));
        int i = 0;
        for (; i < MAXARGS; ++i){
          arguments[i] = malloc(MAX* sizeof(char));
        }
        findCommand(command2, cmd);
        strncpy(arguments[0], cmd, sizeof(cmd));
        int len = strlen(cmd);
        getArguments(command2, arguments, len);
        if(execvp(cmd, arguments) == -1)
        {
          perror("Shell error: ");
          exit(0);
        }
      }
      else
      {
        // Parent
        wait(NULL);
        // Run next command
        ret = fork();
        if(ret < 0)
          perror("fork");
        if(ret == 0)
        {
          close(STDIN_FILENO);
          dup2(fd4[0], STDIN_FILENO);
          char cmd[MAX];
          char **arguments = calloc(MAXARGS, sizeof(char*));
          int i = 0;
          for (; i < MAXARGS; ++i){
            arguments[i] = malloc(MAX* sizeof(char));
          }
          findCommand(command3, cmd);
          strncpy(arguments[0], cmd, sizeof(cmd));
          int len = strlen(cmd);
          getArguments(command3, arguments, len);
          if(execvp(cmd, arguments) == -1)
          {
            perror("Shell error: ");
            exit(0);
          }
        }
        else
        {
          // Parent
          wait(NULL);
          // Run third command
          ret = fork();
          if(ret < 0)
            perror("fork");
          if(ret == 0)
          {
            close(STDIN_FILENO);
            dup2(fd3[0], STDIN_FILENO);
            char cmd[MAX];
            char **arguments = calloc(MAXARGS, sizeof(char*));
            int i = 0;
            for (; i < MAXARGS; ++i){
              arguments[i] = malloc(MAX* sizeof(char));
            }
            findCommand(command4, cmd);
            strncpy(arguments[0], cmd, sizeof(cmd));
            int len = strlen(cmd);
            getArguments(command4, arguments, len);
            if(execvp(cmd, arguments) == -1)
            {
              perror("Shell error: ");
              exit(0);
            }
          }
          else
            wait(NULL);
        }
      }
      // This would be the last command
      return;
    }
    type = findNextPipe(command);
  }
  // Last command
  if(redirec)
    nostdin = 1;
  else
    nostdin = 0;
  executeCommand(command, &redirec, &isPipe, fd1, nostdin, 1, fd2);
}