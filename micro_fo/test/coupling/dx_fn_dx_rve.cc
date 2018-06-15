#include "bioMultiscaleRVEAnalysis.h"
#include "bioFiberNetworkIO.h"
#include "io.h"
#include <lasConfig.h>
#include <lasCSRCore.h>
#include <PCU.h>
#include <mpi.h>
#include <fstream>
#include <string>
std::string fn_fn("fn.test");
std::string fn_dR_dx_rve("dR_dx_rve.test");
std::string fn_k("K_dR_dx_rve.test");
std::string fn_dx_fn_dx_rve("dx_fn_dx_rve.test");
inline void notify(std::ostream & out,const std::string & fn)
{
  out << "Attempting to read " << fn << std::endl;
}
int main(int ac, char * av[])
{
  MPI_Init(&ac,&av);
  PCU_Comm_Init();
  bio::FiberNetwork fn(bio::loadFromFile(fn_fn));
  las::Sparsity * csr = las::createCSR(fn.getUNumbering(),fn.getDofCount());
  bio::FiberRVEAnalysis * ans = bio::makeFiberRVEAnalysis(&fn,csr);
  // load k and overwrite ans->k
  apf::DynamicVector kv;
  notify(std::cout,fn_k);
  std::ifstream fin_k(fn_k.c_str());
  fin_k >> kv;
  fin_k.close();
  double * ka = &(*reinterpret_cast<las::csrMat*>(ans->k))(0,0);
  memcpy(ka,&kv[0],sizeof(double)*kv.size());
  apf::DynamicMatrix dR_dx_rve;
  notify(std::cout,fn_dR_dx_rve);
  std::ifstream fin_dR_dx_rve(fn_dR_dx_rve.c_str());
  fin_dR_dx_rve >> dR_dx_rve;
  fin_dR_dx_rve.close();
  apf::DynamicMatrix dx_fn_dx_rve;
  calcdx_fn_dx_rve(dx_fn_dx_rve,ans,dR_dx_rve);
  notify(std::cout,fn_dx_fn_dx_rve);
  std::ifstream fin_dx_fn_dx_rve(fn_dx_fn_dx_rve);
  apf::DynamicMatrix dx_fn_dx_rve_tst;
  fin_dx_fn_dx_rve >> dx_fn_dx_rve_tst;
  bool eq = (dx_fn_dx_rve == dx_fn_dx_rve_tst);
  std::cout << "Calculation of dx_fn_dx_rve " << ( eq ? "succeeded!" : "failed!") << std::endl;
  PCU_Comm_Free();
  MPI_Finalize();
  return !eq;
}