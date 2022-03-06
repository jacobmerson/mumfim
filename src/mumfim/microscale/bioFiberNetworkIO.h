#ifndef MUMFIM_FIBER_NETWORK_IO_H_
#define MUMFIM_FIBER_NETWORK_IO_H_
#include <apf.h>
#include <apfMesh2.h>
#include <iostream>
#include "bioFiberReactions.h"
namespace mumfim
{
  apf::Mesh2 * loadFromStream(std::istream & strm);
  apf::Mesh2 * loadFromFile(const std::string & fnm);
  // FIXME constrain these functions to only work on output iterators
  // https://stackoverflow.com/questions/8751460/how-to-restrict-an-iterator-to-being-a-forward-iterator
  // FIXME also use the appropriate unique_ptr for the reactions
  template <typename O>
  void loadParamsFromFile(apf::Mesh2 * msh, const std::string & fnm, O rctns);
  template <typename O>
  void loadParamsFromStream(apf::Mesh2 * msh, std::istream & strm, O rctns);
}  // namespace mumfim
#include "bioFiberNetworkIO_impl.h"
#endif