#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <new>

constexpr size_t CACHE_LINE = 64;

/**
 * Lock-free MPSC (Multi Producer Single Consumer) очередь
 * Основана на связном списке с атомарными операциями
 *
 * Особенности:
 * - Без блокировок на стороне producer (CAS операции)
 * - Без блокировок на стороне consumer
 * - Поддержка множественных производителей
 * - Cache-aligned узлы для производительности
 */
template<typename T>
class MPSCQueue {
private:
    struct alignas(CACHE_LINE) Node {
        T data;
        std::atomic<Node*> next;

        Node() : next(nullptr) {}
        explicit Node(const T& value) : data(value), next(nullptr) {}
    };

public:
    MPSCQueue() {
        // Создаем фиктивный узел для упрощения логики
        Node* dummy = new Node();
        head_.store(dummy, std::memory_order_relaxed);
        tail_ = dummy;
    }

    ~MPSCQueue() {
        // Очистка всех оставшихся узлов
        T dummy;
        while (try_pop(dummy)) {}
        delete tail_;
    }

    MPSCQueue(const MPSCQueue&) = delete;
    MPSCQueue& operator=(const MPSCQueue&) = delete;

    /**
     * Добавление элемента в очередь (может вызываться из множества потоков)
     * @param item элемент для добавления
     */
    void push(const T& item) {
        Node* new_node = new Node(item);
        Node* prev_head = head_.exchange(new_node, std::memory_order_acq_rel);
        prev_head->next.store(new_node, std::memory_order_release);
    }

    /**
     * Попытка извлечь элемент из очереди (только один поток-consumer)
     * @param item ссылка для сохранения извлеченного элемента
     * @return true если успешно извлечен, false если очередь пустая
     */
    bool try_pop(T& item) {
        Node* tail = tail_;
        Node* next = tail->next.load(std::memory_order_acquire);

        if (next == nullptr) {
            return false; // Очередь пустая
        }

        item = next->data;
        tail_ = next;
        delete tail;
        return true;
    }

    /**
     * Проверка, пуста ли очередь
     * Внимание: результат может быть неактуальным в многопоточной среде
     */
    bool empty() const {
        return tail_->next.load(std::memory_order_acquire) == nullptr;
    }

private:
    // Head используется производителями, выравнивание по cache line
    alignas(CACHE_LINE) std::atomic<Node*> head_;

    // Tail используется только consumer'ом, отдельная cache line
    alignas(CACHE_LINE) Node* tail_;
};
