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

#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

using namespace std;

int main () {
    string prevDir;
    bool prevDirSet = false;
    vector<pid_t> bgPids;

    for (;;) {
        int waitSt;
        pid_t donePid;
        while ((donePid=waitpid(-1,&waitSt,WNOHANG))>0){
            bgPids.erase(remove(bgPids.begin(), bgPids.end(),donePid),bgPids.end());
        }
        const char* usr = getenv("USER");
        if(!usr) usr="user";
        char cwd[4096];
        getcwd(cwd,sizeof(cwd));
        time_t nowTs = time(nullptr);
        char tbuf[64];
        strftime(tbuf,sizeof(tbuf),"%b %d %H:%M:%S", localtime(&nowTs));



        cout << YELLOW << tbuf <<" "<< usr << " :"<<cwd<<"$ "<<NC;
        cout.flush();
        
        string line;
        if(!getline(cin, line)){
            cout<<endl;
            break;
        }

        if (line == "exit") {
            cout << RED << "Now exiting shell..." << endl << "Goodbye" << NC << endl;
            break;
        }

        Tokenizer tok(line);
        if (tok.hasError()||tok.commands.empty()) {
            continue;
        }
        if(tok.commands.size()==1 && !tok.commands[0]->args.empty()){
            Command* cmd = tok.commands[0];
            if(cmd->args[0]=="cd"){
                string tgt;
                if(cmd->args.size()==1){
                    const char* homeDir = getenv("HOME");
                    tgt=homeDir?string(homeDir):"/";
                }else{
                    tgt =cmd->args[1];
                }
                if(tgt=="-"){
                    if(!prevDirSet) cerr<<"cd: OLDPWD not set"<<endl;
                    else{
                        char curDir[4096];
                        getcwd(curDir, sizeof(curDir));
                        if(chdir(prevDir.c_str())!=0) perror("chdir");
                        else{
                            cout<<prevDir<<endl;
                            prevDir = string(curDir);
                        }
                    }

                }else{
                    char curDir[4096];
                    getcwd(curDir, sizeof(curDir));
                    if(chdir(tgt.c_str())!=0) perror("chdir");
                    else{
                        prevDir = string(curDir);
                        prevDirSet =true;
                    }
                }
                continue;
            }
        }
        bool bg = false;
        for(auto cmd: tok.commands){
            if(cmd->isBackground()){
                bg=true;
                break;

            }
        }
        int stdinDup = dup(STDIN_FILENO);
        vector<pid_t> pids;
        int prevRd = -1;

        for(size_t i = 0; i<tok.commands.size(); ++i){
            Command* cmd = tok.commands[i];
            bool last = (i==tok.commands.size()-1);
            int pfd[2] = {-1,-1};
            if(!last && pipe(pfd)<0){
                perror("pipe");
                break;
            }
            pid_t pid = fork();
            if(pid<0){
                perror("fork");
                break;
            }
            if(pid==0){
                if(prevRd!=-1){
                    dup2(prevRd,STDIN_FILENO);
                    close(prevRd);
                }else if(cmd->hasInput()){
                    int inFd = open(cmd->in_file.c_str(),O_RDONLY);
                    if(inFd<0){perror("open input"); exit(1);}
                    dup2(inFd,STDIN_FILENO);
                    close(inFd);
                }
                if(!last){
                    dup2(pfd[1],STDOUT_FILENO);

                }else if(cmd->hasOutput()){
                    int outFd = open(cmd->out_file.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0644);
                    if(outFd<0){perror("open output"); exit(1);}
                    dup2(outFd,STDOUT_FILENO);
                    close(outFd);
                }
                if(pfd[0]!=-1) close(pfd[0]);
                if(pfd[1]!=-1) close(pfd[1]);
                if(stdinDup!=-1) close(stdinDup);
                
                vector<char*> argv;
                for (auto& s: cmd->args)
                    argv.push_back(const_cast<char*>(s.c_str()));
                argv.push_back(nullptr);
                execvp(argv[0],argv.data());
                perror("execvp");
                exit(1);

            }else{
                pids.push_back(pid);
                if(pfd[1]!=-1) close(pfd[1]);
                if(prevRd!=-1) close(prevRd);
                prevRd = pfd[0];
            }
        }
        dup2(stdinDup,STDIN_FILENO);
        close(stdinDup);
        if(bg){
            for(auto pid:pids) bgPids.push_back(pid);
            if(!pids.empty()) cout<<"["<<pids.front()<<"]"<<endl;

        }else{
            for(auto pid:pids) waitpid(pid,nullptr,0);
        }


        
        
        
        
    }
    return 0;
}