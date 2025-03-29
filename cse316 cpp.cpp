#include <iostream>
#include <queue>
#include <vector>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <algorithm>  // ✅ Required for sort()

using namespace std;
using namespace chrono;

// Mutex and condition variable for safe task management
mutex mtx;
condition_variable cv;

// Task structure to store task properties
struct Task {
    int id;
    int period;             // Task period in milliseconds
    int executionTime;      // Execution time in milliseconds
    int deadline;           // Absolute deadline in milliseconds
    int remainingTime;      // Remaining execution time

    // Priority based on the deadline (earlier deadline = higher priority)
    bool operator>(const Task& other) const {
        return deadline > other.deadline;
    }
};

// Priority queue for task management (min-heap based on deadline)
priority_queue<Task, vector<Task>, greater<Task>> taskQueue;

// Global start time to simulate real-time behavior
steady_clock::time_point startTime;

// Task execution simulation
void executeTask(Task task) {
    cout << "[INFO] Executing Task " << task.id << " for " << task.executionTime << " ms" << endl;
    this_thread::sleep_for(milliseconds(task.executionTime));
    cout << "[SUCCESS] Task " << task.id << " completed." << endl;
}

// Rate Monotonic (RM) priority assignment
void assignRM(vector<Task>& tasks) {
    sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.period < b.period; // Lower period = higher priority
    });
}

// Earliest Deadline First (EDF) priority assignment
void assignEDF(vector<Task>& tasks) {
    sort(tasks.begin(), tasks.end(), [](const Task& a, const Task& b) {
        return a.deadline < b.deadline; // Earlier deadline = higher priority
    });
}

// Task scheduler to insert tasks into the queue
void taskScheduler(vector<Task>& tasks, bool useEDF) {
    if (useEDF) {
        assignEDF(tasks);
    } else {
        assignRM(tasks);
    }

    for (Task& task : tasks) {
        task.remainingTime = task.executionTime;
        task.deadline = duration_cast<milliseconds>(steady_clock::now() - startTime).count() + task.period;

        // Lock the queue to insert tasks safely
        {
            lock_guard<mutex> lock(mtx);
            taskQueue.push(task);
        }
        cv.notify_one(); // Notify task executor of new task arrival
    }
}

// Task executor to handle task execution
void taskExecutor() {
    while (true) {
        unique_lock<mutex> lock(mtx);
        cv.wait(lock, [] { return !taskQueue.empty(); });

        Task task = taskQueue.top();
        taskQueue.pop();
        lock.unlock(); // Unlock before task execution

        int currentTime = duration_cast<milliseconds>(steady_clock::now() - startTime).count();
        if (currentTime > task.deadline) {
            cout << "[WARNING] Task " << task.id << " missed its deadline!" << endl;
            continue;
        }

        executeTask(task);
    }
}

// Dynamic priority recalibration based on system load
void adaptivePriorityRecalibration(vector<Task>& tasks) {
    int systemLoad = rand() % 100; // Simulate fluctuating system load (0 to 100%)
    if (systemLoad > 70) {
        cout << "[ALERT] High system load detected. Switching to EDF scheduling." << endl;
        assignEDF(tasks);
    } else {
        cout << "[INFO] Low/Moderate system load. Using RM scheduling." << endl;
        assignRM(tasks);
    }
}

// Periodic task monitoring for system load adjustment
void taskMonitor(vector<Task>& tasks) {
    while (true) {
        this_thread::sleep_for(seconds(5)); // Monitor system load every 5 seconds
        adaptivePriorityRecalibration(tasks);
        taskScheduler(tasks, true); // Re-run task scheduling after recalibration
    }
}

int main() {
    // Initialize system start time
    startTime = steady_clock::now();

    // Define task list with ID, period, and execution time
    vector<Task> tasks = {
        {1, 1000, 300, 0, 0},  // Task 1: Period 1000 ms, Exec Time 300 ms
        {2, 1500, 500, 0, 0},  // Task 2: Period 1500 ms, Exec Time 500 ms
        {3, 2000, 700, 0, 0}   // Task 3: Period 2000 ms, Exec Time 700 ms
    };

    // Start task executor in a separate thread
    thread executorThread(taskExecutor);

    // Start task monitor to dynamically adjust task priorities
    thread monitorThread(taskMonitor, ref(tasks));

    // Initial task scheduling using Rate Monotonic (RM)
    taskScheduler(tasks, false);

    // Wait for the threads to finish (never exits in a real-time scenario)
    executorThread.join();
    monitorThread.join();

    return 0;
}
