#pragma once

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
	virtual void add(const string& text, WORD color = 7) {
		lock_guard<mutex> lock(console_mutex);
		buffer.push_back({ text, color });
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), color);
		SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE), 7);
	}
	
};

class Screen : public abstract_screen {
private:
	string processName;
	int currentLine;
	int totalLine;
	time_t timestamp;
	bool finished = false;
	bool initialized = false;
	int core_id = -1;
	ll memStart = -1;
	ll memEnd = -1;

	stringstream
		printScreen_helper() {
		stringstream ss;
		char buffer[26];
		ctime_s(buffer, sizeof(buffer), &timestamp);
		ss << "Process: " << processName << endl;
		if (finished) {
			ss << "Finished!" << endl;
		}
		else {
			ss << "Current Line: " << currentLine << " / " << totalLine << endl;
		}
		ss << "Timestamp: " << buffer;

		return ss;
	}

public:
	Screen(std::string name, int min, int max)
		: processName(name), currentLine(1), timestamp(time(nullptr)) {
		srand(static_cast<unsigned int>(time(nullptr)));
		totalLine = min + rand() % ((max + 1) - min);
	}

	Screen(const Screen& other) {
		processName = other.processName;
		currentLine = other.currentLine;
		totalLine = other.totalLine;
		timestamp = other.timestamp;
		finished = other.finished;
		initialized = other.initialized;
		core_id = other.core_id;

	}

	Screen& operator=(const Screen& other) {
		if (this == &other)
			return *this;
		processName = other.processName;
		currentLine = other.currentLine;
		totalLine = other.totalLine;
		timestamp = other.timestamp;
		buffer = other.buffer;
		return *this;
	}

	Screen() {
	}
	void setCoreId(int id) {
		core_id = id;
	}
	int getCoreId() const {
		return core_id;
	}
	string convert_unix_to_string(time_t unix_timestamp) {
		struct tm time_info;

		localtime_s(&time_info, &unix_timestamp);

		char time_string[100];
		strftime(time_string, sizeof(time_string), "(%m/%d/%Y %I:%M:%S %p)", &time_info);
		return string(time_string);
	}
	string listProcess() {
		stringstream ss;
		if (isFinished()) {
			ss << left << setw(20) << getProcessName() << setw(2) << "" << setw(23) << convert_unix_to_string(getTimestamp()) << setw(4) << "" << "Finished" << setw(5) << "" << getCurrentLine() << " / " << getTotalLine();
		}
		else {
			ss << left << setw(20) << getProcessName() << setw(2) << "" << setw(23) << convert_unix_to_string(getTimestamp()) << setw(4) << "" << "Core: " << getCoreId() << setw(5) << "" << getCurrentLine() << " / " << getTotalLine();
		}
		return ss.str();
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

	bool isInitialized() {
		return initialized;
	}

	void initialize() {
		initialized = true;
	}

	void execute() {
		currentLine++;
		if (currentLine == totalLine) {
			finished = true;
		}
		for (int i = 0; i < 100; i++) {
			// Do nothing
		}
	}

	void openScreen() {
		system("cls");
		screenInfo();
	}

	bool isFinished() {
		return finished;
	}
	void screenInfo() {
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

	bool screenCommand(vector<string> seperatedCommand, string command_to_check) {

		const set<string> commands = { "exit", "process-smi" };

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

		if (seperatedCommand[0] == "process-smi") {
			if (seperatedCommand.size() != 1) {
				write("INVALID COMMAND");
				return false;
			}
			write("");
			screenInfo();
			return false;
		}
		return false;
	}

	void setMemAddr(ll start, ll end) {
		memStart = start;
		memEnd = end;
	}
	ll getMemStart() {
		return memStart;
	}
	ll getMemEnd() {
		return memEnd;
	}
};
