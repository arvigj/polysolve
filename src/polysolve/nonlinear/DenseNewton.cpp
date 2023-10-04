#include "DenseNewton.hpp"
// #include <unsupported/Eigen/SparseExtra>

namespace polysolve::nonlinear
{
    DenseNewton::DenseNewton(
        const json &solver_params,
        const json &linear_solver_params,
        const double dt, const double characteristic_length,
        spdlog::logger &logger)
        : Superclass(solver_params,
                     linear_solver_params,
                     dt,
                     characteristic_length,
                     logger)
    {
        linear_solver = polysolve::linear::Solver::create(
            linear_solver_params["solver"], linear_solver_params["precond"]);
        linear_solver->setParameters(linear_solver_params);
    }

    double DenseNewton::solve_linear_system(Problem &objFunc,
                                            const TVector &x,
                                            const TVector &grad,
                                            TVector &direction)
    {
        Eigen::MatrixXd hessian;

        {
            POLYSOLVE_SCOPED_STOPWATCH("assembly time", this->assembly_time, m_logger);

            objFunc.hessian(x, hessian);

            if (reg_weight > 0)
            {
                for (int k = 0; k < x.size(); k++)
                    hessian(k, k) += reg_weight;
            }
        }

        {
            POLYSOLVE_SCOPED_STOPWATCH("linear solve", this->inverting_time, m_logger);

            try
            {
                linear_solver->analyzePattern_dense(hessian, hessian.rows());
                linear_solver->factorize_dense(hessian);
                linear_solver->solve(-grad, direction);
            }
            catch (const std::runtime_error &err)
            {
                increase_descent_strategy();

                // warn if using gradient descent
                m_logger.log(
                    log_level(), "Unable to factorize Hessian: \"{}\"; reverting to {}",
                    err.what(), this->descent_strategy_name());

                // polyfem::write_sparse_matrix_csv("problematic_hessian.csv", hessian);
                return std::nan("");
            }
        }

        const double residual = (hessian * direction + grad).norm(); // H Δx + g = 0

        json info;
        linear_solver->getInfo(info);
        internal_solver_info.push_back(info);

        return residual;
    }
} // namespace polysolve::nonlinear
