#ifndef NUMBER_FILE_READER_H
#define NUMBER_FILE_READER_H

#include <ctype.h>
#include <stdbool.h>

/**
 * @brief Структура, представляющая состояние чтения чисел из файла
 */
typedef struct file_read_state file_read_state;

/**
 * @brief Создать новый экземпляр для чтения чисел из файла
 * 
 * @param fd Файловый дескриптор файла
 * @param buffer_size Размер буфера для чтения
 * @return file_read_state* Новый экземпляр
 */
file_read_state *file_read_state_new(int fd, int buffer_size);

/**
 * @brief Очистить экземпляр, освободить занятые ресурсы
 * 
 * @param state Объект состояния
 */
void file_read_state_delete(file_read_state *state);

/**
 * @brief Прочитать следующее число из файла
 * 
 * @param state Указатель на объект чтения
 * @param number Указатель на число, в который необходимо записать результат
 * @return true Число прочитано успешно
 * @return false Файл закончился, чисел больше нет
 */
bool file_read_state_get_next_number(file_read_state* state, int *number);

#endif