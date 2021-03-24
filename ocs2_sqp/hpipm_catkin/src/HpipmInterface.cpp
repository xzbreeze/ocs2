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

#include "hpipm_catkin/HpipmInterface.h"

extern "C" {
#include <hpipm_d_ocp_qp.h>
#include <hpipm_d_ocp_qp_dim.h>
#include <hpipm_d_ocp_qp_ipm.h>
#include <hpipm_d_ocp_qp_sol.h>
#include <hpipm_timing.h>
}

namespace {
/**
 * Manages a block of memory. Allows reuse of memory blocks if the required size does not exceed the old size.
 */
class MemoryBlock {
 public:
  /** No memory allocated */
  MemoryBlock() : MemoryBlock(0){};

  /** Allocate memory of requested size */
  explicit MemoryBlock(size_t size) : ptr_(nullptr), size_(0) { reserve(size); }
  ~MemoryBlock() noexcept { free(ptr_); }

  /**
   * Ensure a block of memory of at least the requested size.
   * Does nothing if the requested size is smaller than equal to the current size.
   * @param size : minimum size of the memory block.
   */
  void reserve(size_t size) {
    if (size > size_) {
      free(ptr_);
      ptr_ = malloc(size);
      if (ptr_ == nullptr) {
        throw std::bad_alloc();
      } else {
        size_ = size;
      }
    }
  }

  /** Get pointer to the memory, might be nullptr */
  void* get() { return ptr_; };

  // Prevents copies
  MemoryBlock(const MemoryBlock&) = delete;
  MemoryBlock& operator=(const MemoryBlock&) = delete;

  // Default move
  MemoryBlock(MemoryBlock&&) = default;
  MemoryBlock& operator=(MemoryBlock&&) = default;

 private:
  void* ptr_;
  size_t size_;
};
}  // namespace

namespace ocs2 {

class HpipmInterface::Impl {
 public:
  Impl(OcpSize ocpSize, const Settings& settings) { initializeMemory(std::move(ocpSize), settings); }

  void initializeMemory(OcpSize ocpSize) { initializeMemory(std::move(ocpSize), settings_); }

  void initializeMemory(OcpSize ocpSize, const Settings& settings) {
    ocpSize.nx[0] = 0;

    if (isSizeEqual(ocpSize) && isSettingsEqual(settings)) {
      return;
    }

    settings_ = settings;
    ocpSize_ = std::move(ocpSize);

    const int dim_size = d_ocp_qp_dim_memsize(ocpSize_.N);
    dimMem_.reserve(dim_size);
    d_ocp_qp_dim_create(ocpSize_.N, &dim_, dimMem_.get());
    d_ocp_qp_dim_set_all(ocpSize_.nx.data(), ocpSize_.nu.data(), ocpSize_.nbx.data(), ocpSize_.nbu.data(), ocpSize_.ng.data(),
                         ocpSize_.nsbx.data(), ocpSize_.nsbu.data(), ocpSize_.nsg.data(), &dim_);

    const int qp_size = d_ocp_qp_memsize(&dim_);
    qpMem_.reserve(qp_size);
    d_ocp_qp_create(&dim_, &qp_, qpMem_.get());

    const int qp_sol_size = d_ocp_qp_sol_memsize(&dim_);
    qpSolMem_.reserve(qp_sol_size);
    d_ocp_qp_sol_create(&dim_, &qpSol_, qpSolMem_.get());

    const int ipm_arg_size = d_ocp_qp_ipm_arg_memsize(&dim_);
    ipmArgMem_.reserve(ipm_arg_size);
    d_ocp_qp_ipm_arg_create(&dim_, &arg_, ipmArgMem_.get());

    applySettings(settings_);

    // Setup workspace after applying the settings
    const int ipm_size = d_ocp_qp_ipm_ws_memsize(&dim_, &arg_);
    ipmMem_.reserve(ipm_size);
    d_ocp_qp_ipm_ws_create(&dim_, &arg_, &workspace_, ipmMem_.get());
  }

