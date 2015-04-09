#ifndef ADVANCEDACROBOTWORLD_HPP
#define ADVANCEDACROBOTWORLD_HPP

#include <vector>
#include "ode/ode.h"

#include "bib/Assert.hpp"
#include "ODEObject.hpp"
#include "ODEFactory.hpp"

#define BONE_LENGTH 0.3
#define BONE_LARGER 0.02
#define STARTING_Z 0.5

#define GRAVITY -9.81
#define BONE_DENSITY 1062 //Average human body density
#define MAX_TORQUE_HINGE 1.5
#define MAX_TORQUE_SLIDER 40
#define WORLD_STEP 0.01

enum bone_joint { HINGE, SLIDER };

class AdvancedAcrobotWorld {
 public:
  AdvancedAcrobotWorld(const std::vector<bone_joint> &types = {HINGE, HINGE},
                       const std::vector<bool> &actuators = {false, false, true});
  virtual ~AdvancedAcrobotWorld();

  void resetPositions();

  virtual void step(const std::vector<float> &motors);
  const std::vector<float> &state() const;
  unsigned int activated_motors() const;
  float perf() const;

 protected:
  void createWorld(const std::vector<bone_joint> &);

 public:
  ODEWorld odeworld;

 protected:
  std::vector<bone_joint> types;
  std::vector<bool> actuators;

  std::vector<ODEObject *> bones;
  std::vector<dJointID> joints;

  std::vector<float> internal_state;

  dGeomID ground;

 private:
  bool goalBeenReached;
  bool goalFailed;
  unsigned int _activated_motors;
};

struct nearCallbackData {
  AdvancedAcrobotWorld *inst;
};

#endif  // ADVANCEDACROBOTWORLD_HPP
