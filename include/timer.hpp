#pragma once

#include <chrono>
#include <thread>

/**
 * Класс для измерения времени с высоким разрешением
 */
class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::duration<double>;

    Timer() : start_time_(Clock::now()) {}

    /**
     * Сброс таймера
     */
    void reset() {
        start_time_ = Clock::now();
    }

    /**
     * Получение времени с момента создания/сброса (секунды)
     */
    double elapsed_seconds() const {
        auto now = Clock::now();
        Duration elapsed = now - start_time_;
        return elapsed.count();
    }

    /**
     * Получение времени с момента создания/сброса (миллисекунды)
     */
    double elapsed_milliseconds() const {
        return elapsed_seconds() * 1000.0;
    }

    /**
     * Получение времени с момента создания/сброса (микросекунды)
     */
    double elapsed_microseconds() const {
        return elapsed_seconds() * 1000000.0;
    }

    /**
     * Получение времени с момента создания/сброса (наносекунды)
     */
    uint64_t elapsed_nanoseconds() const {
        auto now = Clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start_time_);
        return static_cast<uint64_t>(elapsed.count());
    }

    /**
     * Busy-wait delay (активное ожидание) в наносекундах
     * Используется для имитации времени обработки
     */
    static void busy_wait_ns(uint64_t nanoseconds) {
        auto start = Clock::now();
        while (true) {
            auto now = Clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - start);
            if (static_cast<uint64_t>(elapsed.count()) >= nanoseconds) {
                break;
            }
        }
    }

    /**
     * Sleep с высокой точностью
     */
    static void sleep_for_ns(uint64_t nanoseconds) {
        std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
    }

private:
    TimePoint start_time_;
};
