#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
#include <algorithm>
#include <string>
#include <set>
#include <windows.h>
#include <sstream>
#include <ctime>
#include <cstdio>
#include <mutex>
#include <map>  
#include <queue>
#include <thread>
#include <condition_variable>

using namespace std;
typedef long long ll;

class CpuWorker;
class Scheduler;

struct BufferEntry {
	string text;
	WORD color;
};

static string generateRandomName() {
	const string characters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	string random_name;
	int length = rand() % 10 + 5; // Random length between 5 and 15

	for (int i = 0; i < length; ++i) {
		random_name += characters[rand() % characters.size()];
	}

	return random_name;
}

class abstract_screen {
protected:
	vector<BufferEntry> buffer;
	mutex console_mutex;

	void write(const string& text, WORD color = 7) {
		lock_guard<mutex> lock(console_mutex);
		buffer.push_back({ text, color });
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		cout << text << endl;
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	}


public:
	virtual void redraw() {
		system("cls");
		lock_guard<mutex> lock(console_mutex);
		for (const auto& entry : buffer) {
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), entry.color);
			cout << entry.text << endl;
			SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
		}
	};

	
	void public_write(const string& text) {
		lock_guard<mutex> lock(console_mutex);
		buffer.push_back({ text, 7});
	}
};

class Screen : public abstract_screen {
private:
	string processName;
	int currentLine;
	int totalLine;
	int core_id;
	time_t timestamp;
	vector<string> statements;

	stringstream printScreen_helper() {
		stringstream ss;
		char buffer[26];
#ifdef _WIN32
		ctime_s(buffer, sizeof(buffer), &timestamp);
#else

		strncpy(buffer, time_str.c_str(), sizeof(buffer) - 1);
		buffer[sizeof(buffer) - 1] = '\0';
#endif

		ss << "Process: " << processName << "\n"
			<< "Current Line: " << currentLine << " / " << totalLine << "\n"
			<< "Timestamp: " << buffer;

		return ss;
	}

public:
	Screen(string name, int currentLine, int totalLine)
		: processName(name), currentLine(currentLine),
		totalLine(totalLine), timestamp(time(nullptr)) {
	}

	Screen(string name)
		: Screen(name, 1, 100) {
	}

	Screen(const Screen& other) {
		processName = other.processName;
		currentLine = other.currentLine;
		totalLine = other.totalLine;
		timestamp = other.timestamp;
	}

	Screen& operator=(const Screen& other) {
		if (this == &other) return *this;
		processName = other.processName;
		currentLine = other.currentLine;
		totalLine = other.totalLine;
		timestamp = other.timestamp;
		buffer = other.buffer;
		return *this;
	}

	Screen() {
	}

	string getProcessName() const {
		return processName;
	}

	int getCurrentLine() const {
		return currentLine;
	}

	int getTotalLine() const {
		return totalLine;
	}

	time_t getTimestamp() const {
		return timestamp;
	}

	vector<string> getStatements() const{
		return statements;
	}

	void setStatements(vector<string> statement_arr){
		statements = statement_arr;
	}

	bool operator<(const Screen& other) const {
		return timestamp < other.timestamp;
	}

	void openScreen() {
		system("cls");

		string content = printScreen_helper().str();
		int width = content.length() + 4;

		write("+" + string(width - 2, '-') + "+");

		stringstream contentStream(content);
		string line;
		while (getline(contentStream, line)) {
			write("| " + line + string(width - line.length() - 4, ' ') + " |");
		}
		write("+" + string(width - 2, '-') + "+");
	}

	boolean screenCommand(vector<string> seperatedCommand, string command_to_check) {

		const set<string> commands = { "exit" };

		if (!commands.count(seperatedCommand[0])) {
			write("Unknown command: " + command_to_check);
			return false;
		}

		if (seperatedCommand[0] == "exit") {
			if (seperatedCommand.size() != 1) {
				write("INVALID COMMAND");
				return false;
			}
			return true;
		}
		return false;
	}

};

class CpuWorker {
public:
    int id;           // Worker ID
    bool stop = false; // Flag to indicate if the worker should stop

    // Constructor that takes a worker ID
    CpuWorker(int worker_id) : id(worker_id) {}

