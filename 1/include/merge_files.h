#ifndef MERGE_FILES_H
#define MERGE_FILES_H

/**
 * @brief Выполнить слияние файлов, с отсортированными значениями в указанный
 * 
 * @param result_fd Файл для записей результатов
 * @param fds Файловые дескрипторы файлов с отсортированными числами
 * @param count Размер массива файловых дескрипторов
 */
void merge_files(int result_fd, int *fds, int count);

#endif