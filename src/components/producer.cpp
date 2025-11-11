#include "producer.hpp"
#include "timer.hpp"
#include <chrono>
#include <thread>

Producer::Producer(
    uint8_t id,
    const ProducerConfig& config,
    std::shared_ptr<OutputQueue> output_queue,
    SystemStatistics& stats
) : id_(id)
  , messages_per_sec_(config.messages_per_sec)
  , output_queue_(output_queue)
  , stats_(stats)
  , rng_(std::random_device{}())
  , sequence_number_(0)
{
    // Подготовка распределения типов сообщений
    for (const auto& [type, prob] : config.distribution) {
        msg_types_.push_back(type);
        probabilities_.push_back(prob);
    }

    // Создание дискретного распределения
    type_distribution_ = std::discrete_distribution<size_t>(
        probabilities_.begin(),
        probabilities_.end()
    );
}

uint8_t Producer::generate_message_type() {
    size_t index = type_distribution_(rng_);
    return msg_types_[index];
}

void Producer::run(std::atomic<bool>& running, uint32_t duration_secs) {
    // Вычисление интервала между сообщениями (наносекунды)
    const uint64_t interval_ns = 1'000'000'000ULL / messages_per_sec_;

    Timer timer;
    uint64_t next_send_time = 0;
    uint64_t messages_sent = 0;

    while (running.load(std::memory_order_relaxed)) {
        // Проверка времени выполнения
        if (timer.elapsed_seconds() >= duration_secs) {
            break;
        }

        uint64_t current_time = timer.elapsed_nanoseconds();

        // Проверка, пора ли отправлять следующее сообщение
        if (current_time >= next_send_time) {
            // Генерация сообщения
            uint8_t msg_type = generate_message_type();
            Message msg = Message::create(msg_type, id_, sequence_number_++);

            // Попытка отправить в очередь
            while (running.load(std::memory_order_relaxed)) {
                if (output_queue_->try_push(msg)) {
                    stats_.messages_produced.fetch_add(1, std::memory_order_relaxed);
                    messages_sent++;
                    break;
                }

                // Если очередь полная, активно ждем
                __builtin_ia32_pause();
            }

            // Планирование следующей отправки
            next_send_time += interval_ns;

            // Если мы отстали от графика, корректируем
            if (next_send_time < current_time) {
                next_send_time = current_time;
            }
        } else {
            // Активное ожидание с минимальной паузой
            __builtin_ia32_pause();
        }
    }
}
