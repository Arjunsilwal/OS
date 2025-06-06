#include <iostream>
#include <vector>
#include <cstring>
#include <string>
#include <sstream>
#include <unistd.h>     // For fork(), execvp(), getcwd(), chdir()
#include <sys/wait.h>   // For waitpid()
#include <cstdlib>      // For exit()
#include <csignal>      // For signal handling
#include <cerrno>       // For errno

using namespace std;


// Use a vector to store the command history. A deque would also be a good choice.
vector<string> history;
const int HISTORY_SIZE = 10;

// A volatile sig_atomic_t is safe to use in a signal handler
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
        // 1. Display a dynamic prompt (Requirement #5)
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        cout << "guish:" << cwd << ":" << history.size() + 1 << "> ";

        // 2. Read user input
        string line;
        if (!getline(cin, line)) {
            // Handle End-of-File (Ctrl+D) as an exit
            cout << endl;
            break;
        }

        // Ignore empty input
        if (line.empty() || line.find_first_not_of(" \t\n\v\f\r") == string::npos) {
            continue;
        }

        // 3. Handle 'r' command specially (Requirement #4)
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
    
    // Before exiting, print the interrupt count (part of 'exit' command requirement)
    cout << "\n[Shell exiting... SIGINT (Ctrl+C) was caught " << sigint_count << " times]" << endl;
    return 0;
}

// Signal handler for SIGINT (Ctrl+C)
void sigint_handler(int signum) {
    sigint_count++;
    // We print a newline to make the prompt reappear cleanly after Ctrl+C
    cout << "\nCaught SIGINT. Press Ctrl+C again to exit, or continue typing." << endl;
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
        return; // Nothing to do
    }

    // --- Handle Built-in Commands ---

    // Built-in: exit (Requirement #1)
    if (args[0] == "exit") {
        // The exit logic is handled in main's loop break/return
        // We call exit() directly to terminate the process
        cout << "[Shell exiting... SIGINT (Ctrl+C) was caught " << sigint_count << " times]" << endl;
        exit(0);
    }
    
    // Built-in: hist (Requirement #2)
    // 'hist' command is added to history before being executed
    add_to_history(cmd);
    if (args[0] == "hist") {
        print_history();
        return;
    }
    
    // Built-in: cd (A useful addition, not required but standard for shells)
    if (args[0] == "cd") {
        if (args.size() > 1) {
            if (chdir(args[1].c_str()) != 0) {
                perror("cd failed");
            }
        } else {
            // cd with no arguments typically goes to the home directory
            chdir(getenv("HOME"));
        }
        return;
    }

    // Command being executed is added to history (if it's not a special case)
    // We already added `hist`, now add everything else.
    // `r` and `exit` are handled outside or don't reach here.
    if (args[0] != "hist") { // Avoid double-adding hist
         add_to_history(cmd);
    }
   
    // --- Execute External Commands ---

    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        perror("fork failed");
        return;
    } else if (pid == 0) {
        // --- Child Process ---
        
        // Convert vector<string> to char* array for execvp
        vector<char*> c_args;
        for (auto& s : args) {
            c_args.push_back(&s[0]);
        }
        c_args.push_back(nullptr);

        execvp(c_args[0], c_args.data());
        
        // If execvp returns, an error occurred (Requirement: Handling Erroneous Programs)
        cerr << "The program '" << args[0] << "' seems missing. Error code is: " << errno << " (" << strerror(errno) << ")" << endl;
        exit(EXIT_FAILURE); // Exit child process with an error code

    } else {
        // --- Parent Process ---
        int status;
        waitpid(pid, &status, 0);

        // Check if the child terminated normally and had a non-zero exit status
        // (Requirement: Handling Erroneous Programs)
        if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
            cout << "[ Program returned exit code " << WEXITSTATUS(status) << " ]" << endl;
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
        if (n >= 1 && n <= history.size()) {
            return history[n - 1]; // History is 0-indexed, but displayed as 1-indexed
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