  bool isSizeEqual(const OcpSize& ocpSize) const {
    // use && instead of &= to enable short-circuit evaluation
    bool same = ocpSize_.N == ocpSize.N;
    same = same && (ocpSize_.nu == ocpSize.nu);
    same = same && (ocpSize_.nx == ocpSize.nx);
    same = same && (ocpSize_.nbu == ocpSize.nbu);
    same = same && (ocpSize_.nbx == ocpSize.nbx);
    same = same && (ocpSize_.ng == ocpSize.ng);
    same = same && (ocpSize_.nsbu == ocpSize.nsbu);
    same = same && (ocpSize_.nsbx == ocpSize.nsbx);
    same = same && (ocpSize_.nsg == ocpSize.nsg);
    return same;
  }

  bool isSettingsEqual(const Settings& settings) const {
    // use && instead of &= to enable short-circuit evaluation
    bool same = (settings_.hpipmMode == settings.hpipmMode);
    same = same && (settings_.iter_max == settings.iter_max);
    same = same && (settings_.alpha_min == settings.alpha_min);
    same = same && (settings_.mu0 == settings.mu0);
    same = same && (settings_.tol_stat == settings.tol_stat);
    same = same && (settings_.tol_eq == settings.tol_eq);
    same = same && (settings_.tol_ineq == settings.tol_ineq);
    same = same && (settings_.tol_comp == settings.tol_comp);
    same = same && (settings_.reg_prim == settings.reg_prim);
    same = same && (settings_.warm_start == settings.warm_start);
    same = same && (settings_.pred_corr == settings.pred_corr);
    same = same && (settings_.ric_alg == settings.ric_alg);
    return same;
  }

  void applySettings(Settings& settings) {
    d_ocp_qp_ipm_arg_set_default(settings.hpipmMode, &arg_);
    d_ocp_qp_ipm_arg_set_iter_max(&settings.iter_max, &arg_);
    d_ocp_qp_ipm_arg_set_alpha_min(&settings.alpha_min, &arg_);
    d_ocp_qp_ipm_arg_set_mu0(&settings.mu0, &arg_);
    d_ocp_qp_ipm_arg_set_tol_stat(&settings.tol_stat, &arg_);
    d_ocp_qp_ipm_arg_set_tol_eq(&settings.tol_eq, &arg_);
    d_ocp_qp_ipm_arg_set_tol_ineq(&settings.tol_ineq, &arg_);
    d_ocp_qp_ipm_arg_set_tol_comp(&settings.tol_comp, &arg_);
    d_ocp_qp_ipm_arg_set_reg_prim(&settings.reg_prim, &arg_);
    d_ocp_qp_ipm_arg_set_warm_start(&settings.warm_start, &arg_);
    d_ocp_qp_ipm_arg_set_pred_corr(&settings.pred_corr, &arg_);
    d_ocp_qp_ipm_arg_set_ric_alg(&settings.ric_alg, &arg_);
  }

