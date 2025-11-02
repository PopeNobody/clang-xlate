#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <regex>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>

using namespace std;

string real_path, sudo_path;

void munge_line(string& line) {
    if (!sudo_path.empty()) {
        size_t pos = line.find(real_path);
        if (pos != string::npos) {
            line.replace(pos, real_path.size(), sudo_path);
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        cerr << "Usage: vi-perl <script> [args...]\n";
        return 1;
    }

    real_path = argv[1];
    sudo_path = getenv("VI_PERL_SUDO") ? getenv("VI_PERL_SUDO") : "";

    // Build command: perl -c script args...
    vector<char const *> cmd = {"perl", "-c", real_path.c_str()};
    for (int i = 2; i < argc; ++i) cmd.push_back(argv[i]);

    // Pipe setup
    int pipefd[2];
    if (pipe(pipefd) == -1) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid == 0) {
        // Child: redirect stdout+stderr to pipe
        close(pipefd[0]);
        dup2(pipefd[1], 1);
        dup2(pipefd[1], 2);
        close(pipefd[1]);

        // Exec perl
        execvp("perl", (char *const *)cmd.data());
        perror("execvp");
        _exit(1);
    }

    close(pipefd[1]);
    FILE* stream = fdopen(pipefd[0], "r");
    char* line = nullptr;
    size_t len = 0;

    while (getline(&line, &len, stream) != -1) {
        string s(line);
        if (s.back() == '\n') s.pop_back();

        munge_line(s);

        // Format real errors
        regex err(R"((.+?) at .+? line (\d+))");
        smatch m;
        if (regex_search(s, m, err)) {
            s = sudo_path + ":" + m[2].str() + ": " + m[1].str();
        }

        cout << s << endl;
    }

    free(line);
    fclose(stream);
    waitpid(pid, nullptr, 0);
    return 0;
}
