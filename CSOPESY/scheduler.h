#pragma once

using namespace std;
typedef long long ll;

class Scheduler {
private:
	int numCpu;
	string scheduler;
	int quantumCycles;
	ll batchProcessFrequency;
	int minIns;
	int maxIns;
	int delayPerExec;
	bool initialized = false;
	queue<shared_ptr<Screen>> readyQueue;
	mutex queueMutex;
	int coresUsed = 0;

public:
	void getConfig() {
		ifstream file("config.txt");
		string line;
		while (getline(file, line)) {
			istringstream iss(line);
			string key;
			string value;
			iss >> key >> value;
			if (key == "num-cpu") {
				numCpu = stoi(value);
			}
			else if (key == "scheduler") {
				scheduler = value.substr(1, value.size() - 2);
			}
			else if (key == "quantum-cycles") {
				quantumCycles = stoull(value);
			}
			else if (key == "batch-process-freq") {
				batchProcessFrequency = stoull(value);
			}
			else if (key == "min-ins") {
				minIns = stoull(value);
			}
			else if (key == "max-ins") {
				maxIns = stoull(value);
			}
			else if (key == "delay-per-exec") {
				delayPerExec = stoull(value);
			}
		}
		file.close();
		start();
		initialized = true;
	}
	int getCoresUsed() {
		return coresUsed;
	}

	ll getBatchProcessFrequency() {
		return batchProcessFrequency;
	}
	string getCpuUtilization() {
		float utilization = (static_cast<float>(coresUsed) / numCpu) * 100;

		ostringstream oss;
		oss << fixed << setprecision(0) << utilization;

		return oss.str() + "%";
	}
	int getCoresAvail() {
		return numCpu - coresUsed;
	}

	ll getMinIns() {
		return minIns;
	}

	ll getMaxIns() {
		return maxIns;
	}

	bool isInitialized() {
		return initialized;
	}

	void start() {
		for (int i = 0; i < numCpu; i++) {
			thread t(&Scheduler::run, this, i);
			t.detach();
		}
	}

	void run(int id) {
		while (true) {
			shared_ptr<Screen> screen;
			{
				lock_guard<mutex> lock(queueMutex);
				if (!readyQueue.empty()) {
					screen = readyQueue.front();
					readyQueue.pop();
				}
				else {
					continue;
				}
			}
			screen->setCoreId(id);
			coresUsed++;
			if (scheduler == "rr") {
				for (int i = 0; i < quantumCycles; i++) {
					if (screen->isFinished()) {
						break;
					}
					screen->execute();
					this_thread::sleep_for(chrono::milliseconds(delayPerExec));
				}
				if (!screen->isFinished()) {
					pushQueue(screen);
				}
			}
			else if (scheduler == "fcfs") {
				while (!screen->isFinished()) {
					screen->execute();
					this_thread::sleep_for(chrono::milliseconds(delayPerExec));
				}
			}
			coresUsed--;
		}
	}

	void pushQueue(shared_ptr<Screen> screen) {
		lock_guard<mutex> lock(queueMutex);
		readyQueue.push(screen);
	}
};