  hpipm_status solve(const vector_t& x0, std::vector<VectorFunctionLinearApproximation>& dynamics,
                     std::vector<ScalarFunctionQuadraticApproximation>& cost, std::vector<VectorFunctionLinearApproximation>* constraints,
                     std::vector<vector_t>& stateTrajectory, std::vector<vector_t>& inputTrajectory, bool verbose) {
    assert(dynamics.size() == ocpSize_.N);
    assert(cost.size() == (ocpSize_.N + 1));
    // TODO: check state input size.

    // Dynamics
    std::vector<scalar_t*> AA(ocpSize_.N);
    std::vector<scalar_t*> BB(ocpSize_.N);
    std::vector<scalar_t*> bb(ocpSize_.N);

    // Costs (all must be N+1) eventhough nu[N] = 0;
    std::vector<scalar_t*> QQ(ocpSize_.N + 1);
    std::vector<scalar_t*> RR(ocpSize_.N + 1);
    std::vector<scalar_t*> SS(ocpSize_.N + 1);
    std::vector<scalar_t*> qq(ocpSize_.N + 1);
    std::vector<scalar_t*> rr(ocpSize_.N + 1);

    // Constraints (all must be N+1) eventhough nu[N] = 0;
    std::vector<ocs2::vector_t> boundData;
    std::vector<scalar_t*> CC(ocpSize_.N + 1);
    std::vector<scalar_t*> DD(ocpSize_.N + 1);
    std::vector<scalar_t*> llg(ocpSize_.N + 1);
    std::vector<scalar_t*> uug(ocpSize_.N + 1);

    // Dynamics k = 0. Absorb initial state into dynamics
    vector_t b0 = dynamics[0].f;
    b0.noalias() += dynamics[0].dfdx * x0;
    AA[0] = dynamics[0].dfdx.data();
    BB[0] = dynamics[0].dfdu.data();
    bb[0] = b0.data();

    // Dynamics k = 1 -> N-1
    for (int k = 1; k < ocpSize_.N; k++) {
      AA[k] = dynamics[k].dfdx.data();
      BB[k] = dynamics[k].dfdu.data();
      bb[k] = dynamics[k].f.data();
    }

    // Cost k = 0. Elimination of initial state requires cost adaptation
    vector_t r0 = cost[0].dfdu;
    r0 += cost[0].dfdux * x0;
    QQ[0] = cost[0].dfdxx.data();
    RR[0] = cost[0].dfduu.data();
    SS[0] = cost[0].dfdux.data();
    qq[0] = cost[0].dfdx.data();  // q[0] would change, but it doesn't matter because x[0] is not a decision variable.
    rr[0] = r0.data();

    // Cost k = 1 -> N
    for (int k = 1; k < (ocpSize_.N + 1); k++) {
      QQ[k] = cost[k].dfdxx.data();
      RR[k] = cost[k].dfduu.data();
      SS[k] = cost[k].dfdux.data();
      qq[k] = cost[k].dfdx.data();
      rr[k] = cost[k].dfdu.data();
    }

    if (constraints != nullptr) {
      auto& constr = *constraints;
      // for ocs2 --> C*dx + D*du + e = 0
      // for hpipm --> ug >= C*dx + D*du >= lg
      boundData.reserve(ocpSize_.N + 1);

      for (int k = 0; k < ocpSize_.N + 1; k++) {
        CC[k] = constr[k].dfdx.data();
        DD[k] = constr[k].dfdu.data();
        boundData.emplace_back(-constr[k].f);
        if (k == 0) {  // Initial constraint
          boundData[k].noalias() -= constr[0].dfdx * x0;
        }
        llg[k] = boundData[k].data();
        uug[k] = boundData[k].data();
      }
    }

    int** hidxbx = nullptr;
    scalar_t** hlbx = nullptr;
    scalar_t** hubx = nullptr;
    int** hidxbu = nullptr;
    scalar_t** hlbu = nullptr;
    scalar_t** hubu = nullptr;
    scalar_t** hZl = nullptr;
    scalar_t** hZu = nullptr;
    scalar_t** hzl = nullptr;
    scalar_t** hzu = nullptr;
    int** hidxs = nullptr;
    scalar_t** hlls = nullptr;
    scalar_t** hlus = nullptr;
    d_ocp_qp_set_all(AA.data(), BB.data(), bb.data(), QQ.data(), SS.data(), RR.data(), qq.data(), rr.data(), hidxbx, hlbx, hubx, hidxbu,
                     hlbu, hubu, CC.data(), DD.data(), llg.data(), uug.data(), hZl, hZu, hzl, hzu, hidxs, hlls, hlus, &qp_);

    d_ocp_qp_ipm_solve(&qp_, &qpSol_, &arg_, &workspace_);

    if (verbose) {
      printStatus();
    }

    getStateSolution(x0, stateTrajectory);
    getInputSolution(inputTrajectory);

    // Return solver status
    int hpipmStatus = -1;
    d_ocp_qp_ipm_get_status(&workspace_, &hpipmStatus);
    return hpipm_status(hpipmStatus);
  }

