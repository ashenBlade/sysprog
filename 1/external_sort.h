#ifndef EXTERNAL_SORT_H
#define EXTERNAL_SORT_H

/// @brief Запустить корутину для внешней сортировки файла
/// @param src_fd Дескриптор исходного файла
/// @param temp_fd Дескриптор временного файла, куда необходимо записать результат
/// @param max_memory_bytes Максимальный объем памяти (в байтах), который может быть использован для сортировки
/// @return 0 в случае успеха, -1 в случае ошибки (errno может указывать на ошибку)
int sort_file_external_coro(int src_fd, int result_fd, int max_memory_bytes);

#endif // EXTERNAL_SORT_H