#ifndef BIO_MACRO_COUPLING_H
#define BIO_MACRO_COUPLING_H

#include "apfUtil.h"
#include <apf.h>
#include <apfDynamicMatrix.h>

namespace bio
{
  /**
   * A class to manage the multi-scale coupling information relating a single fiber network
   *  to a single macro-scale integration point and mesh element.
   */
  class MacroInfo
  {
  protected:
    int gss_id;
    apf::Vector3 lcl_gss;
    apf::Mesh * macro_msh;
    apf::MeshEntity * macro_ent;
    apf::MeshElement * macro_melmnt;
    apf::Element * macro_elmnt;
    int nnd; // num nodes effecting the macro element

    void dCidFE(apf::DynamicMatrix&,const int,const apf::Vector3 &, double);
  public:
    /**
     * Calculate the term relating the macro-scale nodal displacements to the
     *  micro-scale cube corner displacements.
     */
    void calcdRVEdFE(apf::DynamicMatrix & drve_dfe, const FiberRVE * rve);
  };
}

#endif
