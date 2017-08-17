/*
 * QuadraticCostFunction.h
 *
 *  Created on: Jan 3, 2016
 *      Author: farbod
 */

#ifndef QUADRATICCOSTFUNCTION_OCS2_H_
#define QUADRATICCOSTFUNCTION_OCS2_H_

#include "ocs2_core/cost/CostFunctionBaseOCS2.h"

namespace ocs2{

/**
 * Quadratic Cost Function
 * @tparam STATE_DIM
 * @tparam CONTROL_DIM
 */
template <size_t STATE_DIM, size_t CONTROL_DIM>
class QuadraticCostFunction : public CostFunctionBaseOCS2< STATE_DIM, CONTROL_DIM >
{
public:
	EIGEN_MAKE_ALIGNED_OPERATOR_NEW

	typedef Dimensions<STATE_DIM, CONTROL_DIM> DIMENSIONS;
	typedef typename DIMENSIONS::scalar_t scalar_t;
	typedef typename DIMENSIONS::state_vector_t state_vector_t;
	typedef typename DIMENSIONS::state_matrix_t state_matrix_t;
	typedef typename DIMENSIONS::control_vector_t control_vector_t;
	typedef typename DIMENSIONS::control_matrix_t control_matrix_t;
	typedef typename DIMENSIONS::control_feedback_t control_feedback_t;

	/**
	 * Constructor
	 * @param [in] Q
	 * @param [in] R
	 * @param [in] x_nominal
	 * @param [in] u_nominal
	 * @param [in] x_final
	 * @param [in] Q_final
	 */
	QuadraticCostFunction(const state_matrix_t& Q,
			const control_matrix_t& R,
	    const state_vector_t& x_nominal,
	    const control_vector_t& u_nominal,
	    const state_vector_t& x_final,
	    const state_matrix_t Q_final)
		: Q_(Q),
		  R_(R),
		  x_nominal_(x_nominal),
		  u_nominal_(u_nominal),
		  x_final_(x_final),
		  Q_final_(Q_final)
	{}

	virtual ~QuadraticCostFunction() {}

	/**
	 * Sets current state and control
	 * @param [in] t
	 * @param [in] x
	 * @param [in] u
	 */
	virtual void setCurrentStateAndControl(const scalar_t& t, const state_vector_t& x, const control_vector_t& u) {

		CostFunctionBase< STATE_DIM, CONTROL_DIM >::setCurrentStateAndControl(t, x, u);
		x_deviation_ = x - x_nominal_;
		u_deviation_ = u - u_nominal_;
	}

	/**
	 * Evaluates the cost
	 * @param [out] cost
	 */
	void evaluate(scalar_t& cost) 	{

		scalar_t costQ = 0.5 * x_deviation_.transpose() * Q_ * x_deviation_;
		scalar_t costR = 0.5 * u_deviation_.transpose() * R_ * u_deviation_;
		cost = costQ + costR;
	}

	/**
	 * Gets the state derivative cost
	 * @param [out] cost
	 */
	void stateDerivative(state_vector_t& cost)  {
		cost =  Q_ * x_deviation_;
	}

	/**
	 * Gets state second order derivative
	 * @param [out] cost
	 */
	void stateSecondDerivative(state_matrix_t& cost)  {
		cost = Q_;
	}

	/**
	 * Gets control derivative
	 * @param [out] cost
	 */
	void controlDerivative(control_vector_t& cost)	{
		cost = R_ * u_deviation_;
	}

	/**
	 * Gets control second derivative
	 * @param [out] cost
	 */
	void controlSecondDerivative(control_matrix_t& cost)	{
		cost = R_;
	}

	/**
	 * Gets the state control derivative
	 * @param [out] cost
	 */
	void stateControlDerivative(control_feedback_t& cost)	{
		cost = control_feedback_t::Zero();
	}

	/**
	 * Gets the terminal cost
	 * @param [out] cost
	 */
	void terminalCost(scalar_t& cost)	{

		state_vector_t x_deviation_final = this->x_ - x_final_;
		cost = 0.5 * x_deviation_final.transpose() * Q_final_ * x_deviation_final;
	}

	/**
	 * Gets the terminal cost state derivative
	 * @param [out] cost
	 */
	void terminalCostStateDerivative(state_vector_t& cost)	{

		state_vector_t x_deviation_final = this->x_ - x_final_;
		cost = Q_final_ * x_deviation_final;
	}

	/**
	 * Gets the terminal cost state second derivative
	 * @param [out] cost
	 */
	void terminalCostStateSecondDerivative(state_matrix_t& cost)	{
		cost = Q_final_;
	}

	/**
	 * Returns pointer to CostFunctionOCS2
	 * @return
	 */
	std::shared_ptr<CostFunctionBase<STATE_DIM, CONTROL_DIM> > clone() const {
		typedef QuadraticCostFunction<STATE_DIM, CONTROL_DIM> quadratic_cost_t;
		return std::allocate_shared<quadratic_cost_t, Eigen::aligned_allocator<quadratic_cost_t>>(
				Eigen::aligned_allocator<quadratic_cost_t>(),*this);
	}

protected:
	state_vector_t x_deviation_;
	state_vector_t x_nominal_;
	state_matrix_t Q_;

	control_vector_t u_deviation_;
	control_vector_t u_nominal_;
	control_matrix_t R_;

	state_vector_t x_final_;
	state_matrix_t Q_final_;

};

} // namespace ocs2

#endif /* QUADRATICCOSTFUNCTION_H_ */
