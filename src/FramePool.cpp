#include "FramePool.h"
#include "logger.h"
#include <cstring>

FramePool::FramePool(int capacity, int width, int height)
    : _capacity(capacity)
    , _size(0)
    , _read_pos(0)
    , _write_pos(0) {
    // 预分配 capacity 个 Frame
    _frames.reserve(_capacity);

    // 扩大frameDataSize 最大支持到800万像素
    size_t frameDataSize = static_cast<size_t>(width) * height * 4 *  3 / 2; 

    for (int i = 0; i < _capacity; ++i) {
        auto frame  = std::make_unique<Frame>();
        frame->width  = width;
        frame->height = height;
        frame->data.resize(frameDataSize); // 预分配内存
        frame->size      = 0; // 初始时数据大小为 0
        frame->keyFrame  = false;
        frame->timestamp = 0;
        _frames.push_back(std::move(frame));
    }

    BLOG(info) << "FramePool created: capacity=" << _capacity << ", width=" << width << ", height=" << height;
}

FramePool::~FramePool() {
    _frames.clear();
    BLOG(debug) << "FramePool destroyed";
}

Frame *FramePool::getFrame() {
    // 获取当前池中的帧数量（在取出之前）
    int currentSize = _size.load(std::memory_order_acquire);
    //  BLOG(debug) << "FramePool::getFrame: current pool size=" << currentSize << "/" << _capacity;

    // 快速检查：如果池为空，返回 nullptr
    if (currentSize == 0) {
        return nullptr;
    }

    // 读取读位置（使用 relaxed，因为只有消费者线程修改）
    int readPos = _read_pos.load(std::memory_order_relaxed);
    return _frames[readPos].get();
}

void FramePool::releaseFrame() {
    // 检查当前是否有可读帧，防止重复释放
    int currentSize = _size.load(std::memory_order_acquire);
    if (currentSize == 0) {
        BLOG(warning) << "releaseRead: pool is empty, nothing to release";
        return;
    }

    // 读取并更新读位置（使用 release，保证之前对帧数据的读取完成）
    int readPos    = _read_pos.load(std::memory_order_relaxed);
    int newReadPos = (readPos + 1) % _capacity;
    _read_pos.store(newReadPos, std::memory_order_release);

    // 原子递减大小（使用 acq_rel，保证与生产者的同步）
    _size.fetch_sub(1, std::memory_order_acq_rel);
}

Frame *FramePool::getWriteFrame() {
    // 快速检查：如果池已满，无法写入
    if (_size.load(std::memory_order_acquire) >= _capacity) {
        BLOG(warning) << "getWriteFrame: pool is full, cannot get write frame";
        return nullptr;
    }

    // 读取写位置（使用 relaxed，因为只有生产者线程修改）
    int writePos = _write_pos.load(std::memory_order_relaxed);

    // 返回写位置的 Frame，调用者可以直接写入数据
    // 注意：此时 write_pos_ 还未更新，需要调用 CommitWrite() 后才更新
    return _frames[writePos].get();
}

void FramePool::commitWrite() {
    // 检查是否真的需要提交（防止重复提交）
    if (_size.load(std::memory_order_acquire) >= _capacity) {
        BLOG(warning) << "commitWrite: pool is full, cannot commit";
        return;
    }

    // 读取当前写位置
    int writePos = _write_pos.load(std::memory_order_relaxed);

    // 更新写位置（使用 release，保证数据写入对消费者可见）
    int newWritePos = (writePos + 1) % _capacity;
    _write_pos.store(newWritePos, std::memory_order_release);

    // 原子递增大小（使用 acq_rel，保证与消费者的同步）
    _size.fetch_add(1, std::memory_order_acq_rel);
}

int FramePool::getAvailableSize() {
    return _capacity - _size.load(std::memory_order_relaxed);
}

int FramePool::getSize() {
    return _size.load(std::memory_order_relaxed);
}
