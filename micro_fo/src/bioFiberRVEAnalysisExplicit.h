#ifndef BIO_FIBER_RVE_ANALYSIS_EXPLICIT
#define BIO_FIBER_RVE_ANALYSIS_EXPLICIT
#include "bioExplicitAmplitude.h"
#include "bioFiberNetwork.h"
#include "bioFiberRVEAnalysis.h"
#include "bioExplicitOutputWriter.h"
#include <string>
namespace bio
{
  // TODO FIXME deal with network with multiple material properties
  class FiberRVEAnalysisExplicit : public FiberRVEAnalysis
  {
    public:
    FiberRVEAnalysisExplicit(FiberNetwork * fn,
                             LinearStructs<las::MICRO_BACKEND> * vecs,
                             const MicroSolutionStrategyExplicit & ss);
    ~FiberRVEAnalysisExplicit()
    {
      delete writer;
      delete amp;
    }
    virtual bool run(const DeformationGradient & dfmGrd);
    virtual FiberRVEAnalysisType getAnalysisType()
    {
      return FiberRVEAnalysisType::Explicit;
    }
    protected:
    // This parameters gives the number of fibers under which the serial
    // implementation will be used.
    int serial_gpu_cutoff;
    double total_time;
    double load_time;
    double fiber_elastic_modulus;
    double fiber_area;
    double fiber_density;
    Amplitude * amp;
    std::string analysis_name;
    double visc_damp_coeff;
    unsigned long print_history_frequency;
    unsigned long print_field_frequency;
    bool print_field_by_num_frames;
    double crit_time_scale_factor;
    double energy_check_eps;
    int disp_bound_nfixed;
    int * disp_bound_nodes;
    int * disp_bound_dof;
    double * disp_bound_vals;
    double * disp_bound_init_vals;
    // this is to attempt to deal with initialization errors that come
    // from running explicit analysis initialization phase on more than
    // a single iteration
    bool system_initialized;
    unsigned long itr_prev;
    void computeDisplcamentBC(const DeformationGradient & dfmGrd);
    virtual void copyForceDataToForceVec();
    virtual void copyDispDataToDispVec();
    virtual void computeStiffnessMatrix();
    void relaxSystem();
    ExplicitOutputWriter * writer; 
  };
}  // namespace bio
#endif