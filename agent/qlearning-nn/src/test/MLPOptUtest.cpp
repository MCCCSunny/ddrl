#include "gtest/gtest.h"

#include <vector>
#include <iostream>
#include <doublefann.h>
#include <fann_train.h>
#include <fann.h>


#include "MLP.hpp"
#include <bib/Utils.hpp>
#include <bib/MetropolisHasting.hpp>
#include <bib/Combinaison.hpp>

#define NB_SOL_OPTIMIZATION 25

double OR(double x1, double x2) {
  if (x1 == 1.f || x2 == 1.f)
    return 1.f;
  return -1.f;
}

double AND(double x1, double x2) {
  if (x1 == 1.f && x2 == 1.f)
    return 1.f;
  return -1.f;
}

double LECUNIZATION(double x){ 
  if(x == 1.f) 
    return 1.5f;
  else
    return -1.5f;
}

double sech(double x) {
  return 2. / (exp(x) + exp(-x));
}

TEST(MLP, ConsistentActivationFunction) {
  MLP nn(1, 1, 0, 0.01f);

  fann_set_weight(nn.getNeuralNet(), 0, 2, 1.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 1.f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.f);

  std::vector<double> sens(0);
  std::vector<double> ac(1);
  ac[0] = 1;

  double lambda = 0.5;
  fann_set_activation_steepness_output(nn.getNeuralNet(), lambda);
  fann_set_activation_steepness(nn.getNeuralNet(), lambda, 1, 0);
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), tanh(lambda * 1.) / 2.);

  lambda = 0.8;
  fann_set_activation_steepness(nn.getNeuralNet(), lambda, 1, 0);
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), tanh(lambda * 1.) / 2.);

  ac[0] = -1;
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), tanh(lambda * -1.) / 2.);

  lambda = 0.5;
  fann_set_activation_steepness(nn.getNeuralNet(), lambda, 1, 0);
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), tanh(lambda * -1.) / 2.);

  fann_set_weight(nn.getNeuralNet(), 0, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 0.f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 5.f);

  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), 5.f / 2.f);

  fann_set_weight(nn.getNeuralNet(), 0, 2, 0.2f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.4f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 0.6f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.8f);

  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), (tanh(lambda * (-1. * 0.2f + 0.4f)) * 0.6f + 0.8f) / 2.f);

  fann_set_activation_steepness_output(nn.getNeuralNet(), 1.f);
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac),  tanh(lambda * (-1. * 0.2f + 0.4f)) * 0.6f + 0.8f);
}


TEST(MLP, LearnOpposite) {
  MLP nn(1, 1, 0, 0.5f);

  fann_set_weight(nn.getNeuralNet(), 0, 2, -1.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);

  fann_set_weight(nn.getNeuralNet(), 2, 4, 1. / tanh(0.5 * 1.));
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.f);

  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = x1 == 1.f ? -1.f : 1.f;

    std::vector<double> sens(0);
    std::vector<double> ac(1);
    ac[0] = x1;

    EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), out);
  }

  // Learn
  for (uint n = 0; n < 1000 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = x1 == 1.f ? -1.f : 1.f;

    std::vector<double> sens(0);
    std::vector<double> ac(1);
    ac[0] = x1;

    nn.learn(sens, ac, out);
  }

  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = x1 == 1.f ? -1.f : 1.f;

    std::vector<double> sens(0);
    std::vector<double> ac(1);
    ac[0] = x1;

    EXPECT_EQ(nn.computeOutVF(sens, ac), out);
  }
}


TEST(MLP, LearnAndOr) {
  MLP nn(3, 10, 2, 0.5f);

  // Learn
  for (uint n = 0; n < 10000 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;
    double x3 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = AND(OR(x1, x2), x3);

    std::vector<double> sens(2);
    sens[0] = x1;
    sens[1] = x2;
    std::vector<double> ac(1);
    ac[0] = x3;

    nn.learn(sens, ac, out);
  }

  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;
    double x3 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = AND(OR(x1, x2), x3);

    std::vector<double> sens(2);
    sens[0] = x1;
    sens[1] = x2;
    std::vector<double> ac(1);
    ac[0] = x3;

    EXPECT_GT(nn.computeOutVF(sens, ac), out - 0.02);
    EXPECT_LT(nn.computeOutVF(sens, ac), out + 0.02);
  }
}


