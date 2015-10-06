#ifndef COMMON_H
#define COMMON_H

#include <mutex>
#include <iostream>
#include <vector>
#include <random>
#include <array>
#include <cmath>

#include <Eigen/Dense>
#include <unsupported/Eigen/MatrixFunctions>

#include "mpreal.h"
#include "prettyprint.hpp"

#include "exponential_integrals.h"

#define AUTODIFF 1
#define EIGEN_NO_AUTOMATIC_RESIZING 1
#define RATE_FUNCTION PiecewiseExponentialRateFunction

#ifdef NDEBUG
#define _DEBUG(x)
#else
#define _DEBUG(x) x
#endif


#if 1
void doProgress(bool);
extern std::mutex mtx;
extern bool do_progress;
#define PROGRESS(x) if (do_progress) { mtx.lock(); std::cout << __FILE__ << ":" << __func__ << x << "... " << std::flush; mtx.unlock(); }
#define PROGRESS_DONE() if (do_progress) { mtx.lock(); std::cout << "done." << std::endl << std::flush; mtx.unlock(); }
#else
#define PROGRESS(x)
#define PROGRESS_DONE()
#endif

constexpr long nC2(int n) { return n * (n - 1) / 2; }

template <typename T> using Matrix = Eigen::Matrix<T, Eigen::Dynamic, Eigen::Dynamic>;
template <typename T> using Vector = Eigen::Matrix<T, Eigen::Dynamic, 1>;

template <typename _Scalar, int NX=Eigen::Dynamic, int NY=Eigen::Dynamic>
struct Functor
{
    typedef _Scalar Scalar;
    enum {
        InputsAtCompileTime = NX,
        ValuesAtCompileTime = NY
    };
    typedef Eigen::Matrix<Scalar,InputsAtCompileTime,1> InputType;
    typedef Eigen::Matrix<Scalar,ValuesAtCompileTime,1> ValueType;
    typedef Eigen::Matrix<Scalar,ValuesAtCompileTime,InputsAtCompileTime> JacobianType;

    const int m_inputs, m_values;

    Functor() : m_inputs(InputsAtCompileTime), m_values(ValuesAtCompileTime) {}
    Functor(int inputs, int values) : m_inputs(inputs), m_values(values) {}

    int inputs() const { return m_inputs; }
    int values() const { return m_values; }

    // you should define that in the subclass :
    // //  void operator() (const InputType& x, ValueType* v, JacobianType* _j=0) const;
};

#ifdef AUTODIFF
#include <unsupported/Eigen/AutoDiff>

/*
template <typename T>
class MyAutoDiffScalar : public Eigen::AutoDiffScalar<T>
{
    public:
    typedef typename Eigen::AutoDiffScalar<T>::Scalar Scalar;
    typedef typename Eigen::AutoDiffScalar<T>::DerType DerType;
    MyAutoDiffScalar() {}
    MyAutoDiffScalar(const Scalar &x) : Eigen::AutoDiffScalar<T>(x) {}
    MyAutoDiffScalar(const Scalar &x, const DerType &y) : Eigen::AutoDiffScalar<T>(x, y) {}
    template <typename OtherDerType>
    MyAutoDiffScalar(const Eigen::AutoDiffScalar<OtherDerType>& other) : 
        Eigen::AutoDiffScalar<T>((Scalar)other.value(), 
        other.derivatives().template cast<typename DerType::Scalar>()) {}
};
*/

typedef Eigen::AutoDiffScalar<Eigen::VectorXd> adouble;
inline double toDouble(const adouble &a) { return a.value(); }
inline double toDouble(const double &d) { return d; }

