#include "bib/Assert.hpp"
#include "arch/Simulator.hpp"
#include "arch/Example.hpp"
#include "HumanoidEnv.hpp"
#include "OfflineCaclaAg.hpp"
#include "nn/DODevMLP.hpp"

int main(int argc, char **argv) {
  FLAGS_minloglevel = 2;
  google::InitGoogleLogging(argv[0]);
  google::InstallFailureSignalHandler();
  
  arch::Simulator<HumanoidEnv, OfflineCaclaAg<DODevMLP>> s;
  s.init(argc, argv);
  
  s.run();
  
  LOG_DEBUG("works !");
}
