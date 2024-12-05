#include "mcc_scheduler.h"
#include <numeric>
using namespace std;

 MCCScheduler:: MCCScheduler(vector<Task>& tasks, int num_cores)
    : tasks(tasks)
    , num_cores(num_cores) 
{
}

vector<vector<int>>  MCCScheduler::selectExecutionUnits() {
    InitialTaskScheduler scheduler(tasks, num_cores);
    vector<int> priority_ordered_tasks = scheduler.getPriorityOrderedTasks();
    auto [entry_tasks, non_entry_tasks] = scheduler.classifyEntryTasks(priority_ordered_tasks);
    scheduler.scheduleEntryTasks(entry_tasks);
    scheduler.scheduleNonEntryTasks(non_entry_tasks);
    return scheduler.sequences;
}

int  MCCScheduler::totalTime() const {
    vector<int> exit_task_times;
    
    for (const Task& task : tasks) {
        if (task.getSuccTasks().empty()) {
            exit_task_times.push_back(
                max(task.getFinishTimeLocal(), task.getFinishTimeWirelessReceive()
                )
            );
        }
    }
    
    return *max_element(exit_task_times.begin(), exit_task_times.end());
}

double  MCCScheduler::calculateTaskEnergyConsumption(const Task& task, const vector<int>& core_powers, double cloud_sending_power) const {
    if (task.isCoreTask()) {
        return static_cast<double>(core_powers[task.getAssignment()]) * 
               static_cast<double>(task.getCoreExecutionTimes()[task.getAssignment()]);
    } else {
        return cloud_sending_power * static_cast<double>(task.getCloudExecutionTimes()[0]);
    }
}

double  MCCScheduler::totalEnergy(const vector<int>& core_powers, double cloud_sending_power) const {
    return accumulate(
        tasks.begin(),
        tasks.end(),
        0.0,
        [this, &core_powers, cloud_sending_power](double current_sum, const Task& task) {
            return current_sum + calculateTaskEnergyConsumption(task, core_powers, cloud_sending_power);
        }
    );
}

void  MCCScheduler::primaryAssignment() {
    for (Task& task : tasks) {
        int t_l_min = *min_element(
            task.getCoreExecutionTimes().begin(),
            task.getCoreExecutionTimes().end()
        );

        const auto& cloud_times = task.getCloudExecutionTimes();
        int t_re = cloud_times[0] + cloud_times[1] + cloud_times[2];
        task.setIsCoreTask(!(t_re < t_l_min));
    }
}

void  MCCScheduler::taskPrioritizing() {
    vector<int> w(tasks.size(), 0);
    for (size_t i = 0; i < tasks.size(); i++) {
        Task& task = tasks[i];
        if (!task.isCoreTask()) {
            const auto& times = task.getCloudExecutionTimes();
            w[i] = times[0] + times[1] + times[2];
        } else {
            const auto& times = task.getCoreExecutionTimes();
            int sum = accumulate(times.begin(), times.end(), 0);
            w[i] = sum / times.size();
        }
    }
    
    map<int, int> computed_priority_scores;
    
    for (Task& task : tasks) {
        calculatePriority(task, w, computed_priority_scores);
    }
    
    for (Task& task : tasks) {
        task.setPriorityScore(computed_priority_scores[task.getId()]);
    }
}

int  MCCScheduler::calculatePriority(Task& task, vector<int>& w,map<int, int>& computed_priority_scores) {
    if (computed_priority_scores.count(task.getId()) > 0) {
        return computed_priority_scores[task.getId()];
    }

    if (task.getSuccTasks().empty()) {
        int priority = w[task.getId() - 1];
        computed_priority_scores[task.getId()] = priority;
        return priority;
    }
    
    int max_successor_priority = -1;
    for (Task* successor : task.getSuccTasks()) {
        int successor_priority = calculatePriority(*successor, w, computed_priority_scores);
        max_successor_priority = max(max_successor_priority, successor_priority);
    }
    
    int task_priority = w[task.getId() - 1] + max_successor_priority;
    computed_priority_scores[task.getId()] = task_priority;
    return task_priority;
}