TEST(MLP, MLPCheckOuput) {
  MLP nn(1, 1, 2, 0.5f);
  std::vector<double> sens(0);

  fann_set_weight(nn.getNeuralNet(), 0, 2, 1.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 1.f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.f);

  uint ndim = 1;
  passdata d = {nn.getNeuralNet(), sens};
  ColumnVector ac(ndim);
  ac(1) = -1.f;
  double fx = -1;
  int result = 0;
  ColumnVector gx(ndim);
  SymmetricMatrix Hx(ndim);
  hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);

  EXPECT_DOUBLE_EQ(fx, tanh(0.5 * 1.));

  fann_set_weight(nn.getNeuralNet(), 0, 2, 0.2f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.4f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 0.6f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.8f);

  hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);

  EXPECT_DOUBLE_EQ(-fx, tanh(0.5 * (-1. * 0.2f + 0.4f)) * 0.6f + 0.8f);
}

TEST(MLP, MLPCheckDerivative) {
  MLP nn(1, 1, 2, 0.5f);
  std::vector<double> sens(0);

  fann_set_weight(nn.getNeuralNet(), 0, 2, 1.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 1.f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.f);

  uint ndim = 1;
  passdata d = {nn.getNeuralNet(), sens};
  ColumnVector ac(ndim);
  double a = -1;
  ac(1) = a;
  double fx = -1;
  int result = 0;

  ColumnVector gx(ndim);

  SymmetricMatrix Hx(ndim);

  double h = 1e-10;
  ac(1) = a - h;
  hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
  double fx_base = fx;
  ac(1) = a + h;
  hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
  double fx_h = fx;
  double derivative1 = (fx_h - fx_base) / (2.0 * h);

  ac(1) = a;
  hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
  double derivative2 = (fx_h - fx) / h;

  hs65(NLPGradient, ndim, ac, fx, gx, Hx, result, &d);

  EXPECT_GT(derivative1, gx(1) - 1e-5);
  EXPECT_LT(derivative1, gx(1) + 1e-5);

  EXPECT_GT(derivative2, gx(1) - 1e-5);
  EXPECT_LT(derivative2, gx(1) + 1e-5);
}

TEST(MLP, MLPCheckAnalyticscFuncDerHess) {
  MLP nn(1, 1, 0, 0.5f);

  fann_set_weight(nn.getNeuralNet(), 0, 2, -1.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);

  fann_set_weight(nn.getNeuralNet(), 2, 4, 1. / tanh(0.5 * 1.));
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.f);

  uint ndim = 1;
  std::vector<double> sens(0);
  passdata d = {nn.getNeuralNet(), sens};
  ColumnVector ac(ndim);
  double a = -1;
  ac(1) = a;
  double fx = -1;
  int result = 0;
  ColumnVector gx(ndim);

  SymmetricMatrix Hx(ndim);

  double C = 1. / (2. * tanh(0.5)) ;

  //exact computation
  for (a = -1; a <= 1.0; a += 1e-2) {
    ac(1) = a;
    hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
    EXPECT_GT(- fx, tanh(-0.5 * a) / tanh(0.5)  - 1e-7);  //opposite taking to maximaze
    EXPECT_LT(- fx, tanh(-0.5 * a) / tanh(0.5)  + 1e-7);

    hs65(NLPGradient, ndim, ac, fx, gx, Hx, result, &d);
    EXPECT_GT(- gx(1), - C * sech(0.5 * a) * sech(0.5 * a) - 1e-7);
    EXPECT_LT(- gx(1), - C * sech(0.5 * a) * sech(0.5 * a) + 1e-7);

    hs65(NLPHessian, ndim, ac, fx, gx, Hx, result, &d);
    EXPECT_GT(- Hx(1, 1), C * tanh(0.5 * a) * sech(0.5 * a) * sech(0.5 * a) - 1e-7);
    EXPECT_LT(- Hx(1, 1), C * tanh(0.5 * a) * sech(0.5 * a) * sech(0.5 * a) + 1e-7);
  }
}

