#pragma once

#include "message.hpp"
#include "config.hpp"
#include "spsc_queue.hpp"
#include <vector>
#include <unordered_map>
#include <atomic>
#include <memory>

// Размер очереди между компонентами
constexpr size_t QUEUE_SIZE = 65536; // Должно быть степенью 2

/**
 * Stage1 Router - маршрутизирует сообщения от производителей к процессорам
 */
class Stage1Router {
public:
    using InputQueue = SPSCQueue<Message, QUEUE_SIZE>;
    using OutputQueue = SPSCQueue<Message, QUEUE_SIZE>;

    Stage1Router(
        const std::vector<Stage1Rule>& rules,
        std::vector<std::shared_ptr<InputQueue>>& input_queues,
        std::vector<std::shared_ptr<OutputQueue>>& output_queues
    );

    /**
     * Основной цикл роутера (запускается в отдельном потоке)
     */
    void run(std::atomic<bool>& running);

private:
    // Правила маршрутизации: msg_type -> список процессоров
    std::unordered_map<uint8_t, std::vector<uint8_t>> routing_table_;

    // Входные очереди от производителей
    std::vector<std::shared_ptr<InputQueue>>& input_queues_;

    // Выходные очереди к процессорам
    std::vector<std::shared_ptr<OutputQueue>>& output_queues_;

    // Счетчик для round-robin балансировки
    std::unordered_map<uint8_t, std::atomic<size_t>> rr_counters_;

    /**
     * Выбор процессора для сообщения (с round-robin балансировкой)
     */
    uint8_t select_processor(uint8_t msg_type);
};

/**
 * Stage2 Router - маршрутизирует обработанные сообщения к стратегиям
 */
class Stage2Router {
public:
    using InputQueue = SPSCQueue<Message, QUEUE_SIZE>;
    using OutputQueue = SPSCQueue<Message, QUEUE_SIZE>;

    Stage2Router(
        const std::vector<Stage2Rule>& rules,
        std::vector<std::shared_ptr<InputQueue>>& input_queues,
        std::vector<std::shared_ptr<OutputQueue>>& output_queues
    );

    /**
     * Основной цикл роутера (запускается в отдельном потоке)
     */
    void run(std::atomic<bool>& running);

private:
    // Правила маршрутизации: msg_type -> strategy_id
    std::unordered_map<uint8_t, uint8_t> routing_table_;

    // Входные очереди от процессоров
    std::vector<std::shared_ptr<InputQueue>>& input_queues_;

    // Выходные очереди к стратегиям
    std::vector<std::shared_ptr<OutputQueue>>& output_queues_;
};
