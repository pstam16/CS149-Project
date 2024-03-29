#include <cctype> // for toupper()
#include <string>
#include <cstdlib> // for EXIT_SUCCESS and EXIT_FAILURE
#include <cstring> // for strerror()
#include <cerrno> // for errno
#include <deque> // for deque (used for ready and blocked queues)
#include <fstream> // for ifstream (used for reading simulated programs)
#include <iostream> // for cout, endl, and cin
#include <sstream> // for stringstream (used for parsing simulated programs)
#include <sys/wait.h> // for wait()
#include <unistd.h> // for pipe(), read(), write(), close(), fork(), and _exit()
#include <vector> // for vector (used for PCB table)
#include <algorithm>  // for find_if (needed for trim function)
using namespace std;
#define SIZE 500

// Trim function to remove leading and trailing white spaces from a string
string trim(const string& str) {
    //Returns true if no white space
    auto notSpace = [](unsigned char c){
        return !std::isspace(c);
    };
    // Locates first character in string that is not a white space
    auto first = find_if(str.begin(), str.end(), notSpace);
    // Locates last non-white space character in string
    auto last = find_if(str.rbegin(), str.rend(), notSpace).base();
    // Returns concat string without the leading or trailing white spaces
    return (first < last ? string(first, last) : string());
}

class Instruction {
public:
    char operation;     // Operation code letter
    int intArg;         // Integer argument for some operations
    string stringArg;   // String argument for some operations
};

class Cpu {
public:
    vector<Instruction> *pProgram;  // Pointer to program being executed
    int programCounter; // Current position in the program
    int value;          // Current value held by CPU
    int timeSlice;      // Time quantum for CPU execution
    int timeSliceUsed;  // Time slice used by CPU
};

enum State {
    STATE_READY,    // Process is ready to run
    STATE_RUNNING,  // Process currently running
    STATE_BLOCKED   // Process blocked
};

class PcbEntry {    // Contains information for  each process entry
public:
    int processId;  // Contains process id
    int parentProcessId;    // Contains parent process id
    vector<Instruction> program;    // Program instruction vector
    unsigned int programCounter;    // Program counter
    int value;  // Value held by the program
    unsigned int priority;  // Priority level of the process
    State state;    // State of the process
    unsigned int startTime; // Start time of process
    unsigned int timeUsed;  // Time used by the process
};

PcbEntry pcbEntry[SIZE];    // Array of processes
unsigned int timestamp = 0; //  Global timestamp
unsigned int uniqueID = 1;  // Unique ID for processes
Cpu cpu;    // Instance of  CPU

// For the states below, -1 indicates empty (since it is an invalid index).
int runningState = -1;
deque<int> readyState;
deque<int> blockedState;

// In this implementation, we'll never explicitly clear PCB entries and the index in
// the table will always be the process ID. These choices waste memory, but since this
// program is just a simulation it the easiest approach. Additionally, debugging is
// simpler since table slots and process IDs are never re-used.

double cumulativeTimeDiff = 0;
double avgTurnaroundTime;
int numTerminatedProcesses = 0;

// Creates program from a file
bool createProgram(const string &filename, vector<Instruction> &program) {
    // Open the file
    ifstream file;
    int lineNum = 0;
    file.open(filename.c_str());
    // Check if file is open
    if (!file.is_open()) {
        cout << "Error opening file " << filename << endl;
        return false;
    }
    // Read  each line from the file
    while (file.good()) {
        string line;
        getline(file, line);
        trim(line); // Trim white spaces

        // Check if the line isn't empty
        if (line.size() > 0) {
            Instruction instruction;
            // Get operation code
            instruction.operation = toupper(line[0]);
            instruction.stringArg = trim(line.erase(0, 1));
            // Get operation arguments
            stringstream argStream(instruction.stringArg);
            // Switch statement based on operation code
            switch (instruction.operation) {
                case 'S':{
                    break;
                } // Integer argument.
                    
                case 'A':{
                    break; // Integer argument.
                }
                case 'D': {
                    break; // Integer argument.
                }
                case 'F':{ // Integer argument.
                    if (!(argStream >> instruction.intArg)) {
                        cout << filename << ":" << lineNum ;
                        cout << " - Invalid integer argument ";
                        cout << instruction.stringArg << " for ";
                        cout << instruction.operation << " operation";
                        cout << endl;
                        file.close();
                        return false;
                    }
                    break;
                }
                case 'B':{ // No argument.
                    break;
                }
                case 'E':{ // No argument.
                    break;
                }
                case 'R':{ // String argument.
                    // Note that since the string is trimmed on both ends, filenames
                    // with leading or trailing whitespace (unlikely) will not work.
                    if (instruction.stringArg.size() == 0) {
                        cout << filename << ":" << lineNum << " - Missing string argument" << endl;
                        file.close();
                        return false;
                    }
                    break;
                }
                //  Handle invalid operation code
                default: {
                    cout << filename << ":" << lineNum << " - Invalid operation, ";
                    cout << instruction.operation << endl;
                    file.close();
                    return false;
                }
            }
            // Add instruction to program
            program.push_back(instruction);
        }
        lineNum++;
    }
    // Close the file and return true on success
    file.close();
    return true;
}

