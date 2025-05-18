//
// Created by Terry on 2025-05-11.
//

#ifndef CONCURRENTQUEUE_H
#define CONCURRENTQUEUE_H
#include <atomic>
#include <optional>
#include <semaphore>

namespace mpsc::sync_queue {
    using uint = unsigned int;

    template <typename T>
    struct DebugItem {
        uint idx;
        std::optional<T> item;
    };

    template <typename T, uint N>
    class MPSCQueue {
        static_assert(N > 0, "N must be positive");
        static_assert(N <= 32, "N must be less than 32");
    public:
        using Item = std::optional<T>;
    private:
        Item* data;
        std::atomic<uint> headptr;
        std::atomic<uint> tailptr;
        std::counting_semaphore<1 << N> slotSem;
        std::counting_semaphore<1 << N> itemSem;
    public:
        explicit MPSCQueue() noexcept
            : headptr(0),
              tailptr(0),
              slotSem(1 << N),
              itemSem(0)
        {
            data = new Item[1 << N];
        }

        ~MPSCQueue() noexcept {
            delete[] data;
        }

        MPSCQueue(const MPSCQueue&) = delete;

        void insert(T&& item) noexcept {
            slotSem.acquire();
            auto idx = tailptr.fetch_add(1);
            idx = idx & ~(~0 << N);
            data[idx] = std::move(item);
            itemSem.release();
        }

        void mark_inactive() noexcept {
            slotSem.acquire();
            auto idx = tailptr.fetch_add(1);
            idx = idx & ~(~0 << N);
            data[idx] = std::nullopt;
            itemSem.release();
        }

        Item remove() noexcept {
            itemSem.acquire();
            auto idx = headptr.fetch_add(1);
            idx = idx & ~(~0 << N);
            Item item = std::move(data[idx]);
            data[idx] = std::nullopt;
            slotSem.release();
            return item;
        }

        DebugItem<T> remove_dbg() noexcept {
            itemSem.acquire();
            auto idx = headptr.fetch_add(1);
            idx = idx & ~(~0 << N);
            Item item = std::move(data[idx]);
            data[idx] = std::nullopt;
            DebugItem<T> debug_item = { idx, item };
            slotSem.release();
            return debug_item;
        }
    };


}
#endif //CONCURRENTQUEUE_H
