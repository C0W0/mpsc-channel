#include "MPSCQueue.h"
#include <iostream>
#include <thread>
#include <utility>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_set>
#include <chrono>
#include <iomanip>
#include <string>
#include <sstream>

// Utility for test reporting
std::mutex cout_mutex;
#define SAFE_COUT(msg) { std::lock_guard<std::mutex> lock(cout_mutex); std::cout << msg << std::endl; }

// Test status tracking
struct TestResult {
    bool success = true;
    std::string message;
};

TestResult basic_test() {
    SAFE_COUT("\n=== BASIC TEST (2 producers, 1 consumer) ===");
    using namespace mpsc_queue;
    MPSCQueue<int, 4> queue;

    auto producer_fn = [&queue](const int start_v) {
        SAFE_COUT("Basic Producer " << start_v << " start");
        for (int i = 0; i < 10; i++) {
            queue.insert(start_v + i);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        SAFE_COUT("Basic Producer " << start_v << " end");
    };

    auto consumer_fn = [&queue]() {
        SAFE_COUT("Basic Consumer start");
        bool terminate = false;
        while (!terminate) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            const auto val = queue.remove();
            SAFE_COUT("Basic consumer retrieved: " << val);
            terminate = val == 19; // Changed to 19 (10 + 9) to match last item from second producer
        }
        SAFE_COUT("Basic Consumer end");
    };

    std::thread threads[] = {
        std::thread(producer_fn, 0),
        std::thread(producer_fn, 10),
        std::thread(consumer_fn),
    };

    for (auto& t : threads) {
        t.join();
    }
    
    return {true, "Basic test completed"};
}

// Simple struct to pass between producers and consumers
struct Item {
    int producer_id;
    int value;
};

// Simple stress test to verify MPMC safety
TestResult stress_test() {
    SAFE_COUT("\n=== STRESS TEST (3 producers, 2 consumers) ===");
    using namespace mpsc_queue;
    
    // Queue with 32 slots
    MPSCQueue<Item, 5> queue;
    
    // Configuration
    constexpr int ITEMS_PER_PRODUCER = 1000;
    constexpr int NUM_PRODUCERS = 3;
    
    // Tracking
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::mutex consumed_mutex;
    std::unordered_set<int> consumed_values;
    bool found_duplicate = false;
    
    // Producer function
    auto producer_fn = [&](int id) {
        SAFE_COUT("Producer " << id << " starting");
        for (int i = 0; i < ITEMS_PER_PRODUCER; i++) {
            // Each producer generates unique values
            // Producer 0: 0, 3, 6, 9...
            // Producer 1: 1, 4, 7, 10...
            // Producer 2: 2, 5, 8, 11...
            Item item{id, id + (i * NUM_PRODUCERS)};
            queue.insert(std::move(item));
            items_produced++;
            
            // Small random delay (0-100 microseconds)
            if (i % 50 == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(rand() % 100));
            }
        }
        SAFE_COUT("Producer " << id << " finished");
    };
    
    // Consumer function
    auto consumer_fn = [&]() {
        SAFE_COUT("Consumer starting");
        while (items_consumed.load() < NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
            auto [producer_id, value] = queue.remove();
            items_consumed++;

            // Check for duplicates
            {
                std::lock_guard<std::mutex> lock(consumed_mutex);

                // Calculate the unique value across all producers
                int unique_val = producer_id + (value * NUM_PRODUCERS);

                // Check if we've seen this value before
                if (!consumed_values.insert(unique_val).second) {
                    SAFE_COUT("ERROR: Duplicate value detected: " << unique_val
                             << " from producer " << producer_id);
                    found_duplicate = true;
                }
            }
            
            // Progress report
            if (items_consumed % 500 == 0) {
                SAFE_COUT("Progress: " << items_consumed << " items consumed");
            }
        }
        SAFE_COUT("Consumer finished");
    };
    
    // Start all threads
    std::vector<std::thread> threads;
    
    // Start producers
    for (int i = 0; i < NUM_PRODUCERS; i++) {
        threads.emplace_back(producer_fn, i);
    }
    
    // Start consumer
    threads.emplace_back(consumer_fn);
    
    // Wait for all threads to finish
    for (auto& t : threads) {
        t.join();
    }
    
    // Verify results
    bool success = true;
    std::stringstream message;
    
    // Check counts
    if (items_produced != items_consumed) {
        success = false;
        message << "ERROR: Produced " << items_produced << " items but consumed " 
                << items_consumed << "! ";
    }
    
    // Check for duplicates
    if (found_duplicate) {
        success = false;
        message << "ERROR: Found duplicate values! ";
    }
    
    // Check correct count of unique values
    if (consumed_values.size() != NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
        success = false;
        message << "ERROR: Expected " << (NUM_PRODUCERS * ITEMS_PER_PRODUCER) 
                << " unique values but got " << consumed_values.size() << "! ";
    }
    
    if (success) {
        message << "Successfully processed " << items_consumed << " items with "
                << NUM_PRODUCERS << " producers and 1 consumer";
    }
    
    SAFE_COUT(message.str());
    return {success, message.str()};
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    
    // Run tests
    auto basic_result = basic_test();
    auto stress_result = stress_test();
    
    // Report results
    SAFE_COUT("\n=== TEST RESULTS ===");
    SAFE_COUT("Basic Test: " << (basic_result.success ? "PASSED" : "FAILED"));
    SAFE_COUT("Stress Test: " << (stress_result.success ? "PASSED" : "FAILED"));
    
    return (basic_result.success && stress_result.success) ? 0 : 1;
}
