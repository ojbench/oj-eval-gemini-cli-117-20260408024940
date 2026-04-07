#ifndef SRC_HPP
#define SRC_HPP

#include <cstddef>

enum class ReplacementPolicy { kDEFAULT = 0, kFIFO, kLRU, kMRU, kLRU_K };

class PageNode {
public:
  PageNode() : page_id_(0), access_times_(nullptr), k_(0), access_count_(0), add_time_(0), last_access_time_(0), valid_(false), head_(0) {}
  ~PageNode() {
    if (access_times_) {
      delete[] access_times_;
    }
  }

  PageNode(const PageNode&) = delete;
  PageNode& operator=(const PageNode&) = delete;

  void Init(std::size_t page_id, std::size_t k, std::size_t current_time) {
    page_id_ = page_id;
    if (k_ != k) {
      if (access_times_) delete[] access_times_;
      k_ = k;
      if (k_ > 0) {
        access_times_ = new std::size_t[k_];
      } else {
        access_times_ = nullptr;
      }
    }
    access_count_ = 0;
    add_time_ = current_time;
    valid_ = true;
    head_ = 0;
    AddAccess(current_time);
  }

  void AddAccess(std::size_t current_time) {
    if (k_ == 0) {
        access_count_++;
        last_access_time_ = current_time;
        return;
    }
    access_times_[head_] = current_time;
    head_ = (head_ + 1) % k_;
    access_count_++;
    last_access_time_ = current_time;
  }

  void Invalidate() {
    valid_ = false;
  }

  void Swap(PageNode& other) {
    auto tmp_page_id = page_id_; page_id_ = other.page_id_; other.page_id_ = tmp_page_id;
    auto tmp_access_times = access_times_; access_times_ = other.access_times_; other.access_times_ = tmp_access_times;
    auto tmp_k = k_; k_ = other.k_; other.k_ = tmp_k;
    auto tmp_access_count = access_count_; access_count_ = other.access_count_; other.access_count_ = tmp_access_count;
    auto tmp_add_time = add_time_; add_time_ = other.add_time_; other.add_time_ = tmp_add_time;
    auto tmp_last_access_time = last_access_time_; last_access_time_ = other.last_access_time_; other.last_access_time_ = tmp_last_access_time;
    auto tmp_valid = valid_; valid_ = other.valid_; other.valid_ = tmp_valid;
    auto tmp_head = head_; head_ = other.head_; other.head_ = tmp_head;
  }

  bool IsValid() const { return valid_; }
  std::size_t GetPageId() const { return page_id_; }
  std::size_t GetAddTime() const { return add_time_; }
  std::size_t GetLastAccessTime() const { return last_access_time_; }
  std::size_t GetKthAccessTime() const {
    if (access_count_ < k_) return 0; 
    return access_times_[head_];
  }
  std::size_t GetFirstAccessTime() const {
    return add_time_; 
  }
  std::size_t GetAccessCount() const { return access_count_; }

private:
  std::size_t page_id_{};
  std::size_t* access_times_{nullptr};
  std::size_t k_{0};
  std::size_t access_count_{0};
  std::size_t add_time_{0};
  std::size_t last_access_time_{0};
  bool valid_{false};
  std::size_t head_{0};
};

class ReplacementManager {
public:
  constexpr static std::size_t npos = -1;

  ReplacementManager() = delete;
  ReplacementManager(const ReplacementManager&) = delete;
  ReplacementManager& operator=(const ReplacementManager&) = delete;

  ReplacementManager(std::size_t max_size, std::size_t k, ReplacementPolicy default_policy) 
    : max_size_(max_size), k_(k), default_policy_(default_policy), current_time_(0), size_(0), capacity_(0), cache_(nullptr) {
  }

  ~ReplacementManager() {
    if (cache_) {
      delete[] cache_;
    }
  }

  void SwitchDefaultPolicy(ReplacementPolicy default_policy) {
    default_policy_ = default_policy;
  }