// Implements the S operation.
void set(int value) {
    // 1. Set the CPU value to the passed-in value.
    cout<<"Before: "<< cpu.value<<endl;
    cpu.value = value;
    cout<<"After: "<<cpu.value<<endl;
    return;
    
}

// Implements the A operation.
void add(int value) {
    // 1. Add the passed-in value to the CPU value.
    cout<<"Before: "<< cpu.value<<endl;
    cpu.value += value;
    cout<<"After: "<<cpu.value<<endl;
    return;
}

// Implements the D operation.
void decrement(int value) {
    // 1. Subtract the integer value from the CPU value.
    cout<<"Before: "<< cpu.value<<endl;
    cpu.value -= value;
    cout<<"After: "<<cpu.value<<endl;
    return;
}

// Performs scheduling.
void schedule() {
    // 1. Return if there is still a processing running (runningState != -1).
    // There is no need to schedule if a process is already running
    if (runningState != -1) {
        return;
    }
    // 2. Get a new process to run, if possible, from the ready queue.
    if (!readyState.empty()) {
    // 3. If we were able to get a new process to run:
    // a. Mark the processing as running (update the new process's PCB state)
        runningState = readyState.front();
        readyState.pop_front();
        pcbEntry[runningState].state = STATE_RUNNING;
    // b. Update the CPU structure with the PCB entry details (program, program counter,
    // value, etc.)
        cpu.pProgram = &(pcbEntry[runningState].program);
        cpu.programCounter = pcbEntry[runningState].programCounter;
        cpu.value = pcbEntry[runningState].value;
    }
}

// Implements the B operation.
void block() {
    // 1. Add the PCB index of the running process (stored in runningState) to the blocked queue.
    blockedState.push_back(runningState);
    // 2. Update the process's PCB entry
    // a. Change the PCB's state to blocked.
    pcbEntry[runningState].state = STATE_BLOCKED;
    // b. Store the CPU program counter in the PCB's program counter.
    pcbEntry[runningState].programCounter = cpu.programCounter;
    // c. Store the CPU's value in the PCB's value.
    pcbEntry[runningState].value = cpu.value;
    pcbEntry[runningState].timeUsed++; // Account for the E call with the ++
    // 3. Update the running state to -1 (basically mark no process as running). Note that a new process will be chosen to run later (via the Q command code calling the schedule() function).
    runningState = -1;
    return;
}

// Implements the E operation.
void end() {   // We need to find a way to save the culmulative time to the process
    // 1. Get the PCB entry of the running process.
    PcbEntry& runningProcess = pcbEntry[runningState]; //What to do with it
    // 2. Update the cumulative time difference (increment it by timestamp + 1 - start time of the process).
    cumulativeTimeDiff += timestamp + 1 - runningProcess.startTime;
    // 3. Increment the number of terminated processes.
    ++numTerminatedProcesses;
    //Cleared the data in the PCB ******
    //pcbEntry[runningProcess]=NULL;
    pcbEntry[runningState].timeUsed++; // Account for the E call with the ++
    // 4. Update the running state to -1 (basically mark no process as running). Note that a new process will be chosen to run later (via the Q command code calling the schedule function).
    runningState = -1;
}

