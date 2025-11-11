#include <benchmark/benchmark.h>
#include "spsc_queue.hpp"
#include "message.hpp"
#include <vector>
#include <memory>

// Бенчмарк: выделение памяти для очередей
static void BM_QueueAllocation(benchmark::State& state) {
    const size_t num_queues = static_cast<size_t>(state.range(0));

    for (auto _ : state) {
        std::vector<std::unique_ptr<SPSCQueue<Message, 65536>>> queues;

        for (size_t i = 0; i < num_queues; ++i) {
            queues.push_back(std::make_unique<SPSCQueue<Message, 65536>>());
        }

        benchmark::DoNotOptimize(queues);
    }
}
BENCHMARK(BM_QueueAllocation)->Arg(1)->Arg(4)->Arg(8)->Arg(16);

// Бенчмарк: использование памяти при работе очереди
static void BM_QueueMemoryUsage(benchmark::State& state) {
    const size_t fill_count = static_cast<size_t>(state.range(0));
    SPSCQueue<Message, 65536> queue;

    for (auto _ : state) {
        state.PauseTiming();
        // Заполнение очереди
        Message msg = Message::create(0, 0, 0);
        for (size_t i = 0; i < fill_count; ++i) {
            queue.try_push(msg);
        }
        state.ResumeTiming();

        // Опустошение очереди
        Message out;
        size_t popped = 0;
        while (queue.try_pop(out)) {
            ++popped;
        }

        benchmark::DoNotOptimize(popped);
    }

    state.SetBytesProcessed(state.iterations() * fill_count * sizeof(Message));
}
BENCHMARK(BM_QueueMemoryUsage)->Arg(100)->Arg(1000)->Arg(10000)->Arg(50000);

// Бенчмарк: cache miss при работе с очередью
static void BM_CacheMisses(benchmark::State& state) {
    SPSCQueue<Message, 65536> queue;
    const size_t stride = static_cast<size_t>(state.range(0));

    // Заполнение очереди
    for (size_t i = 0; i < 10000; ++i) {
        Message msg = Message::create(static_cast<uint8_t>(i % 4), 0, i);
        queue.try_push(msg);
    }

    std::vector<Message> buffer;
    Message msg;
    while (queue.try_pop(msg)) {
        buffer.push_back(msg);
    }

    for (auto _ : state) {
        // Доступ с заданным шагом (stride) для имитации cache misses
        uint64_t sum = 0;
        for (size_t i = 0; i < buffer.size(); i += stride) {
            sum += buffer[i].sequence_number;
        }
        benchmark::DoNotOptimize(sum);
    }
}
BENCHMARK(BM_CacheMisses)->Arg(1)->Arg(8)->Arg(64)->Arg(256);

// Бенчмарк: влияние размера очереди на производительность
static void BM_QueueSizeImpact(benchmark::State& state) {
    // Различные размеры очередей (степени 2)
    const size_t size = static_cast<size_t>(state.range(0));

    for (auto _ : state) {
        state.PauseTiming();
        SPSCQueue<Message, 65536> queue;
        state.ResumeTiming();

        // Push и Pop операции
        Message msg = Message::create(0, 0, 0);
        for (size_t i = 0; i < size; ++i) {
            queue.try_push(msg);
        }

        Message out;
        size_t popped = 0;
        for (size_t i = 0; i < size; ++i) {
            if (queue.try_pop(out)) {
                ++popped;
            }
        }

        benchmark::DoNotOptimize(popped);
    }

    state.SetItemsProcessed(state.iterations() * size * 2); // push + pop
}
BENCHMARK(BM_QueueSizeImpact)
    ->Arg(256)
    ->Arg(1024)
    ->Arg(4096)
    ->Arg(16384)
    ->Arg(65536);

BENCHMARK_MAIN();
