#ifndef UTILS_H
#define UTILS_H

/// @brief Получить все имена файлов, которые необходимо отсортировать.
/// Дополнительно, этот метод проверяет и возможность работы с файлами (права доступа, файл или директория)
/// @param argc Количество аргументов командной строки
/// @param argv Аргументы командной строки
/// @return Список указателей на названия файлов (из @ref argv)
const char **extract_filenames(int argc, const char **argv);

#endif