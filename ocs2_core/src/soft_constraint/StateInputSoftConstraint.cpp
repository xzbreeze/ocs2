/******************************************************************************
Copyright (c) 2020, Farbod Farshidian. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of its
  contributors may be used to endorse or promote products derived from
  this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
******************************************************************************/

#include <ocs2_core/soft_constraint/StateInputSoftConstraint.h>

namespace ocs2 {

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
StateInputSoftConstraint::StateInputSoftConstraint(std::unique_ptr<StateInputConstraint> constraintPtr,
                                                   std::vector<std::unique_ptr<PenaltyFunctionBase>> penaltyFunctionPtrArray,
                                                   ConstraintOrder constraintOrder)
    : constraintPtr_(std::move(constraintPtr)), penalty_(std::move(penaltyFunctionPtrArray)), constraintOrder_(constraintOrder) {}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
StateInputSoftConstraint::StateInputSoftConstraint(std::unique_ptr<StateInputConstraint> constraintPtr, size_t numConstraints,
                                                   std::unique_ptr<PenaltyFunctionBase> penaltyFunction, ConstraintOrder constraintOrder)
    : constraintPtr_(std::move(constraintPtr)), penalty_(numConstraints, std::move(penaltyFunction)), constraintOrder_(constraintOrder) {}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
StateInputSoftConstraint::StateInputSoftConstraint(const StateInputSoftConstraint& other)
    : constraintPtr_(other.constraintPtr_->clone()), penalty_(other.penalty_), constraintOrder_(other.constraintOrder_) {}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
StateInputSoftConstraint* StateInputSoftConstraint::clone() const {
  return new StateInputSoftConstraint(*this);
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
scalar_t StateInputSoftConstraint::getValue(scalar_t time, const vector_t& state, const vector_t& input,
                                            const CostDesiredTrajectories&) const {
  return penalty_.getValue(constraintPtr_->getValue(time, state, input));
}

/******************************************************************************************************/
/******************************************************************************************************/
/******************************************************************************************************/
ScalarFunctionQuadraticApproximation StateInputSoftConstraint::getQuadraticApproximation(scalar_t time, const vector_t& state,
                                                                                         const vector_t& input,
                                                                                         const CostDesiredTrajectories&) const {
  switch (constraintOrder_) {
    case ConstraintOrder::Linear:
      return penalty_.getQuadraticApproximation(constraintPtr_->getLinearApproximation(time, state, input));
    case ConstraintOrder::Quadratic:
      return penalty_.getQuadraticApproximation(constraintPtr_->getQuadraticApproximation(time, state, input));
  }
}

}  // namespace ocs2