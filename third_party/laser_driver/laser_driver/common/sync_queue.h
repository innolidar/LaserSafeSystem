#pragma once
#include <laser_driver/common/common.h>
namespace inno
{
template<typename T>
class SyncQueue
{
public:
    inline size_t Push(const T &value)
    {
        std::lock_guard<std::mutex> ul(m_mtx);
        m_queue.push(value);
        size_t sz=m_queue.size();
        m_cv.notify_one();
        return sz;
    }
    inline T Pop()
    {
        T value=NULL;
        std::lock_guard<std::mutex> ul(m_mtx);
        if(!m_queue.empty())
        {
            value=m_queue.front();
            m_queue.pop();
        }
        return value;
    }
    inline T PopWait(uint32_t msec = 1)
      {
        T value = NULL;

        std::unique_lock<std::mutex> ul(m_mtx);
        m_cv.wait_for(ul, std::chrono::microseconds(msec), [this] { return (!m_queue.empty()); });
        if (!m_queue.empty())
        {
          value = m_queue.front();
          m_queue.pop();
        }
        return value;
      }
    inline void Clear()
    {
        std::queue<T> empty;
        std::lock_guard<std::mutex> ul(m_mtx);
        std::swap(empty, m_queue);
    }
    inline bool IsEmpty()
    {
        bool empty=false;
        std::lock_guard<std::mutex> ul(m_mtx);
        empty=m_queue.empty();
        return empty;
    }
private:
    std::mutex              m_mtx;
    std::queue<T>           m_queue;
    std::condition_variable m_cv;
};

}
