#include <benchmark/benchmark.h>
#include "message.hpp"
#include "router.hpp"
#include "config.hpp"
#include "spsc_queue.hpp"
#include <memory>
#include <vector>

// Бенчмарк: накладные расходы на маршрутизацию
static void BM_RoutingOverhead(benchmark::State& state) {
    // Создание правил маршрутизации
    std::vector<Stage1Rule> rules = {
        {0, {0}},
        {1, {1}},
        {2, {2}},
        {3, {3}}
    };

    // Создание очередей
    std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> input_queues;
    std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> output_queues;

    for (int i = 0; i < 4; ++i) {
        input_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
        output_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
    }

    Stage1Router router(rules, input_queues, output_queues);

    // Заполнение входных очередей
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 100; ++j) {
            Message msg = Message::create(static_cast<uint8_t>(i), 0, static_cast<uint64_t>(j));
            input_queues[i]->try_push(msg);
        }
    }

    std::atomic<bool> running{true};

    for (auto _ : state) {
        // Один проход маршрутизации
        bool processed = false;
        for (auto& input_queue : input_queues) {
            Message msg;
            if (input_queue->try_pop(msg)) {
                msg.stage1_entry_ns = Message::get_timestamp_ns();

                // Простая маршрутизация
                uint8_t proc_id = msg.msg_type % 4;

                msg.stage1_exit_ns = Message::get_timestamp_ns();
                output_queues[proc_id]->try_push(msg);
                processed = true;
                break;
            }
        }
        benchmark::DoNotOptimize(processed);
    }
}
BENCHMARK(BM_RoutingOverhead);

// Бенчмарк: пропускная способность маршрутизации
static void BM_RoutingThroughput(benchmark::State& state) {
    std::vector<Stage1Rule> rules = {
        {0, {0}},
        {1, {1}},
        {2, {2}},
        {3, {3}}
    };

    std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> input_queues;
    std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> output_queues;

    for (int i = 0; i < 4; ++i) {
        input_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
        output_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
    }

    uint64_t messages_routed = 0;

    for (auto _ : state) {
        // Добавление сообщений
        for (size_t i = 0; i < input_queues.size(); ++i) {
            Message msg = Message::create(static_cast<uint8_t>(i), 0, messages_routed);
            input_queues[i]->try_push(msg);
        }

        // Маршрутизация
        for (auto& input_queue : input_queues) {
            Message msg;
            if (input_queue->try_pop(msg)) {
                uint8_t proc_id = msg.msg_type % 4;
                output_queues[proc_id]->try_push(msg);
                ++messages_routed;
            }
        }
    }

    state.SetItemsProcessed(messages_routed);
}
BENCHMARK(BM_RoutingThroughput);

// Бенчмарк: задержка маршрутизации
static void BM_RoutingLatency(benchmark::State& state) {
    std::vector<Stage1Rule> rules = {{0, {0}}};

    std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> input_queues;
    std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> output_queues;

    input_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
    output_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());

    for (auto _ : state) {
        Message msg = Message::create(0, 0, 0);

        auto start = std::chrono::high_resolution_clock::now();

        input_queues[0]->try_push(msg);
        Message out;
        input_queues[0]->try_pop(out);

        out.stage1_entry_ns = Message::get_timestamp_ns();
        out.stage1_exit_ns = Message::get_timestamp_ns();

        output_queues[0]->try_push(out);
        Message final;
        output_queues[0]->try_pop(final);

        auto end = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

        state.SetIterationTime(elapsed.count() / 1e9);
        benchmark::DoNotOptimize(final);
    }
}
BENCHMARK(BM_RoutingLatency)->UseManualTime();

BENCHMARK_MAIN();