TEST(MLP, MLPCheckDerivativeHard) {
  MLP nn(4, 10, 3, 0.5f);
  std::vector<double> sens(3);
  sens[0] = bib::Utils::randin(-1, 1);
  sens[1] = bib::Utils::randin(-1, 1);
  sens[2] = bib::Utils::randin(-1, 1);

  uint ndim = 1;
  passdata d = {nn.getNeuralNet(), sens};
  ColumnVector ac(ndim);
  ColumnVector gx(ndim);
  SymmetricMatrix Hx(ndim);
  int result = 0;
  double fx = -1;

  for (double a = -1; a <= 1.0; a += 1e-2) {
    ac(1) = a;

    double h = 1e-10;
    ac(1) = a - h;
    hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
    double fx_base = fx;
    ac(1) = a + h;
    hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
    double fx_h = fx;
    double derivative1 = (fx_h - fx_base) / (2.0 * h);

    ac(1) = a;
    hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
    double derivative2 = (fx_h - fx) / h;

    hs65(NLPGradient, ndim, ac, fx, gx, Hx, result, &d);

    EXPECT_GT(derivative1, gx(1) - 1e-5);
    EXPECT_LT(derivative1, gx(1) + 1e-5);

    EXPECT_GT(derivative2, gx(1) - 1e-5);
    EXPECT_LT(derivative2, gx(1) + 1e-5);
  }
}

TEST(MLP, MLPCheckHessian) {
  MLP nn(4, 10, 3, 0.5f);
  std::vector<double> sens(3);
  sens[0] = bib::Utils::randin(-1, 1);
  sens[1] = bib::Utils::randin(-1, 1);
  sens[2] = bib::Utils::randin(-1, 1);

  uint ndim = 1;
  passdata d = {nn.getNeuralNet(), sens};
  ColumnVector ac(ndim);
  ColumnVector gx(ndim);
  SymmetricMatrix Hx(ndim);
  int result = 0;
  double fx = -1;

  for (double a = -1; a <= 1.0; a += 1e-2) {
    ac(1) = a;

    double h = 1e-5;
    ac(1) = a - h;
    hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
    double fx_base = fx;
    ac(1) = a + h;
    hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
    double fx_h = fx;

    ac(1) = a;
    hs65(NLPFunction, ndim, ac, fx, gx, Hx, result, &d);
    double snd_derivative = (fx_h - 2.0 * fx + fx_base) / (h * h);

    hs65(NLPHessian, ndim, ac, fx, gx, Hx, result, &d);

    EXPECT_GT(snd_derivative, Hx(1, 1) - 1e-5);
    EXPECT_LT(snd_derivative, Hx(1, 1) + 1e-5);
  }
}

TEST(MLP, OptimizeOpposite) {
  MLP nn(1, 1, 0, 0.5f);

  fann_set_weight(nn.getNeuralNet(), 0, 2, -1.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);

  fann_set_weight(nn.getNeuralNet(), 2, 4, 1. / tanh(0.5 * 1.));
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.f);

  // Test
  std::vector<double> sens(0);
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = x1 == 1.f ? -1.f : 1.f;

    std::vector<double> ac(1);
    ac[0] = x1;

    EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), out);
  }

  std::vector<double>* ac = nn.optimized(sens);
  EXPECT_GT(ac->at(0), -1. - 0.01);
  EXPECT_LT(ac->at(0), -1. + 0.01);
  delete ac;

  fann_set_weight(nn.getNeuralNet(), 0, 2, 1.f);
  ac = nn.optimized(sens);
  EXPECT_GT(ac->at(0), 1. - 0.01);
  EXPECT_LT(ac->at(0), 1. + 0.01);
  delete ac;

  // Learn
  for (uint n = 0; n < 1000 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = x1 == 1.f ? -1.f : 1.f;

    std::vector<double> sens(0);
    std::vector<double> acc(1);
    acc[0] = x1;

    nn.learn(sens, acc, out);
  }

  ac = nn.optimized(sens);
  EXPECT_GT(ac->at(0), -1. - 0.01);
  EXPECT_LT(ac->at(0), -1. + 0.01);

