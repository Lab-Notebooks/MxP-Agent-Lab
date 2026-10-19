#ifndef IO_HPP
#define IO_HPP

#include <vector>
#include <string>

std::vector<std::vector<double>> load_matrix_from_csv(const std::string &filename);
void save_matrix_to_csv(const std::vector<std::vector<double>> &matrix, const std::string &filename);

#endif // IO_HPP