/*
    Maths module for SharedCppLib2.

    This file is mostly templates and concepts.

    standard: cxx23
*/
#pragma once

#include <concepts>
#include <type_traits>
#include <generator>
#include <utility>

// On C++23 I do whatever I like. This is template anyway.
#include <functional>

namespace scl2 {

// Detect types that support basic mathematical operations.
// For general algorithms to check whether the template argument
// support everything needed for operations.
template<typename T>
concept is_mathematical = 
    requires(T a, T b) {
        { a + b } -> std::convertible_to<T>;
        { a - b } -> std::convertible_to<T>;
        { a * b } -> std::convertible_to<T>;
        { a / b } -> std::convertible_to<T>;
        { a > b } -> std::convertible_to<bool>;
        { a < b } -> std::convertible_to<bool>;
        { a == b } -> std::convertible_to<bool>;
    };

template<typename T>
requires is_mathematical<T>
class matrix : private std::vector<T>
{
public:
    typedef std::pair<size_t, size_t> pos_t;
    typedef pos_t dim_t;
    typedef std::pair<pos_t, T&> pos_ref_t;
    typedef std::pair<pos_t, const T&> pos_const_ref_t;

    // this default constructor makes an invalid matrix.
    // it is only added for places that requires a default constructor.
    // You need to assign sizes before writing values, using resize() or = operator.
    matrix() {}

    matrix(size_t rows, size_t cols) : std::vector<T>(rows * cols), m_rows(rows), m_cols(cols) {}
    matrix(size_t rows, size_t cols, const T& value) : std::vector<T>(rows * cols, value), m_rows(rows), m_cols(cols) {}

    // matrix mathematical constructors

    static matrix<T> zero(size_t rows, size_t cols) {
        return matrix<T>(rows, cols, T(0));
    }

    static matrix<T> identity(size_t size) {
        matrix<T> result(size, size, T(0));
        for (size_t i = 0; i < size; ++i) {
            result(i, i) = T(1);
        }
        return result;
    }

    static matrix<T> diagonal(size_t size, const T& value) {
        matrix<T> result(size, size, T(0));
        for (size_t i = 0; i < size; ++i) {
            result(i, i) = value;
        }
        return result;
    }

    // special constructors that simplify the grammar.
    matrix(size_t rows, size_t cols, std::function<T&&(size_t, size_t)> init_func)
        : std::vector<T>(rows * cols), m_rows(rows), m_cols(cols) {
        for (size_t i = 0; i < rows; ++i) {
            for (size_t j = 0; j < cols; ++j) {
                operator()(i, j) = init_func(i, j);
            }
        }
    }

    // assign
    
    matrix(const matrix<T>& other) : std::vector<T>(other), m_rows(other.m_rows), m_cols(other.m_cols) {}
    matrix(matrix<T>&& other) noexcept : std::vector<T>(std::move(other)), m_rows(other.m_rows), m_cols(other.m_cols) {}

    matrix<T>& operator=(const matrix<T>& other) {
        m_rows = other.m_rows;
        m_cols = other.m_cols;
        std::vector<T>::operator=(other);
        return *this;
    }

    matrix<T>& operator=(matrix<T>&& other) noexcept {
        m_rows = other.m_rows;
        m_cols = other.m_cols;
        std::vector<T>::operator=(std::move(other));
        return *this;
    }

    // access

    T& operator()(size_t row, size_t col) {
        if (empty()) throw std::runtime_error("matrix: invalid access on empty matrix");
        if (row >= m_rows || col >= m_cols) throw std::out_of_range("matrix: index out of range");
        return std::vector<T>::operator[](index(row, col));
    }

    const T& operator()(size_t row, size_t col) const {
        if (empty()) throw std::runtime_error("matrix: invalid access on empty matrix");
        if (row >= m_rows || col >= m_cols) throw std::out_of_range("matrix: index out of range");
        return std::vector<T>::operator[](index(row, col));
    }

    // column based access
    // (not that useful, but is here anyway)

    class column_view {
    public:
        column_view(matrix<T>& mat, size_t col) : m_mat(mat), m_col(col) {
            if (col >= mat.m_cols) throw std::out_of_range("matrix: column index out of range");
        }

