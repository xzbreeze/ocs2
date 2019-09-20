/******************************************************************************
Copyright (c) 2017, Farbod Farshidian. All rights reserved.

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

#include <ocs2_comm_interfaces/ocs2_ros_interfaces/mrt/MRT_ROS_Interface.h>

#include "ocs2_double_slit_example/DoubleSlitInterface.h"
#include "ocs2_double_slit_example/definitions.h"
#include "ocs2_double_slit_example/ros_comm/MRT_ROS_Dummy_Double_Slit.h"

int main(int argc, char* argv[]) {
  // task file
  if (argc <= 1) {
    throw std::runtime_error("No task file specified. Aborting.");
  }
  std::string taskFileFolderName(argv[1]);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

  // doubleSlitInterface
  using ocs2::double_slit::DoubleSlitInterface;
  DoubleSlitInterface doubleSlitInterface(taskFileFolderName);

  using ocs2::double_slit::INPUT_DIM_;
  using ocs2::double_slit::STATE_DIM_;

  // Rollout
  double rollout_dt;
  ocs2::loadData::loadCppDataType(doubleSlitInterface.taskFile_, "pathIntegral.rollout_settings.minTimeStep", rollout_dt);
  ocs2::Rollout_Settings rolloutSettings(1e-9, 1e-6, 5000, rollout_dt, ocs2::IntegratorType::EULER, false, true);
  using rollout_t = ocs2::TimeTriggeredRollout<STATE_DIM_, INPUT_DIM_>;
  std::unique_ptr<rollout_t> rollout(new rollout_t(doubleSlitInterface.getDynamics(), rolloutSettings));

  // Dummy double_slit
  using ocs2::double_slit::MrtRosDummyDoubleSlit;
  ocs2::MRT_ROS_Interface<STATE_DIM_, INPUT_DIM_> mrt("double_slit");
  mrt.initRollout(std::move(rollout));
  MrtRosDummyDoubleSlit dummyDoubleSlit(mrt, doubleSlitInterface.mpcSettings().mrtDesiredFrequency_,
                                        doubleSlitInterface.mpcSettings().mpcDesiredFrequency_);
  dummyDoubleSlit.launchNodes(argc, argv);

  // Run dummy
  MrtRosDummyDoubleSlit::system_observation_t initObservation;
  doubleSlitInterface.getInitialState(initObservation.state());

  MrtRosDummyDoubleSlit::cost_desired_trajectories_t initCostDesiredTraj;
  initCostDesiredTraj.desiredTimeTrajectory().push_back(0.0);
  initCostDesiredTraj.desiredInputTrajectory().push_back(DoubleSlitInterface::input_vector_t::Zero());
  initCostDesiredTraj.desiredStateTrajectory().push_back(DoubleSlitInterface::state_vector_t::Zero());

  dummyDoubleSlit.run(initObservation, initCostDesiredTraj);

  return 0;
}
