#pragma once

#include "message.hpp"
#include "config.hpp"
#include "spsc_queue.hpp"
#include "statistics.hpp"
#include <atomic>
#include <memory>
#include <unordered_map>

constexpr size_t PROCESSOR_QUEUE_SIZE = 65536;

/**
 * Processor - обрабатывает сообщения с имитацией времени обработки
 */
class Processor {
public:
    using InputQueue = SPSCQueue<Message, PROCESSOR_QUEUE_SIZE>;
    using OutputQueue = SPSCQueue<Message, PROCESSOR_QUEUE_SIZE>;

    Processor(
        uint8_t id,
        const ProcessorConfig& config,
        std::shared_ptr<InputQueue> input_queue,
        std::shared_ptr<OutputQueue> output_queue,
        SystemStatistics& stats
    );

    /**
     * Основной цикл процессора (запускается в отдельном потоке)
     */
    void run(std::atomic<bool>& running);

private:
    uint8_t id_;                        // ID процессора
    std::shared_ptr<InputQueue> input_queue_;
    std::shared_ptr<OutputQueue> output_queue_;
    SystemStatistics& stats_;

    // Время обработки по типам сообщений (наносекунды)
    std::unordered_map<uint8_t, uint64_t> processing_times_;

    /**
     * Получение времени обработки для типа сообщения
     */
    uint64_t get_processing_time(uint8_t msg_type) const;
};