  void Visit(std::size_t page_id, std::size_t &evict_id, ReplacementPolicy policy = ReplacementPolicy::kDEFAULT) {
    if (max_size_ == 0) {
      evict_id = npos;
      return;
    }
    current_time_++;
    if (policy == ReplacementPolicy::kDEFAULT) {
      policy = default_policy_;
    }

    for (std::size_t i = 0; i < size_; ++i) {
      if (cache_[i].GetPageId() == page_id) {
        cache_[i].AddAccess(current_time_);
        evict_id = npos;
        return;
      }
    }

    if (size_ < max_size_) {
      if (size_ == capacity_) {
        std::size_t new_capacity = capacity_ == 0 ? 1 : capacity_ * 2;
        if (new_capacity > max_size_) new_capacity = max_size_;
        PageNode* new_cache = new PageNode[new_capacity];
        for (std::size_t i = 0; i < size_; ++i) {
          new_cache[i].Swap(cache_[i]);
        }
        if (cache_) delete[] cache_;
        cache_ = new_cache;
        capacity_ = new_capacity;
      }
      cache_[size_].Init(page_id, k_, current_time_);
      size_++;
      evict_id = npos;
    } else {
      std::size_t evict_idx = FindEvictIndex(policy);
      evict_id = cache_[evict_idx].GetPageId();
      cache_[evict_idx].Init(page_id, k_, current_time_);
    }
  }

  bool RemovePage(std::size_t page_id) {
    if (max_size_ == 0) return false;
    for (std::size_t i = 0; i < size_; ++i) {
      if (cache_[i].GetPageId() == page_id) {
        if (i != size_ - 1) {
          cache_[i].Swap(cache_[size_ - 1]);
        }
        cache_[size_ - 1].Invalidate();
        size_--;
        return true;
      }
    }
    return false;
  }

  [[nodiscard]] std::size_t TryEvict(ReplacementPolicy policy = ReplacementPolicy::kDEFAULT) const {
    if (max_size_ == 0 || size_ < max_size_) {
      return npos;
    }
    if (policy == ReplacementPolicy::kDEFAULT) {
      policy = default_policy_;
    }
    std::size_t evict_idx = FindEvictIndex(policy);
    return cache_[evict_idx].GetPageId();
  }

  [[nodiscard]] bool Empty() const {
    return size_ == 0;
  }

  [[nodiscard]] bool Full() const {
    return max_size_ > 0 && size_ == max_size_;
  }

  [[nodiscard]] std::size_t Size() const {
    return size_;
  }

private:
  std::size_t max_size_;
  std::size_t k_;
  ReplacementPolicy default_policy_;
  std::size_t current_time_;
  std::size_t size_;
  std::size_t capacity_;
  PageNode* cache_;

  std::size_t FindEvictIndex(ReplacementPolicy policy) const {
    std::size_t best_idx = 0;

    for (std::size_t i = 1; i < size_; ++i) {
      if (policy == ReplacementPolicy::kFIFO) {
        if (cache_[i].GetAddTime() < cache_[best_idx].GetAddTime()) {
          best_idx = i;
        }
      } else if (policy == ReplacementPolicy::kLRU) {
        if (cache_[i].GetLastAccessTime() < cache_[best_idx].GetLastAccessTime()) {
          best_idx = i;
        }
      } else if (policy == ReplacementPolicy::kMRU) {
        if (cache_[i].GetLastAccessTime() > cache_[best_idx].GetLastAccessTime()) {
          best_idx = i;
        }
      } else if (policy == ReplacementPolicy::kLRU_K) {
        bool i_inf = cache_[i].GetAccessCount() < k_;
        bool best_inf = cache_[best_idx].GetAccessCount() < k_;

        if (i_inf && !best_inf) {
          best_idx = i;
        } else if (i_inf && best_inf) {
          if (cache_[i].GetFirstAccessTime() < cache_[best_idx].GetFirstAccessTime()) {
            best_idx = i;
          }
        } else if (!i_inf && !best_inf) {
          if (cache_[i].GetKthAccessTime() < cache_[best_idx].GetKthAccessTime()) {
            best_idx = i;
          }
        }
      }
    }
    return best_idx;
  }
};
#endif