    // Method to process the screen
    void processScreen(Screen screen) {
        string filename = "dog_" + screen.getProcessName();
        vector<string> commands_to_be_printed = screen.getStatements();

        // Open the file to append the print command information
        ofstream outFile(filename, ios::app);
        if (outFile.is_open()) {
            // Get current time
            time_t now = time(nullptr);
            
            // Format and write the current timestamp and core ID to the file
            ostringstream oss;
            oss << "Timestamp: " << put_time(localtime(&now), "%Y-%m-%d %H:%M:%S") << " | Core: " << id << " | Commands: ";
            screen.public_write(oss.str());

            // Write each command to the file
            for (const auto& command : commands_to_be_printed) {
                outFile << command << "; "; // Separate commands with a semicolon
            }

            outFile << endl; // End the line after all commands are written
            outFile.close(); // Close the file after writing
        } else {
            cerr << "Error opening file: " << filename << endl;
        }
    }
};

class Scheduler {
public:
    int num_threads = 4;
    queue<Screen> screen_queue; // Queue for process
    vector<CpuWorker> workers;
    vector<thread> workerThreads; // Store worker threads
    mutex queue_mutex; 
    condition_variable cv; 
    bool stop = false;

    Scheduler() {
        for (int i = 0; i < num_threads; ++i) {
            workers.emplace_back(i); // Create a new worker with its ID
        }
        startWorkers(); // Start the worker threads after creation
    }

    void startWorkers() {
        for (auto& worker : workers) {
            try {
                workerThreads.emplace_back([this, &worker]() {
                    while (true) {
                        Screen screen(""); // Default initialization

                        unique_lock<mutex> lock(queue_mutex);
                        cv.wait(lock, [this] { 
                            return stop || !screen_queue.empty(); 
                        }); // Wait for a task or stop signal

                        if (stop && screen_queue.empty()) return; // Exit if stop signal received and no tasks left

                        screen = screen_queue.front(); // Retrieve the next task
                        screen_queue.pop(); // Remove the task from the queue

                        // Process the screen content
                        worker.processScreen(screen);
                    }
                });
            } catch (const exception& e) {
                cerr << "Exception in worker thread: " << e.what() << endl;
            } catch (...) {
                cerr << "Unknown exception in worker thread." << endl;
            }
        }
    }

    void stopAll() {
        {
            lock_guard<mutex> lock(queue_mutex);
            stop = true; // Set the stop flag
        }
        cv.notify_all(); // Notify all threads to wake up
        for (auto& workerThread : workerThreads) {
            if (workerThread.joinable()) {
                workerThread.join(); // Wait for all threads to finish
            }
        }
    }

    ~Scheduler() {
        stopAll();
    }

    void enqueue(const Screen& screen) {
        {
            lock_guard<mutex> lock(queue_mutex);
            screen_queue.push(screen); // Add a new screen task
        }
        cv.notify_one(); // Notify one worker thread
    }
};


class MainConsole : public abstract_screen {
private:
	Scheduler scheduler;
	string currentView = "MainMenu";
	bool continue_program = true;
	map<string, Screen> screensAvailable;

	void dummy_screens() {
		set<string> dups; // To track duplicate names
		for (int i = 0; i < 10; i++) {
			string name = generateRandomName();
			// Ensure unique name
			while (dups.count(name)) {
				name = generateRandomName();
			}
			dups.insert(name);

			// Create a new Screen object with a unique name
			Screen screen(name); 

			vector<string> process_lister;
			for (int j = 0; j < 100; j++) { // Generate 100 random statements
				process_lister.push_back(generateRandomName());
			}

			// Set the statements to the screen
			screen.setStatements(process_lister);

			// Optionally write the commands if needed
			/*for (const auto& x : screen.getStatements()) {
				string temp = screen.getProcessName() + " " + x;
				write(temp); // Assuming write function is defined elsewhere
			}*/

			// Enqueue the screen after setting statements
			scheduler.enqueue(screen);
		}
		scheduler.startWorkers();
	}

	void commandRecognize(string command_to_check) {
		write(command_to_check + " command recognized. Doing something.");
	}

