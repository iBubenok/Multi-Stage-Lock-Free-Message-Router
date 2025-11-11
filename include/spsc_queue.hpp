#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>

// Размер cache line для предотвращения false sharing
constexpr size_t CACHE_LINE_SIZE = 64;

/**
 * Lock-free SPSC (Single Producer Single Consumer) очередь
 * Основана на кольцевом буфере с атомарными индексами
 *
 * Особенности:
 * - Без блокировок, использует только атомарные операции
 * - Cache-aligned для избежания false sharing
 * - Поддерживает только POD типы для производительности
 */
template<typename T, size_t Capacity>
class SPSCQueue {
    static_assert((Capacity & (Capacity - 1)) == 0,
                  "Capacity должна быть степенью двойки");
    static_assert(std::is_trivially_copyable_v<T>,
                  "T должен быть trivially copyable");

public:
    SPSCQueue() : head_(0), tail_(0) {}

    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    /**
     * Попытка добавить элемент в очередь (producer side)
     *
     * Memory ordering:
     * - tail_.load: relaxed - producer единственный, кто пишет в tail
     * - head_.load: acquire - синхронизация с consumer's release при pop
     * - tail_.store: release - публикация нового элемента для consumer
     *
     * @param item элемент для добавления
     * @return true если успешно добавлен, false если очередь полная
     */
    bool try_push(const T& item) noexcept {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) & (Capacity - 1);

        // Проверка переполнения: если next_tail догнал head, очередь полная
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false;
        }

        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    /**
     * Попытка извлечь элемент из очереди (consumer side)
     *
     * Memory ordering:
     * - head_.load: relaxed - consumer единственный, кто пишет в head
     * - tail_.load: acquire - получение элемента, опубликованного producer
     * - head_.store: release - освобождение слота для producer
     *
     * @param item ссылка для сохранения извлеченного элемента
     * @return true если успешно извлечен, false если очередь пустая
     */
    bool try_pop(T& item) noexcept {
        const size_t current_head = head_.load(std::memory_order_relaxed);

        // Проверка пустоты: если head догнал tail, очередь пустая
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false;
        }

        item = buffer_[current_head];
        head_.store((current_head + 1) & (Capacity - 1), std::memory_order_release);
        return true;
    }

    /**
     * Проверка, пуста ли очередь
     * Внимание: результат может быть неактуальным в многопоточной среде
     */
    bool empty() const noexcept {
        return head_.load(std::memory_order_acquire) ==
               tail_.load(std::memory_order_acquire);
    }

    /**
     * Приблизительный размер очереди
     * Внимание: результат может быть неточным в многопоточной среде
     */
    size_t size() const noexcept {
        const size_t h = head_.load(std::memory_order_acquire);
        const size_t t = tail_.load(std::memory_order_acquire);
        return (t >= h) ? (t - h) : (Capacity - h + t);
    }

    /**
     * Максимальная вместимость очереди
     */
    static constexpr size_t capacity() noexcept {
        return Capacity - 1; // -1 для различения полной и пустой очереди
    }

private:
    // Выравнивание по cache line для избежания false sharing
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
    alignas(CACHE_LINE_SIZE) T buffer_[Capacity];
};
