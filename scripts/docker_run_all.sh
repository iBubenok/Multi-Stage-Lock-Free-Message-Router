#!/bin/bash
# Скрипт для запуска всех тестов в Docker

set -e

echo "=========================================="
echo "Сборка Docker образа"
echo "=========================================="
docker-compose build

echo ""
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
    echo "=========================================="
    echo "Сценарий: $scenario"
    echo "=========================================="

    docker-compose run --rm router-test "configs/${scenario}.json" 2>&1 | \
        tee "results/${scenario}_result.txt"

    echo ""
    sleep 1
done

echo ""
echo "=========================================="
echo "Запуск всех бенчмарков"
echo "=========================================="
docker-compose run --rm all-benchmarks

echo ""
echo "=========================================="
echo "Все тесты и бенчмарки завершены!"
echo "Результаты находятся в директории: ./results"
echo "=========================================="
