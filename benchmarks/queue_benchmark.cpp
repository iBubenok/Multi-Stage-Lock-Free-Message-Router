#include <benchmark/benchmark.h>
#include "spsc_queue.hpp"
#include "message.hpp"
#include <thread>
#include <atomic>

// Бенчмарк: производительность SPSC очереди (single-threaded push/pop)
static void BM_SPSC_PushPop(benchmark::State& state) {
    SPSCQueue<Message, 65536> queue;
    Message msg = Message::create(0, 0, 0);

    for (auto _ : state) {
        queue.try_push(msg);
        Message out;
        queue.try_pop(out);
        benchmark::DoNotOptimize(out);
    }

    state.SetItemsProcessed(state.iterations());
}
BENCHMARK(BM_SPSC_PushPop);

// Бенчмарк: пропускная способность SPSC очереди (multi-threaded)
static void BM_SPSC_Throughput(benchmark::State& state) {
    SPSCQueue<Message, 65536> queue;
    std::atomic<bool> running{true};
    std::atomic<uint64_t> messages_sent{0};
    std::atomic<uint64_t> messages_received{0};

    // Producer thread
    std::thread producer([&]() {
        Message msg = Message::create(0, 0, 0);
        uint64_t seq = 0;
        while (running.load(std::memory_order_relaxed)) {
            if (queue.try_push(msg)) {
                messages_sent.fetch_add(1, std::memory_order_relaxed);
                msg.sequence_number = ++seq;
            }
        }
    });

    // Consumer (benchmark main thread)
    for (auto _ : state) {
        Message msg;
        if (queue.try_pop(msg)) {
            messages_received.fetch_add(1, std::memory_order_relaxed);
            benchmark::DoNotOptimize(msg);
        }
    }

    running.store(false, std::memory_order_release);
    producer.join();

    uint64_t total = messages_received.load();
    state.SetItemsProcessed(total);
    state.SetBytesProcessed(total * sizeof(Message));
}
BENCHMARK(BM_SPSC_Throughput)->Threads(1)->UseRealTime();

// Бенчмарк: задержка в SPSC очереди
static void BM_SPSC_Latency(benchmark::State& state) {
    SPSCQueue<Message, 65536> queue;

    for (auto _ : state) {
        Message msg = Message::create(0, 0, 0);
        auto start = std::chrono::high_resolution_clock::now();

        queue.try_push(msg);
        Message out;
        queue.try_pop(out);

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        state.SetIterationTime(elapsed.count() / 1e9);
        benchmark::DoNotOptimize(out);
    }
}
BENCHMARK(BM_SPSC_Latency)->UseManualTime();

// Бенчмарк: размер очереди (capacity test)
static void BM_SPSC_Fill(benchmark::State& state) {
    const size_t capacity = static_cast<size_t>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<Message, 65536> queue;
        Message msg = Message::create(0, 0, 0);
        state.ResumeTiming();

        // Заполнение очереди
        size_t filled = 0;
        for (size_t i = 0; i < capacity; ++i) {
            if (queue.try_push(msg)) {
                ++filled;
            } else {
                break;
            }
        }

        benchmark::DoNotOptimize(filled);
    }
}
BENCHMARK(BM_SPSC_Fill)->Arg(1000)->Arg(10000)->Arg(50000);

// Главная функция для бенчмарков
BENCHMARK_MAIN();
