# Linux-окружение курса в контейнере: для Маков, пока комплект не несёт
# готовых границ под macOS.
#
# Архитектура выбрана x86-64 намеренно: собранные os и edr в комплекте есть
# для win-x64 и linux-x64, и программа, собранная под arm64, с этими
# библиотеками не слинкуется. На Apple Silicon образ идёт через эмуляцию,
# и как её ускорить, написано в SETUP.md, раздел «macOS: Linux в контейнере».
ARG NANO_EDR_PLATFORM=linux/amd64
FROM --platform=${NANO_EDR_PLATFORM} ubuntu:26.04

# Русский текст в выводе сценариев и тестов.
ENV LANG=C.UTF-8

# Номеров версий здесь нет намеренно: пакеты без номера дают то, что в системе
# считается основным, а нижнюю границу — clang 19 и libstdc++ 14 — проверяет
# сам комплект при конфигурации и говорит о ней внятно. В Ubuntu 26.04 это
# clang 21.
#
# Зачем g++, если курс собирается clang: clang приносит компилятор, но не
# стандартную библиотеку и на Linux берёт libstdc++ от GCC. Пакет g++ тянет
# её за собой; отдельного `libstdc++-dev` без номера версии не существует —
# это виртуальный пакет с шестью поставщиками, и apt на нём отказывает.
#
# sudo — чтобы доставить пакет внутри контейнера, не пересобирая образ.
RUN apt-get update \
 && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        clang g++ cmake ninja-build gdb git python3 sudo ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# Пользователь ubuntu с uid 1000 в образе уже есть; ему нужен sudo без пароля.
RUN echo 'ubuntu ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/ubuntu \
 && chmod 0440 /etc/sudoers.d/ubuntu

# Каталог курса приезжает в контейнер монтированием, и владелец файлов в нём —
# хозяйская система. Без этой строки git внутри отказывается работать
# с «чужим» репозиторием.
RUN git config --system --add safe.directory '*'

USER ubuntu
