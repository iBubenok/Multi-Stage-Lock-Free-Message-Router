# Multi-stage build для Message Router

# ========== Этап сборки ==========
FROM ubuntu:22.04 AS builder

# Установка зависимостей для сборки
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    wget \
    clang-19 \
    libc++-19-dev \
    libc++abi-19-dev \
    && rm -rf /var/lib/apt/lists/*

# Установка clang-19 как компилятора по умолчанию
ENV CC=clang-19
ENV CXX=clang++-19

# Создание рабочей директории
WORKDIR /build

# Копирование исходного кода
COPY CMakeLists.txt ./
COPY include/ ./include/
COPY src/ ./src/
COPY benchmarks/ ./benchmarks/

# Сборка проекта (Release mode с оптимизациями)
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release \
          -DCMAKE_CXX_FLAGS_RELEASE="-O3 -march=native -DNDEBUG" \
          -DBUILD_BENCHMARKS=ON \
          .. && \
    make -j$(nproc)

# ========== Этап выполнения ==========
FROM ubuntu:22.04

# Установка runtime зависимостей
RUN apt-get update && apt-get install -y \
    libc++1-19 \
    libc++abi1-19 \
    && rm -rf /var/lib/apt/lists/*

# Создание пользователя для запуска (безопасность)
RUN useradd -m -s /bin/bash router && \
    mkdir -p /app/configs /app/results/benchmarks && \
    chown -R router:router /app

# Установка рабочей директории
WORKDIR /app

# Копирование скомпилированных бинарников из этапа сборки
COPY --from=builder /build/build/router_test ./router_test
COPY --from=builder /build/build/queue_benchmark ./queue_benchmark
COPY --from=builder /build/build/routing_benchmark ./routing_benchmark
COPY --from=builder /build/build/memory_benchmark ./memory_benchmark
COPY --from=builder /build/build/scaling_benchmark ./scaling_benchmark

# Копирование конфигурационных файлов
COPY configs/ ./configs/

# Копирование скриптов
COPY scripts/ ./scripts/
RUN chmod +x ./scripts/*.sh 2>/dev/null || true

# Переключение на пользователя router
USER router

# Настройка производительности (можно изменить через docker run)
ENV OMP_NUM_THREADS=4
ENV OMP_PROC_BIND=true

# Точка входа по умолчанию
ENTRYPOINT ["./router_test"]
CMD ["configs/baseline.json"]
