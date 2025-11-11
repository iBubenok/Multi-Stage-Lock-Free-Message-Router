#include "router.hpp"
#include <iostream>

// Stage1Router реализация

Stage1Router::Stage1Router(
    const std::vector<Stage1Rule>& rules,
    std::vector<std::shared_ptr<InputQueue>>& input_queues,
    std::vector<std::shared_ptr<OutputQueue>>& output_queues
) : input_queues_(input_queues), output_queues_(output_queues) {
    // Построение таблицы маршрутизации
    for (const auto& rule : rules) {
        routing_table_[rule.msg_type] = rule.processors;
        rr_counters_[rule.msg_type].store(0, std::memory_order_relaxed);
    }
}

uint8_t Stage1Router::select_processor(uint8_t msg_type) {
    auto it = routing_table_.find(msg_type);
    if (it == routing_table_.end() || it->second.empty()) {
        // Если правило не найдено, используем тип сообщения по модулю
        return msg_type % static_cast<uint8_t>(output_queues_.size());
    }

    const auto& processors = it->second;
    if (processors.size() == 1) {
        return processors[0];
    }

    // Round-robin балансировка между несколькими процессорами
    size_t counter = rr_counters_[msg_type].fetch_add(1, std::memory_order_relaxed);
    return processors[counter % processors.size()];
}

void Stage1Router::run(std::atomic<bool>& running) {
    while (running.load(std::memory_order_relaxed)) {
        bool processed_any = false;

        // Обработка сообщений из всех входных очередей
        for (auto& input_queue : input_queues_) {
            Message msg;
            if (input_queue->try_pop(msg)) {
                // Отметка времени входа в Stage1
                msg.stage1_entry_ns = Message::get_timestamp_ns();

                // Выбор процессора
                uint8_t processor_id = select_processor(msg.msg_type);

                // Попытка отправить в выходную очередь
                // ВАЖНО: продолжаем пытаться отправить даже если running==false,
                // чтобы не потерять сообщение, которое уже извлекли из входной очереди
                while (true) {
                    msg.stage1_exit_ns = Message::get_timestamp_ns();

                    if (output_queues_[processor_id]->try_push(msg)) {
                        processed_any = true;
                        break;
                    }

                    // Если очередь полная, активно ждем (busy-wait)
                    // Это минимизирует задержку
                    __builtin_ia32_pause();
                }
            }
        }

        // Если ничего не обработали, делаем небольшую паузу
        // чтобы не нагружать CPU на 100% без пользы
        if (!processed_any) {
            // Минимальная пауза (можно убрать для максимальной производительности)
            __builtin_ia32_pause();
        }
    }
}

// Stage2Router реализация

Stage2Router::Stage2Router(
    const std::vector<Stage2Rule>& rules,
    std::vector<std::shared_ptr<InputQueue>>& input_queues,
    std::vector<std::shared_ptr<OutputQueue>>& output_queues
) : input_queues_(input_queues), output_queues_(output_queues) {
    // Построение таблицы маршрутизации
    for (const auto& rule : rules) {
        routing_table_[rule.msg_type] = rule.strategy;
    }
}

void Stage2Router::run(std::atomic<bool>& running) {
    while (running.load(std::memory_order_relaxed)) {
        bool processed_any = false;

        // Обработка сообщений из всех входных очередей
        for (auto& input_queue : input_queues_) {
            Message msg;
            if (input_queue->try_pop(msg)) {
                // Отметка времени входа в Stage2
                msg.stage2_entry_ns = Message::get_timestamp_ns();

                // Определение стратегии по типу сообщения
                auto it = routing_table_.find(msg.msg_type);
                uint8_t strategy_id = (it != routing_table_.end())
                    ? it->second
                    : (msg.msg_type % static_cast<uint8_t>(output_queues_.size()));

                // Попытка отправить в выходную очередь
                // ВАЖНО: продолжаем пытаться отправить даже если running==false
                while (true) {
                    msg.stage2_exit_ns = Message::get_timestamp_ns();

                    if (output_queues_[strategy_id]->try_push(msg)) {
                        processed_any = true;
                        break;
                    }

                    // Если очередь полная, активно ждем (busy-wait)
                    __builtin_ia32_pause();
                }
            }
        }

        // Если ничего не обработали, минимальная пауза
        if (!processed_any) {
            __builtin_ia32_pause();
        }
    }
}