// Implements the F operation.
void fork(int value) {
    int newProcessId = uniqueID;
    uniqueID++;
    // 2. Get the PCB entry for the current running process.
    PcbEntry& parentEntry = pcbEntry[runningState];
    // 3. Ensure the passed-in value is not out of bounds.
    if (value < 0 || value >= parentEntry.program.size()-(parentEntry.programCounter-1)) { //consider the curr program counter could result in out of bounds ******
        cout << "Invalid fork value" << endl;
        return;
    }
    // 4. Populate the PCB entry obtained in #1
    PcbEntry& childEntry = pcbEntry[newProcessId];
    // a. Set the process ID to the PCB index obtained in #1.
    childEntry.processId = newProcessId;
    // b. Set the parent process ID to the process ID of the running process (use the running process's PCB entry to get this).
    childEntry.parentProcessId = parentEntry.processId;
    // c. Set the program counter to the cpu program counter.
    childEntry.programCounter = cpu.programCounter; //Set it to the next one*******
    // d. Set the value to the cpu value.
    childEntry.value = cpu.value;
    // e. Set the priority to the same as the parent process's priority.
    childEntry.priority = parentEntry.priority;
    // f. Set the state to the ready state.
    childEntry.state = STATE_READY;
    // g. Set the start time to the current timestamp
    childEntry.startTime = timestamp+1;
    // 5. Add the pcb index to the ready queue.
    readyState.push_back(newProcessId);
    // 6. Increment the cpu's program counter by the value read in #3
    childEntry.program = parentEntry.program;
    // Copy timeUsed and value
    childEntry.timeUsed = 0;
    cpu.programCounter += (value); // ++1
}

// Implements the R operation.   
void replace(string &argument) {
    // 1. Clear the CPU's program (cpu.pProgram->clear()).
    cpu.pProgram->clear();
    // 2. Use createProgram() to read in the filename specified by argument into the CPU (*cpu.pProgram)
    if (!createProgram(argument, *cpu.pProgram)) { //handle the exception ******
    // a. Consider what to do if createProgram fails. I printed an error, incremented the cpu program counter and then returned. Note that createProgram can fail if the file could not be opened or did not exist.
        cout << "Failed to replace program from file: " << argument << endl;
        return;
    }
    // 3. Set the program counter to 0.
    cpu.programCounter = 0;
    cpu.value = 0; //int value needs to be undefined **********
}

// Implements the Q command.
void quantum() {
    Instruction instruction;
    cout << "In quantum: "<<endl;
    // Check if there's a running process
    if (runningState == -1) {
        cout << "No processes are running" << endl;
        return;
    }
    // Check if the program counter is within bounds
    if (cpu.programCounter < cpu.pProgram->size()) {
        // Get the next instruction from the program
        instruction = (*cpu.pProgram)[cpu.programCounter];
        ++cpu.programCounter;
    } else {
        // When program ends without E operation
        cout << "End of program reached without E operation" << endl;
        instruction.operation = 'E';
    }
    // Switch statement based on the operation code
    switch (instruction.operation) {
        case 'S': { // Set
            set(stoi(instruction.stringArg));
            cout << "instruction S " << stoi(instruction.stringArg) << endl;
            break;
        }
        case 'A':{ // Add
            add(stoi(instruction.stringArg));
            cout << "instruction A " << stoi(instruction.stringArg) << endl;
            break;
        }
        case 'D':{ // Decrement
            decrement(stoi(instruction.stringArg));
            cout << "instruction D " << stoi(instruction.stringArg) << endl;
            break;
        }
        case 'B':{ // Block
            block();
            cout << "instruction B" << endl;
            break;
        }
        case 'E':{ // End
            end(); 
            cout << "instruction E" << endl;
            break;
        }
        case 'F':{ // Fork
            fork(instruction.intArg);
            cout << "instruction F " << stoi(instruction.stringArg) << endl;
            break;
        }
        case 'R':{ // Replace
            replace(instruction.stringArg);
            cout << "instruction R " << instruction.stringArg << endl;
            break;
        }
    }
    if(runningState!=-1){
         pcbEntry[runningState].timeUsed++;
    }
    ++timestamp;
    schedule();
    cout << "Timestamp: " << timestamp << endl;
    cout << endl<< "Enter Q, P, U or T" << endl;
}

// Implements the U command.
void unblock() {
    // 1. If the blocked queue contains any processes:
    if (!blockedState.empty()) {
    // a. Remove a process form the front of the blocked queue.
        int blockedProcessIndex = blockedState.front();
        blockedState.pop_front();
    // b. Add the process to the ready queue.
        readyState.push_back(blockedProcessIndex);
    // c. Change the state of the process to ready (update its PCB entry).
        pcbEntry[blockedProcessIndex].state = STATE_READY;
    // 2. Call the schedule() function to give an unblocked process a chance to run (if possible).
        schedule();
    }
}