  void getStateSolution(const vector_t& x0, std::vector<vector_t>& stateTrajectory) {
    stateTrajectory.resize(ocpSize_.N + 1);
    stateTrajectory.front() = x0;
    for (int k = 1; k < (ocpSize_.N + 1); ++k) {
      stateTrajectory[k].resize(ocpSize_.nx[k]);
      d_ocp_qp_sol_get_x(k, &qpSol_, stateTrajectory[k].data());
    }
  }

  void getInputSolution(std::vector<vector_t>& inputTrajectory) {
    inputTrajectory.resize(ocpSize_.N);
    for (int k = 0; k < ocpSize_.N; ++k) {
      inputTrajectory[k].resize(ocpSize_.nu[k]);
      d_ocp_qp_sol_get_u(k, &qpSol_, inputTrajectory[k].data());
    }
  }

  void getRiccatiZeroStage(const matrix_t& A0, const matrix_t& B0, const vector_t& b0, const matrix_t& Q0, const matrix_t& R0,
                           const matrix_t& S0, const vector_t& q0, const vector_t& r0, matrix_t& P0, matrix_t& K0, vector_t& p0,
                           vector_t& k0) {
    // at zero stage, I think P, p, K, k should be computed together -- Yang

    // fake_k0 is directly returned by hpipm, I haven't figure out its meaning yet.
    vector_t fake_k0;
    fake_k0.resize(ocpSize_.nu[0]);
    d_ocp_qp_ipm_get_ric_k(&qp_, &arg_, &workspace_, 0, fake_k0.data());

    matrix_t P1, Lr0, Lr0_inv;
    vector_t p1;
    P1.resize(ocpSize_.nx[1], ocpSize_.nx[1]);
    Lr0.resize(ocpSize_.nu[0], ocpSize_.nu[0]);
    Lr0_inv.resize(ocpSize_.nu[0], ocpSize_.nu[0]);
    p1.resize(ocpSize_.nx[1]);
    d_ocp_qp_ipm_get_ric_P(&qp_, &arg_, &workspace_, 1, P1.data());
    d_ocp_qp_ipm_get_ric_Lr(&qp_, &arg_, &workspace_, 0, Lr0.data());
    d_ocp_qp_ipm_get_ric_p(&qp_, &arg_, &workspace_, 1, p1.data());
    Lr0_inv = Lr0.inverse();

    int nx0 = A0.rows();  // in hpipm setting, nx0 = 0, so we need to get it from A0 matrix dimension
    K0.resize(ocpSize_.nu[0], nx0);
    P0.resize(nx0, nx0);
    k0.resize(ocpSize_.nu[0]);
    p0.resize(nx0);

    K0 = -(Lr0_inv.transpose() * Lr0_inv) * (S0 + B0.transpose() * P1 * A0);
    k0 = -(Lr0_inv.transpose() * Lr0_inv) * (r0 + B0.transpose() * p1 + B0.transpose() * P1 * b0);
    P0 = Q0 + A0.transpose() * P1 * A0 -
         (S0 + B0.transpose() * P1 * A0).transpose() * (Lr0_inv.transpose() * Lr0_inv) * (S0 + B0.transpose() * P1 * A0);
    p0 = q0 + A0.transpose() * p1 + A0.transpose() * P1 * b0 -
         (S0 + B0.transpose() * P1 * A0).transpose() * (Lr0_inv.transpose() * Lr0_inv) *
             (r0 + B0.transpose() * p1 + B0.transpose() * P1 * b0);
  }

