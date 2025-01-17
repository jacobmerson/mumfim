#ifndef MUMFIM_NEOHOOKEAN_INTEGRATOR_H_
#define MUMFIM_NEOHOOKEAN_INTEGRATOR_H_
#include <ElementalSystem.h>
#include <amsiDeformation.h>
#include <apfShape.h>
#include <cmath>  //natural log
#include <cstring>
#include "materials/Materials.h"
#include "NonlinearTissueStep.h"
#include "UpdatedLagrangianMaterialIntegrator.h"
namespace mumfim
{
  // Parameters from V. Lai et al. Journal of Biomechanical Engineering, Vol
  // 135, 071007 (2013)
  //
  // The Voigt notation in this integrator are NOT consistent
  // (in different portions of the integrator)!
  // Fortunately, we are OK because this is an isotropic material,
  // but for consistency (with the rest of the code)
  // we should use 1->11, 2->22, 3->33, 4->23, 5->13, 6->12
  class NeoHookeanIntegrator : public amsi::ElementalSystem
  {
    public:
    NeoHookeanIntegrator(apf::Field * displacements,
                         apf::Field * dfm_grd,
                         apf::Field * current_coords,
                         apf::Field * cauchy_stress_field,
                         apf::Field * green_lagrange_strain_field,
                         apf::Numbering* numbering,
                         double youngs_modulus,
                         double poisson_ratio,
                         int o)
        : ElementalSystem(displacements, numbering, o)
        , current_integration_point(0)
        , dim(0)
        , dfm_grd_fld(dfm_grd)
        , current_coords(current_coords)
        , ShearModulus(0.0)
        , PoissonsRatio(poisson_ratio)
        , cauchy_stress_field_(cauchy_stress_field)
        , green_lagrange_strain_field_(green_lagrange_strain_field)
    {
      ShearModulus = youngs_modulus / (2.0 * (1.0 + poisson_ratio));
    }
    // ce is the mesh element that corresponds to the current coordinates
    void inElement(apf::MeshElement * me) final
    {
      ElementalSystem::inElement(me);
      msh = apf::getMesh(f);
      dim = apf::getDimension(me);
      current_integration_point = 0;
      // testing of new apf work
      // create a mesh element that tracks with current coords
      ccme = apf::createMeshElement(current_coords, apf::getMeshEntity(me));
      // current coordinate field w.r.t. current coordinate mesh elements
      // elements
      cccce = apf::createElement(current_coords, ccme);
      // element of current coordinate field w.r.t. reference coordinate mesh
      // elements
      ref_lmnt =
          apf::createMeshElement(apf::getMesh(f), apf::getMeshEntity(me));
      du_lmnt = apf::createElement(f, ref_lmnt);
    }
    void outElement() final
    {
      apf::destroyElement(cccce);
      apf::destroyMeshElement(ccme);
      apf::destroyMeshElement(ref_lmnt);
      apf::destroyElement(du_lmnt);
      ElementalSystem::outElement();
    }
    bool includesBodyForces() final { return true; }
    void atPoint(apf::Vector3 const & p, double w, double) final
    {
      int & nen = nenodes;   // = 4 (tets)
      int & nedof = nedofs;  // = 12 (tets)
      apf::Matrix3x3 J, Jinv;
      // note that the jacobian is w.r.t the current coordinates!
      apf::getJacobian(ccme, p, J);
      apf::getJacobianInv(ccme, p, Jinv);
      double detJ = apf::getJacobianDeterminant(J, dim);
      apf::Matrix3x3 F;
      amsi::deformationGradient(du_lmnt, p, F);
      apf::setMatrix(dfm_grd_fld, apf::getMeshEntity(me),
                     current_integration_point, F);
      double detF = getDeterminant(F);
      if (detF < 0.0)
      {
        throw mumfim_error("detF < 0");
      }
      apf::NewArray<apf::Vector3> grads;
      apf::getShapeGrads(cccce, p, grads);
      // Calculate derivative of shape fxns wrt current coordinates.
      /*=======================================================
        Constitutive Component of Tangent Stiffness Matrix: K0
        - See Bathe PP.557, Table 6.6 (Updated Lagrangian)
        =======================================================
      */
      // hard-coded for 3d, make a general function... to produce this
      apf::DynamicMatrix BL(6, nedof);  // linear strain disp
      BL.zero();
      for (int ii = 0; ii < nen; ii++)
      {
        BL(0, dim * ii) = grads[ii][0];      // N_(ii,1)
        BL(1, dim * ii + 1) = grads[ii][1];  // N_(ii,2)
        BL(2, dim * ii + 2) = grads[ii][2];  // N_(ii,3)
        BL(3, dim * ii) = grads[ii][1];      // N_(ii,2)
        BL(3, dim * ii + 1) = grads[ii][0];  // N_(ii,1)
        BL(4, dim * ii + 1) = grads[ii][2];  // N_(ii,3)
        BL(4, dim * ii + 2) = grads[ii][1];  // N_(ii,2)
        BL(5, dim * ii) = grads[ii][2];      // N_(ii,3)
        BL(5, dim * ii + 2) = grads[ii][0];  // N_(ii,1)
      }
      /*
        Determine stress-strain matrix, D, for NeoHookean material.
        See Bonet and Wood 2nd Edition, PP.250.
        - lambda and mu are effective lame parameters.
        - lambda = ShearModulus/detJ, mu = ( PoissonsRatio - ShearModulus *
        ln(detJ) )/detJ
      */
      // Calculate Cauchy stress from compressible NeoHookean equation. See
      // Bonet and Wood 2nd Ed. PP.163.
      apf::DynamicMatrix leftCauchyGreen(3, 3);   // leftCauchyGreen.zero();
      apf::DynamicMatrix rightCauchyGreen(3, 3);  // rightCauchyGreen.zero();
      apf::DynamicMatrix FT(3, 3);
      FT.zero();
      apf::transpose(fromMatrix(F), FT);
      apf::multiply(fromMatrix(F), FT, leftCauchyGreen);
      apf::multiply(FT, fromMatrix(F), rightCauchyGreen);
      // apf::DynamicMatrix Cauchy(3,3);
      apf::Matrix3x3 Cauchy;
      double mu = ShearModulus;
      double lambda = (2.0 * mu * PoissonsRatio) / (1.0 - 2.0 * PoissonsRatio);
      for (int i = 0; i < dim; i++)
      {
        for (int j = 0; j < dim; j++)
        {
          Cauchy[i][j] = mu / detF * (leftCauchyGreen(i, j) - (i == j)) +
                         lambda / detF * log(detF) * (i == j);
        }
      }
      double lambda_prime = lambda / detF;
      double mu_prime = (mu - lambda * log(detF)) / detF;
      // material stiffness
      apf::DynamicMatrix D(6, 6);
      D.zero();
      D(0, 0) = lambda_prime + (2.0 * mu_prime);
      D(0, 1) = lambda_prime;
      D(0, 2) = lambda_prime;
      D(1, 0) = lambda_prime;
      D(1, 1) = lambda_prime + (2.0 * mu_prime);
      D(1, 2) = lambda_prime;
      D(2, 0) = lambda_prime;
      D(2, 1) = lambda_prime;
      D(2, 2) = lambda_prime + (2.0 * mu_prime);
      D(3, 3) = mu_prime;
      D(4, 4) = mu_prime;
      D(5, 5) = mu_prime;
      apf::DynamicMatrix K0(nedof, nedof);
      apf::DynamicMatrix BLT(nedof, 6);
      apf::DynamicMatrix DBL(6, nedof);
      apf::transpose(BL, BLT);
      apf::multiply(D, BL, DBL);
      apf::multiply(BLT, DBL, K0);
      /*=======================================================================
        Initial Stress (or Geometric) component of Tangent Stiffness Matrix: K1
        - See Bathe PP.557, Table 6.6 (Updated Lagrangian)
        ========================================================================*/
      apf::DynamicMatrix BNL(9, nedof);  // nonlinear strain disp
      BNL.zero();
      for (int i = 0; i < nenodes; ++i)
      {
        BNL(0, i * dim) = grads[i][0];
        BNL(1, i * dim) = grads[i][1];
        BNL(2, i * dim) = grads[i][2];
        BNL(3, i * dim + 1) = grads[i][0];
        BNL(4, i * dim + 1) = grads[i][1];
        BNL(5, i * dim + 1) = grads[i][2];
        BNL(6, i * dim + 2) = grads[i][0];
        BNL(7, i * dim + 2) = grads[i][1];
        BNL(8, i * dim + 2) = grads[i][2];
      }
      apf::DynamicMatrix tau(9, 9);
      tau.zero();
      for (int i = 0; i < dim; ++i)
      {
        for (int j = 0; j < dim; ++j)
        {
          tau(i, j) = Cauchy[i][j];
          tau(i + dim, j + dim) = Cauchy[i][j];
          tau(i + 2 * dim, j + 2 * dim) = Cauchy[i][j];
        }
      }
      apf::DynamicMatrix K1(nedof, nedof);
      K1.zero();
      apf::DynamicMatrix BNLT(nedof, 9);
      BNLT.zero();
      apf::DynamicMatrix BNLTxtau(9, nedof);
      BNLTxtau.zero();
      apf::transpose(BNL, BNLT);
      apf::multiply(BNLT, tau, BNLTxtau);
      apf::multiply(BNLTxtau, BNL, K1);
      /*=======================================================================
        Terms for force vector (RHS)
        - See Bathe PP.557, Table 6.6 (Updated Lagrangian)
        ========================================================================*/
      apf::DynamicVector cauchyVoigt(6);
      cauchyVoigt(0) = Cauchy[0][0];
      cauchyVoigt(1) = Cauchy[1][1];
      cauchyVoigt(2) = Cauchy[2][2];
      cauchyVoigt(3) = Cauchy[0][1];
      cauchyVoigt(4) = Cauchy[1][2];
      cauchyVoigt(5) = Cauchy[0][2];
      apf::DynamicVector BLTxCauchyVoigt(nedof);
      apf::multiply(BLT, cauchyVoigt, BLTxCauchyVoigt);
      for (int ii = 0; ii < nedof; ii++)
      {
        fe[ii] -= w * detJ * BLTxCauchyVoigt(ii);
        for (int jj = 0; jj < nedof; jj++)
          Ke(ii, jj) += w * detJ * (K0(ii, jj) + K1(ii, jj));
      }
      // Calculate rightCauchyGreen tensor.
      auto green_strain = computeGreenLagrangeStrain(F);
      apf::setMatrix(dfm_grd_fld, apf::getMeshEntity(me), current_integration_point, F);
      apf::setMatrix(green_lagrange_strain_field_, apf::getMeshEntity(me), current_integration_point, green_strain);
      apf::Matrix3x3 cauchy_stress_matrix;
      for(size_t i=0; i<3; ++i) {
        for(size_t j=0; j<3; ++j) {
          cauchy_stress_matrix[i][j] = Cauchy[i][j];
        }
      }
      apf::setMatrix(cauchy_stress_field_, apf::getMeshEntity(me), current_integration_point, cauchy_stress_matrix);
      current_integration_point++;
    }
    int current_integration_point;

    private:
    int dim;
    apf::Mesh * msh;
    apf::Field * dfm_grd_fld;
    // new stuff to try out new apf functions
    apf::Field * current_coords;
    apf::Element * cccce;
    apf::MeshElement * ccme;
    apf::MeshElement * ref_lmnt;
    apf::Element * du_lmnt;
    //
    double ShearModulus;
    double PoissonsRatio;
    apf::Field* cauchy_stress_field_;
    apf::Field* green_lagrange_strain_field_;
  };
}  // namespace mumfim
#endif
