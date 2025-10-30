#include <iostream>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <vector>
#include <string>
#include <cstring>
#include <cerrno>
#include <ctime>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include "Tokenizer.h"

// all the basic colours for a shell prompt
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    string prev_dir;
    bool prev_dir_set = false;
    vector<pid_t> bg_pids;

    for (;;) {
        int status;
        pid_t done;
        while ((done=waitpid(-1,&status,WNOHANG))>0){
            bg_pids.erase(remove(bg_pids.begin(), bg_pids.end(),done),bg_pids.end());
        }
        // need date/time, username, and absolute path to current dir
        const char* user = getenv("USER");
        if(!user) user="user";
        char cwd[4096];
        getcwd(cwd,sizeof(cwd));
        time_t now = time(nullptr);
        char timebuf[64];
        strftime(timebuf,sizeof(timebuf),"%b %d %H:%M:%S", localtime(&now));



        cout << YELLOW << timebuf << user << " :"<<cwd<<"$ "<<NC;
        cout.flush();
        
        // get user inputted command
        string input;
        if(!getline(cin, input)){
            cout<<endl;
            break;
        }

        if (input == "exit") {  // print exit message and break out of infinite loop
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        // get tokenized commands from user input
        Tokenizer tknr(input);
        if (tknr.hasError()||tknr.commands.empty()) {  // continue to next prompt if input had an error
            continue;
        }
        if(tknr.commands.size()==1 && !tknr.commands[0]->args.empty()){
            Command* cmd = tknr.commands[0];
            if(cmd->args[0]=="cd"){
                string target;
                if(cmd->args.size()==1){
                    const char* home = getenv("HOME");
                    target=home?string(home):"/";
                }else{
                    target =cmd->args[1];
                }
                if(target=="-"){
                    if(!prev_dir_set) cerr<<"cd: OLDPWD not set"<<endl;
                    else{
                        char curr[4096];
                        getcwd(curr, sizeof(curr));
                        if(chdir(prev_dir.c_str())!=0) perror("chdir");
                        else{
                            cout<<prev_dir<<endl;
                            prev_dir = string(curr);
                        }
                    }

                }else{
                    char curr[4096];
                    getcwd(curr, sizeof(curr));
                    if(chdir(target.c_str())!=0) perror("chdir");
                    else{
                        prev_dir = string(curr);
                        prev_dir_set =true;
                    }
                }
                continue;
            }
        }
        bool background = false;
        for(auto c: tknr.commands){
            if(c->isBackground()){
                background=true;
                break;

            }
        }
        int stdin_copy = dup(STDIN_FILENO);
        vector<pid_t> pids;
        int prev_read = -1;

        for(size_t i = 0; i<tknr.commands.size(); ++i){
            Command* cmd = tknr.commands[i];
            bool last = (i==tknr.commands.size()-1);
            int pipefd[2] = {-1,-1};
            if(!last && pipe(pipefd)<0){
                perror("pipe");
                break;
            }
            pid_t pid = fork();
            if(pid<0){
                perror("fork");
                break;
            }
            if(pid==0){
                if(prev_read!=-1){
                    dup2(prev_read,STDIN_FILENO);
                    close(prev_read);
                }else if(cmd->hasInput()){
                    int fdin = open(cmd->in_file.c_str(),O_RDONLY);
                    if(fdin<0){perror("open input"); exit(1);}
                    dup2(fdin,STDIN_FILENO);
                    close(fdin);
                }
                if(!last){
                    dup2(pipefd[1],STDOUT_FILENO);

                }else if(cmd->hasOutput()){
                    int fdout = open(cmd->out_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
                    if(fdout<0){perror("open output"); exit(1);}
                    dup2(fdout,STDOUT_FILENO);
                    close(fdout);
                }
                if(pipefd[0]!=-1) close(pipefd[0]);
                if(pipefd[1]!=-1) close(pipefd[1]);
                if(stdin_copy!=-1) close(stdin_copy);
                
                vector<char*> argv;
                for (auto& s: cmd->args)
                    argv.push_back(const_cast<char*>(s.c_str()));
                argv.push_back(nullptr);
                execvp(argv[0],argv.data());
                perror("execvp");
                exit(1);

            }else{
                pids.push_back(pid);
                if(pipefd[1]!=-1) close(pipefd[1]);
                if(prev_read!=-1) close(prev_read);
                prev_read = pipefd[0];
            }
        }
        dup2(stdin_copy,STDIN_FILENO);
        close(stdin_copy);
        if(background){
            for(auto pid:pids) bg_pids.push_back(pid);
            if(!pids.empty()) cout<<"["<<pids.front()<<"]"<<endl;

        }else{
            for(auto pid:pids) waitpid(pid,nullptr,0);
        }


        // // print out every command token-by-token on individual lines
        // // prints to cerr to avoid influencing autograder
        // for (auto cmd : tknr.commands) {
        //     for (auto str : cmd->args) {
        //         cerr << "|" << str << "| ";
        //     }
        //     if (cmd->hasInput()) {
        //         cerr << "in< " << cmd->in_file << " ";
        //     }
        //     if (cmd->hasOutput()) {
        //         cerr << "out> " << cmd->out_file << " ";
        //     }
        //     cerr << endl;
        // }

        // fork to create child
        
        
    }
    return 0;
}