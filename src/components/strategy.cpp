#include "strategy.hpp"
#include "timer.hpp"

Strategy::Strategy(
    uint8_t id,
    const StrategyConfig& config,
    std::shared_ptr<InputQueue> input_queue,
    SystemStatistics& stats
) : id_(id)
  , input_queue_(input_queue)
  , stats_(stats)
  , processing_time_ns_(100) // По умолчанию
{
    // Получение времени обработки для этой стратегии
    auto it = config.processing_times_ns.find(id);
    if (it != config.processing_times_ns.end()) {
        processing_time_ns_ = it->second;
    }
}

void Strategy::process_message(const Message& msg) {
    // Имитация времени обработки (busy-wait)
    if (processing_time_ns_ > 0) {
        Timer::busy_wait_ns(processing_time_ns_);
    }

    // Отслеживание порядка сообщений
    stats_.track_message_order(msg);

    // Запись статистики задержек
    stats_.record_message_latencies(msg);

    // Увеличение счетчика доставленных сообщений
    stats_.messages_delivered.fetch_add(1, std::memory_order_relaxed);
}

void Strategy::run(std::atomic<bool>& running) {
    while (running.load(std::memory_order_relaxed)) {
        Message msg;

        // Попытка получить сообщение из входной очереди
        if (input_queue_->try_pop(msg)) {
            // Обработка сообщения
            process_message(msg);
        } else {
            // Если очередь пустая, минимальная пауза
            __builtin_ia32_pause();
        }
    }
}
