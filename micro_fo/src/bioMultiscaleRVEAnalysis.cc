#include "bioMultiscaleRVEAnalysis.h"
#include "bioFiberNetworkIO2.h"
#include <apfFEA.h> // amsi
#include <apfMDS.h>
#include <gmi.h>
#include <cassert>
namespace bio
{
  FiberRVEAnalysis * initFromMultiscale(FiberNetwork * fn,
                                        las::SparskitBuffers * bfrs,
                                        micro_fo_header & hdr,
                                        micro_fo_params & prm,
                                        micro_fo_init_data & ini)
  {
    FiberRVEAnalysis * rve = makeFiberRVEAnalysis(fn,bfrs);
    rve->multi = new MultiscaleRVE(rve->rve,fn,hdr,prm,ini);
    return rve;
  }
  class ApplyDeformationGradient : public amsi::FieldOp
  {
  protected:
    apf::Field * xyz;
    apf::Field * du;
    apf::Field * u;
    apf::MeshEntity * ent;
    apf::Matrix3x3 FmI;
  public:
    ApplyDeformationGradient(apf::Matrix3x3 F, apf::Mesh * msh, apf::Field * du_, apf::Field * u_)
      : xyz(msh->getCoordinateField())
      , du(du_)
      , u(u_)
      , ent(NULL)
    {
      int d = msh->getDimension();
      for(int ii = 0; ii < d; ++ii)
        for(int jj = 0; jj < d; ++jj)
          FmI[ii][jj] = F[ii][jj] - (ii == jj ? 1.0 : 0.0);
    }
    virtual bool inEntity(apf::MeshEntity * m)
    {
      ent = m;
      return true;
    }
    virtual void outEntity() {}
    virtual void atNode(int nd)
    {
      apf::Vector3 nd_xyz;
      apf::Vector3 nd_u_old;
      apf::getVector(xyz,ent,nd,nd_xyz);
      apf::getVector(u,ent,nd,nd_u_old);
      apf::Vector3 nd_u = FmI * nd_xyz;
      apf::Vector3 nd_du = nd_u - nd_u_old;
      apf::setVector(u,ent,nd,nd_u);
      apf::setVector(du,ent,nd,nd_du);
    }
    void run()
    {
      apply(u);
    }
  };
  void applyMultiscaleCoupling(FiberRVEAnalysis * ans, micro_fo_data * data)
  {
    int d = ans->rve->getDim();
    assert(d == 3 || d == 2);
    apf::Matrix3x3 F;
    for(int ei = 0; ei < d; ++ei)
      for(int ej = 0; ej < d; ++ej)
        F[ei][ej] = data->data[ei*d + ej];
    apf::Mesh * rve_msh = ans->rve->getMesh();
    apf::Field * rve_du = ans->rve->getdUField();
    apf::Field * rve_u = ans->rve->getUField();
    ApplyDeformationGradient(F,rve_msh,rve_du,rve_u).run();
    apf::Mesh * fn_msh = ans->fn->getNetworkMesh();
    apf::Field * fn_du = ans->fn->getdUField();
    apf::Field * fn_u = ans->fn->getUField();
    ApplyDeformationGradient(F,fn_msh,fn_du,fn_u).run();
  }
  void recoverMultiscaleResults(FiberRVEAnalysis * ans, micro_fo_result * data)
  {
    (void)ans;
    (void)data;
  }
  MultiscaleRVEAnalysis::MultiscaleRVEAnalysis()
    : eff()
    , wgt()
    , tmg()
    , recv_ptrn()
    , send_ptrn()
    , rve_dd(NULL)
    , M2m_id()
    , m2M_id()
    , dat_tp()
    , rve_tp_cnt(0)
    , fns()
    , sprs()
    , bfrs(NULL)
    , macro_iter(0)
    , macro_step(0)
  {
    M2m_id = amsi::getRelationID(amsi::getMultiscaleManager(),amsi::getScaleManager(),"macro","micro_fo");
    m2M_id = amsi::getRelationID(amsi::getMultiscaleManager(),amsi::getScaleManager(),"micro_fo","macro");
  }
  void MultiscaleRVEAnalysis::init()
  {
    initLogging();
    initCoupling();
    initAnalysis();
  }
  void MultiscaleRVEAnalysis::initLogging()
  {
    eff = amsi::activateLog("micro_fo_efficiency");
    wgt = amsi::activateLog("micro_fo_weights");
    tmg = amsi::activateLog("micro_fo_timing");
  }
  void MultiscaleRVEAnalysis::initCoupling()
  {
    amsi::ControlService * cs = amsi::ControlService::Instance();
    rve_dd = amsi::createDataDistribution(amsi::getLocal(),"macro_fo_data");
    recv_ptrn = cs->RecvCommPattern("micro_fo_data","macro",
                                    "macro_fo_data","micro_fo");
    cs->CommPattern_Reconcile(recv_ptrn);
    send_ptrn = cs->CommPattern_UseInverted(recv_ptrn,"macro_fo_data",
                                            "micro_fo","macro");
    cs->CommPattern_Assemble(send_ptrn);
    cs->CommPattern_Reconcile(send_ptrn);
  }
  void MultiscaleRVEAnalysis::initAnalysis()
  {
    int num_rve_tps = 0;
    amsi::ControlService * cs = amsi::ControlService::Instance();
    cs->scaleBroadcast(M2m_id,&num_rve_tps);
    char ** rve_tp_dirs = new char * [num_rve_tps];
    MPI_Request rqsts[num_rve_tps];
    // the order of receipt might be non-deterministic. need to handle that
    for(int ii = 0; ii < num_rve_tps; ++ii)
    {
      int cnt = 0;
      while((cnt = cs->aRecvBroadcastSize<char>(M2m_id)) == 0)
      { }
      rve_tp_dirs[ii] = new char [cnt];
      // don't have to block to wait since we know the message was available for size info
      cs->aRecvBroadcast(&rqsts[ii],M2m_id,&rve_tp_dirs[ii][0],cnt);
    }
    MPI_Status stss[num_rve_tps];
    MPI_Waitall(num_rve_tps,&rqsts[0],&stss[0]);
    MPI_Request hdr_rqst;
    rve_tp_cnt.resize(num_rve_tps);
    cs->aRecvBroadcast(&hdr_rqst,M2m_id,&rve_tp_cnt[0],num_rve_tps);
    MPI_Status hdr_sts;
    MPI_Waitall(1,&hdr_rqst,&hdr_sts);
    // Read in all the fiber network meshes and reactions
    for(int ii = 0; ii < num_rve_tps; ++ii)
    {
      fns.push_back(new FiberNetworkReactions* [rve_tp_cnt[ii]]);
      sprs.push_back(new las::CSR* [rve_tp_cnt[ii]]);
    }
    int dof_max = -1;
    for(int ii = 0; ii < num_rve_tps; ii++)
    {
      for(int jj = 0; jj < rve_tp_cnt[ii]; ++jj)
      {
        std::stringstream fl;
        fl << rve_tp_dirs[ii] << jj+1 << ".txt";
        FiberNetworkReactions * fn_rctns = new FiberNetworkReactions;
        fn_rctns->msh = loadFromFile(fl.str());
        apf::Mesh2 * fn = fn_rctns->msh;
        fl << ".params";
        loadParamsFromFile(fn,fl.str(),std::back_inserter(fn_rctns->rctns));
        fns[ii][jj] = fn_rctns;
        apf::Field * u = apf::createLagrangeField(fn,"u",apf::VECTOR,1);
        apf::zeroField(u);
        apf::Numbering * n = apf::createNumbering(u);
        int dofs = apf::NaiveOrder(n);
        sprs[ii][jj] = las::createCSR(n,dofs);
        apf::destroyNumbering(n);
        apf::destroyField(u);
        dof_max = dofs > dof_max ? dofs : dof_max;
      }
    }
    bfrs = new las::SparskitBuffers(dof_max);
  }
  void MultiscaleRVEAnalysis::updateCoupling()
  {
    std::vector<micro_fo_header> hdrs;
    std::vector<micro_fo_params> prms;
    std::vector<micro_fo_init_data> inis;
    std::vector<int> to_delete;
    amsi::ControlService * cs = amsi::ControlService::Instance();
    cs->RemoveData(recv_ptrn,to_delete);
    for(auto idx = to_delete.rbegin(); idx != to_delete.rend(); ++idx)
      destroyAnalysis(ans[*idx]);
    std::vector<int> to_add;
    std::vector<int> empty;
    size_t recv_init_ptrn = cs->AddData(recv_ptrn,empty,to_add);
    int ii = 0;
    ans.resize(ans.size()+to_add.size());
    cs->Communicate(recv_init_ptrn, hdrs, dat_tp.hdr);
    cs->Communicate(recv_init_ptrn, prms, dat_tp.prm);
    cs->Communicate(recv_init_ptrn, inis, dat_tp.ini);
    gmi_model * nl_mdl = gmi_load(".null");
    for(auto rve = ans.begin(); rve != ans.end(); ++rve)
    {
      if(*rve == NULL)
      {
        micro_fo_header & hdr = hdrs[ii];
        micro_fo_params & prm = prms[ii];
        micro_fo_init_data & dat = inis[ii];
        int tp = hdr.data[RVE_TYPE];
        int rnd = rand() % rve_tp_cnt[tp];
        apf::Mesh * msh_cpy = apf::createMdsMesh(nl_mdl,fns[tp][rnd]->msh);
        std::string fbr_rct_str("fiber_reaction");
        // copy fiber_reaction tag from origin mesh and set the reactions vector inside the fn
        amsi::copyIntTag(fbr_rct_str,fns[tp][rnd]->msh,msh_cpy,1,1);
        FiberNetwork * fn = new FiberNetwork(msh_cpy);
        fn->getFiberReactions() = fns[tp][rnd]->rctns; // hate this
        *rve = initFromMultiscale(fn,bfrs,hdr,prm,dat);
        ii++;
      }
    }
    cs->CommPattern_UpdateInverted(recv_ptrn,send_ptrn);
    cs->CommPattern_Assemble(send_ptrn);
    cs->CommPattern_Reconcile(send_ptrn);
  }
  void MultiscaleRVEAnalysis::run()
  {
    amsi::ControlService * cs = amsi::ControlService::Instance();
    int sim_complete = 0;
    while(!sim_complete)
    {
      int step_complete = 0;
      while(!step_complete)
      {
        // migration
        updateCoupling();
        std::vector<micro_fo_data> data;
        cs->Communicate(recv_ptrn,data,dat_tp.dat);
        std::vector<micro_fo_result> results(data.size());
        int ii = 0;
        for(auto rve = ans.begin(); rve != ans.end(); ++rve)
        {
          applyMultiscaleCoupling(*rve,&data[ii]);
          FiberRVEIteration itr(*rve);
          FiberRVEConvergence cnvrg(*rve);
          amsi::numericalSolve(&itr,&cnvrg);
          recoverMultiscaleResults(*rve,&results[ii]);
          ++ii;
        }
        cs->Communicate(send_ptrn,results,dat_tp.rst);
        macro_iter++;
        cs->scaleBroadcast(M2m_id,&step_complete);
      }
      macro_iter = 0;
      macro_step++;
    }
  }
}