//     LOG_DEBUG(nn.computeOut(sens, *ac));
  ac->at(0) = -1;
//     LOG_DEBUG(nn.computeOut(sens, *ac));
  delete ac;
}


TEST(MLP, OptimizeAndOr) {
  MLP nn(3, 10, 2, 0.5f);

  // Learn
  for (uint n = 0; n < 10000 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;
    double x3 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = AND(OR(x1, x2), x3);

    std::vector<double> sens(2);
    sens[0] = x1;
    sens[1] = x2;
    std::vector<double> ac(1);
    ac[0] = x3;

    nn.learn(sens, ac, out);
  }



  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;

    std::vector<double> sens(2);
    sens[0] = x1;
    sens[1] = x2;

    std::vector<double>* ac = nn.optimized(sens);
    double his_sol = nn.computeOutVF(sens, *ac);

    ac->at(0) = 1.f;
    double my_sol = nn.computeOutVF(sens, *ac);

    EXPECT_GE(his_sol, my_sol - 0.01);
    delete ac;
  }
}


TEST(MLP, OptimizeMultiDim) {
  MLP nn(3, 20, 0, 0.5f);

  // Learn
  for (uint n = 0; n < 10000 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;
    double x3 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = AND(OR(x1, x2), x3);

    std::vector<double> sens(0);
    std::vector<double> ac(3);
    ac[0] = x1;
    ac[1] = x2;
    ac[2] = x3;

    nn.learn(sens, ac, out);
  }

  // Test
  for (uint n = 0; n < 100 ; n++) {
    std::vector<double> sens(0);
    std::vector<double>* ac = nn.optimized(sens);
    double his_sol = nn.computeOutVF(sens, *ac);
    double my_sol = 1.;

    EXPECT_GE(his_sol, my_sol - 0.01);
    delete ac;
  }
}

TEST(MLP, OptimizeNonExtremum) {
  //learn x^2 - x
  //root min ~0.474
  //root max -1

  double sensv = 1. ;
  for (uint n = 0; n < 1 ; n++) {
    MLP nn(1, 3, 0, 0.2f);
    std::vector<double> sens(0);
    std::vector<double> ac(1);

    // Learn
    for (uint n = 0; n < 100000 ; n++) {
      double x1 = (bib::Utils::rand01() * 2.) - 1;

      double out = x1 * x1 - x1;
      ac[0] = x1;

      nn.learn(sens, ac, sensv * out);
    }

    //Test learned
    for (uint n = 0; n < 1000 ; n++) {
      double x1 = (bib::Utils::rand01() * 2.) - 1;

      double out = x1 * x1 - x1;
      ac[0] = x1;

      double myout = nn.computeOutVF(sens, ac);
      EXPECT_GT(myout, sensv * out - 0.1);
      EXPECT_LT(myout, sensv * out + 0.1);
      LOG_FILE("OptimizeNonExtremum.data", x1 << " " << myout << " " << out);
//     close all; X=load('OptimizeNonExtremum.data'); plot(X(:,1),X(:,2), '.'); hold on; plot(X(:,1),X(:,3), 'r.');
    }

    std::vector<double>* acopt = nn.optimized(sens, {}, NB_SOL_OPTIMIZATION);
    if (sensv == -1.) {
      EXPECT_GT(acopt->at(0), 0.46);
      EXPECT_LT(acopt->at(0), 0.48);
    } else {
      EXPECT_LT(acopt->at(0), -0.99);
      EXPECT_GT(acopt->at(0), -1.01);
    }
    delete acopt;
    sensv *= -1;
  }
}


