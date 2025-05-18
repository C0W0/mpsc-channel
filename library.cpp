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
#include <map>
#include <unordered_map>
#include <string>
#include <sstream>

#include "Handles.h"

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
    using namespace mpsc::sync_queue;
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
            const auto val = queue.remove().value();
            SAFE_COUT("Basic consumer retrieved: " << val);
            terminate = val == 19; // Changed to 19 (10 + 9) to match last item from second producer
        }
        SAFE_COUT("Basic Consumer end");
    };

    std::vector<std::thread> threads;
    threads.emplace_back(producer_fn, 0);
    threads.emplace_back(producer_fn, 10);
    threads.emplace_back(consumer_fn);

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

// Simple stress test to verify MPSC safety
TestResult stress_test() {
    SAFE_COUT("\n=== STRESS TEST (3 producers, 1 consumer) ===");
    using namespace mpsc::sync_queue;
    
    // Queue with 32 slots
    MPSCQueue<Item, 5> queue;
    
    // Configuration
    constexpr int ITEMS_PER_PRODUCER = 1000;
    constexpr int NUM_PRODUCERS = 2;
    
    // Tracking
    std::atomic<int> items_produced{0};
    std::atomic<int> items_consumed{0};
    std::mutex consumed_mutex;
    std::unordered_set<int> consumed_values;
    std::map<int, int> index_freqs;
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
            auto [idx, item] = queue.remove_dbg();
            auto [producer_id, value] = item.value();
            items_consumed++;

            // Check for duplicates
            {
                std::lock_guard<std::mutex> lock(consumed_mutex);
                index_freqs[idx] ++;

                // Check if we've seen this value before
                if (!consumed_values.insert(value).second) {
                    SAFE_COUT("ERROR: Duplicate value detected: " << value
                             << " from producer " << producer_id << " at index " << idx);
                    SAFE_COUT("Frequency of removed indices: ");
                    for (auto&& [k, v] : index_freqs) {
                        SAFE_COUT(" " << k << ": " << v);
                    }
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

std::thread auto_exit_test1_util(bool* test_passed) {
    auto [tx, rx] = mpsc::channel::create<int>();

    auto consumer_fn = [rx = std::move(rx), &test_passed]() {
        SAFE_COUT("Consumer starting");
        *test_passed = !rx->remove().has_value();
        SAFE_COUT("Consumer finished");
    };

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::thread consumer_fn_thread(std::move(consumer_fn));
    return consumer_fn_thread;
}

// Test to make sure the consumer gets unblocked if the queue is dropped
TestResult auto_exit_consumer_test1() {
    SAFE_COUT("\n=== Auto exit consumer test 1: empty queue ===");
    using namespace mpsc::sync_queue;

    bool test_passed = true;

    std::thread consumer_thread = auto_exit_test1_util(&test_passed);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    consumer_thread.join();

    if (test_passed) {
        return {true, "Consumer received empty value when the producer is dropped"};
    }
    return {false, "Auto exit consumer test 1 failed: expecting empty value to unblock consumer"};
}

int main() {
    srand(static_cast<unsigned>(time(nullptr)));
    using namespace mpsc::channel;
    using namespace mpsc;

    // Run tests
    std::map<std::string, TestResult> results;
    // results["Basic Test"] = basic_test();
    results["Stress Test"] = stress_test();
    // results["Auto exit Test 1"] = auto_exit_consumer_test1();
    
    // Report results
    SAFE_COUT("\n=== TEST RESULTS ===");
    for (auto&& [k, v]: results) {
        SAFE_COUT(k << ": " << (v.success ? "succeeded" : "failed") << " - " << v.message );
    }

    return 0;
}
