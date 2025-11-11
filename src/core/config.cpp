#include "config.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>
#include <iostream>

using json = nlohmann::json;

SystemConfig SystemConfig::load_from_file(const std::string& filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("Не удалось открыть файл конфигурации: " + filename);
    }

    json j;
    try {
        file >> j;
    } catch (const json::parse_error& e) {
        throw std::runtime_error("Ошибка парсинга JSON: " + std::string(e.what()));
    }

    SystemConfig config;

    // Базовые параметры
    config.scenario = j.value("scenario", "unknown");
    config.duration_secs = j.value("duration_secs", 10);

    // Конфигурация производителей
    if (j.contains("producers")) {
        const auto& prod = j["producers"];
        config.producers.count = prod.value("count", 4);
        config.producers.messages_per_sec = prod.value("messages_per_sec", 1000000);

        if (prod.contains("distribution")) {
            for (const auto& [key, value] : prod["distribution"].items()) {
                // Извлекаем номер типа из строки вида "msg_type_0"
                if (key.find("msg_type_") == 0) {
                    uint8_t type = std::stoi(key.substr(9));
                    config.producers.distribution[type] = value.get<double>();
                }
            }
        }
    }

    // Конфигурация процессоров
    if (j.contains("processors")) {
        const auto& proc = j["processors"];
        config.processors.count = proc.value("count", 4);

        if (proc.contains("processing_times_ns")) {
            for (const auto& [key, value] : proc["processing_times_ns"].items()) {
                if (key.find("msg_type_") == 0) {
                    uint8_t type = std::stoi(key.substr(9));
                    config.processors.processing_times_ns[type] = value.get<uint64_t>();
                }
            }
        }
    }

    // Конфигурация стратегий
    if (j.contains("strategies")) {
        const auto& strat = j["strategies"];
        config.strategies.count = strat.value("count", 3);

        if (strat.contains("processing_times_ns")) {
            for (const auto& [key, value] : strat["processing_times_ns"].items()) {
                if (key.find("strategy_") == 0) {
                    uint8_t id = std::stoi(key.substr(9));
                    config.strategies.processing_times_ns[id] = value.get<uint64_t>();
                }
            }
        }
    }

    // Правила Stage1
    if (j.contains("stage1_rules")) {
        for (const auto& rule : j["stage1_rules"]) {
            Stage1Rule r;
            r.msg_type = rule.value("msg_type", 0);

            if (rule.contains("processors")) {
                for (const auto& proc : rule["processors"]) {
                    r.processors.push_back(proc.get<uint8_t>());
                }
            }

            config.stage1_rules.push_back(r);
        }
    }

    // Правила Stage2
    if (j.contains("stage2_rules")) {
        for (const auto& rule : j["stage2_rules"]) {
            Stage2Rule r;
            r.msg_type = rule.value("msg_type", 0);
            r.strategy = rule.value("strategy", 0);
            r.ordering_required = rule.value("ordering_required", true);

            config.stage2_rules.push_back(r);
        }
    }

    // Валидация конфигурации
    if (!config.validate()) {
        throw std::runtime_error("Конфигурация не прошла валидацию");
    }

    return config;
}

bool SystemConfig::validate() const {
    // Проверка базовых параметров
    if (duration_secs == 0) {
        std::cerr << "Ошибка: duration_secs должен быть больше 0" << std::endl;
        return false;
    }

    // Проверка производителей
    if (producers.count == 0 || producers.count > 16) {
        std::cerr << "Ошибка: количество producers должно быть от 1 до 16" << std::endl;
        return false;
    }

    // Проверка суммы распределения
    double sum = 0.0;
    for (const auto& [type, prob] : producers.distribution) {
        sum += prob;
    }
    if (std::abs(sum - 1.0) > 0.01) {
        std::cerr << "Предупреждение: сумма вероятностей распределения = " << sum
                  << " (ожидается 1.0)" << std::endl;
    }

    // Проверка процессоров
    if (processors.count == 0 || processors.count > 16) {
        std::cerr << "Ошибка: количество processors должно быть от 1 до 16" << std::endl;
        return false;
    }

    // Проверка стратегий
    if (strategies.count == 0 || strategies.count > 16) {
        std::cerr << "Ошибка: количество strategies должно быть от 1 до 16" << std::endl;
        return false;
    }

    // Проверка правил Stage1
    if (stage1_rules.empty()) {
        std::cerr << "Ошибка: должно быть хотя бы одно правило stage1" << std::endl;
        return false;
    }

    for (const auto& rule : stage1_rules) {
        if (rule.processors.empty()) {
            std::cerr << "Ошибка: правило stage1 для типа " << static_cast<int>(rule.msg_type)
                      << " не содержит процессоров" << std::endl;
            return false;
        }

        for (uint8_t proc_id : rule.processors) {
            if (proc_id >= processors.count) {
                std::cerr << "Ошибка: правило stage1 ссылается на несуществующий процессор "
                          << static_cast<int>(proc_id) << std::endl;
                return false;
            }
        }
    }

    // Проверка правил Stage2
    if (stage2_rules.empty()) {
        std::cerr << "Ошибка: должно быть хотя бы одно правило stage2" << std::endl;
        return false;
    }

    for (const auto& rule : stage2_rules) {
        if (rule.strategy >= strategies.count) {
            std::cerr << "Ошибка: правило stage2 ссылается на несуществующую стратегию "
                      << static_cast<int>(rule.strategy) << std::endl;
            return false;
        }
    }

    return true;
}