TEST(MLP, Optimize2LocalMaxima) {
  //learn - cos(5.*x1)/2.

  MLP nn(1, 10, 0, 0.1f);
  std::vector<double> sens(0);
  std::vector<double> ac(1);

  fann_set_training_algorithm(nn.getNeuralNet(), FANN_TRAIN_RPROP); //adaptive algorithm without learning rate
  fann_set_train_error_function(nn.getNeuralNet(), FANN_ERRORFUNC_TANH);

  struct fann_train_data* data = fann_create_train(2500, 1, 1);

  // Learn
  for (uint n = 0; n < 2500 ; n++) {
    double x1 = (bib::Utils::rand01() * 2.) - 1;

    double out = - cos(5.*x1) / 2.;
    ac[0] = x1;

    data->input[n][0] = x1;
    data->output[n][0] = out;
  }

  fann_train_on_data(nn.getNeuralNet(), data, 3000, 0, 0.0005);
  fann_destroy_train(data);

  //Test learned
  for (uint n = 0; n < 1000 ; n++) {
    double x1 = (bib::Utils::rand01() * 2.) - 1;

    double out = - cos(5.*x1) / 2.;
    ac[0] = x1;

    double myout = nn.computeOutVF(sens, ac);
    LOG_FILE("Optimize2LocalMaxima.data", x1 << " " << myout << " " << out);
//     close all; X=load('Optimize2LocalMaxima.data'); plot(X(:,1),X(:,2), '.'); hold on; plot(X(:,1),X(:,3), 'r.');
  }

  std::vector<double>* acopt = nn.optimized(sens, {}, NB_SOL_OPTIMIZATION);
//     ac[0] = acopt->at(0);
//     double my_sol = nn.computeOut(sens, ac);
//     ac[0] = M_PI/5;
//     double best_sol = nn.computeOut(sens, ac);
//     LOG_DEBUG("my sol : " << my_sol << " | best : " << best_sol << " (" << acopt->at(0) << " vs " << M_PI/5 << ") ");

  double precision = 0.1;
  if (acopt->at(0) > 0) {
    EXPECT_GT(acopt->at(0), M_PI / 5. - precision);
    EXPECT_LT(acopt->at(0), M_PI / 5. + precision);
  } else {
    EXPECT_GT(acopt->at(0), -M_PI / 5. - precision);
    EXPECT_LT(acopt->at(0), -M_PI / 5 + precision);
  }

  delete acopt;
}


TEST(MLP, OptimizeTrapInEvilLocalOptimal) {
  //learn sin(-3*x*x*x+2*x*x+x)
  //local max ~0.6
  //global max -0.7

  MLP nn(1, 12, 0, 0.1f);
  std::vector<double> sens(0);
  std::vector<double> ac(1);

  fann_set_training_algorithm(nn.getNeuralNet(), FANN_TRAIN_RPROP); //adaptive algorithm without learning rate
  fann_set_train_error_function(nn.getNeuralNet(), FANN_ERRORFUNC_TANH);

  struct fann_train_data* data = fann_create_train(2500, 1, 1);

  // Learn
  for (uint n = 0; n < 2500 ; n++) {
    double x1 = (bib::Utils::rand01() * 2.) - 1;

    double out = sin(-3 * x1 * x1 * x1 + 2 * x1 * x1 + x1);
    ac[0] = x1;

    data->input[n][0] = x1;
    data->output[n][0] = out;
  }

  fann_train_on_data(nn.getNeuralNet(), data, 10000, 0, 0.0005);
  fann_destroy_train(data);

  //Test learned
  for (uint n = 0; n < 1000 ; n++) {
    double x1 = (bib::Utils::rand01() * 2.) - 1;

    double out = sin(-3 * x1 * x1 * x1 + 2 * x1 * x1 + x1);
    ac[0] = x1;

    double myout = nn.computeOutVF(sens, ac);
    LOG_FILE("OptimizeTrapInEvilLocalOptimal.data", x1 << " " << myout << " " << out);
//     close all; X=load('OptimizeTrapInEvilLocalOptimal.data'); plot(X(:,1),X(:,2), '.'); hold on; plot(X(:,1),X(:,3), 'r.');
  }

  std::vector<double>* acopt = nn.optimized(sens, {}, NB_SOL_OPTIMIZATION+100);
//     LOG_DEBUG(acopt->at(0));

  double precision = 0.1;

  if(acopt->at(0) < -0.7 - precision || acopt->at(0) > -0.7 + precision) {
    LOG_DEBUG(nn.computeOutVF(sens, *acopt));
    ac[0] = -0.7;
    LOG_DEBUG(nn.computeOutVF(sens, ac));
  }

  EXPECT_GT(acopt->at(0), -0.7 - precision);
  EXPECT_LT(acopt->at(0), -0.7 + precision);

  delete acopt;
}



