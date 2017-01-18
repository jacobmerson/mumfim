#ifndef BIO_ANALYSIS_H_
#define BIO_ANALYSIS_H_
#include <amsiUtil.h>
#include <MeshSim.h>
#include <apf.h>
namespace bio
{
  /**
   * @brief for each model item in the range [bgn_mdl_itm,nd_mdl_itm),
   *  print the step, entity tag, and volume to the specified log
   */
  template <typename I>
    void logVolumes(I bgn_mdl_itm, I nd_mdl_itm, amsi::Log log, int stp, pMesh msh, apf::Field * U);
}
#include "bioAnalysis_impl.h"
#endif
