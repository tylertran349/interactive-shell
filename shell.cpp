#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <cstring>
#include <sys/wait.h>
#include <fcntl.h>
using namespace std;

void printErrorMessage() {
    char error_message[30] = "An error has occurred\n";
    write(STDERR_FILENO, error_message, strlen(error_message)); 
}

vector<string> parseInput(string& input) {
    vector<string> tokens;
    stringstream ss(input);
    string token;
    while(ss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

vector<string> splitCommands(const string& input, const char delimiter) {
    vector<string> commands;
    stringstream ss(input);
    string command;
    while(getline(ss, command, delimiter)) {
        command.erase(0, command.find_first_not_of(" \t")); // Remove spaces on left side
        command.erase(command.find_last_not_of(" \t") + 1); // Remove spaces on right side
        if(!command.empty()) {
            commands.push_back(command);
        }
    }
    return commands;
}

void executeCommand(const string& command, const vector<string>& args, const vector<string>& customPath, const string& outputFile, bool batchMode) {
    pid_t pid = fork();
    if (pid == -1) {
        printErrorMessage();
        return;
    }
    if (pid == 0) { // Child process
        // Redirection handling
        int fileDescriptor = -1;
        if (!outputFile.empty()) {
            fileDescriptor = open(outputFile.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        }
        if(fileDescriptor == -1 && !outputFile.empty()) {
            printErrorMessage();
            exit(1);
        }
        if(!outputFile.empty()) {
            dup2(fileDescriptor, STDOUT_FILENO);
            dup2(fileDescriptor, STDERR_FILENO);
            close(fileDescriptor);
        }
        // Prepare command
        vector<char*> commandArgs;
        commandArgs.push_back(const_cast<char*>(command.c_str()));
        for (const auto& arg : args) {
            commandArgs.push_back(const_cast<char*>(arg.c_str()));
        }
        commandArgs.push_back(nullptr);
        for (const auto& dir : customPath) {
            string fullPath = dir + "/" + command;
            if (access(fullPath.c_str(), X_OK) == 0) {
                execv(fullPath.c_str(), commandArgs.data());
            }
        }
        printErrorMessage();
        exit(1);
    } else if(pid > 0) {
        if(batchMode && !outputFile.empty()){
            int fileDescriptor = open(outputFile.c_str(), O_RDONLY);
            char buffer[1024];
            ssize_t bytesRead;
            while((bytesRead = read(fileDescriptor, buffer, sizeof(buffer) - 1)) > 0){
                buffer[bytesRead] = '\0';
                write(STDOUT_FILENO, buffer, bytesRead);
            }
            close(fileDescriptor);
        }
        waitpid(pid, nullptr, 0);
    }
}

int main(int argc, char* argv[]) {
    vector<string> customPath = {"/bin", "/usr/bin"};
    bool batchMode = false;
    if(argc > 2) { // Shell is invoked with more than 1 file, error
        printErrorMessage(); 
        exit(1);
    }
    if(argc == 2) { // Batch mode
        batchMode = true;
        int batchFile = open(argv[1], O_RDONLY);
        if(batchFile == -1) {
            printErrorMessage();
            exit(1);
        }
    
        // Read batch file content
        char buffer[1024];
        ssize_t bytesRead;
        string input;
        while((bytesRead = read(batchFile, buffer, sizeof(buffer) - 1)) > 0) {
            buffer[bytesRead] = '\0';
            input += string(buffer);
        }
        if(bytesRead == -1) {
            printErrorMessage();
            exit(1);
        }
        close(batchFile);
        size_t pos;
        while((pos = input.find('\n')) != string::npos) {
            string line = input.substr(0, pos);
            input.erase(0, pos + 1);
            if(!line.empty()) {
                vector<string> parallelCommands = splitCommands(line, '&');
                vector<pid_t> childPIDs;

                for (const auto& fullCommand : parallelCommands) {
                    size_t redirPos = fullCommand.find(">");
                    string commandPart;
                    string outputFile;

                    // Handle redirection
                    if (redirPos != string::npos) {
                        commandPart = fullCommand.substr(0, redirPos);
                        string redirectionPart = fullCommand.substr(redirPos + 1);

                        // Trim whitespace
                        redirectionPart.erase(0, redirectionPart.find_first_not_of(" \t"));
                        redirectionPart.erase(redirectionPart.find_last_not_of(" \t") + 1);

                        // If there is no command before redirection symbol, print error message
                        if(commandPart.empty()) {
                            printErrorMessage();
                            continue;
                        }

                        // Split redirectionPart into tokens
                        stringstream ss(redirectionPart);
                        vector<string> redirectionTokens;
                        string token;
                        while (ss >> token) {
                            redirectionTokens.push_back(token);
                        }

                        // Ensure there is exactly one token (valid file name)
                        if (redirectionTokens.size() != 1) {
                            printErrorMessage();
                            continue;
                        }
                        outputFile = redirectionTokens[0];
                    } else {
                        commandPart = fullCommand;
                    }


                    // Parse command and arguments
                    vector<string> tokens = parseInput(commandPart);
                    if (tokens.empty()) continue;

                    string command = tokens[0];
                    vector<string> args(tokens.begin() + 1, tokens.end());
                    if (command == "exit") {
                        if (args.size() > 0) {
                            printErrorMessage();
                        } else {
                            exit(0);
                        }
                    } else if (command == "cd") {
                        if(args.size() != 1) {
                            printErrorMessage();
                        } else if(chdir(args[0].c_str()) != 0) {
                            printErrorMessage();
                        }
                    } else if (command == "path") {
                        customPath = args;
                    } else {
                        pid_t pid = fork();
                        if(pid == 0) {
                            executeCommand(command, args, customPath, outputFile, batchMode);
                            exit(0);
                        } else if(pid > 0) {
                            childPIDs.push_back(pid);
                        } else {
                            printErrorMessage();
                        }
                    }
                }

                // Wait for all child processes
                for (pid_t pid : childPIDs) {
                    waitpid(pid, nullptr, 0);
                }
            }
        }
    } 
    if(argc == 1 && string(argv[0]) == "./shell") { 
        string input;
        cout << "shell> ";
        while (getline(cin, input)) {
            if (input.empty()) {
                cout << "shell> ";
                continue;
            }

            vector<string> parallelCommands = splitCommands(input, '&');
            vector<pid_t> childPIDs;

            for (const auto& fullCommand : parallelCommands) {
                size_t redirPos = fullCommand.find(">");
                string commandPart;
                string outputFile;

                // Handle redirection
                if (redirPos != string::npos) {
                    commandPart = fullCommand.substr(0, redirPos);
                    string redirectionPart = fullCommand.substr(redirPos + 1);

                    // Trim whitespace
                    redirectionPart.erase(0, redirectionPart.find_first_not_of(" \t"));
                    redirectionPart.erase(redirectionPart.find_last_not_of(" \t") + 1);

                    // If there is no command before redirection symbol, print error message
                    if(commandPart.empty()) {
                        printErrorMessage();
                        continue;
                    }

                    // Split redirectionPart into tokens
                    stringstream ss(redirectionPart);
                    vector<string> redirectionTokens;
                    string token;
                    while (ss >> token) {
                        redirectionTokens.push_back(token);
                    }

                    // Ensure there is exactly one token (valid file name)
                    if (redirectionTokens.size() != 1) {
                        printErrorMessage();
                        continue;
                    }
                    outputFile = redirectionTokens[0];
                } else {
                    commandPart = fullCommand;
                }


                // Parse command and arguments
                vector<string> tokens = parseInput(commandPart);
                if (tokens.empty()) continue;

                string command = tokens[0];
                vector<string> args(tokens.begin() + 1, tokens.end());
                if (command == "exit") {
                    if (args.size() > 0) {
                        printErrorMessage();
                    } else {
                        exit(0);
                    }
                } else if (command == "cd") {
                    if(args.size() != 1) {
                        printErrorMessage();
                    } else if(chdir(args[0].c_str()) != 0) {
                        printErrorMessage();
                    }
                } else if (command == "path") {
                    customPath = args;
                } else {
                    pid_t pid = fork();
                    if(pid == 0) {
                        executeCommand(command, args, customPath, outputFile, batchMode);
                        exit(0);
                    } else if(pid > 0) {
                        childPIDs.push_back(pid);
                    } else {
                        printErrorMessage();
                    }
                }
            }

            // Wait for all child processes
            for (pid_t pid : childPIDs) {
                waitpid(pid, nullptr, 0);
            }
            sleep(1); // Add 1 second delay after all child processes have finished running so "shell> " prompt always gets printed out after entire output of command is printed
            cout << "shell> ";
        }
    }
}