  void getRiccatiFeedbackFeedforward(std::vector<matrix_t>& KMatrics, std::vector<vector_t>& kVectors) {
    KMatrics.resize(ocpSize_.N);
    kVectors.resize(ocpSize_.N);
    matrix_t Lr, Ls;
    for (int k = 1; k < ocpSize_.N; ++k) {
      Lr.resize(ocpSize_.nu[k], ocpSize_.nu[k]);
      Ls.resize(ocpSize_.nx[k], ocpSize_.nu[k]);
      KMatrics[k].resize(ocpSize_.nu[k], ocpSize_.nx[k]);
      d_ocp_qp_ipm_get_ric_Lr(&qp_, &arg_, &workspace_, k, Lr.data());
      d_ocp_qp_ipm_get_ric_Ls(&qp_, &arg_, &workspace_, k, Ls.data());
      KMatrics[k] = -(Ls * Lr.inverse()).transpose();

      kVectors[k].resize(ocpSize_.nu[k]);
      d_ocp_qp_ipm_get_ric_k(&qp_, &arg_, &workspace_, k, kVectors[k].data());
    }
  }

  void getRiccatiCostToGo(std::vector<matrix_t>& PMatrics, std::vector<vector_t>& pVectors) {
    PMatrics.resize(ocpSize_.N + 1);
    pVectors.resize(ocpSize_.N + 1);

    PMatrics[ocpSize_.N].resize(ocpSize_.nx[ocpSize_.N], ocpSize_.nx[ocpSize_.N]);
    pVectors[ocpSize_.N].resize(ocpSize_.nx[ocpSize_.N]);
    d_ocp_qp_ipm_get_ric_P(&qp_, &arg_, &workspace_, ocpSize_.N, PMatrics[ocpSize_.N].data());
    d_ocp_qp_ipm_get_ric_p(&qp_, &arg_, &workspace_, ocpSize_.N, pVectors[ocpSize_.N].data());

    for (int k = 1; k < ocpSize_.N; ++k) {
      PMatrics[k].resize(ocpSize_.nx[k], ocpSize_.nx[k]);
      pVectors[k].resize(ocpSize_.nx[k]);
      d_ocp_qp_ipm_get_ric_P(&qp_, &arg_, &workspace_, k, PMatrics[k].data());
      d_ocp_qp_ipm_get_ric_p(&qp_, &arg_, &workspace_, k, pVectors[k].data());
    }
  }

  void printStatus() {
    int hpipmStatus = -1;
    d_ocp_qp_ipm_get_status(&workspace_, &hpipmStatus);
    fprintf(stderr, "\n=== HPIPM ===\n");
    fprintf(stderr, "HPIPM returned with flag %i. -> ", hpipmStatus);
    if (hpipmStatus == 0) {
      fprintf(stderr, "QP solved!\n");
    } else if (hpipmStatus == 1) {
      fprintf(stderr, "Solver failed! Maximum number of iterations reached\n");
    } else if (hpipmStatus == 2) {
      fprintf(stderr, "Solver failed! Minimum step length reached\n");
    } else if (hpipmStatus == 3) {
      fprintf(stderr, "Solver failed! NaN in computations\n");
    } else if (hpipmStatus == 4) {
      fprintf(stderr, "Solver failed! Unconsistent equality constraints\n");
    } else {
      fprintf(stderr, "Solver failed! Unknown return flag\n");
    }

    int iter;
    d_ocp_qp_ipm_get_iter(&workspace_, &iter);
    scalar_t res_stat;
    d_ocp_qp_ipm_get_max_res_stat(&workspace_, &res_stat);
    scalar_t res_eq;
    d_ocp_qp_ipm_get_max_res_eq(&workspace_, &res_eq);
    scalar_t res_ineq;
    d_ocp_qp_ipm_get_max_res_ineq(&workspace_, &res_ineq);
    scalar_t res_comp;
    d_ocp_qp_ipm_get_max_res_comp(&workspace_, &res_comp);
    scalar_t* stat;
    d_ocp_qp_ipm_get_stat(&workspace_, &stat);
    int stat_m;
    d_ocp_qp_ipm_get_stat_m(&workspace_, &stat_m);
    fprintf(stderr, "ipm iter = %d\n", iter);
    fprintf(stderr, "ipm residuals max: res_g = %e, res_b = %e, res_d = %e, res_m = %e\n", res_stat, res_eq, res_ineq, res_comp);
    fprintf(stderr,
            "\nalpha_aff\tmu_aff\t\tsigma\t\talpha_prim\talpha_dual\tmu\t\tres_stat\tres_eq\t\tres_ineq\tres_comp\tlq fact\t\titref "
            "pred\titref corr\tlin res stat\tlin res eq\tlin res ineq\tlin res comp\n");
    {  // print Stats. Implementation adapted from d_print_exp_tran_mat
      for (int j = 0; j < iter + 1; j++) {
        for (int i = 0; i < stat_m; i++) {
          fprintf(stderr, "%e\t", stat[i + stat_m * j]);
        }
        fprintf(stderr, "\n");
      }
    }
  }

