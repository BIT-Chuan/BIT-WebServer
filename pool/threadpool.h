#ifndef THREADPOOL_H
#define THREADPOOL_H

#include<thread>
#include<mutex>
#include<queue>
#include<semaphore.h>
#include<functional>

class ThreadPool{
public:
    ThreadPool(int maxThreads = 8) : m_stop(false) {
        sem_init(&m_sem, 0, 0);
        for(int i = 0; i < maxThreads; ++i){
            std::thread([&]{
                while(!m_stop){
                    sem_wait(&m_sem);
                    std::unique_lock<std::mutex>locker(m_mutex);
                    if(m_tasks.empty()){
                        locker.unlock();
                        continue;
                    }
                    auto task = std::move(m_tasks.front());
                    m_tasks.pop();
                    locker.unlock();
                    task();
                }
            }).detach();
        }
    }

    ~ThreadPool(){
        m_stop = true;
        sem_destroy(&m_sem);
    }

    template<typename T>
    void addTask(T&& task){
        {
            std::lock_guard<std::mutex>locker(m_mutex);
            m_tasks.emplace(std::forward<T>(task));
        }
        sem_post(&m_sem);
    }
private:
    std::queue<std::function<void()>>m_tasks;
    std::mutex m_mutex;
    sem_t m_sem;
    bool m_stop;
};

#endif