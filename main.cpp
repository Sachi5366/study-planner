// study_planner.cpp
// Simple console Study Planner
// Compile: g++ -std=c++11 study_planner.cpp -o study_planner
// Run: ./study_planner

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

struct Task {
    int id;
    std::string title;
    std::string subject;
    int duration_minutes; // estimated time in minutes
    int priority; // 1 = highest, larger = lower priority
    std::string due_date; // simple YYYY-MM-DD string
    bool completed;

    std::string serialize() const {
        // pipe-separated; escape not implemented for simplicity
        std::ostringstream o;
        o << id << "|" << title << "|" << subject << "|" << duration_minutes << "|"
          << priority << "|" << due_date << "|" << (completed ? 1 : 0);
        return o.str();
    }

    static Task deserialize(const std::string &line) {
        Task t;
        std::istringstream in(line);
        std::string field;
        std::getline(in, field, '|'); t.id = std::stoi(field);
        std::getline(in, t.title, '|');
        std::getline(in, t.subject, '|');
        std::getline(in, field, '|'); t.duration_minutes = std::stoi(field);
        std::getline(in, field, '|'); t.priority = std::stoi(field);
        std::getline(in, t.due_date, '|');
        std::getline(in, field, '|'); t.completed = (field == "1");
        return t;
    }
};

std::vector<Task> tasks;
int nextId = 1;
const std::string DBFILE = "tasks.db";

void saveTasks() {
    std::ofstream out(DBFILE);
    if (!out) {
        std::cout << "Error saving tasks to file.\n";
        return;
    }
    for (const auto &t : tasks) out << t.serialize() << "\n";
    out.close();
}

void loadTasks() {
    tasks.clear();
    std::ifstream in(DBFILE);
    if (!in) return;
    std::string line;
    int maxId = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        Task t = Task::deserialize(line);
        tasks.push_back(t);
        if (t.id > maxId) maxId = t.id;
    }
    in.close();
    nextId = maxId + 1;
}

void printTask(const Task &t) {
    std::cout << "[" << (t.completed ? "X" : " ") << "] "
              << "ID:" << t.id << " | " << t.title
              << " | Subject: " << t.subject
              << " | " << t.duration_minutes << "m"
              << " | Pri:" << t.priority
              << " | Due: " << t.due_date << "\n";
}

void listTasks(bool showCompleted=true) {
    if (tasks.empty()) {
        std::cout << "No tasks yet.\n";
        return;
    }
    // sort by (completed, priority, due_date)
    std::vector<Task> tmp = tasks;
    std::sort(tmp.begin(), tmp.end(), [](const Task &a, const Task &b){
        if (a.completed != b.completed) return a.completed < b.completed; // incomplete first
        if (a.priority != b.priority) return a.priority < b.priority; // smaller = higher priority
        return a.due_date < b.due_date;
    });
    for (auto &t : tmp) {
        if (!showCompleted && t.completed) continue;
        printTask(t);
    }
}

Task* findTaskById(int id) {
    for (auto &t : tasks) if (t.id == id) return &t;
    return nullptr;
}

void addTask() {
    Task t;
    t.id = nextId++;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "Title: "; std::getline(std::cin, t.title);
    std::cout << "Subject: "; std::getline(std::cin, t.subject);
    std::cout << "Estimated duration (minutes): "; std::cin >> t.duration_minutes;
    std::cout << "Priority (1 = highest): "; std::cin >> t.priority;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::cout << "Due date (YYYY-MM-DD) or blank: "; std::getline(std::cin, t.due_date);
    t.completed = false;
    tasks.push_back(t);
    saveTasks();
    std::cout << "Added task with ID " << t.id << ".\n";
}

void editTask() {
    std::cout << "Enter task ID to edit: ";
    int id; std::cin >> id;
    Task* t = findTaskById(id);
    if (!t) { std::cout << "Task not found.\n"; return; }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    std::string s;
    std::cout << "Title (" << t->title << "): "; std::getline(std::cin, s); if (!s.empty()) t->title = s;
    std::cout << "Subject (" << t->subject << "): "; std::getline(std::cin, s); if (!s.empty()) t->subject = s;
    std::cout << "Estimated duration (minutes) (" << t->duration_minutes << "): "; std::getline(std::cin, s); if (!s.empty()) t->duration_minutes = std::stoi(s);
    std::cout << "Priority (" << t->priority << "): "; std::getline(std::cin, s); if (!s.empty()) t->priority = std::stoi(s);
    std::cout << "Due date (" << t->due_date << "): "; std::getline(std::cin, s); if (!s.empty()) t->due_date = s;
    saveTasks();
    std::cout << "Task updated.\n";
}

