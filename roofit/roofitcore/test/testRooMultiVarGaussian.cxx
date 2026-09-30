// Test for parametric covariance matrix in RooMultiVarGaussian

#include "RooMultiVarGaussian.h"
#include "RooRealVar.h"
#include "RooConstVar.h"
#include "RooArgList.h"
#include "TMatrixDSym.h"
#include "RooGlobalFunc.h"

#include "gtest/gtest.h"

using namespace RooFit;

TEST(RooMultiVarGaussian, ParametricCovariance)
{
   // Test the new parametric covariance functionality
   
   // Create observables
   RooRealVar x("x", "x", 0.0, -5.0, 5.0);
   RooRealVar y("y", "y", 0.0, -5.0, 5.0);
   RooArgList xvec;
   xvec.add(x);
   xvec.add(y);
   
   // Create mean parameters
   RooRealVar mu_x("mu_x", "mu_x", 1.0);
   RooRealVar mu_y("mu_y", "mu_y", 2.0);
   RooArgList mu;
   mu.add(mu_x);
   mu.add(mu_y);
   
   // Create covariance parameters
   // For 2x2 matrix, we need 3 elements: (0,0), (0,1), (1,1)
   RooRealVar cov_00("cov_00", "cov_00", 1.0);
   RooRealVar cov_01("cov_01", "cov_01", 0.2);
   RooRealVar cov_11("cov_11", "cov_11", 1.5);
   RooArgList covElements;
   covElements.add(cov_00);
   covElements.add(cov_01);
   covElements.add(cov_11);
   
   // Create the parametric multivariate Gaussian
   RooMultiVarGaussian mvg("mvg", "parametric multivariate gaussian", xvec, mu, covElements);
   
   // Test evaluation at mean point - should be maximum
   x.setVal(1.0);
   y.setVal(2.0);
   double val_at_mean = mvg.getVal();
   
   // Test evaluation at different point
   x.setVal(2.0);
   y.setVal(3.0);
   double val_away_from_mean = mvg.getVal();
   
   // Value at mean should be higher than away from mean
   EXPECT_GT(val_at_mean, val_away_from_mean);
   
   // Test that changing covariance parameters affects the result
   x.setVal(1.0);
   y.setVal(2.0);
   double val1 = mvg.getVal();
   
   // Change covariance parameters
   cov_00.setVal(2.0);
   cov_01.setVal(0.5);
   cov_11.setVal(2.5);
   
   double val2 = mvg.getVal();
   
   // Values should be different when covariance changes
   EXPECT_NE(val1, val2);
   
   // Test that changing mean parameters affects the result
   mu_x.setVal(1.5);
   mu_y.setVal(2.5);
   
   double val3 = mvg.getVal();
   
   // Value should be different when mean changes
   EXPECT_NE(val2, val3);
}

TEST(RooMultiVarGaussian, ParametricVsStaticCovariance)
{
   // Test that parametric covariance gives same results as static covariance
   // when parameters are constant
   
   // Create observables
   RooRealVar x("x", "x", 1.5, -5.0, 5.0);
   RooRealVar y("y", "y", 2.5, -5.0, 5.0);
   RooArgList xvec;
   xvec.add(x);
   xvec.add(y);
   
   // Create mean vector
   RooRealVar mu_x("mu_x", "mu_x", 1.0);
   RooRealVar mu_y("mu_y", "mu_y", 2.0);
   RooArgList mu;
   mu.add(mu_x);
   mu.add(mu_y);
   
   // Static covariance matrix
   TMatrixDSym staticCov(2);
   staticCov(0, 0) = 1.0;
   staticCov(0, 1) = 0.3;
   staticCov(1, 0) = 0.3;
   staticCov(1, 1) = 1.2;
   
   // Create static multivariate Gaussian
   RooMultiVarGaussian mvg_static("mvg_static", "static covariance", xvec, mu, staticCov);
   
   // Create equivalent parametric covariance
   RooConstVar cov_00("cov_00", "cov_00", 1.0);
   RooConstVar cov_01("cov_01", "cov_01", 0.3);
   RooConstVar cov_11("cov_11", "cov_11", 1.2);
   RooArgList covElements;
   covElements.add(cov_00);
   covElements.add(cov_01);
   covElements.add(cov_11);
   
   // Create parametric multivariate Gaussian
   RooMultiVarGaussian mvg_param("mvg_param", "parametric covariance", xvec, mu, covElements);
   
   // Both should give the same result
   double val_static = mvg_static.getVal();
   double val_param = mvg_param.getVal();
   
   EXPECT_NEAR(val_static, val_param, 1e-10);
}

TEST(RooMultiVarGaussian, InvalidCovarianceElements)
{
   // Test error handling for wrong number of covariance elements
   
   RooRealVar x("x", "x", 0.0);
   RooRealVar y("y", "y", 0.0);
   RooRealVar z("z", "z", 0.0);
   RooArgList xvec;
   xvec.add(x);
   xvec.add(y);
   xvec.add(z);
   
   RooArgList mu;
   mu.add(RooConst(0));
   mu.add(RooConst(0));
   mu.add(RooConst(0));
   
   // For 3x3 matrix, we need 6 elements, but provide only 3
   RooRealVar cov_00("cov_00", "cov_00", 1.0);
   RooRealVar cov_01("cov_01", "cov_01", 0.0);
   RooRealVar cov_11("cov_11", "cov_11", 1.0);
   RooArgList covElements;
   covElements.add(cov_00);
   covElements.add(cov_01);
   covElements.add(cov_11);
   
   // Should throw an exception
   EXPECT_THROW(
      RooMultiVarGaussian mvg("mvg", "bad covariance", xvec, mu, covElements),
      std::invalid_argument
   );
}