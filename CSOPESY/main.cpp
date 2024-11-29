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
#include <chrono>
#include "screen.h"
#include "scheduler.h"
CONST ll divide = 1000000;
using namespace std;
typedef long long ll;

volatile ll mainCtr = 0;
mutex ctrMutex;

void runCPU() {
	while (true) {
		unique_lock<mutex> lock(ctrMutex);
		this_thread::sleep_for(chrono::milliseconds(100));
		mainCtr++;
		mainCtr = mainCtr % divide;
		lock.unlock();
	}
}

class MainConsole : public abstract_screen {
private:
	string currentView = "MainMenu";
	bool continue_program = true;
	map<string, shared_ptr<Screen>> screenList;
	Scheduler scheduler;
	atomic<bool> scheduleBool = false;
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
		const set<string> commands = { "initialize", "screen", "scheduler-test", "scheduler-stop", "report-util", "clear", "exit", "vmstat", "process-smi"};

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

		
		else if(seperatedCommand[0] == "vmstat") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			lock_guard<mutex> lock(scheduler.queueMutex);
			write(string(50, '-'));
			write("Virtual Memory Statistics (vmstat)");
			write(string(50, '-'));
			write("Total Memory: " + to_string(scheduler.getMaxMem()));
			write("Used Memory: " + to_string(scheduler.getUsedMem()));
			write("Free Memory: " + to_string(scheduler.getFreeMem()));
			write("");
			ll currMainCtr = mainCtr;
			ll idleTicks = scheduler.getIdleTicks();
			write("Idle CPU Ticks: " + to_string(idleTicks));
			write("Active CPU Ticks: " + to_string(currMainCtr -idleTicks));
			write("Total CPU Ticks: " + to_string(currMainCtr));
			write("");
			write("Num Paged in: " + to_string(scheduler.getPagesIn()));
			write("Num Paged out: " + to_string(scheduler.getPagesOut()));
		}
		else if (seperatedCommand[0] == "process-smi") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			lock_guard<mutex> lock(scheduler.queueMutex);
			write(string(50, '-'));
			write(" | PROCESS-SMI V01.00 Driver Version: 01.00 | ");
			write(string(50, '-'));
			write("CPU-Util: " + scheduler.getCpuUtilization());
			write("Memory Usage: " + to_string(scheduler.getUsedMem()) + "/" + to_string(scheduler.getMaxMem()) + " MB");
			std::stringstream ss;
			ss << std::fixed << std::setprecision(2)
				<< (static_cast<double>(scheduler.getUsedMem()) * 100.0 / scheduler.getMaxMem());
			write("Memory-Util: " + ss.str() + "%");
			write("");
			write(string(50, '='));
			write("Running processes and memory usage:");
			write(string(50, '-'));
			for (const auto& [id, screenPtr] : scheduler.runningScreens) {
				if (screenPtr != nullptr) {
					write(screenPtr->getProcessName() + " " + to_string(screenPtr->allocatedMemory));
				}
			}
			write(string(50, '-'));
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
				vector<Screen> processingScreens;
				vector<Screen> finishedScreens;
				{
					lock_guard<mutex> lock(scheduler.queueMutex);
					write("CPU utilization: " + scheduler.getCpuUtilization());
					write("Cores used: " + to_string(scheduler.getCoresUsed()));
					write("Cores available: " + to_string(scheduler.getCoresAvail()));

					for (const auto& [_, sc] : screenList) {
						if (sc->isFinished()) {
							finishedScreens.push_back(*sc); // Copy the Screen object
							cout << endl;
						}
					}
					for (const auto& [id, screenPtr] : scheduler.runningScreens) {
						if (screenPtr != nullptr) {
							processingScreens.push_back(*screenPtr); // Copy the Screen object
						}
					}
				}
				write("");
				write("--------------------------------------");
				write("Running processes:");

				for (auto sc : processingScreens) {
					write(sc.listProcess());
				}
				write("\nFinished processes:");
				for (auto sc : finishedScreens) {
					write(sc.listProcess());
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
					currentView = "MainMenu";
					return true;
				}
				if (screenList[seperatedCommand[2]]->isInitialized()) {
					screenList[seperatedCommand[2]]->redraw();
					processCommand("process-smi");
					screenList[seperatedCommand[2]]->add("root:\\> process-smi");
				}
				else {
					screenList[seperatedCommand[2]]->openScreen();
					screenList[seperatedCommand[2]]->initialize();
				}
			}
			else if (seperatedCommand[1] == "-s") {
				if (screenList.count(seperatedCommand[2])) {
					write("This Process is already in use.");
					return true;
				}
				auto sc = make_shared<Screen>(seperatedCommand[2], scheduler.getMinIns(), scheduler.getMaxIns(), scheduler.getMinMemPerProc(), scheduler.getMaxMemPerProc());
				screenList[seperatedCommand[2]] = sc;
				screenList[seperatedCommand[2]]->openScreen();
				screenList[seperatedCommand[2]]->initialize();
				currentView = screenList[seperatedCommand[2]]->getProcessName();
				lock_guard<mutex> lock(scheduler.queueMutex);
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
			if (scheduleBool) {
				write("Scheduler is already running.");
			}
			else {
				scheduleBool = true;
				thread testThread(&MainConsole::schedulerTest, this);
				testThread.detach();
				write("Scheduler Test started.");
			}
		}
		else if (seperatedCommand[0] == "scheduler-stop") {
			if (!(seperatedCommand.size() == 1)) {
				invalidCommand(command_to_check);
				return true;
			}
			scheduleBool = false;
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
			vector<Screen> processingScreens;
			vector<Screen> finishedScreens;
			{
				lock_guard<mutex> lock(scheduler.queueMutex);
				outFile << "CPU utilization: " << scheduler.getCpuUtilization() << endl;
				outFile << "Cores used: " << scheduler.getCoresUsed() << endl;
				outFile << "Cores available: " << scheduler.getCoresAvail() << endl;
				for (const auto& [_, sc] : screenList) {
					if (sc->isFinished()) {
						finishedScreens.push_back(*sc); // Copy the Screen object
					}
				}
				for (const auto& [id, screenPtr] : scheduler.runningScreens) {
					if (screenPtr != nullptr) {
						processingScreens.push_back(*screenPtr); // Copy the Screen object
					}
				}
			}

			outFile << endl;
			outFile << "--------------------------------------" << endl;
			outFile << "Running processes:" << endl;
			for (auto sc : processingScreens) {
				outFile << sc.listProcess() << endl;
			}
			outFile << endl
				<< "Finished processes:" << endl;
			for (auto sc : finishedScreens) {
				outFile << sc.listProcess() << endl;
			}
			outFile << "--------------------------------------";
			outFile.close();
			write("Report generated at " + outputFileName);
		}

		return true;
	}	

	void schedulerTest() {
		ll freq = scheduler.getBatchProcessFrequency();
		ll ctr = 1;
		int prevCtr = -1;
		while (scheduleBool) {
			if (ctr >= freq) {
				ctr = 1;
				string processName;
				while (true) {
					processName = "p" + to_string(schedulerCtr);
					if (!screenList.count(processName)) {
						break;
					}
					schedulerCtr++;
				}
				this_thread::sleep_for(chrono::milliseconds(100));
				auto sc = make_shared<Screen>(processName, scheduler.getMinIns(), scheduler.getMaxIns(),scheduler.getMinMemPerProc(), scheduler.getMaxMemPerProc());
				screenList[processName] = sc;
				lock_guard<mutex> lock(scheduler.queueMutex);
				scheduler.pushQueue(sc);
			}
			else {
				if (prevCtr != mainCtr) {
					ctr++;
					prevCtr = mainCtr;
				}
			}
		}
	}

	bool processCommand(const string& command) {
		string command_to_check = command;
		transform(command_to_check.begin(), command_to_check.end(), command_to_check.begin(), ::tolower);

		if (command_to_check.empty() || command_to_check == " " || command_to_check == "\n") {
			return true;
		}

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
	thread cpuThread(runCPU);
	cpuThread.detach();
	MainConsole console;
	console.run();
	return 0;
}