        T& operator[](size_t row) {
            return m_mat(row, m_col);
        }

        const T& operator[](size_t row) const {
            return m_mat(row, m_col);
        }
    };

    column_view col(size_t col) {
        return column_view(*this, col);
    }

    // iterators
    // generator based, so only works with C++23 and above.

private:
    template<typename ElemRef_Type, typename MatRef_Type>
    struct _Pm_gen {
        enum type { elems, diagonal, row, column };
        static std::generator<ElemRef_Type> run(MatRef_Type& mat, type t, size_t arg = 0) {
            switch (type) {
            case type::elems:
                for (size_t i = 0; i < mat.m_rows; ++i) {
                    for (size_t j = 0; j < mat.m_cols; ++j) {
                        co_yield { {i, j}, mat(i, j) };
                    }
                }
                break;
            case type::diagonal:
                for (size_t min_dim = std::min(mat.m_rows, mat.m_cols), i = 0; i < min_dim; ++i) {
                    co_yield { {i, i}, mat(i, i) };
                }
                break;
            case type::row:
                for (size_t j = 0; j < mat.m_cols; ++j) {
                    co_yield { {arg, j}, mat(arg, j) };
                }
                break;
            case type::column:
                for (size_t i = 0; i < mat.m_rows; ++i) {
                    co_yield { {i, arg}, mat(i, arg) };
                }
                break;
            }
        }
    };

public:
    std::generator<pos_ref_t> elems() { return _Pm_gen<pos_ref_t, matrix<T>>::run(*this, _Pm_gen<pos_ref_t, matrix<T>>::type::elems); }
    std::generator<pos_const_ref_t> elems() const { return _Pm_gen<pos_const_ref_t, const matrix<T>>::run(*this, _Pm_gen<pos_const_ref_t, const matrix<T>>::type::elems); }

    std::generator<pos_ref_t> diagonal_elems() { return _Pm_gen<pos_ref_t, matrix<T>>::run(*this, _Pm_gen<pos_ref_t, matrix<T>>::type::diagonal); }
    std::generator<pos_const_ref_t> diagonal_elems() const { return _Pm_gen<pos_const_ref_t, const matrix<T>>::run(*this, _Pm_gen<pos_const_ref_t, const matrix<T>>::type::diagonal); }

    std::generator<pos_ref_t> row_elems(size_t row) { return _Pm_gen<pos_ref_t, matrix<T>>::run(*this, _Pm_gen<pos_ref_t, matrix<T>>::type::row, row); }
    std::generator<pos_const_ref_t> row_elems(size_t row) const { return _Pm_gen<pos_const_ref_t, const matrix<T>>::run(*this, _Pm_gen<pos_const_ref_t, const matrix<T>>::type::row, row); }

    std::generator<pos_ref_t> column_elems(size_t col) { return _Pm_gen<pos_ref_t, matrix<T>>::run(*this, _Pm_gen<pos_ref_t, matrix<T>>::type::column, col); }
    std::generator<pos_const_ref_t> column_elems(size_t col) const { return _Pm_gen<pos_const_ref_t, const matrix<T>>::run(*this, _Pm_gen<pos_const_ref_t, const matrix<T>>::type::column, col); }

    // assertions (over elements)

    bool all(std::predicate<const T&> auto pred) const {
        for (auto& [_, v] : elems())
            if (!pred(v)) return false;
        return true;
    }

    bool any(std::predicate<const T&> auto pred) const {
        for (auto& [_, v] : elems())
            if (pred(v)) return true;
        return false;
    }

    // operators (over each elements)

    void foreach(std::function<void(T&, size_t, size_t)> func) {
        for (size_t i = 0; i < m_rows; ++i) {
            for (size_t j = 0; j < m_cols; ++j) {
                func(operator()(i, j), i, j);
            }
        }
    }

    bool operator==(const matrix<T>& other) const {
        if (m_rows != other.m_rows || m_cols != other.m_cols) return false;
        for (size_t i = 0; i < m_rows * m_cols; ++i) {
            if (std::vector<T>::operator[](i) != other[i]) return false;
        }
        return true;
    }

