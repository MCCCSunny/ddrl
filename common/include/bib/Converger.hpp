#ifndef CONVERGER_HPP
#define CONVERGER_HPP

#include <limits>

#include <bib/Logger.hpp>

namespace bib {

class Converger {
 public:
  template<typename Function1, typename Function2>
  static uint determinist(Function1 && iter, Function2 && eval, uint max_epoch, float precision, uint display_each = 0) {

    bool converged = false;
    bool limit_reached = false;
    uint epoch = 0;
    double last_mse = 0;
    double last2_mse = 0;
    if (display_each == 0)
      display_each = max_epoch + 1;
    while (!converged && !limit_reached) {
      iter();

      if (epoch > 0 && epoch % display_each == 0)
        LOG_DEBUG(epoch << " " << eval());

      if (epoch > 1 && fabs(eval() - last_mse) < precision && fabs(last2_mse - last_mse) < precision) {
        converged = true;
      } else {
        converged = eval() < precision;
      }

      last2_mse = last_mse;
      last_mse = eval();
      limit_reached = epoch >= max_epoch;
      epoch++;
    }
    if (display_each != max_epoch + 1)
      LOG_DEBUG(epoch << " " << eval());

    return epoch;
  }

  template<typename Function1, typename Function2, typename Function3>
  static uint min_stochastic(Function1 && iter, Function2 && eval, Function3 && save, uint max_epoch,
                             float precision, uint display_each = 0, uint number_consecp = 20) {

    bool converged = false;
    bool limit_reached = false;
    bool too_consec_bad_mov = false;

    uint epoch = 0;
    uint consecutive_bad_movement = 0;
    double last_mse = 0;
    double last2_mse = 0;
    double minv = std::numeric_limits<double>::max();

    if (display_each == 0)
      display_each = max_epoch + 1;
    while (!converged && !limit_reached && !too_consec_bad_mov) {
      iter();

      double v = eval();
      if(v <= minv) {
        minv = v;
        save();
        consecutive_bad_movement=0;
      } else
        consecutive_bad_movement++;

      if (epoch > 0 && epoch % display_each == 0)
        LOG_DEBUG(epoch << " " << minv << " " << v);

      if (epoch > 1 && fabs(v - last_mse) < precision && fabs(last2_mse - last_mse) < precision) {
        converged = true;
      } else {
        converged = v < precision;
      }

      last2_mse = last_mse;
      last_mse = v;
      limit_reached = epoch >= max_epoch;
      too_consec_bad_mov = consecutive_bad_movement > number_consecp;

      epoch++;
    }
    if (display_each != max_epoch + 1)
      LOG_DEBUG(epoch << " " << minv);

    return epoch;
  }

};

}

#endif