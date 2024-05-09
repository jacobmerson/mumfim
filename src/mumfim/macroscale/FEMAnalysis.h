#ifndef MUMFIM_TISSUE_ANALYSIS_H_
#define MUMFIM_TISSUE_ANALYSIS_H_
#include <amsiNonlinearAnalysis.h>
#include "NonlinearTissueStep.h"
#include <model_traits/CategoryNode.h>
namespace mumfim
{
  /**
   * Extracts the volume convergence regions from a simmetrix case
   * @param solution_strategy associated solution strategy node
   * @param las the amsi las system used for convergence
   * @param out typically std::back_inserter to some sort of list type.
   */
  template <typename O>
  void buildLASConvergenceOperators(
      const mt::CategoryNode * solution_strategy,
      amsi::MultiIteration * it,
      amsi::LAS * las,
      O out);

  class FEMAnalysis
  {
    public:
    FEMAnalysis(apf::Mesh * mesh,
                   std::unique_ptr<const mt::CategoryNode> cs,
                   MPI_Comm c,
                   const amsi::Analysis & amsi_analysis);
    virtual ~FEMAnalysis();
    virtual void run();
    virtual void checkpoint();
    virtual void finalizeStep();
    virtual void finalizeIteration(int);

    protected:
    // util
    MPI_Comm cm;
    std::unique_ptr<const mt::CategoryNode> analysis_case;
    apf::Mesh* mesh;
    // analysis
    double t;
    double dt;
    int stp;
    int mx_stp;
    int iteration{0};
    AnalysisStep * analysis_step_;
    // this is actually a MultiIteration
    amsi::MultiIteration * itr;
    std::vector<amsi::Iteration *> itr_stps;
    // this is actually a MultiConvergence
    amsi::Convergence * cvg;
    std::vector<amsi::Convergence *> cvg_stps;
    // name of track volume model trait and ptr to volume calc
    amsi::LAS * las;
    bool completed;
    // log filenames
    std::string state_fn;
    // logs
    amsi::Log state;
  };
  class TissueIteration : public amsi::Iteration
  {
    protected:
    AnalysisStep * tssu;
    amsi::LAS * las;

    public:
    TissueIteration(AnalysisStep * t, amsi::LAS * l) : tssu(t), las(l) {}
    virtual void iterate()
    {
      // Note this is same as FEMLinearIteration
      LinearSolver(tssu, las);

      tssu->iter();
      // copies LHS/RHS into "old" state as needed for computing convergence
      las->iter();
      amsi::Iteration::iterate();
    }
  };
  class TissueCheckpointIteration : public amsi::Iteration
  {
    protected:
    FEMAnalysis * tssu;

    public:
    TissueCheckpointIteration(FEMAnalysis * t) : tssu(t) {}
    virtual void iterate()
    {
      std::cout << "Checkpointing iteration: " << this->iteration()
                << std::endl;
      tssu->checkpoint();
      amsi::Iteration::iterate();
    }
  };
}  // namespace mumfim
#include "TissueAnalysis_impl.h"
#endif
