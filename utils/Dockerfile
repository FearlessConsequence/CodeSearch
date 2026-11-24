FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    cmake \
    xclip \
    wl-clipboard \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . .

# Сборка через Makefile
RUN make && make tests

# Прогон тестов
CMD ["./tests"]