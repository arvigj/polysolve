#include "BoxConstraintSolver.hpp"

#include "LBFGSB.hpp"
#include "MMA.hpp"

namespace polysolve::nonlinear
{

    // Static constructor
    std::unique_ptr<Solver> BoxConstraintSolver::create(const std::string &solver,
                                                        const json &solver_params,
                                                        const json &linear_solver_params,
                                                        const double characteristic_length,
                                                        spdlog::logger &logger)
    {
        if (solver == "LBFGSB" || solver == "L-BFGS-B")
        {
            return std::make_unique<LBFGSB>(solver_params, characteristic_length, logger);
        }
        else if (solver == "MMA")
        {
            return std::make_unique<MMA>(solver_params, characteristic_length, logger);
        }
        throw std::runtime_error("Unrecognized solver type: " + solver);
    }

    std::vector<std::string> BoxConstraintSolver::available_solvers()
    {
        return {{"L-BFGS-B",
                 "MMA"}};
    }

    BoxConstraintSolver::BoxConstraintSolver(const json &solver_params,
                                             const double characteristic_length,
                                             spdlog::logger &logger)
        : Superclass(solver_params, characteristic_length, logger)
    {
        json box_constraint_params = solver_params["box_constraints"];
        if (box_constraint_params["max_change"] > 0)
            max_change_val_ = box_constraint_params["max_change"];
        // todo
        // else
        //     nlohmann::adl_serializer<Eigen::VectorXd>::from_json(box_constraint_params["max_change"], max_change_);

        if (box_constraint_params.contains("bounds"))
        {
            if (box_constraint_params["bounds"].is_string())
            {
                if (std::filesystem::is_regular_file(box_constraint_params["bounds"].get<std::string>()))
                {
                    // todo
                    // polyfem::io::read_matrix(box_constraint_params["bounds"].get<std::string>(), bounds_);
                    assert(bounds_.cols() == 2);
                }
            }
            else if (box_constraint_params["bounds"].is_array() && box_constraint_params["bounds"].size() == 2)
            {
                if (box_constraint_params["bounds"][0].is_number())
                {
                    bounds_.setZero(1, 2);
                    bounds_ << box_constraint_params["bounds"][0], box_constraint_params["bounds"][1];
                }
                else if (box_constraint_params["bounds"][0].is_array())
                {
                    bounds_.setZero(box_constraint_params["bounds"][0].size(), 2);
                    Eigen::VectorXd tmp;
                    // todo
                    // nlohmann::adl_serializer<Eigen::VectorXd>::from_json(box_constraint_params["bounds"][0], tmp);
                    bounds_.col(0) = tmp;
                    // todo
                    // nlohmann::adl_serializer<Eigen::VectorXd>::from_json(box_constraint_params["bounds"][1], tmp);
                    bounds_.col(1) = tmp;
                }
            }
        }
    }

    double BoxConstraintSolver::compute_grad_norm(const Eigen::VectorXd &x,
                                                  const Eigen::VectorXd &grad) const
    {
        auto min = get_lower_bound(x, false);
        auto max = get_upper_bound(x, false);

        return ((x - grad).cwiseMax(min).cwiseMin(max) - x).norm();
        // Eigen::VectorXd proj_grad = grad;
        // for (int i = 0; i < x.size(); i++)
        // 	if (x(i) < min(i) + 1e-14 || x(i) > max(i) - 1e-14)
        // 		proj_grad(i) = 0;

        // return proj_grad.norm();
    }

    Eigen::VectorXd BoxConstraintSolver::get_lower_bound(const Eigen::VectorXd &x,
                                                         bool consider_max_change) const
    {
        Eigen::VectorXd min;
        if (bounds_.rows() == x.size())
            min = bounds_.col(0);
        else if (bounds_.size() == 2)
            min = Eigen::VectorXd::Constant(x.size(), 1, bounds_(0));
        else
            log_and_throw_error(m_logger, "Invalid bounds!");

        if (consider_max_change)
            return min.array().max(x.array() - get_max_change(x).array());
        else
            return min;
    }

    Eigen::VectorXd BoxConstraintSolver::get_upper_bound(const Eigen::VectorXd &x,
                                                         bool consider_max_change) const
    {
        Eigen::VectorXd max;
        if (bounds_.rows() == x.size())
            max = bounds_.col(1);
        else if (bounds_.size() == 2)
            max = Eigen::VectorXd::Constant(x.size(), 1, bounds_(1));
        else
            log_and_throw_error(m_logger, "Invalid bounds!");

        if (consider_max_change)
            return max.array().min(x.array() + get_max_change(x).array());
        else
            return max;
    }

    Eigen::VectorXd BoxConstraintSolver::get_max_change(const Eigen::VectorXd &x) const
    {
        if (max_change_.size() == x.size())
            return max_change_;
        else if (max_change_val_ > 0)
            return Eigen::VectorXd::Ones(x.size()) * max_change_val_;
        else
            log_and_throw_error(m_logger, "Invalid max change!");

        return Eigen::VectorXd();
    }

} // namespace polysolve::nonlinear