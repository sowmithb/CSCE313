#include "pool.h"
#include <mutex>
#include <iostream>

Task::Task() = default;
Task::~Task() = default;

ThreadPool::ThreadPool(int num_threads) {
    for (int i = 0; i < num_threads; i++) {
        threads.emplace_back(new std::thread(&ThreadPool::run_thread, this));
    }
}

ThreadPool::~ThreadPool() {
    for (std::thread *t: threads) {
        delete t;
    }
    threads.clear();

    for (Task *q: queue) {
        delete q;
    }
    queue.clear();
}

void ThreadPool::SubmitTask(const std::string &name, Task *task) {
    //TODO: Add task to queue, make sure to lock the queue
    std::lock_guard<std::mutex> lock(mtx);

    if (done){
        std::cout << "Cannot added task to queue: " << name << std::endl;
        return;
    }

    task->name = name;
    task->running = false;
    queue.push_back(task);
    num_tasks_unserviced++;

    std::cout << "Added task to queue: " << name << std::endl;
}

void ThreadPool::run_thread()
{
    while (true)
    {
        Task *task = nullptr;

        {
            std::lock_guard<std::mutex> lock(mtx);

            // If Stop() was called *and* there is no more work left,
            // this worker can terminate.
            if (done && queue.empty())
            {
                std::cout << "Stopping thread " << std::this_thread::get_id() << std::endl;
                break;
            }

            // Grab next task from the queue, if any.
            if (!queue.empty())
            {
                task = queue.front();
                queue.erase(queue.begin());
                --num_tasks_unserviced;
                task->running = true;
                std::cout << "Started task " << task->name << std::endl;
            }
        }

        // Nothing to do right now – yield and loop again.
        if (!task)
        {
            std::this_thread::yield();
            continue;
        }

        // Run the task outside the lock.
        task->Run();
        task->running = false;
        std::cout << "Finished task " << task->name << std::endl;

        // The pool owns the Task objects once submitted, so we clean
        // them up after they finish running.
        delete task;
    }
}


// Remove Task t from queue if it's there
void ThreadPool::remove_task(Task *t) {
    mtx.lock();
    for (auto it = queue.begin(); it != queue.end();) {
        if (*it == t) {
            queue.erase(it);
            mtx.unlock();
            return;
        }
        ++it;
    }
    mtx.unlock();
}

void ThreadPool::Stop() {
    //TODO: Delete threads, but remember to wait for them to finish first
    std::cout << "Called Stop()" << std::endl;
    done = true;

    // Join all worker threads so that destruction of the pool is safe.
    for (std::thread *t : threads)
    {
        if (t && t->joinable())
        {
            t->join();
        }
    }
}
