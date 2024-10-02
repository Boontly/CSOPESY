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
#include <iomanip>

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


	string nvidiaPrint(const string& str, size_t width) {
		cout << setw(width);
		if (str.length() > width) {
			if (str.length() > 8) {
				return "..." + str.substr(str.length() - (width - 3));
			}
			else {
				return "." + str.substr(str.length() - (width - 1));
			}
		} 
		return str;
	}

	void nvidiaSmi() {

		//process object
		class process {
		public:
			string gpu;
			string gi;
			string ci;
			string pid;
			string type;
			string processName;
			string gpuMemory;
			process(string gpu, string gi, string ci, string pid, string type, string processName, string gpuMemory) {
				this->gpu = gpu;
				this->gi = gi;
				this->ci = ci;
				this->pid = pid;
				this->type = type;
				this->processName = processName;
				this->gpuMemory = gpuMemory;
			}
		};

		// list of process
		vector<process> processList;

		processList.emplace_back("0", "N/A", "N/A", "4800", "C+G", "C:\\Windows\\explorer.exe", "N/A");
		processList.emplace_back("0", "N/A", "N/A", "15276", "C+G", "C:\\Users\\user\\AppData\\Local\\Discord\\app-1.0.9002\\Discord.exe", "N/A");
		processList.emplace_back("0", "N/A", "N/A", "6224", "C+G", "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe", "N/A");
		processList.emplace_back("0", "N/A", "N/A", "20280", "C+G", "C:\\Users\\user\\AppData\\Roaming\S\potify\\Spotify.exe", "N/A");
		processList.emplace_back("0", "N/A", "N/A", "23148", "C+G", "E:\\Telegram Desktop\\Telegram.exe", "N/A");
	
	
			

		// print process
		auto printProcess = [&](process p) {
			cout << "| " << right << nvidiaPrint(p.gpu, 4) << left << nvidiaPrint("",3) << nvidiaPrint(p.gi,3) << nvidiaPrint("", 2) << nvidiaPrint(p.ci, 5) << nvidiaPrint("",2) << right << nvidiaPrint(p.pid,6) << nvidiaPrint("",2) << nvidiaPrint(p.type,5) << left << nvidiaPrint("",3) << nvidiaPrint(p.processName, 38) << nvidiaPrint("", 4) << nvidiaPrint(p.gpuMemory, 8) << " |" << endl;
		};

		// get current time
		time_t now = time(nullptr);
		tm local_time;
		localtime_s(&local_time, &now);
		char buffer[80];
		strftime(buffer, sizeof(buffer), "%a %b %d %H:%M:%S %Y", &local_time);
		cout << buffer << endl;

		string nvidiaVersion = "536.23";
		string driverVersion = "526.23";
		string cudaVersion = "12.2";
		string gpuValue = "0";
		string gpuName = "NVIDIA GeForce GT 1030";
		string gpuGraphic = "WDDM";
		string gpuBusId = "00000000:01:00.0";
		string dispA = "On";
		string volatileUncorr = "N/A";
		string fan = "50%";
		string temp = "39C";
		string perf = "P0";
		string pwr = "N/A /  30W";
		string memoryUsage = "844MiB /  2048MiB";
		string gpuUtil = "0%";
		string compute = "Default";
		string mig = "N/A";

		// print part 1
		cout << "+---------------------------------------------------------------------------------------+" << endl;
		cout << "| " << left << "NVIDIA-SMI " <<  nvidiaPrint(nvidiaVersion,22)  << " Driver Version: "  << nvidiaPrint(driverVersion,12) <<  " CUDA Version: "  << nvidiaPrint(cudaVersion,8) << " |" << endl;
		cout << "|-----------------------------------------+----------------------+----------------------|" << endl;
		cout << "| " << right << setw(3) << "GPU" << nvidiaPrint("",2) << left << nvidiaPrint("Name", 25) << nvidiaPrint("TCC/WDDM", 9) << " | " << nvidiaPrint("Bus-Id", 14) << nvidiaPrint("Disp.A", 6) << " | " << right << setw(20) << "Volatile Uncorr. ECC" << " |" << endl;
		cout << "| " << left << nvidiaPrint("Fan", 5)  << nvidiaPrint("Temp", 7) << nvidiaPrint("Perf", 14) << nvidiaPrint("pwr:Usage/Cap", 13)  << " | " << right << nvidiaPrint("Memory-Usage", 20) << " | " << nvidiaPrint("GPU-Util", 8) << right << setw(12) << "Compute M." << " |" << endl;
		cout << "| " << right << setw(39) << "" << " | " << setw(20) << "" << " | " << setw(20) << "MIG M." << " |" << endl;
		cout << "|=========================================+======================+======================|" << endl;
		cout << "| " << right << nvidiaPrint(gpuValue, 3) << setw(2) << "" << left << nvidiaPrint(gpuName, 29) << nvidiaPrint(gpuGraphic, 4) << setw(1) << "" << " | " << nvidiaPrint(gpuBusId, 18) << nvidiaPrint(dispA, 2) << " | " << right << nvidiaPrint(volatileUncorr, 20) << " |" << endl;
		cout << "| " << left << nvidiaPrint(fan, 4) << setw(2) << "" << nvidiaPrint(temp, 6) << setw(1) << "" << nvidiaPrint(perf, 16) << nvidiaPrint(pwr, 10) << " | " << right << nvidiaPrint(memoryUsage,20) << " | " << right << nvidiaPrint(gpuUtil,7) << nvidiaPrint(compute, 13) << " |" << endl;
		cout << "| " << right << setw(39) << "" << " | " << setw(20) << "" << " | " << nvidiaPrint(mig, 20) << " |" << endl;	
		cout << "+-----------------------------------------+----------------------+----------------------+" << endl << endl;

		// print part 2
		cout << "+---------------------------------------------------------------------------------------+" << endl;
		cout << "| " << left << nvidiaPrint("Processes: ",85) << " |" << endl;
		cout << "| " << right << nvidiaPrint("GPU", 4) << left << nvidiaPrint("", 3) << nvidiaPrint("GI",5) << nvidiaPrint("CI",6) << nvidiaPrint("",2) << right << setw(5) << "PID" << left << nvidiaPrint("",3) << nvidiaPrint("Type",4) << nvidiaPrint("",3) << nvidiaPrint("Process name",38) << nvidiaPrint("",2) << nvidiaPrint("GPU Memory",10) << " |"  << endl;
		cout << "| " << left << nvidiaPrint("", 7) << nvidiaPrint("ID", 5) << nvidiaPrint("ID", 63) << nvidiaPrint("Usage",10) << nvidiaPrint(" |", 2) << endl;
		cout << "|=======================================================================================|" << endl;
		for (const auto p : processList) {
			printProcess(p);
		}
		cout << "+---------------------------------------------------------------------------------------+" << endl;
	}

	bool mainMenuCommand(vector<string> seperatedCommand, string command_to_check) {
		const set<string> commands = { "initialize", "screen", "scheduler-test", "scheduler-stop", "report-util", "clear", "exit","nvidia-smi"};

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

		else if (seperatedCommand[0] == "nvidia-smi") {
			nvidiaSmi();
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