// Implements the P command.
void print() { // Prints information to simulate a reporter process
    if(runningState==-1){
        return;
    }else{
        cout << "The Process ID is: " << pcbEntry[runningState].processId << endl;
        cout << "The Parent Process ID is: " << pcbEntry[runningState].parentProcessId << endl;
        cout << "The Value is: " << cpu.value << endl;
	    cout << "CPU Program Counter: " << cpu.programCounter << endl;
        cout << "Process Start Time: " << pcbEntry[runningState].startTime << endl;
        cout << "Timestamp: " << timestamp << endl;
    }
}

//Implements the T command
void term(){
    // Prints the average turnaround time once finished
    if (numTerminatedProcesses > 0) {
        avgTurnaroundTime = cumulativeTimeDiff / numTerminatedProcesses;
        cout << "Average Turnaround Time: " << avgTurnaroundTime << endl;
    } else {
        // Cannot print average turnaround time if no processes have finished
        cout << "No processes have terminated yet." << endl;
    }
}

// Function that implements the process manager.
int runProcessManager(int fileDescriptor) {

    // Attempt to create the init process.
    if (!createProgram("init.txt", pcbEntry[0].program)) {
        return EXIT_FAILURE;
    }
    // Initialzie process info
    pcbEntry[0].processId = 0;
    pcbEntry[0].parentProcessId = -1;
    pcbEntry[0].programCounter = 0;
    pcbEntry[0].value = 0;
    pcbEntry[0].priority = 0;
    pcbEntry[0].state = STATE_RUNNING;
    pcbEntry[0].startTime = 0;
    pcbEntry[0].timeUsed = 0;
    runningState = 0;
    cpu.pProgram = &(pcbEntry[0].program);
    cpu.programCounter = pcbEntry[0].programCounter;
    cpu.value = pcbEntry[0].value;
    timestamp = 0;
    // Loop until a 'T' is read, then terminate.
    char ch;
    do {
        // Read a command character from the pipe.
        if (read(fileDescriptor, &ch, sizeof(ch)) != sizeof(ch)) {
            // Assume the parent process exited, breaking the pipe.
            break;
        }
        switch (ch) {
            case 'Q':{ // Quantum
                cout << "You entered Q" << endl;
                quantum();
                break;
            }
            case 'U': { // Unblock
                cout << "You entered U" << endl;
                unblock();
                break;
            }
            case 'P':{ // Print
                cout << "You entered P" << endl;
                pid_t processRtpPid = fork();
                if(processRtpPid == -1){
                    _exit(-1);
                }
                if(processRtpPid == 0){
                    if(runningState==-1){
                        cout<<"Nothing Running"<<endl;
                        _exit(-2);
                    }
                    print();
                    _exit(1);
                }
                wait(NULL);
                break;
            }
            case 'T':{ // Terminate
                term();
                cout << "You entered T. Exiting program..." << endl;
                break;
            }
            default:{
                cout << "You entered an invalid character!" << endl;
            }
        }
    } while (ch != 'T');
    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    int pipeDescriptors[2];
    pid_t processMgrPid;
    char ch;
    int result;
    // Create a pipe
    pipe(pipeDescriptors);
    // USE fork() SYSTEM CALL to create the child process and save the value returned in processMgrPid variable
    if ((processMgrPid = fork()) == -1)
        exit(1); /* FORK FAILED */
    if (processMgrPid == 0) {
        // The process manager process is running.
        // Close the unused write end of the pipe for the process manager process.
        close(pipeDescriptors[1]);
        // Run the process manager.
        result = runProcessManager(pipeDescriptors[0]);
        // Close the read end of the pipe for the process manager process (for cleanup purposes).
        close(pipeDescriptors[0]);
        _exit(result);
    } else {
        // The commander process is running.
        // Close the unused read end of the pipe for the commander process.
        close(pipeDescriptors[0]);
        // Loop until a 'T' is written or until the pipe is broken.
        cout << endl<< "Enter Q, P, U or T" << endl;
        cout << "$ ";
        do {
            cin >> ch;
            // Pass commands to the process manager process via the pipe.
            if (write(pipeDescriptors[1], &ch, sizeof(ch)) != sizeof(ch)) {
                // Assume the child process exited, breaking the pipe.
                break;
            }
        } while (ch != 'T');
        write(pipeDescriptors[1], &ch, sizeof(ch));
        // Close the write end of the pipe for the commander process (for cleanup purposes).
        close(pipeDescriptors[1]);
        // Wait for the process manager to exit.
        wait(&result);
    }
    return result;
}