#pragma once

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>

template<typename T>
class threadsafe_queue
{
  private:
    mutable std::mutex mut; // mutex to protect the queue
    std::queue<T> _data_queue; // the queue
    std::condition_variable _cv; // condition variable to wait for the queue to be not empty

  public:
    threadsafe_queue() {}
    
    threadsafe_queue(const threadsafe_queue& other) // copy constructor
    {
        //Take the other mutex and copy the queue
	    std::lock_guard<std::mutex> lk(other.mut); // lock the other mutex
        _data_queue = other._data_queue; // copy the queue
    }

    // delete copy assignment operator 
    threadsafe_queue& operator=(const threadsafe_queue&) = delete;

    void push(T new_value) // push a new value into the queue
    {   
        //push the new value into the queue
	    std::lock_guard<std::mutex> lk(mut); // lock the mutex
        _data_queue.push(new_value); // push the new value into the queue
        _cv.notify_one(); // notify the condition variable, notify one thread waiting for the queue to be not empty
    }

    bool try_pop(T& value) // pop a value from the queue if the queue is not empty
    {
        std::lock_guard<std::mutex> lk(mut); // lock the mutex
        if(_data_queue.empty())
            return false;
        else {
            value = _data_queue.front(); // get the front element of the queue
            _data_queue.pop(); // pop the front element
            return true;
        }
    }

    void wait_and_pop(T& value) // pop a value from the queue if the queue is not empty, otherwise wait
    {
	    std::unique_lock<std::mutex> lk(mut); // lock the mutex
        _cv.wait(lk, [this]{return !_data_queue.empty();}); // wait until the queue is not empty
        value = _data_queue.front(); // get the front element of the queue
        _data_queue.pop(); // pop the front element
    }

    std::shared_ptr<T> wait_and_pop() // shared pointer version of wait_and_pop
    {
	    std::unique_lock<std::mutex> lk(mut); // lock the mutex
        _cv.wait(lk, [this]{return !_data_queue.empty();}); // wait until the queue is not empty
        std::shared_ptr<T> res(std::make_shared<T>(_data_queue.front())); // make a copy of the front element of the queue
        _data_queue.pop(); // pop the front element
        return res; // return the copy
    }

    bool empty() const
    {
        std::lock_guard<std::mutex> lk(mut); // lock the mutex
        return _data_queue.empty(); // return true if the queue is empty
    }
};
