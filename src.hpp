#ifndef SRC_HPP
#define SRC_HPP

#include <cstddef>

/**
 * 枚举类，用于枚举可能的置换策略
 */
enum class ReplacementPolicy { kDEFAULT = 0, kFIFO, kLRU, kMRU, kLRU_K };

/**
 * @brief 该类用于维护每一个页对应的信息以及其访问历史，用于在尝试置换时查询需要的信息。
 */
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
    : max_size_(max_size), k_(k), default_policy_(default_policy), current_time_(0), size_(0) {
    if (max_size_ > 0) {
      cache_ = new PageNode[max_size_];
    } else {
      cache_ = nullptr;
    }
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

    for (std::size_t i = 0; i < max_size_; ++i) {
      if (cache_[i].IsValid() && cache_[i].GetPageId() == page_id) {
        cache_[i].AddAccess(current_time_);
        evict_id = npos;
        return;
      }
    }

    if (size_ < max_size_) {
      for (std::size_t i = 0; i < max_size_; ++i) {
        if (!cache_[i].IsValid()) {
          cache_[i].Init(page_id, k_, current_time_);
          size_++;
          evict_id = npos;
          return;
        }
      }
    } else {
      std::size_t evict_idx = FindEvictIndex(policy);
      evict_id = cache_[evict_idx].GetPageId();
      cache_[evict_idx].Init(page_id, k_, current_time_);
    }
  }

  bool RemovePage(std::size_t page_id) {
    if (max_size_ == 0) return false;
    for (std::size_t i = 0; i < max_size_; ++i) {
      if (cache_[i].IsValid() && cache_[i].GetPageId() == page_id) {
        cache_[i].Invalidate();
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
  PageNode* cache_;

  std::size_t FindEvictIndex(ReplacementPolicy policy) const {
    std::size_t best_idx = 0;
    bool found_first = false;

    for (std::size_t i = 0; i < max_size_; ++i) {
      if (!cache_[i].IsValid()) continue;

      if (!found_first) {
        best_idx = i;
        found_first = true;
        continue;
      }

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
