#include <iostream>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <cerrno>

#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>

#include "Tokenizer.h"

// Color codes must remain identical for same visual output
#define RED     "\033[1;31m"
#define GREEN	"\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE	"\033[1;34m"
#define WHITE	"\033[1;37m"
#define NC      "\033[0m"

// Reap any background children that have finished
static void reap_background(std::vector<pid_t>& running) {
    int st = 0;
    pid_t done = 0;
    while ((done = ::waitpid(-1, &st, WNOHANG)) > 0) {
        running.erase(std::remove(running.begin(), running.end(), done), running.end());
    }
}

// Compose and print the prompt exactly as before
static void show_prompt() {
    const char* user = std::getenv("USER");
    if (!user) user = "user";

    char cwd[4096] = {0};
    ::getcwd(cwd, sizeof(cwd));

    std::time_t now = std::time(nullptr);
    char tbuf[64] = {0};
    std::strftime(tbuf, sizeof(tbuf), "%b %d %H:%M:%S", std::localtime(&now));

    std::cout << YELLOW << tbuf << " " << user << " :" << cwd << "$ " << NC;
    std::cout.flush();
}

// Convert Command args to argv (null-terminated)
static void to_argv(const Command* c, std::vector<char*>& argv) {
    argv.clear();
    argv.reserve(c->args.size() + 1);
    for (const auto& s : c->args) argv.push_back(const_cast<char*>(s.c_str()));
    argv.push_back(nullptr);
}

// Builtin: cd (with HOME, "-", error messages identical)
static bool builtin_cd(Command* c, std::string& oldpwd, bool& oldpwd_set) {
    if (c->args.empty() || c->args[0] != "cd") return false;

    std::string target;
    if (c->args.size() == 1) {
        const char* home = std::getenv("HOME");
        target = home ? std::string(home) : "/";
    } else {
        target = c->args[1];
    }

    if (target == "-") {
        if (!oldpwd_set) {
            std::cerr << "cd: OLDPWD not set" << std::endl;
            return true;
        }
        char cur[4096] = {0};
        ::getcwd(cur, sizeof(cur));
        if (::chdir(oldpwd.c_str()) != 0) {
            ::perror("chdir");
        } else {
            std::cout << oldpwd << std::endl;
            oldpwd = std::string(cur);
        }
        return true;
    }

    char cur[4096] = {0};
    ::getcwd(cur, sizeof(cur));
    if (::chdir(target.c_str()) != 0) {
        ::perror("chdir");
    } else {
        oldpwd = std::string(cur);
        oldpwd_set = true;
    }
    return true;
}

// Determine if any command in the pipeline is backgrounded
static bool wants_background(const std::vector<Command*>& cmds) {
    for (auto* c : cmds) {
        if (c->isBackground()) return true;
    }
    return false;
}

// Execute a pipeline with optional I/O redirection; return child PIDs in order
static std::vector<pid_t> execute_pipeline(const std::vector<Command*>& cmds) {
    std::vector<pid_t> pids;
    pids.reserve(cmds.size());

    // Preserve original stdin for later restoration
    int saved_stdin = ::dup(STDIN_FILENO);

    int prev_read = -1;             // read end from previous pipe
    for (size_t idx = 0; idx < cmds.size(); ++idx) {
        Command* step = cmds[idx];
        const bool last = (idx + 1 == cmds.size());

        int pipefd[2] = {-1, -1};
        if (!last && ::pipe(pipefd) < 0) {
            ::perror("pipe");
            break;
        }

        pid_t child = ::fork();
        if (child < 0) {
            ::perror("fork");
            break;
        }

        if (child == 0) {
            // Child process
            if (prev_read != -1) {
                ::dup2(prev_read, STDIN_FILENO);
                ::close(prev_read);
            } else if (step->hasInput()) {
                int in_fd = ::open(step->in_file.c_str(), O_RDONLY);
                if (in_fd < 0) { ::perror("open input"); _exit(1); }
                ::dup2(in_fd, STDIN_FILENO);
                ::close(in_fd);
            }

            if (!last) {
                ::dup2(pipefd[1], STDOUT_FILENO);
            } else if (step->hasOutput()) {
                int out_fd = ::open(step->out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (out_fd < 0) { ::perror("open output"); _exit(1); }
                ::dup2(out_fd, STDOUT_FILENO);
                ::close(out_fd);
            }

            if (pipefd[0] != -1) ::close(pipefd[0]);
            if (pipefd[1] != -1) ::close(pipefd[1]);
            if (saved_stdin != -1) ::close(saved_stdin);

            std::vector<char*> argv;
            to_argv(step, argv);
            ::execvp(argv[0], argv.data());
            ::perror("execvp");
            _exit(1);
        }

        // Parent process
        pids.push_back(child);
        if (pipefd[1] != -1) ::close(pipefd[1]);
        if (prev_read != -1) ::close(prev_read);
        prev_read = pipefd[0];
    }

    // Restore original stdin
    if (saved_stdin != -1) {
        ::dup2(saved_stdin, STDIN_FILENO);
        ::close(saved_stdin);
    }

    return pids;
}

int main() {
    std::string last_dir;
    bool last_dir_set = false;
    std::vector<pid_t> background_pids;

    for (;;) {
        // 1) Clean up any finished background processes
        reap_background(background_pids);

        // 2) Prompt + read line
        show_prompt();
        std::string line;
        if (!std::getline(std::cin, line)) {
            std::cout << std::endl;
            break;
        }

        // 3) Exit command (must match original prints)
        if (line == "exit") {
            std::cout << RED << "Now exiting shell..." << std::endl << "Goodbye" << NC << std::endl;
            break;
        }

        // 4) Tokenize; ignore invalid/empty
        Tokenizer tz(line);
        if (tz.hasError() || tz.commands.empty()) continue;

        // 5) Builtins (only when single command and has args)
        if (tz.commands.size() == 1 && !tz.commands[0]->args.empty()) {
            if (builtin_cd(tz.commands[0], last_dir, last_dir_set)) {
                continue;
            }
        }

        // 6) Background detection and pipeline exec
        const bool run_in_background = wants_background(tz.commands);
        std::vector<pid_t> kids = execute_pipeline(tz.commands);

        if (run_in_background) {
            // Track all children; print first PID in brackets exactly as before
            background_pids.insert(background_pids.end(), kids.begin(), kids.end());
            if (!kids.empty()) {
                std::cout << "[" << kids.front() << "]" << std::endl;
            }
        } else {
            // Foreground: wait in order
            for (pid_t c : kids) {
                ::waitpid(c, nullptr, 0);
            }
        }
    }

    return 0;
}
