#!/usr/bin/bash

if [[ -z "$1" ]]; then
    echo "Использование: $0 <количество файлов>"
    echo "Каждый файл содержит в себе 100000 чисел без указания верхнего порога (т.е. макс. значение = макс. значение int)"
    exit 1
fi

if [[ "$1" -le "0" ]]; then
    echo "Количество файлов не может быть отрицательным. Указано $1"
    exit 1
fi

TEST_DIR='test'
BUILD_DIR='build'

echo 'Запускаю сборку приложения'
mkdir -p "$BUILD_DIR"
if [[ "$?" -ne 0 ]]; then
    echo "Ошибка создания директории $BUILD_DIR"
    exit 1
fi

echo "Захожу в директорию $BUILD_DIR"
cd "$BUILD_DIR"
cmake ..
if [[ "$?" -ne 0 ]]; then
    echo "Ошибка при запуске CMAKE: неуспешный код ответа $?"
    exit 1
fi

echo "Запускаю make"
make all
if [[ "$?" -ne 0 ]]; then
    echo "Ошибка при запуске make: неуспешный код ответа $?"
    exit 1
fi


echo "Выхожу из директории $BUILD_DIR"

cd ..

echo "Создаю директорию $TEST_DIR"
if [[ -d '$TEST_DIR' ]]; then
    echo "Директория $TEST_DIR уже существует"
else 
    mkdir -p "$TEST_DIR"
    if [[ "$?" -ne 0 ]]; then
        echo "Ошибка при создании директории $TEST"
        exit 1
    fi
fi

TEST_FILES=""

for NUM in $(seq 1 $1); do
    FILENAME="$TEST_DIR/numbers$NUM.txt"
    echo "Создаю файл $FILENAME"
    python3 ./generator.py -f "$FILENAME" -c 100000
    if [[ "$?" -ne 0 ]]; then
        echo "Ошибка при генерации файла"
        exit 1
    fi
    TEST_FILES="$TEST_FILES $FILENAME"
done

echo 'Запускаю приложение для тестирования'
"./$BUILD_DIR/coroutines" $TEST_FILES
if [[ "$?" -ne 0 ]]; then
    echo "При запуске приложения возникла ошибка"
    exit 1
fi

echo 'Запускаю проверку корректности работы'
python3 ./checker.py -f ./result.txt
if [[ "$?" -ne 0 ]]; then
    echo "При проверке корректности возникла ошибка"
    exit 1
fi

echo "Проверка завершилась успешно"