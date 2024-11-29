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
	deque<shared_ptr<Screen>> readyQueue;
	deque<MemoryFrame> memoryFrames;
	map<int, int> coresUsed;
	atomic<int> qq = 0;
	vector<shared_ptr<Screen>> procInMem;
	map<shared_ptr<Screen>, vector<MemoryFrame>> memoryMap;
	map<shared_ptr<Screen>, pair<ll, ll>> flatMemoryMap;
	string allocation_type = "paging";
	deque<shared_ptr<Screen>> oldest;
	map<int, atomic<bool>> current_process_task;
	vector<bool> flatMemoryArray;

public:
	mutex queueMutex;
	mutex memoryMutex;
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
				minIns = stoull(value);
			}
			else if (key == "max-ins") {
				maxIns = stoull(value);
			}
			else if (key == "delays-per-exec") {
				delayPerExec = stoull(value);
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
		allocation_type = (maxOverallMem == memPerFrame) ? "flat" : "paging";
		if(allocation_type == "flat") {
			initFlatMemory();
		}else{
			initMemory();
		}	
		createBackingStore();
		start();
		initialized = true;
	}

	void createBackingStore() {
		ofstream file("backing_store.txt");
		file.close();
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

	void delay() {
		ll ctr = mainCtr;
		while (true) {
			if (((ctr + delayPerExec) % 1000000) >= (mainCtr % 1000000)) {
				break;
			}
		}
	}

	void initMemory() {
		// Initialize memory blocks where the first memory block starts from 0 and goes up to max overall mem
		lock_guard<mutex> lock(memoryMutex);
		int numFrames = maxOverallMem / memPerFrame;
		for (int i = 0; i < numFrames; i++) {
			MemoryFrame frame;
			frame.start = i * memPerFrame;
			frame.end = frame.start + memPerFrame - 1;
			frame.free = true;
			memoryFrames.push_back(frame);
		}
	}

	void initFlatMemory(){
		lock_guard<mutex> lock(memoryMutex);
		flatMemoryArray.clear();
		flatMemoryArray.resize(maxOverallMem, true);
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

	void pushQueue(shared_ptr<Screen> screen) {
		readyQueue.push_back(screen);
	}

	void removeFromBackingStore(const string& nameToRemove) {
		vector<string> lines;
		ifstream inputFile("backing_store.txt");

		if (!inputFile.is_open()) {
			cerr << "Could not open file for reading." << endl;
			return;
		}

		string line;
		while (getline(inputFile, line)) {
			if (line != nameToRemove) {
				lines.push_back(line);
			}
			else {
			}
		}

		inputFile.close();

		ofstream outputFile("backing_store.txt", ios::trunc);
		if (!outputFile.is_open()) {
			cerr << "Could not open file for writing." << endl;
			return;
		}

		for (const auto& updatedLine : lines) {
			outputFile << updatedLine << endl;
		}

		outputFile.close();
	}

	void putInBackingStore(shared_ptr<Screen> oldest) {
		ofstream file("backing_store.txt", ios::app);
		if (file.is_open()) {
			file << oldest->getProcessName() << endl;
		}
		file.close();
	}

	void runPaging(int id) {
		int prevCtr = -1;
		while (true) {
			if (prevCtr == mainCtr) {
				continue;
			}
			prevCtr = mainCtr;
			shared_ptr<Screen> screen;
			{
				lock_guard<mutex> lock(queueMutex);
				if (readyQueue.empty()) {
					runningScreens.erase(id);
					continue;
				}
				screen = readyQueue.front();
				readyQueue.pop_front();
				if (!screen->memoryAllocated) {
					if (scheduler == "rr") {
						allocateMemoryPagingWithInterupt(screen);
					} else if (!allocateMemoryPagingFCFS(screen)) {
						readyQueue.push_front(screen);
						continue;
					}
				}
				current_process_task[id] = true;
				runningScreens[id] = screen;
				screen->setCoreId(id);
			}

			if (scheduler == "rr") {
				for (int i = 0; i < quantumCycles; i++) {
					screen->execute();

					if (screen->isFinished()) {
						{
							lock_guard<mutex> lock(memoryMutex);
							freeMemoryPaging(screen);
							auto it = find(oldest.begin(), oldest.end(), screen);
							if (it != oldest.end()) {
								oldest.erase(it);
							}
						}
						delay();
						break;
					}

					delay();

					if (!current_process_task[id]) {
						break;
					}
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

				lock_guard<mutex> lock(memoryMutex);
				freeMemoryPaging(screen);
				auto it = find(oldest.begin(), oldest.end(), screen);
				if (it != oldest.end()) {
					oldest.erase(it);
				}
			}
		}
	}

	void freeMemoryPaging(shared_ptr<Screen> screen) {
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

	void allocateMemoryPagingWithInterupt(shared_ptr<Screen> screen) {
		lock_guard<mutex> lock(memoryMutex);
		ll mem_to_allocate = safeCeil(screen->memory, memPerFrame);
		removeFromBackingStore(screen->getProcessName());
		while (memoryFrames.size() < mem_to_allocate) {
			shared_ptr<Screen> oldestScreen = oldest.front();
			if (runningScreens[oldestScreen->getCoreId()] == oldestScreen) {
				runningScreens.erase(oldestScreen->getCoreId());
				current_process_task[oldestScreen->getCoreId()] = false;
			}
			putInBackingStore(oldestScreen);
			freeMemoryPaging(oldestScreen);
			oldestScreen->memoryAllocated = false;
			oldest.pop_front();
		}

		while (mem_to_allocate-- > 0) {
			MemoryFrame frame = memoryFrames.front();
			memoryFrames.pop_front();
			frame.free = false;
			memoryMap[screen].push_back(frame);
		}
		screen->memoryAllocated = true;
		oldest.push_back(screen);
	}

	bool allocateMemoryPagingFCFS(shared_ptr<Screen> screen) {
		lock_guard<mutex> lock(memoryMutex);
		ll mem_to_allocate = safeCeil(screen->memory, memPerFrame);

		if(memoryFrames.size() < mem_to_allocate) {
			return false;
		}
		
		while (mem_to_allocate-- > 0) {
			MemoryFrame frame = memoryFrames.front();
			memoryFrames.pop_front();
			frame.free = false;
			memoryMap[screen].push_back(frame);
		}
		screen->memoryAllocated = true;
		oldest.push_back(screen);
		return true;
	}

	void runFlat(int id) {
		int prevCtr = -1;

		while (true) {
			if (prevCtr == mainCtr) {
				continue;
			}
			prevCtr = mainCtr;
			shared_ptr<Screen> screen;
			{
				lock_guard<mutex> lock(queueMutex);
				if (readyQueue.empty()) {
					runningScreens.erase(id);
					continue;
				}
				screen = readyQueue.front();
				readyQueue.pop_front();
				if (!screen->memoryAllocated) {
					if (scheduler == "rr") {
						allocateMemoryFlatWithInterupt(screen);
					} else if (!allocateMemoryFlatFCFS(screen)) {
						readyQueue.push_front(screen);
						continue;
					}
				}
				current_process_task[id] = true;
				runningScreens[id] = screen;
				screen->setCoreId(id);
			}

			if (scheduler == "rr") {
				for (int i = 0; i < quantumCycles; i++) {
					if (!current_process_task[id]) {
						continue; 
					}

					screen->execute();

					if (screen->isFinished()) {
						{
							lock_guard<mutex> lock(memoryMutex);
							freeMemoryFlat(flatMemoryMap[screen].first, flatMemoryMap[screen].second);

							auto it = find(oldest.begin(), oldest.end(), screen);
							if (it != oldest.end()) {
								oldest.erase(it);
							}
						}
						delay();
						break;
					}
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

				lock_guard<mutex> lock(memoryMutex);
				freeMemoryFlat(flatMemoryMap[screen].first, flatMemoryMap[screen].second);

				auto it = find(oldest.begin(), oldest.end(), screen);
				if (it != oldest.end()) {
					oldest.erase(it);
				}

				flatMemoryMap.erase(screen);
			}
		}
	}


	void freeMemoryFlat(int start, int end) {
		for (int i = start; i <= end; i++) {
			flatMemoryArray[i] = true;
		}
	}

	void occupyMemoryFlat(int start, int end) {
		for (int i = start; i <= end; i++) {
			flatMemoryArray[i] = false;
		}
	}

	void allocateMemoryFlatWithInterupt(std::shared_ptr<Screen> screen) {
		lock_guard<mutex> lock(memoryMutex);
		ll mem_to_allocate = memPerFrame;
		removeFromBackingStore(screen->getProcessName());
		int ctr;
		int startIdx, endIdx;
		bool found;

		auto allocateMemoryBlock = [&]() {

			startIdx = -1, endIdx = -1, ctr = 0, found = false;

			for (int i = 0; i < flatMemoryArray.size(); i++) {
				if (flatMemoryArray[i]) {
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
			putInBackingStore(oldestScreen);
			freeMemoryFlat(flatMemoryMap[oldestScreen].first, flatMemoryMap[oldestScreen].second);
			flatMemoryMap.erase(oldestScreen);
			oldestScreen->memoryAllocated = false;
			oldest.pop_front();
			allocateMemoryBlock();
		}

		flatMemoryMap[screen] = { startIdx, endIdx };
		occupyMemoryFlat(startIdx, endIdx);
		oldest.push_back(screen);
	}

	bool allocateMemoryFlatFCFS(std::shared_ptr<Screen> screen) {
		lock_guard<mutex> lock(memoryMutex);
		ll mem_to_allocate = memPerFrame;

		int ctr;
		int startIdx, endIdx;
		bool found;

		auto allocateMemoryBlock = [&]() {

			startIdx = -1, endIdx = -1, ctr = 0, found = false;

			for (int i = 0; i < flatMemoryArray.size(); i++) {
				if (flatMemoryArray[i]) {
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

		if(!found) {
			return false;
		}

		flatMemoryMap[screen] = { startIdx, endIdx };
		occupyMemoryFlat(startIdx, endIdx);
		oldest.push_back(screen);
		return true;
	}

};