namespace Eigen {
    // Allow for casting of adouble matrices to double
    namespace internal 
    {
        template <>
            struct cast_impl<adouble, double>
            {
                static inline double run(const adouble &x)
                {
                    return x.value();
                }
            };
    }

// Copied from Eigen's AutoDiffScalar.h
#define EIGEN_AUTODIFF_DECLARE_GLOBAL_UNARY(FUNC,CODE) \
  template<typename DerType> \
  inline const Eigen::AutoDiffScalar<Eigen::CwiseUnaryOp<Eigen::internal::scalar_multiple_op<typename Eigen::internal::traits<typename Eigen::internal::remove_all<DerType>::type>::Scalar>, const typename Eigen::internal::remove_all<DerType>::type> > \
  FUNC(const Eigen::AutoDiffScalar<DerType>& x) { \
    using namespace Eigen; \
    typedef typename Eigen::internal::traits<typename Eigen::internal::remove_all<DerType>::type>::Scalar Scalar; \
    typedef AutoDiffScalar<CwiseUnaryOp<Eigen::internal::scalar_multiple_op<Scalar>, const typename Eigen::internal::remove_all<DerType>::type> > ReturnType; \
    CODE; \
  }

using mpfr::exp;
using mpfr::expm1;
using std::exp;
using std::expm1;
using mpfr::log1p;
using std::log1p;

EIGEN_AUTODIFF_DECLARE_GLOBAL_UNARY(expm1,
  Scalar expm1x = expm1(x.value());
  Scalar expx = exp(x.value());
  return ReturnType(expm1x, x.derivatives() * expx);
)

EIGEN_AUTODIFF_DECLARE_GLOBAL_UNARY(ceil,
  Scalar ceil = std::ceil(x.value());
  return ReturnType(ceil, x.derivatives() * Scalar(0));
)

EIGEN_AUTODIFF_DECLARE_GLOBAL_UNARY(log1p,
  Scalar log1px = std::log1p(x.value());
  return ReturnType(log1px, x.derivatives() * (Scalar(1) / (Scalar(1) + x.value())));
)

EIGEN_AUTODIFF_DECLARE_GLOBAL_UNARY(expintei,
  Scalar eintx = eint::expintei(x.value());
  Scalar expx = exp(x.value()) / x.value();
  return ReturnType(eintx, x.derivatives() * expx);
)

#undef EIGEN_AUTODIFF_DECLARE_GLOBAL_UNARY

};

#else

typedef double adouble;

#endif

template <typename T, typename U>
inline int insertion_point(const T x, const std::vector<U>& ary, int first, int last)
{
   int mid;
    while(first + 1 < last)
    {
        mid = (int)((first + last) / 2);
        if (ary[mid] > x)
            last = mid;
        else    
            first = mid;
    }
    return first;
}

inline double myabs(double a) { return std::abs(a); }
inline adouble myabs(adouble a) { return Eigen::abs(a); }
inline mpfr::mpreal myabs(mpfr::mpreal a) { return mpfr::abs(a); }

inline void init_eigen() { Eigen::initParallel(); }
inline void fill_jacobian(const adouble &ll, double* outjac)
{
    Eigen::VectorXd d = ll.derivatives();
    Eigen::Map<Eigen::Matrix<double, Eigen::Dynamic, 1>, Eigen::RowMajor> _jac(outjac, d.rows());
    _jac = d;
}

void store_matrix(Matrix<double> *M, double* out);
void store_matrix(Matrix<adouble> *M, double* out);
void store_admatrix(const Matrix<adouble> &M, int nd, double* out, double* outjac);

template <typename T>
inline T dmin(const T a, const T b) { if (a > b) return b; return a; }
template <typename T>
inline T dmax(const T a, const T b) { if (a > b) return a; return b; }

/*
inline adouble dmin(adouble a, adouble b)
{
    return (a + b - myabs(a - b)) / 2;
}

inline adouble dmax(adouble a, adouble b)
{
    return (a + b + myabs(a - b)) / 2;
}
*/

inline void check_nan(const double x) { if (std::isnan(x)) throw std::domain_error("nan detected"); }

template <typename T>
void check_nan(const Vector<T> &x) 
{ 
    for (int i = 0; i < x.rows(); ++i) 
        check_nan(x(i));
}

template <typename T>
void check_nan(const Eigen::AutoDiffScalar<T> &x)
{
    check_nan(x.value());
    check_nan(x.derivatives());
}

template <typename Derived>
void check_nan(const Eigen::DenseBase<Derived> &M)
{
    for (int i = 0; i < M.rows(); ++i)
        for (int j = 0; j < M.cols(); ++j)
            check_nan(M(i, j));
}

#endif
