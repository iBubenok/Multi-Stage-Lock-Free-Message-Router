#include "config.hpp"
#include "message.hpp"
#include "statistics.hpp"
#include "producer.hpp"
#include "processor.hpp"
#include "strategy.hpp"
#include "router.hpp"
#include "timer.hpp"

#include <iostream>
#include <vector>
#include <thread>
#include <memory>
#include <atomic>
#include <csignal>
#include <cstdlib>

// Глобальный флаг для остановки системы
std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        std::cout << "\n Получен сигнал завершения. Остановка системы..." << std::endl;
        g_running.store(false, std::memory_order_release);
    }
}

int main(int argc, char* argv[]) {
    // Установка обработчика сигналов
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Проверка аргументов
    if (argc < 2) {
        std::cerr << "Использование: " << argv[0] << " <config.json>" << std::endl;
        return 1;
    }

    std::string config_file = argv[1];

    try {
        // Загрузка конфигурации
        std::cout << "Загрузка конфигурации из: " << config_file << std::endl;
        SystemConfig config = SystemConfig::load_from_file(config_file);
        std::cout << "Конфигурация загружена успешно" << std::endl;
        std::cout << "Сценарий: " << config.scenario << std::endl;
        std::cout << "Длительность: " << config.duration_secs << " секунд" << std::endl;
        std::cout << std::endl;

        // Инициализация статистики
        SystemStatistics stats(
            config.producers.count,
            config.processors.count,
            config.strategies.count
        );

        // ========== Создание очередей ==========

        // Очереди от производителей к Stage1 Router
        std::vector<std::shared_ptr<SPSCQueue<Message, PRODUCER_QUEUE_SIZE>>> producer_queues;
        for (size_t i = 0; i < config.producers.count; ++i) {
            producer_queues.push_back(
                std::make_shared<SPSCQueue<Message, PRODUCER_QUEUE_SIZE>>()
            );
        }

        // Очереди от Stage1 Router к процессорам
        std::vector<std::shared_ptr<SPSCQueue<Message, PROCESSOR_QUEUE_SIZE>>> stage1_to_processor_queues;
        for (size_t i = 0; i < config.processors.count; ++i) {
            stage1_to_processor_queues.push_back(
                std::make_shared<SPSCQueue<Message, PROCESSOR_QUEUE_SIZE>>()
            );
        }

        // Очереди от процессоров к Stage2 Router
        std::vector<std::shared_ptr<SPSCQueue<Message, PROCESSOR_QUEUE_SIZE>>> processor_to_stage2_queues;
        for (size_t i = 0; i < config.processors.count; ++i) {
            processor_to_stage2_queues.push_back(
                std::make_shared<SPSCQueue<Message, PROCESSOR_QUEUE_SIZE>>()
            );
        }

        // Очереди от Stage2 Router к стратегиям
        std::vector<std::shared_ptr<SPSCQueue<Message, STRATEGY_QUEUE_SIZE>>> stage2_to_strategy_queues;
        for (size_t i = 0; i < config.strategies.count; ++i) {
            stage2_to_strategy_queues.push_back(
                std::make_shared<SPSCQueue<Message, STRATEGY_QUEUE_SIZE>>()
            );
        }

        // ========== Создание компонентов ==========

        // Производители
        std::vector<std::unique_ptr<Producer>> producers;
        for (size_t i = 0; i < config.producers.count; ++i) {
            producers.push_back(std::make_unique<Producer>(
                static_cast<uint8_t>(i),
                config.producers,
                producer_queues[i],
                stats
            ));
        }

        // Процессоры
        std::vector<std::unique_ptr<Processor>> processors;
        for (size_t i = 0; i < config.processors.count; ++i) {
            processors.push_back(std::make_unique<Processor>(
                static_cast<uint8_t>(i),
                config.processors,
                stage1_to_processor_queues[i],
                processor_to_stage2_queues[i],
                stats
            ));
        }

        // Стратегии
        std::vector<std::unique_ptr<Strategy>> strategies;
        for (size_t i = 0; i < config.strategies.count; ++i) {
            strategies.push_back(std::make_unique<Strategy>(
                static_cast<uint8_t>(i),
                config.strategies,
                stage2_to_strategy_queues[i],
                stats
            ));
        }

        // Роутеры
        Stage1Router stage1_router(
            config.stage1_rules,
            producer_queues,
            stage1_to_processor_queues
        );

        Stage2Router stage2_router(
            config.stage2_rules,
            processor_to_stage2_queues,
            stage2_to_strategy_queues
        );

        // ========== Запуск потоков ==========

        std::cout << "Запуск системы..." << std::endl;
        std::cout << "  Producers: " << config.producers.count << std::endl;
        std::cout << "  Processors: " << config.processors.count << std::endl;
        std::cout << "  Strategies: " << config.strategies.count << std::endl;
        std::cout << std::endl;

        std::vector<std::thread> threads;

        // Запуск производителей
        for (auto& producer : producers) {
            threads.emplace_back([&producer, &g_running, duration = config.duration_secs]() {
                producer->run(g_running, duration);
            });
        }

        // Запуск Stage1 Router
        threads.emplace_back([&stage1_router, &g_running]() {
            stage1_router.run(g_running);
        });

        // Запуск процессоров
        for (auto& processor : processors) {
            threads.emplace_back([&processor, &g_running]() {
                processor->run(g_running);
            });
        }

        // Запуск Stage2 Router
        threads.emplace_back([&stage2_router, &g_running]() {
            stage2_router.run(g_running);
        });

        // Запуск стратегий
        for (auto& strategy : strategies) {
            threads.emplace_back([&strategy, &g_running]() {
                strategy->run(g_running);
            });
        }

        // ========== Мониторинг ==========

        Timer global_timer;
        uint32_t seconds_elapsed = 0;

        while (g_running.load(std::memory_order_relaxed) &&
               seconds_elapsed < config.duration_secs) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            seconds_elapsed++;

            // Обновление глубин очередей
            for (size_t i = 0; i < stage1_to_processor_queues.size(); ++i) {
                stats.stage1_queue_depths[i]->store(
                    stage1_to_processor_queues[i]->size(),
                    std::memory_order_relaxed
                );
            }
            for (size_t i = 0; i < stage2_to_strategy_queues.size(); ++i) {
                stats.stage2_queue_depths[i]->store(
                    stage2_to_strategy_queues[i]->size(),
                    std::memory_order_relaxed
                );
            }

            // Вывод текущей статистики
            stats.print_current_stats(global_timer.elapsed_seconds());
        }

        // Остановка системы
        std::cout << "\nОстановка системы..." << std::endl;
        g_running.store(false, std::memory_order_release);

        // Ожидание завершения всех потоков
        for (auto& thread : threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        double final_duration = global_timer.elapsed_seconds();

        // ========== Финальный отчет ==========

        std::cout << "\nОжидание обработки оставшихся сообщений..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        stats.print_final_report(config.scenario, final_duration);

        return stats.validate() ? 0 : 1;

    } catch (const std::exception& e) {
        std::cerr << "Ошибка: " << e.what() << std::endl;
        return 1;
    }
}
