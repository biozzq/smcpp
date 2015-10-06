#include "transition.h"

template <typename T>
struct trans_integrand_helper
{
    const int i, j;
    const PiecewiseExponentialRateFunction<T> *eta;
    const double left, right;
};

template <typename T>
struct p_intg_helper
{
    const PiecewiseExponentialRateFunction<T>* eta;
    const double rho, left, right;
};

template <typename T>
T p_integrand(const double x, p_intg_helper<T> *pih)
{
    double v, jac;
    if (pih->right < INFINITY)
    {
        v = x * pih->right + (1. - x) * pih->left;
        jac = pih->right - pih->left;
    }
    else
    {
        v = pih->left + x / (1. - x);
        jac = 1. / (1. - x) / (1. - x);
    }
    if (v == INFINITY)
        return 0.;
    T ret = pih->eta->eta(v) * exp(-(pih->eta->R(v) + pih->rho * v)) * jac;
    return ret;
}

template <typename T>
T Transition<T>::P_no_recomb(const int i)
{
    std::vector<double> t = eta->hidden_states;
    p_intg_helper<T> h = {eta, 2. * rho, t[i - 1], t[i]};
    T num = adaptiveSimpsons(std::function<T(const double, p_intg_helper<T>*)>(p_integrand<T>), &h, 0., 1., 1e-10, 32);
    T denom = exp(-eta->R(t[i - 1]));
    if (t[i] < INFINITY)
        denom -= exp(-eta->R(t[i]));
    return num / denom;
}

template <typename T>
T trans_integrand(const double x, trans_integrand_helper<T> *tih)
{
    const int i = tih->i, j = tih->j; 
    double h, jac;
    if (tih->right < INFINITY)
    {
        h = x * tih->right + (1. - x) * tih->left;
        jac = tih->right - tih->left;
    }
    else
    {
        h = tih->left + x / (1. - x);
        jac = 1. / (1. - x) / (1. - x);
    }
    const PiecewiseExponentialRateFunction<T> *eta = tih->eta;
    const std::vector<double> &t = eta->hidden_states;
    if (h == 0)
        return eta->eta(0) * (exp(-eta->R(t[j - 1])) - exp(-eta->R(t[j])));
    T Rh = eta->R(h);
    T eRh = exp(-Rh);
    T ret = eta->zero, tmp;
    // f1
    T htj_min = dmin(h, t[j]);
    T htj_max = dmax(h, t[j]);
    if (h < t[j])
        ret = eta->R_integral(h, -2 * Rh) * (exp(-eta->R(dmax(h, t[j - 1]))) - exp(-eta->R(t[j])));
    // f2
    if (h >= t[j - 1])
    {
        tmp = eta->R_integral(t[j - 1], -2 * eta->R(t[j - 1]) - Rh);
        tmp -= eta->R_integral(htj_min, -2 * eta->R(htj_min) - Rh);
        tmp += eRh * (htj_min - t[j - 1]);
        ret += 0.5 * tmp;
    }
    if (i == j)
        ret += 0.5 * (eRh * h - eta->R_integral(h, -3 * Rh));
    // ret = f1 + f2 + f3;
    ret *= eta->eta(h) / h;
    ret *= jac;
    if (ret < 0)
        throw std::domain_error("negative value of positive integral");
    return ret;
}

template <typename T>
T Transition<T>::trans(int i, int j)
{
    const std::vector<double> t = eta->hidden_states;
    trans_integrand_helper<T> tih = {i, j, eta, t[i - 1], t[i]};
    T num = adaptiveSimpsons(std::function<T(const double, trans_integrand_helper<T>*)>(trans_integrand<T>),
            &tih, 0., 1., 1e-10, 20);
    T denom = exp(-eta->R(t[i - 1]));
    if (t[i] != INFINITY)
        denom -= exp(-eta->R(t[i]));
    T ret = num / denom;
    if (ret > 1 or ret < 0)
        throw std::domain_error("ret is not a probability");
    return ret;
}


template <typename T>
Transition<T>::Transition(const PiecewiseExponentialRateFunction<T> &eta, const double rho) :
    eta(&eta), M(eta.hidden_states.size()), Phi(M - 1, M - 1), rho(rho)
{
    Phi.setZero();
    compute();
}

template <typename T>
void Transition<T>::compute(void)
{
    for (int i = 1; i < M; ++i)
    {
        T pnr = P_no_recomb(i);
        for (int j = 1; j < M; ++j)
        {
            T tr = trans(i, j);
            Phi(i - 1, j - 1) = (1. - pnr) * tr;
            if (i == j)
                Phi(i - 1, j - 1) += pnr;
        }
    }
}

template <typename T>
Matrix<T>& Transition<T>::matrix(void) { return Phi; }

template class Transition<double>;
template class Transition<adouble>;

int main(int argc, char** argv)
{
    std::vector<std::vector<double> > params = {
        {0.6, 1.0},
        {0.5, 1.0},
        {0.1, 0.1}
    };
    std::vector<double> hs = {0.0, 1.0, 2.0, 3.0};
    std::vector<std::pair<int, int> > deriv = { {0,0} };
    double rho = 4 * 1e4 * 1e-9;
    PiecewiseExponentialRateFunction<adouble> eta(params, deriv, hs);
    params[0][0] += 1e-8;
    PiecewiseExponentialRateFunction<double> eta2(params, deriv, hs);
    Transition<adouble> T(eta, rho);
    Transition<double> T2(eta2, rho);
    Matrix<adouble> M = T.matrix();
    std::cout << M.template cast<double>() << std::endl << std::endl;
    std::cout << M.unaryExpr([=](adouble x) { 
            if (x.derivatives().size() == 0) return 0.; 
            return x.derivatives()(0); }).template cast<double>() << std::endl << std::endl;
    Matrix<double> M2 = T2.matrix();
    std::cout << (M2 - M.template cast<double>()) * 1e8 << std::endl << std::endl;
}
