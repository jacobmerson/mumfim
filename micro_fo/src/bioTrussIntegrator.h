#ifndef BIO_TRUSS_INTEGRATOR_H_
#define BIO_TRUSS_INTEGRATOR_H_
#include "bioFiberNetwork2.h"
#include "bioFiberReactions.h"
#include <amsiElementalSystem.h>
#include <amsiLASAssembly.h>
#include <apf.h>
#include <apfDynamicMatrix.h>
#include <apfElementalSystem.h> //amsi
#include <apfMeasure.h> //amsi
#include <cassert>
namespace las
{
  class Mat;
  class Vec;
}
namespace bio
{
  class ElementalSystem;
  /**
   * An integrator class to produce elemental jacobian matrices
   *  and force vectors for trusses using the provided FiberReaction.
   *  The elemental systems are assembled into the global system formed
   *  by the skMat k and skVec f using the supplied numbering, which
   *  should already be numbered.
   * @todo Bill : refactor and simplify, it feels like there is too much
   *              happening in this class
   */
   class TrussIntegrator : public apf::Integrator
   {
   protected:
    apf::Field * u;
    apf::Mesh * msh;
    apf::Numbering * nm;
    apf::Element * elmt;
    amsi::ElementalSystem2 * es;
    double l;
    double lo;
    apf::Vector3 strn;
    int dim;
    FiberReaction ** frs;
    FiberReaction * fr;
    apf::MeshTag * rct_tg;
    las::LasOps * ops;
    las::Mat * k;
    las::Vec * f;
  public:
   TrussIntegrator(apf::Numbering * n, FiberReaction ** frs_, las::LasOps * op, las::Mat * k_, las::Vec * f_, int o)
      : apf::Integrator(o)
      , u(apf::getField(n))
      , msh(apf::getMesh(u))
      , nm(n)
      , elmt(NULL)
      , es(NULL)
      , l(0.0)
      , lo(0.0)
      , strn()
      , dim()
      , frs(frs_)
      , fr()
      , rct_tg(msh->findTag("fiber_reaction"))
      , ops(op)
      , k(k_)
      , f(f_)
    { }
    void inElement(apf::MeshElement * me)
    {
      elmt = apf::createElement(u,me);
      lo = apf::measure(me);
      apf::MeshEntity * ent = apf::getMeshEntity(me);
      l = amsi::measureDisplacedMeshEntity(ent,u);
      es = amsi::buildApfElementalSystem(elmt,nm);
      int tg = -1;
      msh->getIntTag(ent,rct_tg,&tg);
      fr = frs[tg];
      dim = msh->getDimension();
      apf::MeshEntity * vs[2];
      msh->getDownward(ent,0,&vs[0]);
      apf::Vector3 crds[2];
      getCoords(msh,&vs[0],&crds[0],2);
      strn = (crds[1] - crds[0]) / l;
    }
    void atPoint(const apf::Vector3 & p, double w, double dV)
    {
      auto f_dfdl = fr->forceReaction(l,lo);
      double f = f_dfdl.first;
      double dfdl = f_dfdl.second;
      double fl = f / l;
      double dfdl_fl = dfdl - fl;
      double frc = 0.0;
      for(int ii = 0; ii < dim; ii++)
      {
       frc = strn[ii] * f;
       es->fe(ii)       = -frc;
       es->fe(2*dim+ii) =  frc;
     }
     apf::Matrix3x3 rctn = apf::tensorProduct(strn,strn*dfdl_fl) + eye()*fl;
     double op = -1.0;
     for(int ii = 0; ii < 2; ii++)
     {
       op *= -1.0;
       for(int jj = 0; jj < 2; jj++)
       {
         op *= -1.0;
         for(int kk = 0; kk < dim; kk++)
           for(int ll = 0; ll < dim; ll++)
             es->ke(ii*dim + kk, jj*dim + ll) = rctn[ii][jj] * op;
         }
       }
     }
     void outElement()
     {
       amsi::assemble(ops,k,f,es);
       delete es;
    }
  };
}
#endif
