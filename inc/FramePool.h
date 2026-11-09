#pragma once

#include "SVACDecoder.h"
#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class FramePool {
public:
    FramePool(int capacity, int width, int height);
    ~FramePool();

    // 从池中取出一个已写入的帧（用于读取）
    Frame *getFrame();
    // 读取完成后释放当前读槽位（推进读指针并减少计数）
    void releaseFrame();
    
    // 获取一个可用于写入的帧（返回预分配的内存，减少内存分配）
    // 如果池已满，返回 nullptr
    Frame *getWriteFrame();
    
    // 提交写入，更新写位置和大小
    // 必须在 GetWriteFrame() 后调用，且写入数据已完成
    void commitWrite();

    int getSize();
    int getAvailableSize();  // 获取可用的写入空间

private:
    std::vector<std::unique_ptr<Frame>> _frames;
    const int _capacity;
    std::atomic_int _size;
    
    // 无锁实现：使用原子变量（适用于 SPSC - 单生产者单消费者场景）
    std::atomic<int> _read_pos;   // 只被消费者线程修改
    std::atomic<int> _write_pos;  // 只被生产者线程修改
};