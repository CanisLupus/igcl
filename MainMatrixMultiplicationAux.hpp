#ifndef MAINMATRIXMULTIPLICATIONAUX_HPP_
#define MAINMATRIXMULTIPLICATIONAUX_HPP_

#include <fstream>
#include <sstream>
#include <ostream>
#include <string>
#include <iomanip>

const char DATA_ROW = 1;
const char DATA_COL = 2;
const char DATA     = 3;

const std::string filepath1 = "./matrices/mat128_1.txt";
const std::string filepath2 = "./matrices/mat128_2.txt";

bool readMatrix(const std::string & filepath, float * & matrix, uint & size)
{
	std::ifstream file;
	file.open(filepath);
	if (file.fail())
		return false;

	std::string line;
	getline(file, line);
	size = stoi(line);

	matrix = (float *) malloc(size*size * sizeof(float));

	for (uint i = 0; i < size; i++) {
		getline(file, line);
		std::stringstream ss(line);
		for (uint j = 0; j < size; j++)
			ss >> matrix[i*size+j];
	}
	file.close();
	return true;
}

template <typename T>
void printMatrix(const T * matrix, const int size)
{
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			std::cout << std::setw(5) << matrix[i * size + j] << ' ';
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

template <typename T>
void transposeMatrix(const T * matrix, T * transposed, int size)
{
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			transposed[j*size + i] = matrix[i*size + j];
		}
	}
}

template <typename T>
void transposeMatrix(T * matrix, int size)
{
	for (int i = 0; i < size-1; i++) {
		for (int j = i+1; j < size; j++) {
			std::swap(matrix[i*size+j], matrix[j*size+i]);
		}
	}
}

template <typename T>
T * multiplyMatrices(const T * mat1, const T * mat2, int size, T * res)
{
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			T sum = 0;
			for (int k = 0; k < size; k++)
				sum += mat1[i*size+k] * mat2[k*size+j];
			res[i*size+j] = sum;
		}
	}
	return res;
}

template <typename T>
T * multiplyMatricesT(const T * mat1, const T * mat2, int size, T * res)
{
	for (int i = 0; i < size; i++) {
		for (int j = 0; j < size; j++) {
			T sum = 0;
			for (int k = 0; k < size; k++)
				sum += mat1[i*size+k] * mat2[j*size+k];
			res[i*size+j] = sum;
		}
	}
	return res;
}

template <typename T>
T multiplyRows(const T * row1, const T * row2, int size)
{
	T res = 0;
	for (int i = 0; i < size; i++) {
		res += row1[i] * row2[i];
	}
	return res;
}

template <typename T>
void multiplyRows(const T * row1, const T * row2, int size, T & res)
{
	res = 0;
	for (int i = 0; i < size; i++) {
		res += row1[i] * row2[i];
	}
}


#endif /* MAINMATRIXMULTIPLICATIONAUX_HPP_ */
