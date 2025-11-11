#pragma once

#include "message.hpp"
#include "config.hpp"
#include "spsc_queue.hpp"
#include "statistics.hpp"
#include <atomic>
#include <memory>
#include <unordered_map>

constexpr size_t STRATEGY_QUEUE_SIZE = 65536;

/**
 * Strategy - финальный получатель сообщений, проверяет порядок
 */
class Strategy {
public:
    using InputQueue = SPSCQueue<Message, STRATEGY_QUEUE_SIZE>;

    Strategy(
        uint8_t id,
        const StrategyConfig& config,
        std::shared_ptr<InputQueue> input_queue,
        SystemStatistics& stats
    );

    /**
     * Основной цикл стратегии (запускается в отдельном потоке)
     */
    void run(std::atomic<bool>& running);

private:
    uint8_t id_;                        // ID стратегии
    std::shared_ptr<InputQueue> input_queue_;
    SystemStatistics& stats_;

    // Время обработки (наносекунды)
    uint64_t processing_time_ns_;

    /**
     * Обработка полученного сообщения
     */
    void process_message(const Message& msg);
};