TEST(MLP, OptimizePlateau) {
  //learn sin(-3*x*x*x+2*x*x+x)
  //local max ~0.6
  //global max -0.7

  MLP nn(1, 2, 0, 0.1f);
  std::vector<double> sens(0);
  std::vector<double> ac(1);

  fann_set_training_algorithm(nn.getNeuralNet(), FANN_TRAIN_RPROP); //adaptive algorithm without learning rate
  fann_set_train_error_function(nn.getNeuralNet(), FANN_ERRORFUNC_TANH);

  struct fann_train_data* data = fann_create_train(5000, 1, 1);

  // Learn
  for (uint n = 0; n < 5000 ; n++) {
    double x1 = n < 2000 ? (bib::Utils::rand01() * 2.) - 1 : bib::Utils::randin(0, 0.5);

    double out = x1 >= 0.2 ? 1. : 0.2;
    ac[0] = x1;

    data->input[n][0] = x1;
    data->output[n][0] = out;
  }

  fann_train_on_data(nn.getNeuralNet(), data, 10000, 0, 0.00001);
  fann_destroy_train(data);

  //Test learned
  for (uint n = 0; n < 1000 ; n++) {
    double x1 = (bib::Utils::rand01() * 2.) - 1;

    double out = x1 >= 0.2 ? 1. : 0.2;
    ac[0] = x1;

    double myout = nn.computeOutVF(sens, ac);
    LOG_FILE("OptimizePlateau.data", x1 << " " << myout << " " << out);
//     close all; X=load('OptimizePlateau.data'); plot(X(:,1),X(:,2), '.'); hold on; plot(X(:,1),X(:,3), 'r.');
  }

  std::vector<double>* acopt = nn.optimized(sens, {}, NB_SOL_OPTIMIZATION);
//     LOG_DEBUG(acopt->at(0));

  double precision = 0.01;

  EXPECT_GT(acopt->at(0), 0.2 - precision);

  delete acopt;
}

TEST(MLP, ConsistentActivationFunctionLecun) {
  MLP nn(1, 1, 0, 0.01f, true);

  fann_set_weight(nn.getNeuralNet(), 0, 2, 1.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 1.f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.f);
  
  std::vector<double> sens(0);
  std::vector<double> ac(1);
  ac[0] = 1.d;

  double lambda = atanh(1.d/sqrt(3.d));
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), sqrt(3.d)*tanh(lambda * 1.d));

  ac[0] = 1.5d;
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), sqrt(3.d)*tanh(lambda * 1.5d));//1.31
  
  fann_set_weight(nn.getNeuralNet(), 0, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 0.f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 5.f);

  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), 5.f);

  fann_set_weight(nn.getNeuralNet(), 0, 2, 0.2f);
  fann_set_weight(nn.getNeuralNet(), 1, 2, 0.4f);
  fann_set_weight(nn.getNeuralNet(), 2, 4, 0.6f);
  fann_set_weight(nn.getNeuralNet(), 3, 4, 0.8f);

  ac[0] = -1.d;
  EXPECT_DOUBLE_EQ(nn.computeOutVF(sens, ac), (sqrt(3.d)*tanh(lambda * (-1. * 0.2f + 0.4f)) * 0.6f + 0.8f));
}

