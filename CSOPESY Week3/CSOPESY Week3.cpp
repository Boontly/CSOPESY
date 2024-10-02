#include <iostream>
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

using namespace std;
typedef long long ll;

struct BufferEntry {
	string text;
	WORD color;
};

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
};

class Screen : public abstract_screen {
private:
	string processName;
	int currentLine;
	int totalLine;
	time_t timestamp;

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

class MainConsole : public abstract_screen {
private:
	string currentView = "MainMenu";
	bool continue_program = true;
	map<string, Screen> screensAvailable;

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
	MainConsole console;
	console.run();
	return 0;
}
