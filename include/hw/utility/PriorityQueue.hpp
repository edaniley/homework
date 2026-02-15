#pragma once
#include <functional>
#include <algorithm>

namespace hw::utility {

//
// simple priority queue; not thread safe
//
template <typename T, size_t N, typename Compare = std::less<T>>
class PriorityQueue {
public:
  PriorityQueue() : _size (0) {}

  bool push(const T& value) {
    if (_size > N) return false;
    size_t i = _size;
    _heap[_size++] = value;

    while (i > 0 && _comp(_heap[(i - 1) / 2], _heap[i])) {
      std::swap( _heap[i], _heap[(i - 1) / 2]);
      i = (i - 1) / 2;
    }
    return true;
  }

  const T& top() const {
    return _heap[0];
  }

  void pop() {
    if (_size == 0)
      return;

    _heap[0] = _heap[--_size];
    size_t i = 0;
    while (true) {
      size_t largest = i;
      size_t left = 2 * i + 1;
      size_t right = 2 * i + 2;
      if (left < _size && _comp(_heap[largest], _heap[left])) {
        largest = left;
      }
      if (right < _size && _comp(_heap[largest], _heap[right])) {
        largest = right;
      }
      if (largest == i)
        break;

      std::swap(_heap[i], _heap[largest]);
      i = largest;
    }
  }

  size_t size() const { return _size; }
  bool empty() const { return _size == 0; }
  void clear() { _size = 0; }

private:
  T _heap[N];
  size_t _size;
  Compare _comp;
};


}