TEST(MLP, LearnAndOrLecun) {
  MLP nn(3, 10, 2, 0.05f, true);

  // Learn
  for (uint n = 0; n < 10000 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;
    double x3 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = AND(OR(x1, x2), x3);

    std::vector<double> sens(2);
    sens[0] = x1;
    sens[1] = x2;
    std::vector<double> ac(1);
    ac[0] = x3;
    
    nn.learn(sens, ac, out);
  }

  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;
    double x3 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = AND(OR(x1, x2), x3);

    std::vector<double> sens(2);
    sens[0] = x1;
    sens[1] = x2;
    std::vector<double> ac(1);
    ac[0] = x3;

    EXPECT_GT(nn.computeOutVF(sens, ac), out - 0.02);
    EXPECT_LT(nn.computeOutVF(sens, ac), out + 0.02);
  }
}

TEST(MLP, LearnAndOrLecun2) {
  MLP nn(3, 5, 1, true);

  struct fann_train_data* data = fann_create_train(2*2*2, 3, 1);
   
  uint n = 0;
  auto iter = [&](const std::vector<double>& x) {
    data->input[n][0]= x[0];
    data->input[n][1]= x[1];
    data->input[n][2]= x[2];
    
    double out = AND(OR(x[0], x[1]), x[2]);
    data->output[n][0]= out;
    n++;
  };
    
  bib::Combinaison::continuous<>(iter, 3, -1, 1, 1);
  
  nn.learn_stoch(data, 20000, 0, 0.00000001);
  
  fann_destroy_train(data);

  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::randBool() ? 1.f : -1.f;
    double x2 = bib::Utils::randBool() ? 1.f : -1.f;
    double x3 = bib::Utils::randBool() ? 1.f : -1.f;

    double out = AND(OR(x1, x2), x3);

    std::vector<double> sens(2);
    sens[0] = x1;
    sens[1] = x2;
    std::vector<double> ac(1);
    ac[0] = x3;

    EXPECT_GT(nn.computeOutVF(sens, ac), out - 0.02);
    EXPECT_LT(nn.computeOutVF(sens, ac), out + 0.02);
    
    std::vector<double> in(3);
    in[0] = x1;
    in[1] = x2;
    in[2] = x3;
    std::vector<double>* outnn = nn.computeOut(in);
    
    EXPECT_GT(outnn->at(0), out - 0.05);
    EXPECT_LT(outnn->at(0), out + 0.05);
    
    delete outnn;
  }
}


// 1 0.2
// 3 0.3
// 5 0.3
// 
// Result:  y = 0.025 x + 1.916666667·10-1
// 
// 2.166666667·10-1 		 1.666666667·10-2 
// 2.666666667·10-1 		 3.333333333·10-2 
// 3.166666667·10-1 		 1.666666667·10-2 
TEST(MLP, LearnLabelWeight) {
  fann* lin_nn =fann_create_standard(2, 1, 1);
  fann_set_activation_function_output(lin_nn, FANN_LINEAR);
  fann_set_learning_momentum(lin_nn, 0.);
  fann_set_training_algorithm(lin_nn, FANN_TRAIN_RPROP);

  struct fann_train_data* data = fann_create_train(3, 1, 1);
  fann_type lw [3];
  
  uint n = 0;
  data->input[n][0]= 1.f;
  data->output[n][0]= 2.f/10.f;
  lw[n] = 1.f;
  n++;
  data->input[n][0]= 3.f;
  data->output[n][0]= 3.f/10.f;
  lw[n] = 0.5f;
  n++;
  data->input[n][0]= 5.f;
  data->output[n][0]= 3.f/10.;
  lw[n] = 1.f;
  n++;
  
  for(int i=0;i<1000; i++)
    fann_train_epoch(lin_nn, data);
  
  fann_type out_no_lw[3];
  fann_type * out;
  n=0;
  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.216 - 0.002);
  EXPECT_LT(out[0], 0.216 + 0.002);
  out_no_lw[n]=out[0];
  n++;

  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.266 - 0.002);
  EXPECT_LT(out[0], 0.266 + 0.002);
  out_no_lw[n]=out[0];
  n++;
  
  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.316 - 0.002);
  EXPECT_LT(out[0], 0.316 + 0.002);
  out_no_lw[n]=out[0];
  
  for(int i=0;i<1000; i++)
    fann_train_epoch_lw(lin_nn, data, lw);
  
  n=0;
  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.2);
  EXPECT_LT(out[0], out_no_lw[n]);
  out_no_lw[n]=out[0];
  n++;

  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.25);
  EXPECT_LT(out[0], out_no_lw[n]);
  out_no_lw[n]=out[0];
  n++;
  
  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.3);
  EXPECT_LT(out[0], out_no_lw[n]);
  out_no_lw[n]=out[0];
  
  lw[1] = 0.05;
  for(int i=0;i<1000; i++)
    fann_train_epoch_lw(lin_nn, data, lw);
    
  n=0;
  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.2);
  EXPECT_LT(out[0], out_no_lw[n]);
  n++;

  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.25);
  EXPECT_LT(out[0], out_no_lw[n]);
  n++;
  
  out = fann_run(lin_nn, data->input[n]);
  EXPECT_GT(out[0], 0.3);
  EXPECT_LT(out[0], out_no_lw[n]);
  
  fann_destroy_train(data);
}