 private:
  Settings settings_;
  OcpSize ocpSize_;

  MemoryBlock dimMem_;
  d_ocp_qp_dim dim_;

  MemoryBlock qpMem_;
  d_ocp_qp qp_;

  MemoryBlock qpSolMem_;
  d_ocp_qp_sol qpSol_;

  MemoryBlock ipmArgMem_;
  d_ocp_qp_ipm_arg arg_;

  MemoryBlock ipmMem_;
  d_ocp_qp_ipm_ws workspace_;
};

HpipmInterface::HpipmInterface(OcpSize ocpSize, const Settings& settings)
    : pImpl_(new HpipmInterface::Impl(std::move(ocpSize), settings)) {}

HpipmInterface::~HpipmInterface() = default;

void HpipmInterface::resize(OcpSize ocpSize) {
  pImpl_->initializeMemory(std::move(ocpSize));
}

void HpipmInterface::resize(OcpSize ocpSize, const Settings& settings) {
  pImpl_->initializeMemory(std::move(ocpSize), settings);
}

hpipm_status HpipmInterface::solve(const vector_t& x0, std::vector<VectorFunctionLinearApproximation>& dynamics,
                                   std::vector<ScalarFunctionQuadraticApproximation>& cost,
                                   std::vector<VectorFunctionLinearApproximation>* constraints, std::vector<vector_t>& stateTrajectory,
                                   std::vector<vector_t>& inputTrajectory, bool verbose) {
  return pImpl_->solve(x0, dynamics, cost, constraints, stateTrajectory, inputTrajectory, verbose);
}

void HpipmInterface::getRiccatiCostToGo(std::vector<matrix_t>& PMatrics, std::vector<vector_t>& pVectors) {
  pImpl_->getRiccatiCostToGo(PMatrics, pVectors);
}

void HpipmInterface::getRiccatiFeedbackFeedforward(std::vector<matrix_t>& KMatrics, std::vector<vector_t>& kVectors) {
  pImpl_->getRiccatiFeedbackFeedforward(KMatrics, kVectors);
}

void HpipmInterface::getRiccatiZeroStage(const matrix_t& A0, const matrix_t& B0, const vector_t& b0, const matrix_t& Q0, const matrix_t& R0,
                                         const matrix_t& S0, const vector_t& q0, const vector_t& r0, matrix_t& P0, matrix_t& K0,
                                         vector_t& p0, vector_t& k0) {
  pImpl_->getRiccatiZeroStage(A0, B0, b0, Q0, R0, S0, q0, r0, P0, K0, p0, k0);
}

}  // namespace ocs2