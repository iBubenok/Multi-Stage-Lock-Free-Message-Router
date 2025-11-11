#include <benchmark/benchmark.h>
#include "message.hpp"
#include "spsc_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>

// Бенчмарк: масштабирование с количеством производителей
static void BM_ProducerScaling(benchmark::State& state) {
    const int num_producers = state.range(0);
    const size_t messages_per_producer = 10000;

    for (auto _ : state) {
        std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> queues;
        std::vector<std::thread> producers;
        std::atomic<uint64_t> total_produced{0};

        // Создание очередей
        for (int i = 0; i < num_producers; ++i) {
            queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
        }

        // Запуск производителей
        for (int i = 0; i < num_producers; ++i) {
            producers.emplace_back([&, i, queue = queues[i]]() {
                for (size_t j = 0; j < messages_per_producer; ++j) {
                    Message msg = Message::create(0, static_cast<uint8_t>(i), j);
                    while (!queue->try_push(msg)) {
                        // Busy wait
                    }
                    total_produced.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        // Потребление сообщений
        uint64_t consumed = 0;
        uint64_t expected = num_producers * messages_per_producer;
        while (consumed < expected) {
            for (auto& queue : queues) {
                Message msg;
                if (queue->try_pop(msg)) {
                    ++consumed;
                }
            }
        }

        // Ожидание завершения производителей
        for (auto& t : producers) {
            t.join();
        }

        benchmark::DoNotOptimize(consumed);
    }

    state.SetItemsProcessed(state.iterations() * num_producers * messages_per_producer);
}
BENCHMARK(BM_ProducerScaling)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();

// Бенчмарк: масштабирование с количеством процессоров
static void BM_ProcessorScaling(benchmark::State& state) {
    const int num_processors = state.range(0);
    const size_t messages_to_process = 10000;

    for (auto _ : state) {
        std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> input_queues;
        std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> output_queues;
        std::vector<std::thread> processors;
        std::atomic<bool> running{true};

        // Создание очередей
        for (int i = 0; i < num_processors; ++i) {
            input_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
            output_queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
        }

        // Заполнение входных очередей
        for (int i = 0; i < num_processors; ++i) {
            for (size_t j = 0; j < messages_to_process / num_processors; ++j) {
                Message msg = Message::create(static_cast<uint8_t>(i), 0, j);
                input_queues[i]->try_push(msg);
            }
        }

        // Запуск процессоров
        for (int i = 0; i < num_processors; ++i) {
            processors.emplace_back([&, i, in = input_queues[i], out = output_queues[i]]() {
                while (running.load(std::memory_order_relaxed)) {
                    Message msg;
                    if (in->try_pop(msg)) {
                        // Имитация обработки
                        msg.processor_id = static_cast<uint8_t>(i);
                        msg.processing_ts_ns = Message::get_timestamp_ns();
                        out->try_push(msg);
                    }
                }
            });
        }

        // Потребление результатов
        uint64_t consumed = 0;
        while (consumed < messages_to_process) {
            for (auto& queue : output_queues) {
                Message msg;
                if (queue->try_pop(msg)) {
                    ++consumed;
                }
            }
        }

        running.store(false, std::memory_order_release);

        // Ожидание завершения процессоров
        for (auto& t : processors) {
            t.join();
        }

        benchmark::DoNotOptimize(consumed);
    }

    state.SetItemsProcessed(state.iterations() * messages_to_process);
}
BENCHMARK(BM_ProcessorScaling)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();

// Бенчмарк: полный pipeline с разным количеством компонентов
static void BM_FullPipelineScaling(benchmark::State& state) {
    const int num_components = state.range(0);
    const size_t messages = 1000;

    for (auto _ : state) {
        std::vector<std::shared_ptr<SPSCQueue<Message, 65536>>> queues;
        std::vector<std::thread> workers;
        std::atomic<bool> running{true};

        // Создание цепочки очередей
        for (int i = 0; i <= num_components; ++i) {
            queues.push_back(std::make_shared<SPSCQueue<Message, 65536>>());
        }

        // Запуск рабочих потоков (передача сообщений по цепочке)
        for (int i = 0; i < num_components; ++i) {
            workers.emplace_back([&, i, in = queues[i], out = queues[i + 1]]() {
                while (running.load(std::memory_order_relaxed)) {
                    Message msg;
                    if (in->try_pop(msg)) {
                        out->try_push(msg);
                    }
                }
            });
        }

        // Отправка сообщений
        for (size_t i = 0; i < messages; ++i) {
            Message msg = Message::create(0, 0, i);
            queues[0]->try_push(msg);
        }

        // Получение результатов
        uint64_t received = 0;
        while (received < messages) {
            Message msg;
            if (queues[num_components]->try_pop(msg)) {
                ++received;
            }
        }

        running.store(false, std::memory_order_release);

        // Ожидание завершения потоков
        for (auto& t : workers) {
            t.join();
        }

        benchmark::DoNotOptimize(received);
    }

    state.SetItemsProcessed(state.iterations() * messages);
}
BENCHMARK(BM_FullPipelineScaling)
    ->Arg(1)
    ->Arg(2)
    ->Arg(4)
    ->Arg(8)
    ->UseRealTime();

BENCHMARK_MAIN();