TEST(MLP, LearnNonLinearLabelWeight) {
  MLP nn(1, 4, 1, true);

  struct fann_train_data* data = fann_create_train(11*2, 1, 1);
  fann_type label_weight[11*2];
  
  uint n = 0;
  auto iter = [&](const std::vector<double>& x) {
    data->input[n][0]= x[0];
    data->output[n][0]= x[0];
    label_weight[n] = 1.f;
    n++;
  };
  
  auto iter2 = [&](const std::vector<double>& x) {
    data->input[n][0]= x[0];
    data->output[n][0]= x[0]*x[0];
    label_weight[n] = 0.2f;
    n++;
  };
  
  bib::Combinaison::continuous<>(iter, 1, 0, 1, 10);
  bib::Combinaison::continuous<>(iter2, 1, 0, 1, 10);
  
  nn.learn_stoch(data, 20000, 0, 0.00000001);
  
  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::rand01();

    double out = (x1 + x1*x1)/2.;

    std::vector<double> sens(1);
    sens[0] = x1;
    std::vector<double> ac(0);
    
    double old_out1 = x1;
    double old_out2 = x1*x1;

    EXPECT_GT(nn.computeOutVF(sens, ac), out - 0.15);
    EXPECT_LT(nn.computeOutVF(sens, ac), out + 0.15);
    
    if(x1 > 0.15 && x1 < 0.85){
      EXPECT_GE(fabs(nn.computeOutVF(sens, ac) - old_out1), fabs(nn.computeOutVF(sens, ac) - out));
      EXPECT_GE(fabs(nn.computeOutVF(sens, ac) - old_out2), fabs(nn.computeOutVF(sens, ac) - out));
    }
      
  }
  
  nn.learn_stoch_lw(data, label_weight, 20000, 0, 0.00000001);
  
  // Test
  for (uint n = 0; n < 100 ; n++) {
    double x1 = bib::Utils::rand01();

    double old_out = (x1 + x1*x1)/2.;
    double out = (x1 + 0.2*x1*x1)/1.2;

    std::vector<double> sens(1);
    sens[0] = x1;
    std::vector<double> ac(0);

    EXPECT_GT(nn.computeOutVF(sens, ac), out - 0.1);
    EXPECT_LT(nn.computeOutVF(sens, ac), out + 0.1);
    
    if(x1 > 0.15 && x1 < 0.85)
      EXPECT_GE(fabs(nn.computeOutVF(sens, ac) - old_out), fabs(nn.computeOutVF(sens, ac) - out));
  }
  
  fann_destroy_train(data);
}