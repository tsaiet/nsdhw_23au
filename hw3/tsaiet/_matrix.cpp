#include <pybind11/pybind11.h>
#include <vector>
#include <stdexcept>
//#include <mkl_cblas.h>

class Matrix {

public:

    Matrix(size_t nrow, size_t ncol)
      : m_nrow(nrow), m_ncol(ncol)
    {
        reset_buffer(nrow, ncol);
    }

    Matrix(size_t nrow, size_t ncol, std::vector<double> const & vec)
      : m_nrow(nrow), m_ncol(ncol)
    {
        reset_buffer(nrow, ncol);
        (*this) = vec;
    }

    Matrix & operator=(std::vector<double> const & vec)
    {
        if (size() != vec.size())
        {
            throw std::out_of_range("number of elements mismatch");
        }

        size_t k = 0;
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = vec[k];
                ++k;
            }
        }

        return *this;
    }

    Matrix(Matrix const & other)
      : m_nrow(other.m_nrow), m_ncol(other.m_ncol)
    {
        reset_buffer(other.m_nrow, other.m_ncol);
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = other(i,j);
            }
        }
    }

    Matrix & operator=(Matrix const & other)
    {
        if (this == &other) { return *this; }
        if (m_nrow != other.m_nrow || m_ncol != other.m_ncol)
        {
            reset_buffer(other.m_nrow, other.m_ncol);
        }
        for (size_t i=0; i<m_nrow; ++i)
        {
            for (size_t j=0; j<m_ncol; ++j)
            {
                (*this)(i,j) = other(i,j);
            }
        }
        return *this;
    }

    Matrix(Matrix && other)
      : m_nrow(other.m_nrow), m_ncol(other.m_ncol)
    {
        reset_buffer(0, 0);
        std::swap(m_nrow, other.m_nrow);
        std::swap(m_ncol, other.m_ncol);
        std::swap(m_buffer, other.m_buffer);
    }

    Matrix & operator=(Matrix && other)
    {
        if (this == &other) { return *this; }
        reset_buffer(0, 0);
        std::swap(m_nrow, other.m_nrow);
        std::swap(m_ncol, other.m_ncol);
        std::swap(m_buffer, other.m_buffer);
        return *this;
    }

    ~Matrix()
    {
        reset_buffer(0, 0);
    }

    double   operator() (size_t row, size_t col) const
    {
        return m_buffer[index(row, col)];
    }
    double & operator() (size_t row, size_t col)
    {
        return m_buffer[index(row, col)];
    }
    bool operator==(const Matrix& rhs) const
    {
        size_t r = rhs.m_nrow;
        size_t c = rhs.m_ncol;
        if(m_nrow != r || m_ncol != c)
            return false;
        for(size_t i = 0;i < r*c;i++)
            if(m_buffer[i] != rhs.m_buffer[i])
                return false;
        return true;
    }

    size_t nrow() const { return m_nrow; }
    size_t ncol() const { return m_ncol; }

    size_t size() const { return m_nrow * m_ncol; }
    double buffer(size_t i) const { return m_buffer[i]; }
    double& buffer(size_t i) { return m_buffer[i]; }
    const double *buffer() const { return m_buffer; }
    double *buffer() { return m_buffer; }
    std::vector<double> buffer_vector() const
    {
        return std::vector<double>(m_buffer, m_buffer+size());
    }

private:

    size_t index(size_t row, size_t col) const
    {
        return row + col * m_nrow;
    }

    void reset_buffer(size_t nrow, size_t ncol)
    {
        if (m_buffer) { delete[] m_buffer; }
        const size_t nelement = nrow * ncol;
        if (nelement) { m_buffer = new double[nelement]; }
        else          { m_buffer = nullptr; }
        m_nrow = nrow;
        m_ncol = ncol;
    }

    size_t m_nrow = 0;
    size_t m_ncol = 0;
    double * m_buffer = nullptr;

};

/*
 * Naive matrix matrix multiplication.
 */
Matrix multiply_naive(Matrix const & mat1, Matrix const & mat2)
{
    if (mat1.ncol() != mat2.nrow())
    {
        throw std::out_of_range(
            "the number of first matrix column "
            "differs from that of second matrix row");
    }

    Matrix ret(mat1.nrow(), mat2.ncol());

    for (size_t i=0; i<ret.nrow(); ++i)
    {
        for (size_t k=0; k<ret.ncol(); ++k)
        {
            double v = 0;
            for (size_t j=0; j<mat1.ncol(); ++j)
            {
                v += mat1(i,j) * mat2(j,k);
            }
            ret(i,k) = v;
        }
    }

    return ret;
}

/*
 * Tiling matrix matrix multiplication.
 */
Matrix multiply_tile(Matrix const & mat1, Matrix const & mat2, size_t tsize)
{
    if (mat1.ncol() != mat2.nrow())
    {
        throw std::out_of_range(
            "the number of first matrix column "
            "differs from that of second matrix row");
    }

    size_t r = mat1.nrow();
    size_t c = mat2.ncol();
    size_t k = mat1.ncol();

    size_t rt_size, ct_size, kt_size;

    Matrix ret(r, c);
    for (size_t rt = 0; rt < r; rt += tsize)
    {
        rt_size = std::min(rt + tsize, r);
        for (size_t ct = 0; ct < c; ct += tsize)
        {
            ct_size = std::min(ct + tsize, c);
            for (size_t kt = 0; kt < k; kt += tsize)
            {
                kt_size = std::min(kt + tsize, k);
                for (size_t i = rt; i < rt_size; ++i){
                    for (size_t j = ct; j < ct_size; ++j)
                    {
                        double v = 0;
                        for (size_t k = kt; k < kt_size; ++k){
                            v += mat1(i,k) * mat2(k,j);
                        }
                        ret(i,j) = v;
                    }
                }
            }
        }
    }

    return ret;
}

/*
 * Using DGEMM to matrix matrix multiplication.
 */
/*Matrix multiply_mkl(Matrix const & mat1, Matrix const & mat2)
{
    if (mat1.ncol() != mat2.nrow())
    {
        throw std::out_of_range(
            "the number of first matrix column "
            "differs from that of second matrix row");
    }

    Matrix ret(mat1.nrow(), mat2.ncol());
    
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, mat1.nrow(), mat2.ncol(), mat1.ncol(), 1.0, mat1, mat1.ncol(), mat2, mat2.ncol(), 1.0, ret, mat2.ncol());
    
    return ret;
}*/

PYBIND11_MODULE(_matrix, m){
	pybind11::class_<Matrix>(m, "Matrix")
        .def(pybind11::init<int, int>())
        .def_property_readonly("nrow", [](const Matrix& mat) { return mat.nrow(); })
        .def_property_readonly("ncol", [](const Matrix& mat) { return mat.ncol(); })
        .def("__eq__", [](const Matrix& a, const Matrix& b) { return a == b; })
        .def("__setitem__", [](Matrix& self, std::pair<int, int> idx, double val) {
            self(idx.first, idx.second) = val;
        })
        .def("__getitem__", [](const Matrix& self, std::pair<int, int> idx) {
            return self(idx.first, idx.second);
        });
    m
    .def("multiply_naive", &multiply_naive, pybind11::arg("mat1"), pybind11::arg("mat2"))
    .def("multiply_tile", &multiply_tile, pybind11::arg("mat1"), pybind11::arg("mat2"), pybind11::arg("tsize"));
    //.def("multiply_mkl", &multiply_mkl, pybind11::arg("mat1"), pybind11::arg("mat2"));
}