	void commandNotRecognize(string command_to_check) {
		write("Unknown command: " + command_to_check);
	}

	void invalidCommand(string command_to_check) {
		write("Invalid Arguments for " + command_to_check);
	}

	void screenNotFound() {
		write("Screen not found");
	}

	bool mainMenuCommand(vector<string> seperatedCommand, string command_to_check) {
		const set<string> commands = { "initialize", "screen", "scheduler-test", "scheduler-stop", "report-util", "clear", "exit" };
		if (seperatedCommand[0] == "dummy") {
			dummy_screens();
			return true;
		}

		if (!commands.count(seperatedCommand[0])) {
			commandNotRecognize(command_to_check);
			return true;
		}

		if (seperatedCommand[0] == "initialize") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			commandRecognize(command_to_check);
		}

		else if (seperatedCommand[0] == "screen") {
			if (seperatedCommand.size() < 2 || seperatedCommand.size() > 3) {
				invalidCommand(command_to_check);
				return true;
			}

			if (seperatedCommand.size() == 2) {
				if (!(seperatedCommand[1] == "-ls")) {
					invalidCommand(command_to_check);
					return true;
				}
				write("Available Screens:");
				for (auto [_, sc] : screensAvailable) {
					string content = sc.getProcessName();
					write(sc.getProcessName());
				}
				write("");
			}

			else if (seperatedCommand[1] == "-r") {
				if (!screensAvailable.count(seperatedCommand[2])) {
					screenNotFound();
					return true;
				}
				screensAvailable[seperatedCommand[2]].redraw();
				currentView = screensAvailable[seperatedCommand[2]].getProcessName();
			}
			else if (seperatedCommand[1] == "-s") {
				if (screensAvailable.count(seperatedCommand[2])) {
					cout << "This Process is already in use." << endl;
					return true;
				}
				Screen sc(seperatedCommand[2]);
				screensAvailable[seperatedCommand[2]] = sc;
				screensAvailable[seperatedCommand[2]].openScreen();
				currentView = screensAvailable[seperatedCommand[2]].getProcessName();
			}
			else {
				invalidCommand(command_to_check);
			}


		}
		else if (seperatedCommand[0] == "clear") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			system("cls");
			this->buffer.clear();
			print_header();
		}

		else if (seperatedCommand[0] == "exit") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			return false;
		}
		else {
			commandRecognize(command_to_check);
		}

		return true;
	}

	bool processCommand(const string& command) {
		string command_to_check = command;
		transform(command_to_check.begin(), command_to_check.end(), command_to_check.begin(), ::tolower);

		stringstream stream(command);
		vector<string> seperatedCommand;

		string splitter;
		while (stream >> splitter) {
			seperatedCommand.push_back(splitter);
		}

		if (currentView == "MainMenu") {
			return mainMenuCommand(seperatedCommand, command_to_check);
		}
		else {
			if (screensAvailable[currentView].screenCommand(seperatedCommand, command_to_check)) {
				currentView = "MainMenu";
				this->redraw();
			}
		}
		return true;
	}

	void setConsoleColor(int color) {
		HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleTextAttribute(hConsole, color);
	}

	void print_header() {
		write("   _____  _____  ____  _____  ______  _______     __");
		write("  / ____|/ ____|/ __ \\|  __ \\|  ____|/ ____\\ \\   / /");
		write(" | |    | (___ | |  | | |__) | |__  | (___  \\ \\_/ /");
		write(" | |     \\___ \\| |  | |  ___/|  __|  \\___ \\  \\   /");
		write(" | |____ ____) | |__| | |    | |____ ____) |  | |");
		write("  \\_____|_____/ \\____/|_|    |______|_____/   |_|");
		write("Hello, Welcome to CSOPESY commandline!", 2);
		write("Type 'exit' to quit, 'clear' to clear the screen", 14);
	}

public:
	void run() {
		string user_input;

		continue_program = true;
		print_header();

		while (continue_program) {
			cout << "Enter a command: ";
			getline(cin, user_input);
			continue_program = processCommand(user_input);
		}
	}
};

signed main() {
	srand(time(0));
	MainConsole console;
	console.run();
	return 0;
}
