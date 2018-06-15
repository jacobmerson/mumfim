#include "bioFiberNetwork.h"
#include <iostream>
#include <cassert> // assert
#include <numeric> // accumulate
#include <string>
namespace bio
{
  FiberNetwork::FiberNetwork(apf::Mesh * f)
    : fn(f)
    , u(NULL)
    , xpufnc(NULL)
    , xpu(NULL)
    , du(NULL)
    , dw(NULL)
    , udof(NULL)
    , wdof(NULL)
    , ucnt(0)
    , wcnt(0)
    , tp(FiberMember::truss)
    , dim(f->getDimension())
  {
    assert(f);
    du = apf::createLagrangeField(fn,"du",apf::VECTOR,1);
    u  = apf::createLagrangeField(fn,"u",apf::VECTOR,1);
    xpufnc = new amsi::XpYFunc(fn->getCoordinateField(),u);
    xpu = apf::createUserField(fn,"xpu",apf::VECTOR,apf::getShape(u),xpufnc);
    apf::zeroField(du);
    apf::zeroField(u);
    // USING 'u' HERE IS VERY IMPORTANT,
    // APPLYDEFORMATIONGRADIENT USES THE
    // FIELD RETRIEVED FROM THIS NUMBERING TO
    // MODIFY, THIS SHOULD BE FIXED BECAUSE
    // IT IS TOO FRAGILE
    udof = apf::createNumbering(u);
    //wdof = apf::createNumbering(dw);
    ucnt = apf::NaiveOrder(udof);
    //wcnt = apf::AdjReorder(wdof);
    //apf::SetNumberingOffset(wdof,ucnt);
  }
  FiberNetwork::~FiberNetwork()
  {
    apf::destroyNumbering(udof);
    apf::destroyField(xpu);
    delete xpufnc;
    apf::destroyField(u);
    apf::destroyField(du);
  }
}