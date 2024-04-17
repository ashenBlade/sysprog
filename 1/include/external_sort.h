#ifndef EXTERNAL_SORT_H
#define EXTERNAL_SORT_H

/** 
 * @brief Запустить корутину для внешней сортировки файла
 * @param src_fd Дескриптор исходного файла
 * @param temp_fd Дескриптор временного файла, куда необходимо записать результат
 * @param max_memory_bytes Максимальный объем памяти (в байтах), который может быть использован для сортировки
 */
void sort_file_external_coro(int src_fd, int temp_fd);

#endif // EXTERNAL_SORT_H