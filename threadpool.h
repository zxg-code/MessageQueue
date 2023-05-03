#include <thread>
#include <mutex>
#include <functional>
#include <unordered_set>
#include <vector>

#include "message_queue.h"

class Threadpool {
 public:
  explicit Threadpool(size_t n_threads, size_t n_tasks)
      : is_stop_(false), 
        n_threads_(n_threads),
        tasks_queue_(std::make_unique<MessageQueue>(n_tasks)) {
    Create(n_threads_);
  }
  explicit Threadpool()
      : is_stop_(false), 
        n_threads_(12),
        tasks_queue_(std::make_unique<MessageQueue>(128)) {
    Create(n_threads_);
  }

  ~Threadpool() {
    Destroy();
  };

  template<typename F>
  int AddTask(F&& task) {
    tasks_queue_->Put(task);
  }

  int Increase();

  void Destroy() {
    std::unique_lock<std::mutex> locker(mtx_);
    this->is_stop_ = true;  // 退出标志
    tasks_queue_->SetNonBlock();  // 立刻返回
    locker.unlock();
    for (auto& t : threads_) {
      t->join();
    }
  }

 private:
  void Routine() {
    while (!is_stop_) {
      std::function<void()> task;
      auto ret = tasks_queue_->Get(task);
      if (!ret) break;  // 任务队列空且不阻塞时，退出
      task();  // 执行任务
    }
  }

  void Create(size_t capacity) {
    for (size_t i = 0; i < capacity; ++i) {
      threads_.emplace_back(std::make_unique<std::thread>(Routine, this));
    }
  }

  bool is_stop_;
  size_t n_threads_;
  std::vector<std::unique_ptr<std::thread>> threads_;
  std::unique_ptr<MessageQueue> tasks_queue_;
  std::mutex mtx_;
};