#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <unistd.h>     // For fork(), execvp(), getcwd(), chdir()
#include <sys/wait.h>   // For waitpid()
#include <cstdlib>      // For exit()
#include <csignal>      // For signal handling
#include <cerrno>       // For errno
#include <cstring>      // For strerror()

using namespace std;

vector<string> history;
const int HISTORY_SIZE = 10;
volatile sig_atomic_t sigint_count = 0;

// --- Function Prototypes ---
void parse_args(const string& line, vector<string>& args);
void execute_command(const string& cmd);
void add_to_history(const string& cmd);
void print_history();
string get_command_from_history(const string& arg);
void sigint_handler(int signum);

int main() {
    // Register the signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, sigint_handler);

    // The main shell loop
    while (true) {
        // 1. Display a dynamic prompt 
        char cwd[1024];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
             // Handle error if getting CWD fails
             cout << "guish:" << ":" << history.size() + 1 << "> ";
        } else {
             cout << "guish:" << cwd << ":" << history.size() + 1 << "> ";
        }

        // 2. Read user input
        string line;
        if (!getline(cin, line)) {
            // Handle End-of-File as an exit
            cout << endl;
            break;
        }

        // Ignore empty input
        if (line.empty() || line.find_first_not_of(" \t\n\v\f\r") == string::npos) {
            continue;
        }

        // 3. Handle 'r' command specially 
        // 'r' is NOT added to history
        if (line[0] == 'r' && (line.length() == 1 || line[1] == ' ')) {
            string arg = (line.length() > 2) ? line.substr(2) : "";
            string cmd_from_hist = get_command_from_history(arg);

            if (!cmd_from_hist.empty()) {
                cout << "Executing: " << cmd_from_hist << endl;
                execute_command(cmd_from_hist);
            } else {
                cerr << "History command not found." << endl;
            }
        } else {
            // 4. For all other commands, execute them directly
            execute_command(line);
        }
    }
    
    // Before exiting, print the interrupt count
    cout << "\n[Shell exiting... SIGINT (Ctrl+C) was caught " << sigint_count << " times]" << endl;
    return 0;
}

// Signal handler for SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    sigint_count++;
    cout << "\nCaught SIGINT. To exit, type 'exit'." << endl;
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        cout << "guish:" << ":" << history.size() + 1 << "> " << flush;
    } else {
        cout << "guish:" << cwd << ":" << history.size() + 1 << "> " << flush;
    }
}


// Parses a command string into a vector of arguments
void parse_args(const string& line, vector<string>& args) {
    args.clear();
    stringstream ss(line);
    string token;
    while (ss >> token) {
        args.push_back(token);
    }
}

// Main function to process and execute a command
void execute_command(const string& cmd) {
    vector<string> args;
    parse_args(cmd, args);

    if (args.empty()) {
        return;
    }

    // Built-in: exit 
    if (args[0] == "exit") {
        cout << "[Shell exiting... SIGINT (Ctrl+C) was caught " << sigint_count << " times]" << endl;
        exit(0);
    }
    
    add_to_history(cmd);

    // Built-in: hist
    if (args[0] == "hist") {
        print_history();
        return; // Done with this command
    }
    
    // Built-in: cd
    if (args[0] == "cd") {
        if (args.size() > 1) {
            if (chdir(args[1].c_str()) != 0) {
                perror("cd failed");
            }
        } else {
            // Go to home directory if 'cd' is alone
            const char* home = getenv("HOME");
            if (home) {
                if (chdir(home) != 0) {
                    perror("cd to HOME failed");
                }
            }
        }
        return; // Done with this command, do not fork
    }

    // --- Execute External Commands ---
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork failed");
        return;
    } 
    
    if (pid == 0) {
        // --- Child Process ---
        vector<char*> c_args;
        for (auto& s : args) {
            c_args.push_back(&s[0]);
        }
        c_args.push_back(nullptr);

        execvp(c_args[0], c_args.data());
        
        // If execvp returns, an error occurred.
        cerr << "The program '" << args[0] << "' seems missing. Error code is: " << errno << " (" << strerror(errno) << ")" << endl;
        exit(127); // Standard exit code for "command not found"

    } else {
        // --- Parent Process ---
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status)) {
            int exit_code = WEXITSTATUS(status);
            // Only print the message if the program returned a non-zero (error) code.
            if (exit_code != 0) {
                 cout << "[ Program '" << args[0] << "' returned exit code " << exit_code << " ]" << endl;
            }
        }
    }
}

// Adds a command to the history, ensuring it doesn't exceed the max size
void add_to_history(const string& cmd) {
    if (history.size() == HISTORY_SIZE) {
        history.erase(history.begin()); // Remove the oldest command
    }
    history.push_back(cmd);
}

// Prints the command history, numbered 1 to 10
void print_history() {
    int start_num = 1;
    for (const auto& cmd : history) {
        cout << "  " << start_num++ << ": " << cmd << endl;
    }
}

// Retrieves a command from history based on 'r' argument
string get_command_from_history(const string& arg) {
    if (history.empty()) {
        return "";
    }
    if (arg.empty()) {
        // 'r' with no number: get the most recent command
        return history.back();
    }

    try {
        int n = stoi(arg);
        if (n >= 1 && n <= static_cast<int>(history.size())) {
            return history[n - 1]; 
        }
    } catch (const std::invalid_argument& ia) {
        cerr << "Invalid number for 'r': " << arg << endl;
        return "";
    } catch (const std::out_of_range& oor) {
        cerr << "Number for 'r' is out of range: " << arg << endl;
        return "";
    }
    
    return ""; // Not found
}