# Архитектура Message Router

## Обзор

Message Router - это высокопроизводительная система маршрутизации сообщений без блокировок, построенная на принципах lock-free программирования. Система обеспечивает обработку миллионов сообщений в секунду с микросекундными задержками.

## Поток данных

```
┌──────────┐     ┌────────┐     ┌──────────┐     ┌────────┐     ┌──────────┐
│Producer 0│────►│        │────►│Processor0│────►│        │────►│Strategy 0│
├──────────┤     │Stage1  │     ├──────────┤     │Stage2  │     ├──────────┤
│Producer 1│────►│Router  │────►│Processor1│────►│Router  │────►│Strategy 1│
├──────────┤     │        │     ├──────────┤     │        │     ├──────────┤
│Producer 2│────►│        │────►│Processor2│────►│        │────►│Strategy 2│
├──────────┤     │        │     ├──────────┤     │        │     └──────────┘
│Producer 3│────►│        │────►│Processor3│────►│        │
└──────────┘     └────────┘     └──────────┘     └────────┘
```

## Компоненты системы

### 1. Producer (Производитель)

**Назначение**: Генерация сообщений с заданной скоростью и распределением типов.

**Ключевые характеристики**:
- Каждый producer работает в отдельном потоке
- Генерирует сообщения согласно конфигурируемому распределению типов
- Поддерживает монотонно возрастающие sequence numbers
- Контролирует скорость отправки через временные интервалы

**Реализация**:
```cpp
class Producer {
    void run(std::atomic<bool>& running, uint32_t duration_secs) {
        const uint64_t interval_ns = 1'000'000'000ULL / messages_per_sec_;

        while (running) {
            if (current_time >= next_send_time) {
                Message msg = generate_message();
                output_queue_->try_push(msg);
                next_send_time += interval_ns;
            }
        }
    }
};
```

**Гарантии**:
- Sequence numbers монотонно возрастают
- Типы сообщений генерируются согласно вероятностному распределению
- Поддерживается заданная скорость генерации

### 2. Stage1 Router (Роутер первого уровня)

**Назначение**: Маршрутизация сообщений от производителей к процессорам на основе типа сообщения.

**Ключевые характеристики**:
- Работает в отдельном потоке
- Читает из множества входных очередей (MPMC pattern)
- Записывает в множество выходных очередей
- Поддерживает round-robin балансировку

**Алгоритм маршрутизации**:
1. Получить сообщение из любой входной очереди
2. Определить тип сообщения
3. Найти целевой процессор по таблице маршрутизации
4. Если указано несколько процессоров, использовать round-robin
5. Отправить в выходную очередь процессора

**Особенности**:
- Busy-waiting для минимальной задержки
- Отметка времени входа и выхода для измерения latency
- Lock-free операции

### 3. Processor (Процессор)

**Назначение**: Обработка сообщений с имитацией реальной работы.

**Ключевые характеристики**:
- Каждый processor работает в отдельном потоке
- Имитирует время обработки через busy-waiting
- Добавляет метаданные обработки к сообщению

**Процесс обработки**:
1. Получить сообщение из входной очереди
2. Отметить время начала обработки
3. Busy-wait в течение заданного времени (имитация работы)
4. Отметить время завершения обработки
5. Добавить processor_id к сообщению
6. Отправить в выходную очередь

**Конфигурация времени обработки**:
- Задается отдельно для каждого типа сообщения
- Позволяет имитировать разную сложность обработки

### 4. Stage2 Router (Роутер второго уровня)

**Назначение**: Маршрутизация обработанных сообщений к стратегиям.

**Ключевые характеристики**:
- Аналогичен Stage1 Router
- Маршрутизирует на основе типа сообщения
- Критически важен для сохранения порядка

**Гарантия порядка**:
- Сообщения одного типа от одного производителя всегда идут к одной стратегии
- Порядок сохраняется благодаря FIFO свойствам SPSC очередей

### 5. Strategy (Стратегия)

**Назначение**: Финальный потребитель сообщений, проверяющий корректность.

**Ключевые характеристики**:
- Каждая strategy работает в отдельном потоке
- Проверяет порядок sequence numbers
- Собирает статистику задержек
- Имитирует финальную обработку

**Проверка порядка**:
```cpp
void track(const Message& msg) {
    auto it = last_sequence.find(msg.msg_type);
    if (it != last_sequence.end()) {
        if (msg.sequence_number <= it->second) {
            order_violations++;  // Нарушение!
        }
    }
    last_sequence[msg.msg_type] = msg.sequence_number;
}
```

## Lock-Free структуры данных

### SPSC Queue (Single Producer Single Consumer)

**Принцип работы**:
- Кольцевой буфер фиксированного размера (степень двойки)
- Два атомарных индекса: head (consumer) и tail (producer)
- Использование bitwise AND для циклического индексирования

**Memory ordering**:
```cpp
bool try_push(const T& item) {
    const size_t current_tail = tail_.load(std::memory_order_relaxed);
    const size_t next_tail = (current_tail + 1) & (Capacity - 1);

    if (next_tail == head_.load(std::memory_order_acquire)) {
        return false; // Полная
    }

    buffer_[current_tail] = item;
    tail_.store(next_tail, std::memory_order_release);
    return true;
}
```

**Cache-line alignment**:
```cpp
alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
alignas(CACHE_LINE_SIZE) T buffer_[Capacity];
```

Это предотвращает **false sharing** между producer и consumer.

### Message структура