void removeTask() {
    std::cout << "Enter task ID to delete: ";
    int id; std::cin >> id;
    auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const Task &t){ return t.id == id; });
    if (it == tasks.end()) { std::cout << "Task not found.\n"; return; }
    tasks.erase(it, tasks.end());
    saveTasks();
    std::cout << "Task deleted.\n";
}

void toggleComplete() {
    std::cout << "Enter task ID to toggle complete: ";
    int id; std::cin >> id;
    Task* t = findTaskById(id);
    if (!t) { std::cout << "Task not found.\n"; return; }
    t->completed = !t->completed;
    saveTasks();
    std::cout << "Task " << (t->completed ? "marked complete.\n" : "marked incomplete.\n");
}

// Greedy daily planner: choose highest priority incomplete tasks that fit available minutes
void generateDailyPlan() {
    std::cout << "Enter available study time today (minutes): ";
    int available; std::cin >> available;
    // copy incomplete tasks
    std::vector<Task> pool;
    for (const auto &t : tasks) if (!t.completed) pool.push_back(t);

    if (pool.empty()) { std::cout << "No incomplete tasks.\n"; return; }

    // sort by priority, then earlier due date, then shorter duration
    std::sort(pool.begin(), pool.end(), [](const Task &a, const Task &b){
        if (a.priority != b.priority) return a.priority < b.priority;
        if (a.due_date != b.due_date) return a.due_date < b.due_date;
        return a.duration_minutes < b.duration_minutes;
    });

    std::vector<Task> plan;
    int timeLeft = available;
    for (const auto &t : pool) {
        if (t.duration_minutes <= timeLeft) {
            plan.push_back(t);
            timeLeft -= t.duration_minutes;
        }
    }

    std::cout << "\n--- Suggested Plan for Today ---\n";
    if (plan.empty()) {
        std::cout << "No single task fits into the available time. Consider breaking tasks into smaller chunks.\n";
    } else {
        int total=0;
        for (const auto &p : plan) { printTask(p); total += p.duration_minutes; }
        std::cout << "Total scheduled: " << total << "m. Free time left: " << timeLeft << "m.\n";
    }
    std::cout << "--------------------------------\n";
}

void importSampleData() {
    tasks.clear();
    tasks.push_back({nextId++, "Read OS: Paging", "Operating Systems", 60, 1, "2025-11-20", false});
    tasks.push_back({nextId++, "Practice DB SQL queries", "Database Systems", 90, 2, "2025-11-25", false});
    tasks.push_back({nextId++, "Revise Networking notes", "Networking", 45, 1, "2025-11-19", false});
    tasks.push_back({nextId++, "Implement C++ assignment", "Programming", 120, 3, "2025-11-30", false});
    saveTasks();
    std::cout << "Sample data imported.\n";
}

void showMenu() {
    std::cout << "\nStudy Planner Menu\n"
              << "1. List tasks (all)\n"
              << "2. List incomplete tasks only\n"
              << "3. Add task\n"
              << "4. Edit task\n"
              << "5. Delete task\n"
              << "6. Toggle complete/incomplete\n"
              << "7. Generate daily plan\n"
              << "8. Import sample data\n"
              << "9. Save tasks\n"
              << "0. Exit\n"
              << "Choose: ";
}

int main() {
    loadTasks();
    bool running = true;
    while (running) {
        showMenu();
        int choice; if (!(std::cin >> choice)) { std::cin.clear(); std::cin.ignore(10000,'\n'); continue; }
        switch (choice) {
            case 1: listTasks(true); break;
            case 2: listTasks(false); break;
            case 3: addTask(); break;
            case 4: editTask(); break;
            case 5: removeTask(); break;
            case 6: toggleComplete(); break;
            case 7: generateDailyPlan(); break;
            case 8: importSampleData(); break;
            case 9: saveTasks(); std::cout << "Saved.\n"; break;
            case 0: running = false; saveTasks(); std::cout << "Goodbye!\n"; break;
            default: std::cout << "Unknown choice.\n";
        }
    }
    return 0;
}
