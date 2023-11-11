#pragma once

#include <atomic>
#include <functional>
#include <vector>

#include<join_threads.hpp>
#include<threadsafe_queue.hpp>

// run jobs until a variable flags the termination of all the threads,
// and the workers can either spin for tasks or call std::this_thread::yield 
// to notify the OS the worker does not have work to do.

class thread_pool
{
  std::atomic<bool> _done; // flag to indicate that the thread pool is done
  size_t _thread_count; // number of threads in the pool
  threadsafe_queue<std::function<void()>> _work_queue; // queue of tasks
  std::vector<std::thread> _threads; // vector of threads
  join_threads _joiner; // joiner to join the threads

  using task_type = void(); // type of the tasks

  void worker_thread() //  continuously pops tasks from the queue and executes them while the _done flag is false.
  {
    while (!_done) // while the thread pool is not done
    {
      std::function<task_type> task; // create a task
      if(_work_queue.try_pop(task)) // try to pop a task from the queue
        task(); // execute the task
      else
        std::this_thread::yield(); // yield to the OS (the thread does not have work to do
    }
  }

  public:
  thread_pool(size_t num_threads = std::thread::hardware_concurrency()) // constructor
    : _done(false), _thread_count(num_threads), _joiner(_threads) // initialize the joiner
  {
    for(size_t i = 0; i < _thread_count; ++i)
      _threads.push_back(std::thread(&thread_pool::worker_thread, this)); // create threads

  }

  ~thread_pool() // destructor
  {
    wait(); // ensure no pending tasks
  }

  void wait()
  {
      // wait for completion
      _done = true; // set the flag to true
      // notify all the threads by pushing empty tasks to the queue
      for (size_t i = 0; i < _thread_count; ++i)
      {
        _work_queue.push(std::function<task_type>()); // push empty tasks to the queue
      }
      // active waiting
      

  }

  template<typename F>
    void submit(F f)
    {
      // submit a task to the queue
      _work_queue.push(std::function<task_type>(f));
    }
};
