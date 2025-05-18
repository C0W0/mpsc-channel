//
// Created by Terry on 2025-05-11.
//

#ifndef CONCURRENTQUEUE_H
#define CONCURRENTQUEUE_H
#include <cstdint>
#include <semaphore>

namespace concurrent_queue {
    using uint = unsigned int;

    template <typename T, uint N>
    class ConcurrentQueue {
        static_assert(N > 0, "N must be positive");
        static_assert(N <= 32, "N must be less than 32");
    private:
        T* data;
        std::atomic_uint headptr;
        std::atomic_uint tailptr;
        std::counting_semaphore<1 << N> slotSem;
        std::counting_semaphore<1 << N> itemSem;
    public:
        explicit ConcurrentQueue() noexcept
            : headptr(0),
              tailptr(0),
              slotSem(1 << N),
              itemSem(0)
        {
            data = new T[1 << N];
        }

        ~ConcurrentQueue() noexcept {
            delete[] data;
        }

        ConcurrentQueue(const ConcurrentQueue&) = delete;

        void insert(T&& item) noexcept {
            slotSem.acquire();
            auto idx = tailptr.fetch_add(1);
            idx = idx & ~(~0 << N);
            data[idx] = std::move(item);
            itemSem.release();
        }

        T remove() noexcept {
            itemSem.acquire();
            auto idx = headptr.fetch_add(1);
            idx = idx & ~(~0 << N);
            T item = std::move(data[idx]);
            slotSem.release();
            return item;
        }
    };


}
#endif //CONCURRENTQUEUE_H
