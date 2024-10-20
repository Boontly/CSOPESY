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
#include <atomic>
#include <thread>
#include <fstream>
#include <queue>
#include <memory>
#include <fstream>
#include <filesystem>

#include "screen.h"
#include "scheduler.h"

using namespace std;
typedef long long ll;

class MainConsole : public abstract_screen {
private:
	string currentView = "MainMenu";
	bool continue_program = true;
	map<string, shared_ptr<Screen>> screenList;
	Scheduler scheduler;
	atomic<bool> scheduleBool = true;
	bool schedulerRunning = false;
	int schedulerCtr = 0;

	void commandNotRecognize(string command_to_check) {
		write("Unknown command: " + command_to_check);
	}

	void invalidCommand(string command_to_check) {
		write("Invalid Arguments for " + command_to_check);
	}

	void screenNotFound(string name) {
		write("Process " + name + " not found.");
	}

	bool mainMenuCommand(vector<string> seperatedCommand, string command_to_check) {
		const set<string> commands = { "initialize", "screen", "scheduler-test", "scheduler-stop", "report-util", "clear", "exit" };

		if (!commands.count(seperatedCommand[0])) {
			commandNotRecognize(command_to_check);
			return true;
		}

		if (seperatedCommand[0] == "exit") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			return false;
		}

		if (seperatedCommand[0] != "initialize" && !scheduler.isInitialized()) {
			write("Please initialize the configuration first.");
			return true;
		}

		if (seperatedCommand[0] == "initialize") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			scheduler.getConfig();
			write("Configuration initialized.");
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
				write("CPU utilization: " + scheduler.getCpuUtilization());
				write("Cores used: " + to_string(scheduler.getCoresUsed()));
				write("Cores available: " + to_string(scheduler.getCoresAvail()));
				write("");
				write("--------------------------------------");
				write("Running processes:");
				vector<shared_ptr<Screen>> processingScreens;
				vector<shared_ptr<Screen>> finishedScreens;
				for (auto [_, sc] : screenList) {
					if (sc->isFinished()) {
						finishedScreens.push_back(sc);
					}
					else {
						processingScreens.push_back(sc);
					}
				}
				for (auto sc : processingScreens) {
					write(sc->listProcess());
				}
				write("\nFinished processes:");
				for (auto sc : finishedScreens) {
					write(sc->listProcess());
				}
				write("--------------------------------------");
			}

			else if (seperatedCommand[1] == "-r") {
				if (!screenList.count(seperatedCommand[2])) {
					screenNotFound(seperatedCommand[2]);
					return true;
				}
				currentView = screenList[seperatedCommand[2]]->getProcessName();
				if (screenList[seperatedCommand[2]]->isFinished()) {
					write("This Process is already finished.");
					return true;
				}
				if (screenList[seperatedCommand[2]]->isInitialized()) {
					screenList[seperatedCommand[2]]->redraw();
					processCommand("process-smi");
					screenList[seperatedCommand[2]]->add("root:\\> process-smi");
				}
				else {
					screenList[seperatedCommand[2]]->openScreen();
				}
			}
			else if (seperatedCommand[1] == "-s") {
				if (screenList.count(seperatedCommand[2])) {
					write("This Process is already in use.");
					return true;
				}
				auto sc = make_shared<Screen>(seperatedCommand[2], scheduler.getMinIns(), scheduler.getMaxIns());
				screenList[seperatedCommand[2]] = sc;
				screenList[seperatedCommand[2]]->openScreen();
				screenList[seperatedCommand[2]]->initialize();
				currentView = screenList[seperatedCommand[2]]->getProcessName();
				scheduler.pushQueue(sc);
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
		else if (seperatedCommand[0] == "scheduler-test") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			if (schedulerRunning) {
				write("Scheduler is already running.");
			}
			else {
				thread testThread(&MainConsole::schedulerTest, this);
				testThread.detach();
				schedulerRunning = true;
				write("Scheduler Test started.");
			}
		}
		else if (seperatedCommand[0] == "scheduler-stop") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			scheduleBool = false;
			schedulerRunning = false;
			write("Scheduler Test stopped.");
		}
		else if (seperatedCommand[0] == "report-util") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			filesystem::path currentPath = filesystem::current_path();
			string outputFileName = (currentPath / "csopesy-log.txt").string();
			ofstream outFile(outputFileName);

			outFile << "CPU utilization: " << scheduler.getCpuUtilization() << endl;
			outFile << "Cores used: " << scheduler.getCoresUsed() << endl;
			outFile << "Cores available: " << scheduler.getCoresAvail() << endl;
			outFile << endl;
			outFile << "--------------------------------------" << endl;
			outFile << "Running processes:" << endl;
			vector<shared_ptr<Screen>> processingScreens;
			vector<shared_ptr<Screen>> finishedScreens;
			for (auto [_, sc] : screenList) {
				if (sc->isFinished()) {
					finishedScreens.push_back(sc);
				}
				else {
					processingScreens.push_back(sc);
				}
			}
			for (auto sc : processingScreens) {
				outFile << sc->listProcess() << endl;
			}
			outFile << endl
				<< "Finished processes:" << endl;
			for (auto sc : finishedScreens) {
				outFile << sc->listProcess() << endl;
			}
			outFile << "--------------------------------------";
			outFile.close();
			write("Report generated at " + outputFileName);
		}

		return true;
	}

	void schedulerTest() {
		ll freq = scheduler.getBatchProcessFrequency();
		while (scheduleBool) {
			for (ll i = 0; i < freq; i++) {
				continue;
			}
			string processName;
			while (true) {
				processName = "p" + to_string(schedulerCtr);
				if (!screenList.count(processName)) {
					break;
				}
				schedulerCtr++;
			}
			auto sc = make_shared<Screen>(processName, scheduler.getMinIns(), scheduler.getMaxIns());
			screenList[processName] = sc;
			scheduler.pushQueue(sc);
		}
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
			if (screenList[currentView]->screenCommand(seperatedCommand, command_to_check)) {
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
			cout << "root:\\> ";
			getline(cin, user_input);
			if (currentView != "MainMenu") {
				screenList[currentView]->add("root:\\> " + user_input);
			}
			else {
				add("root:\\> " + user_input);
			}
			continue_program = processCommand(user_input);
		}
	}
};

signed main() {
	MainConsole console;
	console.run();
	return 0;
}