    bool operator!=(const matrix<T>& other) const {
        return !(*this == other);
    }

    matrix<T> operator+(const matrix<T>& other) const {
        if (!has_same_dimensions(*this, other))
            throw std::runtime_error("matrix: dimension mismatch for addition");
        return matrix<T>(m_rows, m_cols, [this, &other](size_t i, size_t j) {
            return operator()(i, j) + other(i, j);
        });
    }

    matrix<T> operator-(const matrix<T>& other) const {
        if (!has_same_dimensions(*this, other))
            throw std::runtime_error("matrix: dimension mismatch for subtraction");
        return matrix<T>(m_rows, m_cols, [this, &other](size_t i, size_t j) {
            return operator()(i, j) - other(i, j);
        });
    }

    matrix<T>& operator+=(const matrix<T>& other) {
        if (!has_same_dimensions(*this, other))
            throw std::runtime_error("matrix: dimension mismatch for addition");
        foreach([&other](T& value, size_t i, size_t j) {
            value += other(i, j);
        });
        return *this;
    }

    matrix<T>& operator-=(const matrix<T>& other) {
        if (!has_same_dimensions(*this, other))
            throw std::runtime_error("matrix: dimension mismatch for subtraction");
        foreach([&other](T& value, size_t i, size_t j) {
            value -= other(i, j);
        });
        return *this;
    }
    
    matrix<T> operator*(const matrix<T>& other) const {
        if (m_cols != other.m_rows)
            throw std::runtime_error("matrix: dimension mismatch for multiplication");
        return matrix<T>(m_rows, other.m_cols, [this, &other](size_t i, size_t j) {
            T sum = T(0);
            for (size_t k = 0; k < m_cols; ++k) {
                sum += operator()(i, k) * other(k, j);
            }
            return sum;
        });
    }

    matrix<T> operator*(const T& scalar) const {
        return matrix<T>(m_rows, m_cols, [this, &scalar](size_t i, size_t j) {
            return operator()(i, j) * scalar;
        });
    }

    matrix<T>& operator*=(const T& scalar) {
        foreach([&scalar](T& value, size_t, size_t) {
            value *= scalar;
        });
        return *this;
    }

    matrix<T> operator-() const {
        return matrix<T>(m_rows, m_cols, [this](size_t i, size_t j) {
            return -operator()(i, j);
        });
    }

    void fill(const T& value) {
        foreach([&value](T& elem, size_t, size_t) {
            elem = value;
        });
    }

    // special comparisons

    static bool has_same_dimensions(const matrix<T>& a, const matrix<T>& b) {
        return a.m_rows == b.m_rows && a.m_cols == b.m_cols;
    }


    // I don't know how to call this part

    matrix<T> transpose() const {
        return matrix<T>(m_cols, m_rows, [this](size_t r, size_t c) {
            return operator()(c, r);
        });
    }

    matrix<T> submatrix(size_t row_start, size_t col_start, size_t rows, size_t cols) const {
        if (row_start >= row_start + rows || col_start >= col_start + cols
        || row_start + rows > m_rows || col_start + cols > m_cols)
            throw std::out_of_range("matrix: invalid submatrix range");
        return matrix<T>(rows, cols, [this, row_start, col_start](size_t i, size_t j) {
            return operator()(i + row_start, j + col_start);
        });
    }

    // utilities

    // what determinant() {}
    // what inverse() {}

    // dimensions

    size_t rows() const { return m_rows; }
    size_t cols() const { return m_cols; }

    void resize(size_t nrow, size_t ncol) {
        // we need to manually move the data to the new size.
    }

    bool empty() const {
        return m_rows == 0 || m_cols == 0;
    }

    void reset() {
        m_rows = 0;
        m_cols = 0;
        std::vector<T>::clear();
    }
    
    dim_t dimensions() const {
        return { m_rows, m_cols };
    }

protected:
    size_t index(size_t row, size_t col) const {
        return row * m_cols + col;
    }

private:
    size_t m_rows = 0, m_cols = 0;

};

} // namespace scl2