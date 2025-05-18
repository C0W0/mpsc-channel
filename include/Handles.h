//
// Created by Terry on 2025-05-18.
//

#ifndef HANDLES_H
#define HANDLES_H
#include <atomic>
#include <memory>
#include <iostream>

#include "MPSCQueue.h"

namespace mpsc {
    template <typename T>
    constexpr sync_queue::uint queue_size() {
        if (sizeof(T) > 100'000) {
            return 8;
        }
        if (sizeof(T) > 50'000) {
            return 9;
        }
        return 10;
    }

    template <typename T>
    using QueueHandle = std::atomic<std::shared_ptr<sync_queue::MPSCQueue<T, queue_size<T>()>>>;

    template <typename T>
    class Producer {
    private:
         QueueHandle<T> queue;
    public:
        explicit Producer(const QueueHandle<T>& queue) noexcept : queue(queue.load()) {}
        // The producer should not be copied; Sharing of a producer should be done via referenced-counted handles.
        Producer(const Producer&) = delete;
        Producer(Producer&&) = default;

        void insert(T&& item) noexcept {
            queue.load()->insert(std::move(item));
        }

        ~Producer() {
            queue.load()->mark_inactive();
        }
    };

    template <typename T>
    class Consumer {
    private:
        QueueHandle<T> queue;
    public:
        explicit Consumer(const QueueHandle<T>& queue) noexcept : queue(queue.load()) {}
        // The consumer should not be copied; It should not be shared.
        Consumer(const Consumer&) = delete;
        Consumer(Consumer&&) = default;

        std::optional<T> remove() noexcept {
            return queue.load()->remove();
        }

        ~Consumer() {
            std::cout << "Consumer exit" << std::endl;
        }
    };

    namespace channel {
        template <typename T>
        using Sender = std::atomic<std::shared_ptr<Producer<T>>>;
        template <typename T>
        using Receiver = std::unique_ptr<Consumer<T>>;

        template <typename T>
        struct Channel {
            Sender<T> sender;
            Receiver<T> receiver;

            explicit Channel(const QueueHandle<T>& queue) noexcept
                : sender(std::make_shared<Producer<T>>(queue)),
                  receiver(std::make_unique<Consumer<T>>(queue)) {}
        };

        template <typename T>
        Channel<T> create() {
            auto queue = std::make_shared<sync_queue::MPSCQueue<T, queue_size<T>()>>();
            return Channel<T>(queue);
        }
    }
}



#endif //HANDLES_H
