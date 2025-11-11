#include "processor.hpp"
#include "timer.hpp"

Processor::Processor(
    uint8_t id,
    const ProcessorConfig& config,
    std::shared_ptr<InputQueue> input_queue,
    std::shared_ptr<OutputQueue> output_queue,
    SystemStatistics& stats
) : id_(id)
  , input_queue_(input_queue)
  , output_queue_(output_queue)
  , stats_(stats)
  , processing_times_(config.processing_times_ns)
{
}

uint64_t Processor::get_processing_time(uint8_t msg_type) const {
    auto it = processing_times_.find(msg_type);
    if (it != processing_times_.end()) {
        return it->second;
    }
    // По умолчанию 100 наносекунд
    return 100;
}

void Processor::run(std::atomic<bool>& running) {
    while (running.load(std::memory_order_relaxed)) {
        Message msg;

        // Попытка получить сообщение из входной очереди
        if (input_queue_->try_pop(msg)) {
            // Отметка времени входа в обработку
            msg.processing_entry_ns = Message::get_timestamp_ns();

            // Установка ID процессора
            msg.processor_id = id_;

            // Имитация времени обработки (busy-wait)
            uint64_t processing_time = get_processing_time(msg.msg_type);
            if (processing_time > 0) {
                Timer::busy_wait_ns(processing_time);
            }

            // Отметка времени завершения обработки
            msg.processing_exit_ns = Message::get_timestamp_ns();
            msg.processing_ts_ns = msg.processing_exit_ns;

            // Попытка отправить в выходную очередь
            while (running.load(std::memory_order_relaxed)) {
                if (output_queue_->try_push(msg)) {
                    stats_.messages_processed.fetch_add(1, std::memory_order_relaxed);
                    break;
                }

                // Если очередь полная, активно ждем
                __builtin_ia32_pause();
            }
        } else {
            // Если очередь пустая, минимальная пауза
            __builtin_ia32_pause();
        }
    }
}
