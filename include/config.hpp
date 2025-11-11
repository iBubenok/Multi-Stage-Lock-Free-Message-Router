#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

/**
 * Конфигурация производителей
 */
struct ProducerConfig {
    uint32_t count;                             // Количество производителей
    uint64_t messages_per_sec;                  // Сообщений в секунду на производителя
    std::unordered_map<uint8_t, double> distribution; // Распределение типов сообщений
};

/**
 * Конфигурация процессоров
 */
struct ProcessorConfig {
    uint32_t count;                             // Количество процессоров
    std::unordered_map<uint8_t, uint64_t> processing_times_ns; // Время обработки по типам
};

/**
 * Конфигурация стратегий
 */
struct StrategyConfig {
    uint32_t count;                             // Количество стратегий
    std::unordered_map<uint8_t, uint64_t> processing_times_ns; // Время обработки по стратегиям
};

/**
 * Правило маршрутизации Stage1
 */
struct Stage1Rule {
    uint8_t msg_type;                          // Тип сообщения
    std::vector<uint8_t> processors;           // Список процессоров (для балансировки)
};

/**
 * Правило маршрутизации Stage2
 */
struct Stage2Rule {
    uint8_t msg_type;                          // Тип сообщения
    uint8_t strategy;                          // ID стратегии
    bool ordering_required;                    // Требуется ли сохранение порядка
};

/**
 * Полная конфигурация системы
 */
struct SystemConfig {
    std::string scenario;                      // Название сценария
    uint32_t duration_secs;                    // Длительность теста (секунды)

    ProducerConfig producers;                  // Конфигурация производителей
    ProcessorConfig processors;                // Конфигурация процессоров
    StrategyConfig strategies;                 // Конфигурация стратегий

    std::vector<Stage1Rule> stage1_rules;      // Правила маршрутизации Stage1
    std::vector<Stage2Rule> stage2_rules;      // Правила маршрутизации Stage2

    /**
     * Загрузка конфигурации из JSON файла
     * @param filename путь к файлу конфигурации
     * @return загруженная конфигурация
     */
    static SystemConfig load_from_file(const std::string& filename);

    /**
     * Проверка корректности конфигурации
     * @return true если конфигурация валидна
     */
    bool validate() const;
};