**Дизайн**:
```cpp
struct Message {
    // Поля производителя (32 bytes)
    uint8_t msg_type;
    uint8_t producer_id;
    uint64_t sequence_number;
    uint64_t timestamp_ns;

    // Поля обработки (16 bytes)
    uint8_t processor_id;
    uint64_t processing_ts_ns;

    // Поля трекинга (48 bytes)
    uint64_t stage1_entry_ns;
    uint64_t stage1_exit_ns;
    uint64_t processing_entry_ns;
    uint64_t processing_exit_ns;
    uint64_t stage2_entry_ns;
    uint64_t stage2_exit_ns;
};

static_assert(std::is_trivially_copyable_v<Message>);
```

**Размер**: ~96 bytes (поместится в 2 cache lines)

## Обработка backpressure

### Проблема
Что происходит, если processor медленнее, чем producer?

### Решение
1. **Bounded queues**: Фиксированный размер очередей
2. **Busy-waiting**: Producer ждет, если очередь полная
3. **Flow control**: Естественное замедление производства

```cpp
while (running) {
    if (output_queue_->try_push(msg)) {
        break;  // Успешно отправлено
    }
    // Busy-wait: очередь полная, ждем освобождения
    __builtin_ia32_pause();
}
```

**Последствия**:
- CPU usage = 100% даже при блокировке
- Минимальная задержка при освобождении места
- Автоматическая адаптация скорости производства

## Гарантии порядка

### Требование
Сообщения от одного producer с одним типом должны приходить в порядке.

### Реализация

1. **Уровень Producer**:
   - Sequence numbers монотонно возрастают
   - Сообщения отправляются в порядке генерации

2. **Уровень SPSC Queue**:
   - FIFO гарантия по дизайну
   - Первым пришел = первым вышел

3. **Уровень Router**:
   - Детерминированная маршрутизация: type → processor
   - Один тип всегда идет к одному processor (для ordering_required)

4. **Уровень Strategy**:
   - Проверка sequence numbers
   - Детектирование нарушений

### Потенциальные проблемы

❌ **Проблема**: Если тип-0 может идти к processor-0 ИЛИ processor-1, порядок может нарушиться.

✅ **Решение**: Один тип сообщения → один процессор (для ordering_required типов).

## Измерение производительности

### Latency трекинг

Каждое сообщение отслеживает время прохождения:

```
timestamp_ns          → Создание producer'ом
stage1_entry_ns       → Вход в Stage1 Router
stage1_exit_ns        → Выход из Stage1 Router
processing_entry_ns   → Вход в Processor
processing_exit_ns    → Выход из Processor
stage2_entry_ns       → Вход в Stage2 Router
stage2_exit_ns        → Выход из Stage2 Router (финал)
```

### Статистика

- **Throughput**: messages_delivered / duration
- **Latency percentiles**: p50, p90, p99, p99.9, max
- **Queue depths**: Мониторинг глубины очередей каждую секунду
- **Order violations**: Счетчик нарушений для каждого producer

## Memory layout оптимизации

### 1. Cache-line alignment
```cpp
constexpr size_t CACHE_LINE_SIZE = 64;

alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_;
alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_;
```

### 2. Data locality
- Буфер очереди непрерывен в памяти
- Высокая вероятность cache hits

### 3. Memory ordering
- `relaxed`: Когда не нужна синхронизация
- `acquire/release`: Для правильной видимости изменений
- `seq_cst`: НЕ используется (слишком дорого)

## Масштабирование

### Горизонтальное
- Добавление производителей → линейное увеличение throughput
- Добавление процессоров → распределение нагрузки

### Ограничения
- Memory bandwidth
- Cache coherency overhead
- Количество ядер CPU

### Рекомендации
- 1 producer на физическое ядро
- 1 processor на физическое ядро
- Роутеры на отдельных ядрах

## Trade-offs

### Busy-waiting vs Sleep
- ✅ Busy-waiting: Минимальная latency, 100% CPU
- ❌ Sleep: Экономия CPU, увеличение latency

**Выбор**: Busy-waiting (требование по latency)

### Fixed size queues vs Dynamic
- ✅ Fixed: Предсказуемая производительность, zero allocations
- ❌ Dynamic: Гибкость, больше overhead

**Выбор**: Fixed size (производительность критична)

### Atomic operations cost
- Чтение: ~4-10 циклов CPU
- CAS: ~20-50 циклов CPU
- При конкуренции: может быть значительно дороже

**Оптимизация**: Минимизация конкуренции через SPSC очереди

## Тестирование корректности

### Unit тесты (не реализованы, но рекомендуется)
- Тест SPSC очереди: push/pop, full/empty
- Тест маршрутизации: корректность направления
- Тест порядка: sequence numbers

### Integration тесты
- 6 тестовых сценариев
- Проверка throughput, latency, ordering
- Стресс-тесты различных паттернов нагрузки

### Benchmarks
- Queue performance
- Routing overhead
- Memory allocation patterns
- Scaling characteristics

## Будущие улучшения

1. **Adaptive batching**: Группировка сообщений для амортизации overhead
2. **NUMA awareness**: Привязка потоков к NUMA нодам
3. **Hardware monitoring**: Использование CPU performance counters
4. **Async I/O**: Если потребуется сохранение результатов
5. **Zero-copy networking**: Интеграция с DPDK/io_uring

## Заключение

Архитектура Message Router демонстрирует применение принципов lock-free программирования для достижения высокой производительности и низких задержек. Ключевые аспекты:

- **Простота**: Четкое разделение компонентов
- **Корректность**: Гарантии порядка и zero message loss
- **Производительность**: Оптимизация на каждом уровне
- **Измеримость**: Подробная статистика и бенчмарки
