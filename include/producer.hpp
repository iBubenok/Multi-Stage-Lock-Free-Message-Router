#pragma once

#include "message.hpp"
#include "config.hpp"
#include "spsc_queue.hpp"
#include "statistics.hpp"
#include <atomic>
#include <memory>
#include <random>

constexpr size_t PRODUCER_QUEUE_SIZE = 65536;

/**
 * Producer - генерирует сообщения с заданной скоростью
 */
class Producer {
public:
    using OutputQueue = SPSCQueue<Message, PRODUCER_QUEUE_SIZE>;

    Producer(
        uint8_t id,
        const ProducerConfig& config,
        std::shared_ptr<OutputQueue> output_queue,
        SystemStatistics& stats
    );

    /**
     * Основной цикл производителя (запускается в отдельном потоке)
     */
    void run(std::atomic<bool>& running, uint32_t duration_secs);

private:
    uint8_t id_;                        // ID производителя
    uint64_t messages_per_sec_;         // Целевая скорость генерации
    std::shared_ptr<OutputQueue> output_queue_;
    SystemStatistics& stats_;

    // Распределение типов сообщений
    std::vector<uint8_t> msg_types_;
    std::vector<double> probabilities_;

    // Генератор случайных чисел
    std::mt19937 rng_;
    std::discrete_distribution<size_t> type_distribution_;

    // Счетчик последовательности
    uint64_t sequence_number_;

    /**
     * Генерация случайного типа сообщения согласно распределению
     */
    uint8_t generate_message_type();
};
