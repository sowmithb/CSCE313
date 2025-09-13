/****************
LE2: Introduction to Unnamed Pipes
****************/
#include <unistd.h> // pipe, fork, dup2, execvp, close
using namespace std;
#include <iostream>
#include <sys/wait.h>

int main () {
    // lists all the files in the root directory in the long format
    char* cmd1[] = {(char*) "ls", (char*) "-al", (char*) "/", nullptr};
    // translates all input from lowercase to uppercase
    char* cmd2[] = {(char*) "tr", (char*) "a-z", (char*) "A-Z", nullptr};

    // TODO: add functionality
    // Create pipe
    // fds[0] = read end, fds[1] = write end
    int fds[2];
    if (pipe(fds) == -1) {
        cerr << "pipe failed\n";
        return 1;
    }

    pid_t pid = fork();
    // Create child to run first command
    // In child, redirect output to write end of pipe
    // Close the read end of the pipe on the child side.
    // In child, execute the command
    if(pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        //close(fds[1]);
        execvp(cmd1[0], cmd1);
    }

    pid_t pid2 = fork();
    // Create another child to run second command
    // In child, redirect input to the read end of the pipe
    // Close the write end of the pipe on the child side.
    // Execute the second command.
    if(pid2 == 0) {
        dup2(fds[0], STDIN_FILENO);
        close(fds[1]);
        execvp(cmd2[0], cmd2);
    }

    // Reset the input and output file descriptors of the parent.
    close(fds[0]);
    close(fds[1]);
    int status;
    waitpid(pid, &status, 0);
    waitpid(pid2, &status, 0);
    return 0;
}
