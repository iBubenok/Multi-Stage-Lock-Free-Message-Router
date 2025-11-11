#pragma once

#include <cstdint>
#include <chrono>

/**
 * Структура сообщения в системе
 * Содержит всю информацию о сообщении на всех этапах обработки
 */
struct Message {
    // Основные поля (устанавливаются Producer'ом)
    uint8_t msg_type;           // Тип сообщения (0-7)
    uint8_t producer_id;        // ID производителя
    uint64_t sequence_number;   // Порядковый номер от производителя
    uint64_t timestamp_ns;      // Временная метка создания (наносекунды)

    // Поля обработки (устанавливаются Processor'ом)
    uint8_t processor_id;       // ID процессора
    uint64_t processing_ts_ns;  // Временная метка обработки (наносекунды)

    // Поля для отслеживания времени прохождения
    uint64_t stage1_entry_ns;   // Время входа в Stage1 Router
    uint64_t stage1_exit_ns;    // Время выхода из Stage1 Router
    uint64_t processing_entry_ns; // Время входа в Processor
    uint64_t processing_exit_ns;  // Время выхода из Processor
    uint64_t stage2_entry_ns;   // Время входа в Stage2 Router
    uint64_t stage2_exit_ns;    // Время выхода из Stage2 Router

    Message()
        : msg_type(0)
        , producer_id(0)
        , sequence_number(0)
        , timestamp_ns(0)
        , processor_id(0)
        , processing_ts_ns(0)
        , stage1_entry_ns(0)
        , stage1_exit_ns(0)
        , processing_entry_ns(0)
        , processing_exit_ns(0)
        , stage2_entry_ns(0)
        , stage2_exit_ns(0)
    {}

    /**
     * Создание нового сообщения
     */
    static Message create(uint8_t type, uint8_t producer_id, uint64_t seq_num) {
        Message msg;
        msg.msg_type = type;
        msg.producer_id = producer_id;
        msg.sequence_number = seq_num;
        msg.timestamp_ns = get_timestamp_ns();
        return msg;
    }

    /**
     * Получение текущего времени в наносекундах
     */
    static uint64_t get_timestamp_ns() {
        return static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::high_resolution_clock::now().time_since_epoch()
            ).count()
        );
    }

    /**
     * Задержка от создания до финальной обработки (микросекунды)
     */
    double end_to_end_latency_us() const {
        if (stage2_exit_ns > timestamp_ns) {
            return static_cast<double>(stage2_exit_ns - timestamp_ns) / 1000.0;
        }
        return 0.0;
    }

    /**
     * Задержка в Stage1 Router (микросекунды)
     */
    double stage1_latency_us() const {
        if (stage1_exit_ns > stage1_entry_ns) {
            return static_cast<double>(stage1_exit_ns - stage1_entry_ns) / 1000.0;
        }
        return 0.0;
    }

    /**
     * Задержка обработки (микросекунды)
     */
    double processing_latency_us() const {
        if (processing_exit_ns > processing_entry_ns) {
            return static_cast<double>(processing_exit_ns - processing_entry_ns) / 1000.0;
        }
        return 0.0;
    }

    /**
     * Задержка в Stage2 Router (микросекунды)
     */
    double stage2_latency_us() const {
        if (stage2_exit_ns > stage2_entry_ns) {
            return static_cast<double>(stage2_exit_ns - stage2_entry_ns) / 1000.0;
        }
        return 0.0;
    }
};

// Проверка, что Message является trivially copyable для использования в lock-free очередях
static_assert(std::is_trivially_copyable_v<Message>,
              "Message должен быть trivially copyable");
