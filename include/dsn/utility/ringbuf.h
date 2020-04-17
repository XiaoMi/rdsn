// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

/*
Example：

int main()
{
    ringbuf<int, 5> q;
    for (int i=1;i<=7;i++){
        q.push(i);
        std::cout<<q.front()<<" "<<q.back()<<" "<<q.size()<<std::endl;
    }
    std::cout<<"-------"<<std::endl;
    while (!q.empty()){
        std::cout<<q.front()<<" "<<q.back()<<" "<<q.size()<<std::endl;
        q.pop();
    }
}

Result:
1 1 1
1 2 2
1 3 3
1 4 4
1 5 5
2 6 5
3 7 5
-------
3 7 5
4 7 4
5 7 3
6 7 2
7 7 1

*/

namespace dsn {
namespace utils {

template <typename Type, int MaxElements>
class ringbuf
{
public:
    ringbuf() : _size(0), _head(0), _tail(0) {}

    void push(const Type &obj)
    {
        _size++;
        _size = std::min(_size, MaxElements);
        if (_tail == _head && _size == MaxElements) {
            _head++;
            if (_head >= MaxElements) {
                _head %= MaxElements;
            }
        }
        _buf[_tail] = obj;
        _tail++;
        if (_tail >= MaxElements) {
            _tail %= MaxElements;
        }
    }

    void pop()
    {
        _head++;
        _size--;
        if (_head >= MaxElements) {
            _head %= MaxElements;
        }
        return;
    }

    Type back() const
    {
        int index = _tail - 1;
        if (index < 0) {
            index += MaxElements;
        }
        return _buf[index];
    }

    Type front() const { return _buf[_head]; }

    bool empty() const
    {
        if (_size != 0)
            return false;
        return true;
    }

    int size() const { return _size; }

private:
    Type _buf[MaxElements + 1];
    int _size;
    int _head, _tail;
};

} // namespace utils
} // namespace dsn