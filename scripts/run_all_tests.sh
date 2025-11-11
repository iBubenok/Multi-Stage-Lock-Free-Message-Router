#!/bin/bash
# Скрипт для запуска всех тестовых сценариев

set -e

CONFIGS_DIR="./configs"
RESULTS_DIR="./results"

# Создание директории для результатов
mkdir -p "$RESULTS_DIR"

echo "=========================================="
echo "Запуск всех тестовых сценариев"
echo "=========================================="
echo ""

# Массив с именами сценариев
scenarios=(
    "baseline"
    "hot_type"
    "burst_pattern"
    "imbalanced_processing"
    "ordering_stress"
    "strategy_bottleneck"
)

# Запуск каждого сценария
for scenario in "${scenarios[@]}"; do
    config_file="$CONFIGS_DIR/${scenario}.json"
    result_file="$RESULTS_DIR/${scenario}_result.txt"

    echo "=========================================="
    echo "Сценарий: $scenario"
    echo "Конфигурация: $config_file"
    echo "=========================================="

    if [ ! -f "$config_file" ]; then
        echo "ОШИБКА: Файл конфигурации не найден: $config_file"
        continue
    fi

    # Запуск теста и сохранение результата
    ./router_test "$config_file" 2>&1 | tee "$result_file"

    echo ""
    echo "Результаты сохранены в: $result_file"
    echo ""

    # Небольшая пауза между тестами
    sleep 2
done

echo "=========================================="
echo "Все тесты завершены!"
echo "Результаты находятся в директории: $RESULTS_DIR"
echo "=========================================="
