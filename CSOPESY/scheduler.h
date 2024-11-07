#pragma once

using namespace std;
typedef long long ll;

struct MemoryFrame {
    int start;
	int end;
    bool free;
};

class Scheduler {
private:
	int numCpu;
	string scheduler;
	int quantumCycles;
	ll batchProcessFrequency;
	int minIns;
	int maxIns;
	int delayPerExec;
	ll max_overall_mem;
	ll mem_per_frame;
	ll mem_per_proc;
	bool initialized = false;
	queue<shared_ptr<Screen>> readyQueue;
	vector<MemoryFrame> memoryFrames;
	map<int, int> coresUsed;


public:
	mutex queueMutex;
	map<int, shared_ptr<Screen>> runningScreens;
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
			} else if (key == "scheduler") {
				scheduler = value.substr(1, value.size() - 2);
			} else if (key == "quantum-cycles") {
				quantumCycles = stoull(value);
			} else if (key == "batch-process-freq") {
				batchProcessFrequency = stoull(value);
			} else if (key == "min-ins") {
				minIns = stoi(value);
			} else if (key == "max-ins") {
				maxIns = stoi(value);
			} else if (key == "delay-per-exec") {
				delayPerExec = stoi(value);
			} else if (key == "max-overall-mem") {
				max_overall_mem = stoull(value);
			} else if (key == "mem-per-frame") {
				mem_per_frame = stoull(value);
			} else if (key == "mem-per-proc") {
				mem_per_proc = stoull(value);
			}
		}
		file.close();
		initMemory();
		start();
		initialized = true;
	}
	int getCoresUsed() {
		int totalUsed = 0;

		for (const auto& [coreId, count] : coresUsed) {
			totalUsed += count; 
		}

		return totalUsed; 
	}

	ll getBatchProcessFrequency() {
		return batchProcessFrequency;
	}
	string getCpuUtilization() {
		float utilization = (static_cast<float>(getCoresUsed()) / numCpu) * 100;

		ostringstream oss;
		oss << fixed << setprecision(0) << utilization;

		return oss.str() + "%";
	}
	int getCoresAvail() {
		return numCpu - getCoresUsed();
	}

	ll getMinIns() {
		return minIns;
	}

	ll getMaxIns() {
		return maxIns;
	}

	ll getDelayPerExec() {
		return delayPerExec;
	}

	ll getMaxMem(){
		return max_overall_mem;
	};

	ll getExternalFragmentation(){
		int ctr = 0;
		lock_guard<mutex> lock(queueMutex);
		for (auto& block : memoryFrames) {
			if (block.free) {
				ctr++;
			}
		}
		return ctr * mem_per_frame;
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

			bool memoryAllocated = false;
			{
				lock_guard<mutex> lock(queueMutex);
				// if readyQ is not empty and there is enough memory
				// compare number of frames required and number of frames available
				if (!readyQueue.empty()) {
					vector<ll> mem = allocateMemory();
					if (mem[0] != -1 && mem[1] != -1) {
						screen = readyQueue.front();
						readyQueue.pop();
						coresUsed[id] = 1; 
						runningScreens[id] = screen;
						screen->setMemAddr(mem[0], mem[1]);
					} else { 
						coresUsed[id] = 0; 
						runningScreens[id] = nullptr; 
						continue; 
					}
				} else { 
					coresUsed[id] = 0; 
					runningScreens[id] = nullptr; 
					continue; 
				}
			}
			screen->setCoreId(id);
			if (scheduler == "rr") {
				for (int i = 0; i < quantumCycles; i++) {
					if (screen->isFinished()) {
						delay();
						break;
					}
					screen->execute();
					delay();
				}
				if (!screen->isFinished()) {
					pushQueue(screen);
				}
			}
			else if (scheduler == "fcfs") {
				while (!screen->isFinished()) {
					screen->execute();
					delay();
				}
			}
			freeMemory(screen->getMemStart(), screen->getMemEnd());
			screen->setMemAddr(-1, -1);
		}
	}

	void delay() {
		ll ctr = 0;
		while (true) {
			ctr++;
			if (ctr >= delayPerExec) {
				break;
			}
		}
	}

	void pushQueue(shared_ptr<Screen> screen) {
		lock_guard<mutex> lock(queueMutex);
		readyQueue.push(screen);
	}

	void initMemory(){
		// Initialize memory blocks where the first memory block starts from 0 and goes up to max overall mem
		lock_guard<mutex> lock(queueMutex);
		int numFrames = max_overall_mem / mem_per_frame;
		for (int i = 0; i < numFrames; i++) {
			MemoryFrame frame;
			frame.start = i * mem_per_frame;
			frame.end = frame.start + mem_per_frame - 1;
			frame.free = true;
			memoryFrames.push_back(frame);
		}
	}

	void freeMemory(int start, int end) {
		lock_guard<mutex> lock(queueMutex);
		for (auto& block : memoryFrames) {
			// If block overlaps with the range, free the block
			if (block.start >= start && block.end <= end) {
				block.free = true;
			}
		}
	}


	vector<ll> allocateMemory() {
		// check if there is enough continuous memory to allocate a process
		int numFrames = mem_per_proc / mem_per_frame;
		int ctr = 0;
		ll start;
		int startIdx = -1;
		for (int i = 0; i < memoryFrames.size(); i++) {
			// if block is free, add to number of frames free
			if (memoryFrames[i].free) {
				if (ctr == 0) {
					start = memoryFrames[i].start;
					startIdx = i;
				}
				ctr++;
				if (ctr >= numFrames) {
					int lastIdx = i + 1;
					for (int j = startIdx; j < lastIdx; j++) {
						memoryFrames[j].free = false;
					}
					return {start, memoryFrames[i].end};
				}
			// else reset the counter since there isn't enough space at this location
			} else {
				ctr = 0;
				startIdx = -1;
			}
		}
		return {-1, -1};
	}

	void memoryAllocatorHelper() {
		
	}
};
