#pragma once

#include "message.hpp"
#include <atomic>
#include <vector>
#include <map>
#include <algorithm>
#include <cstdint>
#include <string>
#include <mutex>

/**
 * Структура для хранения статистики задержек
 */
struct LatencyStats {
    std::vector<double> latencies;  // Сохраненные значения задержек

    void add(double latency_us) {
        latencies.push_back(latency_us);
    }

    void clear() {
        latencies.clear();
    }

    // Вычисление перцентилей
    double percentile(double p) const {
        if (latencies.empty()) return 0.0;

        std::vector<double> sorted = latencies;
        std::sort(sorted.begin(), sorted.end());

        size_t index = static_cast<size_t>(p * sorted.size());
        if (index >= sorted.size()) {
            index = sorted.size() - 1;
        }

        return sorted[index];
    }

    double p50() const { return percentile(0.50); }
    double p90() const { return percentile(0.90); }
    double p99() const { return percentile(0.99); }
    double p999() const { return percentile(0.999); }
    double max() const {
        if (latencies.empty()) return 0.0;
        return *std::max_element(latencies.begin(), latencies.end());
    }
};

/**
 * Отслеживание порядка сообщений от конкретного производителя
 */
struct OrderTracker {
    std::map<uint8_t, uint64_t> last_sequence;  // Последний seq_num для каждого типа
    std::atomic<uint64_t> messages_received{0};
    std::atomic<uint64_t> order_violations{0};

    void track(const Message& msg) {
        messages_received.fetch_add(1, std::memory_order_relaxed);

        uint8_t key = msg.msg_type;
        auto it = last_sequence.find(key);

        if (it != last_sequence.end()) {
            // Проверка порядка: новый seq должен быть больше предыдущего
            if (msg.sequence_number <= it->second) {
                order_violations.fetch_add(1, std::memory_order_relaxed);
            }
        }

        last_sequence[key] = msg.sequence_number;
    }

    bool is_ordered() const {
        return order_violations.load(std::memory_order_relaxed) == 0;
    }
};

/**
 * Глобальная статистика системы
 */
class SystemStatistics {
public:
    // Счетчики сообщений
    std::atomic<uint64_t> messages_produced{0};
    std::atomic<uint64_t> messages_processed{0};
    std::atomic<uint64_t> messages_delivered{0};
    std::atomic<uint64_t> messages_lost{0};

    // Глубины очередей (по индексам) - используем unique_ptr чтобы избежать проблем с move
    std::vector<std::unique_ptr<std::atomic<size_t>>> stage1_queue_depths;
    std::vector<std::unique_ptr<std::atomic<size_t>>> stage2_queue_depths;

    // Статистика задержек
    LatencyStats stage1_latencies;
    LatencyStats processing_latencies;
    LatencyStats stage2_latencies;
    LatencyStats total_latencies;

    // Отслеживание порядка для каждого производителя (используем unique_ptr чтобы избежать проблем с move)
    std::vector<std::unique_ptr<OrderTracker>> producer_order_trackers;

    // Мьютекс для защиты latency stats (только для add)
    std::mutex latency_mutex;

    SystemStatistics(size_t num_producers, size_t num_processors, size_t num_strategies) {
        // Используем unique_ptr для атомиков чтобы избежать проблем с move
        for (size_t i = 0; i < num_processors; ++i) {
            stage1_queue_depths.push_back(std::make_unique<std::atomic<size_t>>(0));
        }

        for (size_t i = 0; i < num_strategies; ++i) {
            stage2_queue_depths.push_back(std::make_unique<std::atomic<size_t>>(0));
        }

        for (size_t i = 0; i < num_producers; ++i) {
            producer_order_trackers.push_back(std::make_unique<OrderTracker>());
        }
    }

    /**
     * Добавление информации о задержке из обработанного сообщения
     */
    void record_message_latencies(const Message& msg) {
        std::lock_guard<std::mutex> lock(latency_mutex);

        stage1_latencies.add(msg.stage1_latency_us());
        processing_latencies.add(msg.processing_latency_us());
        stage2_latencies.add(msg.stage2_latency_us());
        total_latencies.add(msg.end_to_end_latency_us());
    }

    /**
     * Отслеживание порядка сообщения
     */
    void track_message_order(const Message& msg) {
        if (msg.producer_id < producer_order_trackers.size()) {
            producer_order_trackers[msg.producer_id]->track(msg);
        }
    }

    /**
     * Вывод текущей статистики (каждую секунду)
     */
    void print_current_stats(double elapsed_secs) const;

    /**
     * Вывод финального отчета
     */
    void print_final_report(const std::string& scenario, double duration_secs) const;

    /**
     * Проверка, все ли сообщения доставлены корректно
     */
    bool validate() const {
        uint64_t produced = messages_produced.load(std::memory_order_relaxed);
        uint64_t delivered = messages_delivered.load(std::memory_order_relaxed);

        // Проверка потерь
        if (produced != delivered) {
            return false;
        }

        // Проверка порядка
        for (const auto& tracker : producer_order_trackers) {
            if (!tracker->is_ordered()) {
                return false;
            }
        }

        return true;
    }

    /**
     * Получение общего количества нарушений порядка
     */
    uint64_t total_order_violations() const {
        uint64_t total = 0;
        for (const auto& tracker : producer_order_trackers) {
            total += tracker->order_violations.load(std::memory_order_relaxed);
        }
        return total;
    }
};
