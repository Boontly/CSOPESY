#pragma once

using namespace std;
typedef long long ll;

extern volatile ll mainCtr;

ll safeCeil(int numerator, int denominator) {
	return (numerator + (denominator - 1)) / denominator;
}

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
	ll maxOverallMem;
	ll memPerFrame;
	ll minMemPerProc;
	ll maxMemPerProc;
	bool initialized = false;
	queue<shared_ptr<Screen>> readyQueue;
	deque<MemoryFrame> memoryFrames;
	map<int, int> coresUsed;
	atomic<int> qq = 0;
	vector<shared_ptr<Screen>> procInMem;
	map<shared_ptr<Screen>, vector<MemoryFrame>> memoryMap;
	map<shared_ptr<Screen>, pair<ll, ll>> flatMemoryMap;
	string allocation_type = "paging";
	deque<shared_ptr<Screen>> oldest;
	map<int, atomic<bool>> current_process_task;


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
				minIns = stoi(value);
			}
			else if (key == "max-ins") {
				maxIns = stoi(value);
			}
			else if (key == "delay-per-exec") {
				delayPerExec = stoi(value);
			}
			else if (key == "max-overall-mem") {
				maxOverallMem = stoull(value);
			}
			else if (key == "mem-per-frame") {
				memPerFrame = stoull(value);
			}
			else if (key == "min-mem-per-proc") {
				minMemPerProc = stoull(value);
			}
			else if (key == "max-mem-per-proc") {
				maxMemPerProc = stoull(value);
			}
		}
		file.close();
		initMemory();
		start();
		initialized = true;
		allocation_type = (maxOverallMem == memPerFrame) ? "flat" : "paging";
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

	ll getMinMemPerProc() {
		return minMemPerProc;
	}
	ll getMaxMemPerProc() {
		return maxMemPerProc;
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

	ll getMaxMem() {
		return maxOverallMem;
	};
	ll getExternalFragmentation() {
		int ctr = 0;
		for (auto& block : memoryFrames) {
			if (block.free) {
				ctr++;
			}
		}
		return ctr * memPerFrame;
	}


	bool isInitialized() {
		return initialized;
	}

	void start() {
		for (int i = 0; i < numCpu; i++) {
			if(allocation_type == "flat") {
				thread t(&Scheduler::runFlat, this, i);
				t.detach();
			}
			else {
				thread t(&Scheduler::runPaging, this, i);
				t.detach();
			}
		}
	}
	
	void runPaging(int id) {
		while (true) {
			shared_ptr<Screen> screen;
			{
				lock_guard<mutex> lock(queueMutex);
				runningScreens.erase(id);
				screen = readyQueue.front();
				if (screen == nullptr) {
					continue;
				}
				readyQueue.pop();
				allocateMemoryPaging(screen);
			}
			current_process_task[id] = true;
			runningScreens[id] = screen;
			screen->setCoreId(id);
			if (scheduler == "rr") {
				for (int i = 0; i < quantumCycles; i++) {
					if (!current_process_task[id]) {
						continue;
					}
					if (screen->isFinished()) {
						freeFlatMemoryPaging(screen);
						delay();
						break;
					}
					screen->execute();
					delay();
				}
				if (!screen->isFinished()) {

					lock_guard<mutex> lock(queueMutex);
					pushQueue(screen);
				}
			}
			else if (scheduler == "fcfs") {
				while (!screen->isFinished()) {
					screen->execute();
					delay();
				}
				freeFlatMemoryPaging(screen);
			}

		}
	}


	void runFlat(int id) {
		while (true) {
			shared_ptr<Screen> screen;
			{
				lock_guard<mutex> lock(queueMutex);
				runningScreens.erase(id);
				screen = readyQueue.front();
				if (screen == nullptr) {
					continue;
				}
				readyQueue.pop();
				allocateFlatMemory(screen);
			}
			current_process_task[id] = true;
			runningScreens[id] = screen;
			screen->setCoreId(id);
			if (scheduler == "rr") {
				for (int i = 0; i < quantumCycles; i++) {
					if (!current_process_task[id]) {
						continue;
					}
					if (screen->isFinished()) {
						freeFlatMemory(flatMemoryMap[screen].first, flatMemoryMap[screen].second);
						delay();
						break;
					}
					screen->execute();
					delay();
				}
				if (!screen->isFinished()) {

					lock_guard<mutex> lock(queueMutex);
					pushQueue(screen);
				}
			}
			else if (scheduler == "fcfs") {
				while (!screen->isFinished()) {
					screen->execute();
					delay();
				}
				freeFlatMemory(flatMemoryMap[screen].first, flatMemoryMap[screen].second);
			}

		}
	}

	/*
	void run(int id) {
		int prev_counter = -1;
		while (true) {
			shared_ptr<Screen> screen;
			if (prev_counter == mainCtr) {
				continue;
			}
			prev_counter = mainCtr;
			bool memoryAllocated = false;
			{
				lock_guard<mutex> lock(queueMutex);
				// if readyQ is not empty and there is enough memory
				// compare number of frames required and number of frames available
				if (runningScreens[id] != nullptr && runningScreens[id]->isFinished()) {
					freeFlatMemory(runningScreens[id]->getMemStart(), runningScreens[id]->getMemEnd());
					for (int i = 0; i < procInMem.size(); i++) {
						if (procInMem[i] == runningScreens[id]) {
							procInMem.erase(procInMem.begin() + i);
							break;
						}
					}
					runningScreens[id]->setMemAddr(-1, -1);
					coresUsed[id] = 0;
					runningScreens[id] = nullptr;
				}
				if (!readyQueue.empty()) {
					if (readyQueue.front()->getMemStart() != -1) {
						screen = readyQueue.front();
						readyQueue.pop();
						runningScreens[id] = screen;
						coresUsed[id] = 1;
					}
					else {
						vector<ll> mem = allocateMemory();
						if (mem[0] != -1 && mem[1] != -1) {
							screen = readyQueue.front();
							procInMem.push_back(screen);
							readyQueue.pop();
							coresUsed[id] = 1;
							runningScreens[id] = screen;
							screen->setMemAddr(mem[0], mem[1]);
						}
						else {
							pushQueue(readyQueue.front());
							readyQueue.pop();
							continue;
						}
					}
				}
				else {
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
					lock_guard<mutex> lock(queueMutex);
					pushQueue(screen);
				}
			}
			else if (scheduler == "fcfs") {
				while (!screen->isFinished()) {
					screen->execute();
					delay();
				}
			}

		}
	}
	*/

	void delay() {
		ll ctr = mainCtr;
		while (true) {
			if (((ctr + delayPerExec) % 1000000) >= (mainCtr % 1000000)) {
				break;
			}
		}
	}

	void pushQueue(shared_ptr<Screen> screen) {
		readyQueue.push(screen);
	}

	// paging
	void freeFlatMemoryPaging(shared_ptr<Screen> screen) {
		map<shared_ptr<Screen>, vector<MemoryFrame>>::iterator it = memoryMap.find(screen);
		if (it == memoryMap.end()) {
			return;
		}
		vector<MemoryFrame> frames = memoryMap[screen];
		for (auto& frame : frames) {
			memoryFrames.push_back(frame);
		}
		memoryMap.erase(it);
	}

	void removeNameFromFile(const string& nameToRemove) {
		vector<string> lines; 
		ifstream inputFile("oldest.txt");
		
		if (!inputFile.is_open()) {
			cerr << "Could not open file for reading." << endl;
			return;
		}

		string line;
		while (getline(inputFile, line)) {
			if (line != nameToRemove) {
				lines.push_back(line); 
			}
		}

		inputFile.close();

		ofstream outputFile("oldest.txt", ios::trunc);
		if (!outputFile.is_open()) {
			cerr << "Could not open file for writing." << endl;
			return;
		}

		for (const auto& updatedLine : lines) {
			outputFile << updatedLine << endl;
		}

		outputFile.close();
	}


	void putOldestInFile(shared_ptr<Screen> oldest) {
		ofstream file("oldest.txt", ios::app);
		if (file.is_open()) {
			file << oldest->getProcessName() << endl; 
		}
		file.close();
	}

	void allocateMemoryPagingWithInterupt(shared_ptr<Screen> screen) {
		ll mem_to_allocate = safeCeil(screen->memory, memPerFrame);
		// check if the object is in oldest.txt
		removeNameFromFile(screen->getProcessName());
		while (memoryFrames.size() < mem_to_allocate) {
			shared_ptr<Screen> oldestScreen = oldest.front();
			if (runningScreens[oldestScreen->getCoreId()] == oldestScreen) {
				runningScreens.erase(oldestScreen->getCoreId());
				current_process_task[oldestScreen->getCoreId()] = false;
			}
			putOldestInFile(oldestScreen);
			freeFlatMemoryPaging(oldestScreen);
			oldest.pop_front();
			readyQueue.push(oldestScreen);
		}

		while (mem_to_allocate-- > 0) {
			MemoryFrame frame = memoryFrames.front();
			memoryFrames.pop_front();
			frame.free = false;
			memoryMap[screen].push_back(frame);
		}
		oldest.push_back(screen);
	}

	bool allocateMemoryPaging(shared_ptr<Screen> screen) {
		ll mem_to_allocate = safeCeil(screen->memory, memPerFrame);
		ll freed = 0;
		bool can_delete = false;

		vector<shared_ptr<Screen>> screensToDelete;

		if (mem_to_allocate > memoryFrames.size()) {
			for (auto it = oldest.begin(); it != oldest.end(); ++it) {
				shared_ptr<Screen> oldestScreen = *it;
				if (!runningScreens.count(oldestScreen->getCoreId())) {
					screensToDelete.push_back(oldestScreen);
					freed += safeCeil(oldestScreen->memory, memPerFrame);
					if (freed + memoryFrames.size() >= mem_to_allocate) {
						can_delete = true;
						break;
					}
				}
			}
			if (can_delete) {
				for (auto x : screensToDelete) {
					freeFlatMemoryPaging(x);
					auto it = find(oldest.begin(), oldest.end(), x);
					if (it != oldest.end()) {
						oldest.erase(it);
					}
					readyQueue.push(x);
				}
			}
			else {
				readyQueue.push(screen);
				return false;
			}
		}

		while (mem_to_allocate-- > 0) {
			MemoryFrame frame = memoryFrames.front();
			memoryFrames.pop_front();
			frame.free = false;
			memoryMap[screen].push_back(frame);
		}

		oldest.push_back(screen);
		return true;
	}

	// pagign end here

	void initMemory() {
		// Initialize memory blocks where the first memory block starts from 0 and goes up to max overall mem
		lock_guard<mutex> lock(queueMutex);
		int numFrames = maxOverallMem / memPerFrame;
		for (int i = 0; i < numFrames; i++) {
			MemoryFrame frame;
			frame.start = i * memPerFrame;
			frame.end = frame.start + memPerFrame - 1;
			frame.free = true;
			memoryFrames.push_back(frame);
		}
	}

	void freeFlatMemory(int start, int end) {
		for (int i = start; i <= end; i++) {
			memoryFrames[i].free = true;
		}
	}

	void allocateFlatMemoryWithInterupt(std::shared_ptr<Screen> screen) {
		removeNameFromFile(screen->getProcessName());
		ll mem_to_allocate = safeCeil(screen->memory, memPerFrame);
		int ctr;
		int startIdx, endIdx;
		bool found;

		auto allocateMemoryBlock = [&]() {

			startIdx = -1, endIdx = -1, ctr = 0, found = false;

			for (int i = 0; i < memoryFrames.size(); i++) {
				if (memoryFrames[i].free) {
					if (ctr == 0) {
						startIdx = i;
					}
					ctr++;

					if (ctr == mem_to_allocate) {
						endIdx = i;
						found = true;
						break;
					}
				}
				else {
					ctr = 0;
					startIdx = -1;
				}
			}
			};

		allocateMemoryBlock();

		while (!found) {
			shared_ptr<Screen> oldestScreen = oldest.front();
			if (runningScreens[oldestScreen->getCoreId()] == oldestScreen) {
				runningScreens.erase(oldestScreen->getCoreId());
				current_process_task[oldestScreen->getCoreId()] = false;
			}
			putOldestInFile(oldestScreen);
			freeFlatMemoryPaging(oldestScreen);
			oldest.pop_front();
			readyQueue.push(oldestScreen);
		}
		flatMemoryMap[screen] = { startIdx, endIdx };
	}

	void allocateFlatMemory(std::shared_ptr<Screen> screen) {
		ll mem_to_allocate = safeCeil(screen->memory, memPerFrame);
		int ctr;
		int startIdx, endIdx;
		bool found;

		auto allocateMemoryBlock = [&]() {

			startIdx = -1, endIdx = -1, ctr = 0, found = false;

			for (int i = 0; i < memoryFrames.size(); i++) {
				if (memoryFrames[i].free) {
					if (ctr == 0) {
						startIdx = i;
					}
					ctr++;

					if (ctr == mem_to_allocate) {
						endIdx = i;
						found = true;
						break;
					}
				}
				else {
					ctr = 0;
					startIdx = -1;
				}
			}
			};

		allocateMemoryBlock();

		while (!found) {
			shared_ptr<Screen> screensToDelete;
			for (auto it = oldest.begin(); it != oldest.end(); ++it) {
				shared_ptr<Screen> oldestScreen = *it;
				if (runningScreens[oldestScreen->getCoreId()] != oldestScreen) {
					screensToDelete = oldestScreen;
					break;
				}
			}
			freeFlatMemory(flatMemoryMap[screen].first, flatMemoryMap[screen].second);
			auto it = find(oldest.begin(), oldest.end(), screensToDelete);
			if (it != oldest.end()) {
				oldest.erase(it);
			}
			readyQueue.push(screensToDelete);
			allocateMemoryBlock();
		}
		flatMemoryMap[screen] = { startIdx, endIdx };
	}

};
