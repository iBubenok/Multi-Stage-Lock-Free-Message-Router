#include "statistics.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>

// Форматирование чисел с разделителями тысяч
std::string format_number(uint64_t num) {
    std::stringstream ss;
    ss.imbue(std::locale(""));
    ss << std::fixed << num;
    return ss.str();
}

void SystemStatistics::print_current_stats(double elapsed_secs) const {
    uint64_t produced = messages_produced.load(std::memory_order_relaxed);
    uint64_t processed = messages_processed.load(std::memory_order_relaxed);
    uint64_t delivered = messages_delivered.load(std::memory_order_relaxed);
    uint64_t lost = messages_lost.load(std::memory_order_relaxed);

    // Преобразование в миллионы
    double prod_m = static_cast<double>(produced) / 1e6;
    double proc_m = static_cast<double>(processed) / 1e6;
    double del_m = static_cast<double>(delivered) / 1e6;

    std::cout << "[" << std::fixed << std::setprecision(2) << elapsed_secs << "s] "
              << "Произведено: " << std::setprecision(2) << prod_m << "M | "
              << "Обработано: " << proc_m << "M | "
              << "Доставлено: " << del_m << "M | "
              << "Потеряно: " << lost << std::endl;

    // Глубины очередей Stage1
    std::cout << "        Stage1 Queues: [";
    for (size_t i = 0; i < stage1_queue_depths.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << stage1_queue_depths[i]->load(std::memory_order_relaxed);
    }
    std::cout << "]";

    // Глубины очередей Stage2
    std::cout << " | Stage2 Queues: [";
    for (size_t i = 0; i < stage2_queue_depths.size(); ++i) {
        if (i > 0) std::cout << ", ";
        std::cout << stage2_queue_depths[i]->load(std::memory_order_relaxed);
    }
    std::cout << "]" << std::endl;

    // Задержки (если есть данные)
    if (!total_latencies.latencies.empty()) {
        std::cout << "        Задержки(μs) - "
                  << "Stage1: " << std::setprecision(2) << stage1_latencies.p50() << " | "
                  << "Processing: " << processing_latencies.p50() << " | "
                  << "Stage2: " << stage2_latencies.p50() << " | "
                  << "Total: " << total_latencies.p50() << std::endl;
    }
}

void SystemStatistics::print_final_report(const std::string& scenario, double duration_secs) const {
    std::cout << "\n=== ИТОГОВЫЙ ОТЧЕТ ===" << std::endl;
    std::cout << "Сценарий: " << scenario << std::endl;
    std::cout << "Длительность: " << std::fixed << std::setprecision(2)
              << duration_secs << " секунд" << std::endl;
    std::cout << std::endl;

    // Статистика сообщений
    uint64_t produced = messages_produced.load(std::memory_order_relaxed);
    uint64_t processed = messages_processed.load(std::memory_order_relaxed);
    uint64_t delivered = messages_delivered.load(std::memory_order_relaxed);
    uint64_t lost = messages_lost.load(std::memory_order_relaxed);

    std::cout << "Статистика сообщений:" << std::endl;
    std::cout << "  Всего произведено:  " << std::setw(15) << format_number(produced) << std::endl;
    std::cout << "  Всего обработано:   " << std::setw(15) << format_number(processed) << std::endl;
    std::cout << "  Всего доставлено:   " << std::setw(15) << format_number(delivered) << std::endl;
    std::cout << "  Потеряно:           " << std::setw(15) << format_number(lost) << std::endl;
    std::cout << std::endl;

    // Пропускная способность
    double throughput = static_cast<double>(delivered) / duration_secs / 1e6;
    std::cout << "Пропускная способность: " << std::fixed << std::setprecision(2)
              << throughput << " миллионов сообщений/сек" << std::endl;
    std::cout << std::endl;

    // Перцентили задержек
    if (!total_latencies.latencies.empty()) {
        std::cout << "Перцентили задержек (микросекунды):" << std::endl;
        std::cout << "  Этап        p50     p90     p99    p99.9   max" << std::endl;

        auto print_latency_row = [](const std::string& name, const LatencyStats& stats) {
            std::cout << "  " << std::setw(10) << std::left << name
                      << std::right << std::fixed << std::setprecision(2)
                      << std::setw(7) << stats.p50()
                      << std::setw(8) << stats.p90()
                      << std::setw(8) << stats.p99()
                      << std::setw(8) << stats.p999()
                      << std::setw(8) << stats.max()
                      << std::endl;
        };

        print_latency_row("Stage1", stage1_latencies);
        print_latency_row("Process", processing_latencies);
        print_latency_row("Stage2", stage2_latencies);
        print_latency_row("Total", total_latencies);
        std::cout << std::endl;
    }

    // Проверка порядка
    std::cout << "Проверка порядка сообщений:" << std::endl;
    for (size_t i = 0; i < producer_order_trackers.size(); ++i) {
        const auto& tracker = producer_order_trackers[i];
        uint64_t received = tracker->messages_received.load(std::memory_order_relaxed);
        uint64_t violations = tracker->order_violations.load(std::memory_order_relaxed);

        std::cout << "  Producer " << i << ": "
                  << format_number(received) << " сообщений - ";

        if (violations == 0) {
            std::cout << "ПОРЯДОК СОБЛЮДЕН ✓" << std::endl;
        } else {
            std::cout << "НАРУШЕНИЯ: " << violations << " ✗" << std::endl;
        }
    }
    std::cout << std::endl;

    // Результат теста
    bool passed = validate();
    std::cout << "Результат теста: " << (passed ? "PASSED ✓" : "FAILED ✗") << std::endl;
    std::cout << std::endl;
}
