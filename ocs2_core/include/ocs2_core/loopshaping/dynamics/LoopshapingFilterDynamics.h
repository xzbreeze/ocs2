//
// Created by rgrandia on 08.05.19.
//

#ifndef OCS2_LOOPSHAPINGFILTERDYNAMICS_H
#define OCS2_LOOPSHAPINGFILTERDYNAMICS_H

#include <ocs2_core/loopshaping/LoopshapingDefinition.h>
#include <ocs2_core/integration/ODE_func.h>
#include "ocs2_core/integration/Integrator.h"

namespace ocs2 {

template<size_t FULL_STATE_DIM, size_t FULL_INPUT_DIM,
    size_t SYSTEM_STATE_DIM, size_t SYSTEM_INPUT_DIM,
    size_t FILTER_STATE_DIM, size_t FILTER_INPUT_DIM>
class LoopshapingFilterDynamics {
 public:
  EIGEN_MAKE_ALIGNED_OPERATOR_NEW

  using Ptr = std::shared_ptr<LoopshapingFilterDynamics>;

  using input_vector_t = Eigen::Matrix<double, FULL_INPUT_DIM, 1>;

  using FILTER_DIMENSIONS = Dimensions<FILTER_STATE_DIM, FILTER_INPUT_DIM>;
  using filter_state_vector_t = typename FILTER_DIMENSIONS::state_vector_t;
  using filter_state_array_t = typename FILTER_DIMENSIONS::state_vector_array_t;
  using scalar_array_t = typename FILTER_DIMENSIONS::scalar_array_t;

  LoopshapingFilterDynamics(std::shared_ptr<LoopshapingDefinition> loopshapingDefinition) :
      loopshapingDefinition_(std::move(loopshapingDefinition)),
      ode_fun_(new ODE_func<FILTER_STATE_DIM>(std::bind(&LoopshapingFilterDynamics::computeFlowMap,
                                                        this,
                                                        std::placeholders::_1,
                                                        std::placeholders::_2,
                                                        input_vector_t::Zero(),
                                                        std::placeholders::_3))),
      integrator_(ode_fun_)
      {
          filter_state_.setZero();
      }

  void integrate(double dt, const input_vector_t &input) {
    // Set up ODE with ZOH input
    ode_fun_->setFlowMap(std::bind(&LoopshapingFilterDynamics::computeFlowMap,
                                  this,
                                  std::placeholders::_1,
                                  std::placeholders::_2,
                                  input,
                                  std::placeholders::_3));

    // integrate with integrator that has a shared_ptr to ode_fun_
    filter_state_array_t stateTrajectory;
    scalar_array_t timeTrajectory;
    integrator_.integrate(filter_state_, 0.0, dt, stateTrajectory, timeTrajectory, dt);

    filter_state_ = stateTrajectory.back();
  }

  void computeFlowMap(const double &time,
                      const filter_state_vector_t &filter_state,
                      const input_vector_t &input,
                      filter_state_vector_t &filterStateDerivative) const {
    const auto &filter = loopshapingDefinition_->getInputFilter();
    switch (loopshapingDefinition_->getType()) {
      case LoopshapingType::outputpattern :
        filterStateDerivative = filter.getA() * filter_state + filter.getB() * input;
        break;
      case LoopshapingType::inputpattern :
      case LoopshapingType::eliminatepattern :
        filterStateDerivative = filter.getA() * filter_state + filter.getB() * input.tail(FILTER_INPUT_DIM);
        break;
    }
  }

  void setFilterState(const filter_state_vector_t &filter_state) { filter_state_ = filter_state; };
  void getFilterState(filter_state_vector_t &filter_state) const { filter_state = filter_state_; };

 private:
  std::shared_ptr<LoopshapingDefinition> loopshapingDefinition_;
  filter_state_vector_t filter_state_;
  std::shared_ptr<ODE_func<FILTER_STATE_DIM> > ode_fun_;
  ODE45<FILTER_STATE_DIM> integrator_;

};

} // namespace ocs2

#endif //OCS2_LOOPSHAPINGFILTERDYNAMICS_H