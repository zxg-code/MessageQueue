// Message queue
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>

class MessageQueue {
 public:
  MessageQueue(size_t maxlen) : msg_max_(maxlen), nonblock_(0) {
    put_queue_ = new std::queue<std::function<void()>>;
    get_queue_ = new std::queue<std::function<void()>>;
  }
  MessageQueue() = default;

  ~MessageQueue() {
    delete put_queue_;
    delete get_queue_;
  };

  // usage: msg应该是一个由bind包装的可调用对象
  template<class F>
  void Put(F&& msg) {
    {
      std::unique_lock<std::mutex> put_locker(put_mtx_);
      // 如果队列是block类型的，则有最长限制
      while (put_queue_->size() >= msg_max_ && !nonblock_) {
        put_cv_.wait(put_locker);
      }
      put_queue_->emplace(std::forward<F>(msg));
    }
    get_cv_.notify_one();
  };

  bool Get(std::function<void()>& msg) {
    bool ret = true;
    {
      std::unique_lock<std::mutex> get_locker(get_mtx_);
      if (get_queue_->size() > 0 || this->SwapGetWithPut() > 0) {
        msg = std::move(get_queue_->front());
        get_queue_->pop();
      } else {
        // msg = nullptr;
        ret = false;
      }
    }
    return ret;
  }

  void SetNonBlock() {
    nonblock_ = 1;
    std::unique_lock<std::mutex> put_locker(put_mtx_);
    get_cv_.notify_one();
    put_cv_.notify_all();
  }

  void SetBlock() {
    nonblock_ = 0;
  }

  // 交换生产者和消费者的“指向”，返回生产者队列中的消息数
  size_t SwapGetWithPut() {
    size_t cnt;  // 返回值，表示消息数
    auto tmp = get_queue_;    // 临时保存消费者指针
    get_queue_ = put_queue_;  // 让消费者指向生产者队列，（半个交换）
    std::unique_lock<std::mutex> put_locker(put_mtx_);  // 同时持有生产者和消费者的锁
    // 注意到现在为止还没有彻底完成交换，因为生产者队列没有变化
    // 如果生产者队列是空的
    while (put_queue_->size() == 0 && !nonblock_) {
      get_cv_.wait(put_locker);
    }
    cnt = put_queue_->size();
    // 如果生产者队列是满的，表示可能有多个生产者在等待
    if (cnt >= msg_max_) {
      put_cv_.notify_all();  // 唤醒多个生产者，和下面一句的顺序要换一下吗？
    }
    put_queue_ = tmp;  // 让生产者指向消费者队列，至此完成了生产者和消费者的交换
    put_locker.unlock();
    return cnt;
  }

 private:
  size_t msg_max_;  // 队列长度
  int nonblock_;    // nonblock队列不限制长度，block队列有最大长度
  std::mutex get_mtx_;
  std::mutex put_mtx_;
  std::condition_variable get_cv_;
  std::condition_variable put_cv_;
  // 队列中是一个无参数，无返回值的可调用对象，可以使用bind来产生
  // 生产者和消费者的身份并不固定，因为它们随时有可能发生互换
  std::queue<std::function<void()>>* put_queue_;  // 生产者队列
  std::queue<std::function<void()>>* get_queue_;  // 消费者队列
};

