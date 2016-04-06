
#ifndef OFFVSETACFITTED_HPP
#define OFFVSETACFITTED_HPP

#include <vector>
#include <string>
#include <boost/serialization/list.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/filesystem.hpp>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

#include "arch/AACAgent.hpp"
#include "bib/Seed.hpp"
#include "bib/Utils.hpp"
#include <bib/MetropolisHasting.hpp>
#include <bib/XMLEngine.hpp>
#include "MLP.hpp"
#include "LinMLP.hpp"
#include "kde.hpp"
#include "kdtree++/kdtree.hpp"

typedef struct _sample {
  std::vector<double> s;
  std::vector<double> pure_a;
  std::vector<double> a;
  std::vector<double> next_s;
  double r;
  bool goal_reached;
  double p0;

  friend class boost::serialization::access;
  template <typename Archive>
  void serialize(Archive& ar, const unsigned int) {
    ar& BOOST_SERIALIZATION_NVP(s);
    ar& BOOST_SERIALIZATION_NVP(pure_a);
    ar& BOOST_SERIALIZATION_NVP(a);
    ar& BOOST_SERIALIZATION_NVP(next_s);
    ar& BOOST_SERIALIZATION_NVP(r);
    ar& BOOST_SERIALIZATION_NVP(goal_reached);
  }

  bool operator< (const _sample& b) const {
    for (uint i = 0; i < s.size(); i++) {
      if(s[i] != b.s[i])
        return s[i] < b.s[i];
    }
    
    for (uint i = 0; i < a.size(); i++) {
      if(a[i] != b.a[i])
        return a[i] < b.a[i];
    }

    return false;
  }
  
  bool operator!=(const _sample& b) const {
    for (uint i = 0; i < s.size(); i++) {
      if(fabs(s[i] - b.s[i])>=0.000001f)
        return true;
    }
    
    for (uint i = 0; i < a.size(); i++) {
      if(fabs(a[i] - b.a[i])>=0.000001f)
        return true;
    }
    
    for (uint i = 0; i < pure_a.size(); i++) {
      if(fabs(pure_a[i] - b.pure_a[i])>=0.000001f)
        return true;
    }
    
    for (uint i = 0; i < next_s.size(); i++) {
      if(fabs(next_s[i] - b.next_s[i])>=0.000001f)
        return true;
    }
    //LOG_DEBUG(s[0] << " " << b.s[0]);
    return fabs(r - b.r)>=0.000001f || goal_reached != b.goal_reached ;
  }
  
  typedef double value_type;

  inline double operator[](size_t const N) const{
        return s[N];
  }
  
} sample;

class OffVSetACFitted : public arch::AACAgent<MLP, arch::AgentProgOptions> {
 public:
  typedef MLP PolicyImpl;
   
  OffVSetACFitted(unsigned int _nb_motors, unsigned int _nb_sensors)
    : arch::AACAgent<MLP, arch::AgentProgOptions>(_nb_motors), nb_sensors(_nb_sensors) {

  }

  virtual ~OffVSetACFitted() {
    delete kdtree_s;
    
    delete vnn;
    delete ann;
  }

  const std::vector<double>& _run(double reward, const std::vector<double>& sensors,
                                 bool learning, bool goal_reached, bool) {

    vector<double>* next_action = ann->computeOut(sensors);
    
    if (last_action.get() != nullptr && learning){
      double p0 = 1.f;
      if(!gaussian_policy){
	  if (std::equal(last_action->begin(), last_action->end(), last_pure_action->begin()))//no explo
	    p0 = 1 - noise;
	  else
	    p0 = noise * 0.5f; //should be 0 but p0 > 0
      } else {
	  p0 = 1.f;
	  for(uint i=0;i < nb_motors;i++)
	    p0 *= exp(-(last_pure_action->at(i)-last_action->at(i))*(last_pure_action->at(i)-last_action->at(i))/(2.f*noise*noise));
      }
      
      trajectory.insert({last_state, *last_pure_action, *last_action, sensors, reward, goal_reached, p0});
      last_trajectory.insert( {last_state, *last_pure_action, *last_action, sensors, reward, goal_reached, p0});
      proba_s.add_data(last_state);
      
      sample sa = {last_state, *last_pure_action, *last_action, sensors, reward, goal_reached, p0};
      kdtree_s->insert(sa);
    }

    last_pure_action.reset(new vector<double>(*next_action));
    if(learning) {
      if(gaussian_policy){
        vector<double>* randomized_action = bib::Proba<double>::multidimentionnalGaussianWReject(*next_action, noise);
        delete next_action;
        next_action = randomized_action;
      } else if(bib::Utils::rand01() < noise){ //e-greedy
        for (uint i = 0; i < next_action->size(); i++)
          next_action->at(i) = bib::Utils::randin(-1.f, 1.f);
      }
    }
    last_action.reset(next_action);


    last_state.clear();
    for (uint i = 0; i < sensors.size(); i++)
      last_state.push_back(sensors[i]);

    return *next_action;
  }


  void _unique_invoke(boost::property_tree::ptree* pt, boost::program_options::variables_map*) override {
    hidden_unit_v           = pt->get<int>("agent.hidden_unit_v");
    hidden_unit_a           = pt->get<int>("agent.hidden_unit_a");
    noise                   = pt->get<double>("agent.noise");
    gaussian_policy         = pt->get<bool>("agent.gaussian_policy");
    lecun_activation        = pt->get<bool>("agent.lecun_activation");
    change_v_each           = pt->get<uint>("agent.change_v_each");
    strategy_u              = pt->get<uint>("agent.strategy_u");
    current_loaded_v        = pt->get<uint>("agent.index_starting_loaded_v");

    if(hidden_unit_v == 0)
      vnn = new LinMLP(nb_sensors , 1, 0.0, lecun_activation);
    else
      vnn = new MLP(nb_sensors, hidden_unit_v, nb_sensors, 0.0, lecun_activation);

    if(hidden_unit_a == 0)
      ann = new LinMLP(nb_sensors , nb_motors, 0.0, lecun_activation);
    else
      ann = new MLP(nb_sensors, hidden_unit_a, nb_motors, lecun_activation);
    
    kdtree_s = new kdtree_sample(nb_sensors);
  }

  void _start_episode(const std::vector<double>& sensors, bool) override {
    last_state.clear();
    for (uint i = 0; i < sensors.size(); i++)
      last_state.push_back(sensors[i]);

    last_action = nullptr;
    last_pure_action = nullptr;
    
    //trajectory.clear();
    last_trajectory.clear();
    
    fann_reset_MSE(vnn->getNeuralNet());
    
    if(episode == 0 || episode % change_v_each == 0){
      if (! boost::filesystem::exists( "vset."+std::to_string(current_loaded_v) ) ){
        LOG_ERROR("file doesn't exists "<< "vset."+std::to_string(current_loaded_v));
        exit(1);
      }
      
      vnn->load("vset."+std::to_string(current_loaded_v));
      current_loaded_v++;
    }
  }
  
  void computePTheta(vector< sample >& vtraj, double *ptheta){
    uint i=0;
    for(auto it = vtraj.begin(); it != vtraj.end() ; ++it) {
      sample sm = *it;
      double p0 = 1.f;
      vector<double>* next_action = ann->computeOut(sm.s);
      
      if(!gaussian_policy){
	  if (std::equal(last_action->begin(), last_action->end(), last_pure_action->begin()))//no explo
	    p0 = 1 - noise;
	  else
	    p0 = noise * 0.5f; //should be 0 but p0 > 0
      } else {
	  p0 = 1.f;
	  for(uint i=0;i < nb_motors;i++)
	    p0 *= exp(-(last_pure_action->at(i)-last_action->at(i))*(last_pure_action->at(i)-last_action->at(i))/(2.f*noise*noise));
      }
	
      ptheta[i] = p0;
      i++;
      delete next_action;
    }
  }
  
  void update_actor_old_lw(){
    if (last_trajectory.size() > 0) {
      struct fann_train_data* data = fann_create_train(last_trajectory.size(), nb_sensors, nb_motors);

      uint n=0;
      std::vector<double> deltas;
      for(auto it = last_trajectory.begin(); it != last_trajectory.end() ; ++it) {
        sample sm = *it;

        double target = 0.f;
        double mine = 0.f;
        
	target = sm.r;
	if (!sm.goal_reached) {
	  double nextV = vnn->computeOutVF(sm.next_s, {});
	  target += gamma * nextV;
	}
	mine = vnn->computeOutVF(sm.s, {});

        if(target > mine) {
          for (uint i = 0; i < nb_sensors ; i++)
            data->input[n][i] = sm.s[i];
          for (uint i = 0; i < nb_motors; i++)
            data->output[n][i] = sm.a[i];
        
          deltas.push_back(target - mine);
          n++;
        }
      }
          

      if(n > 0) {
        double* importance = new double[n];
        double norm = *std::max_element(deltas.begin(), deltas.end());
        for (uint i =0 ; i < n; i++){
            importance[i] = deltas[i] / norm;
        }    
        
        struct fann_train_data* subdata = fann_subset_train_data(data, 0, n);

        ann->learn_stoch_lw(subdata, importance, 5000, 0, 0.0001);

        fann_destroy_train(subdata);
        
        delete[] importance;
      }
      fann_destroy_train(data);
      
    } 
  }
  
  void update_actor_old_lw_all(){
    if (trajectory.size() > 0) {
      struct fann_train_data* data = fann_create_train(trajectory.size(), nb_sensors, nb_motors);

      uint n=0;
      std::vector<double> deltas;
      for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
        sample sm = *it;

        double target = 0.f;
        double mine = 0.f;
        
	target = sm.r;
	if (!sm.goal_reached) {
	  double nextV = vnn->computeOutVF(sm.next_s, {});
	  target += gamma * nextV;
	}
	mine = vnn->computeOutVF(sm.s, {});

        if(target > mine) {
          for (uint i = 0; i < nb_sensors ; i++)
            data->input[n][i] = sm.s[i];
          for (uint i = 0; i < nb_motors; i++)
            data->output[n][i] = sm.a[i];
        
          deltas.push_back(target - mine);
          n++;
        }
      }
          

      if(n > 0) {
        double* importance = new double[n];
        double norm = *std::max_element(deltas.begin(), deltas.end());
        for (uint i =0 ; i < n; i++){
            importance[i] = deltas[i] / norm;
        }    
        
        struct fann_train_data* subdata = fann_subset_train_data(data, 0, n);

        ann->learn_stoch_lw(subdata, importance, 5000, 0, 0.0001);

        fann_destroy_train(subdata);
        
        delete[] importance;
      }
      fann_destroy_train(data);
      
    } 
  }

  void update_actor_old(){
     if (last_trajectory.size() > 0) {
      bool update_delta_neg;
      bool update_pure_ac;
      
      switch(strategy_u){
        case 0:
          update_pure_ac=false;
          update_delta_neg=false;
          break;
        case 1:
          update_pure_ac=true;
          update_delta_neg=false;
          break;
        case 2:
          update_pure_ac=false;
          update_delta_neg=true;
          break;
      }

      struct fann_train_data* data = fann_create_train(last_trajectory.size(), nb_sensors, nb_motors);

      uint n=0;
      for(auto it = last_trajectory.begin(); it != last_trajectory.end() ; ++it) {
        sample sm = *it;

        double target = 0.f;
        double mine = 0.f;
        
	target = sm.r;
	if (!sm.goal_reached) {
	  double nextV = vnn->computeOutVF(sm.next_s, {});
	  target += gamma * nextV;
	}
	mine = vnn->computeOutVF(sm.s, {});

        if(target > mine) {
          for (uint i = 0; i < nb_sensors ; i++)
            data->input[n][i] = sm.s[i];
          if(update_pure_ac){
            for (uint i = 0; i < nb_motors; i++)
              data->output[n][i] = sm.pure_a[i];
          } else {
            for (uint i = 0; i < nb_motors; i++)
              data->output[n][i] = sm.a[i];
          }

          n++;
        } else if(update_delta_neg && !update_pure_ac){
            for (uint i = 0; i < nb_sensors ; i++)
              data->input[n][i] = sm.s[i];
            for (uint i = 0; i < nb_motors; i++)
              data->output[n][i] = sm.pure_a[i];
            n++;
        }
      }

      if(n > 0) {
        struct fann_train_data* subdata = fann_subset_train_data(data, 0, n);

        ann->learn_stoch(subdata, 5000, 0, 0.0001);

        fann_destroy_train(subdata);
      }
      fann_destroy_train(data);
    }
  }
  
  bool dominatedMx(const double* delta, const std::vector<double>& x, const std::vector<double>& sigma, uint n){
    double max_gauss = std::numeric_limits<double>::lowest();
    uint i=0;
    for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
      const std::vector<double>& s = it->s;
      double v = delta[i];
      double toexp = 0;
      for(uint j=0;j< x.size(); j++){
        ASSERT(sigma[j] > 0.000000000f, "wrong sigma " << sigma[j]);
        toexp += ((s[j] - x[j])*(s[j] - x[j]))/(sigma[j]*sigma[j]);
      }
      toexp *= -0.5f;
      v = v * exp(toexp);
      
      if(v>=max_gauss){
        max_gauss = v;
        if(max_gauss > delta[n])
          return true;
      }
      i++;
    }
    
    return false;
  }
  
  double Mx(const double* delta, const std::vector<double>& x, const std::vector<double>& sigma){
    double max_gauss = std::numeric_limits<double>::lowest();
    uint i=0;
    for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
      const std::vector<double>& s = it->s;
      double v = delta[i];
      double toexp = 0;
      for(uint j=0;j< x.size(); j++){
        ASSERT(sigma[j] > 0.000000000f, "wrong sigma " << sigma[j]);
        toexp += ((s[j] - x[j])*(s[j] - x[j]))/(sigma[j]*sigma[j]);
      }
      toexp *= -0.5f;
      v = v * exp(toexp);
      
      if(v>=max_gauss)
        max_gauss = v;
      i++;
    }
    
    return max_gauss;
  }
  
  void update_actor_new(){
    if (trajectory.size() > 4) {
    bool update_pure_ac = strategy_u == 4;
    
    struct fann_train_data* data = fann_create_train(trajectory.size(), nb_sensors, nb_motors);
    double *importance_sample = new double [trajectory.size()];
    double *delta = new double [trajectory.size()];
    double min_delta = std::numeric_limits<double>::max();
    double max_delta = std::numeric_limits<double>::lowest();
    
    std::vector<double> sum(nb_sensors);
    std::vector<double> square_sum(nb_sensors);
    
    uint n=0;
    for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
      sample sm = *it;

      for (uint i = 0; i < nb_sensors ; i++){
        data->input[n][i] = sm.s[i];
        sum[i] += sm.s[i];
        square_sum[i] += sm.s[i]*sm.s[i];
      }
      
      if(update_pure_ac){
        for (uint i = 0; i < nb_motors; i++)
          data->output[n][i] = sm.pure_a[i];
      } else {
        for (uint i = 0; i < nb_motors; i++)
          data->output[n][i] = sm.a[i];
      }
      
      delta[n] = sm.r;
      if (!sm.goal_reached) {
        double nextV = vnn->computeOutVF(sm.next_s, {});
        delta[n] += gamma * nextV;
      }
      
      if(delta[n] <= min_delta)
        min_delta = delta[n];
      if(delta[n] >= max_delta)
        max_delta = delta[n];

      n++;
    }
       
    n=0;
    for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
      delta[n] = bib::Utils::transform(delta[n], min_delta, max_delta, 0, 1);
      n++;
    }
    
    std::vector<double> sigma(nb_sensors);
    if(strategy_u <=5){
      for (uint i = 0; i < nb_sensors ; i++){
        sigma[i] = sqrt(square_sum[i]/((double)trajectory.size()) - (sum[i]/((double)trajectory.size()))*(sum[i]/((double)trajectory.size())))/((double)trajectory.size()) ;
      }
    } else if(strategy_u <=9) {
      n=0;
      double sum_w = 0;
      std::vector<double> all_w(trajectory.size());
      
      std::vector<double>* all_sigma = new std::vector<double>[nb_sensors];
      
      for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
        if(delta[n] <= 0.000000000000000000001f ){
          for(uint dim = 0;dim < nb_sensors ; dim++)
            all_sigma[dim].push_back(0.000000000000000f);
          all_w[n] = 0.000000000000000f; 
	  n++;
          continue;
        }
        
        struct UniqTest
        {
          const sample& _sa;
          OffVSetACFitted* ptr;
          double _min_delta;

          bool operator()(const sample& t ) const
          {
              if( !( t != _sa))
                return false;
              
              double delta_ns= t.r;
              if(!t.goal_reached){
                double nextV = ptr->vnn->computeOutVF(t.next_s, {});
                delta_ns += ptr->gamma * nextV;
              }
              
              if(delta_ns <= _min_delta)
                return false;
              
              return true;
          }
        };
        UniqTest t={*it, this, min_delta};
        //auto closest = kdtree_s->find_nearest(*it, std::numeric_limits<double>::max());
        auto closest = kdtree_s->find_nearest_if(*it, std::numeric_limits<double>::max(), t);
 
        ASSERT(closest.second < std::numeric_limits<double>::max(), "cannot find elem " << closest.second << " "<<kdtree_s->size());
        const sample& ns = *closest.first;
        
        double dpis = bib::Utils::euclidien_dist(it->a, ns.a, 1.f);
        double delta_ns= ns.r;
        if(!ns.goal_reached){
          double nextV = vnn->computeOutVF(ns.next_s, {});
          delta_ns += gamma * nextV;
        }
        delta_ns = bib::Utils::transform(delta_ns, min_delta, max_delta, 0.f, 1.f);
        
        double dvis = sqrt((delta[n] - delta_ns)*(delta[n] - delta_ns));
        if(strategy_u <=6){
          dvis = 1.f; 
          dpis = 1.f;
        } else if(strategy_u <=7){
          dvis = 1.f; 
        } else if(strategy_u <=8){
          dpis = 1.f;
        }
        double w = dvis*dpis;
        sum_w += w;
        all_w[n] = w;
        
        double ln = delta_ns > delta[n] ? log(delta_ns / delta[n]) : log(delta[n] / delta_ns);
        ln = 2.f * ln;
          
        for(uint dim = 0;dim < nb_sensors ; dim++){
          double sq = (it->s[dim] - ns.s[dim])*(it->s[dim] - ns.s[dim]);
          
          double wanted_sigma = sqrt(sq/ln);
          if(wanted_sigma >= sqrt(square_sum[dim]/((double)trajectory.size()) - (sum[dim]/((double)trajectory.size()))*(sum[dim]/((double)trajectory.size())))){
            all_sigma[dim].push_back( sqrt(square_sum[dim]/((double)trajectory.size()) - (sum[dim]/((double)trajectory.size()))*(sum[dim]/((double)trajectory.size()))) );
          } else  {
            all_sigma[dim].push_back(wanted_sigma);
          }
        }
      
        n++;
      }
      
      
      for(uint dim = 0;dim < nb_sensors ; dim++){
          sigma[dim] = 0;

          n=0;
          for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
            sigma[dim] = sigma[dim] + (all_w[n] * all_sigma[dim][n]);
            n++;
          }
        
          sigma[dim] /= sum_w;
	  if(sigma[dim] <= 0.00000000000001f || std::isnan(sigma[dim]))
	  	sigma[dim] = sqrt(square_sum[dim]/((double)trajectory.size()) - (sum[dim]/((double)trajectory.size()))*(sum[dim]/((double)trajectory.size())));
      }
      
      delete[] all_sigma;
    } else if (strategy_u == 10){
        for (uint i = 0; i < nb_sensors ; i++){
          sigma[i] = 10*sqrt(square_sum[i]/((double)trajectory.size()) - (sum[i]/((double)trajectory.size()))*(sum[i]/((double)trajectory.size())))/((double)trajectory.size()) ;
        }
      
        uint dominated = trajectory.size();
        while(((float)dominated)/((float)trajectory.size()) >= 0.7){
          for(uint dim = 0;dim < nb_sensors ; dim++)
            sigma[dim] *= 0.999;
          dominated = 0;
          n=0;
          for(auto it = trajectory.begin(); it != trajectory.end() ; ++it){
            if(dominatedMx(delta, it->s, sigma, n))
              dominated++;
            n++;
          }
          //LOG_DEBUG(((float)dominated)/((float)trajectory.size()) << " " << dominated << " dominated over " << trajectory.size());
        }
        n=0;
        dominated=0;
        for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
          if(Mx(delta, it->s, sigma) > delta[n])
            dominated++;
          n++;
        }
        LOG_DEBUG(((float)dominated)/((float)trajectory.size()) << " " << dominated << " dominated over " << trajectory.size());
      }
    
    
    
    if(strategy_u != 10){
      n=0;
      std::vector<double> Mxs(trajectory.size());
      std::vector<double> Mxs_fornorm(trajectory.size());
      for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
        Mxs[n] = Mx(delta, it->s, sigma);
        Mxs_fornorm[n] = Mxs[n] - (delta[n]);
        n++;
      }
      
      double norm_term = *std::max_element(Mxs_fornorm.begin(), Mxs_fornorm.end());
      n=0;
      for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
        importance_sample[n] = 1.f - Mxs_fornorm[n]/norm_term;
        //LOG_DEBUG(importance_sample[n]<< " " << Mxs[n]<<" "<< Mxs_fornorm[n] << " " << delta[n] <<" " <<norm_term );
        n++;
      }
    } else {
      n=0;
      for(auto it = trajectory.begin(); it != trajectory.end() ; ++it) {
        
        importance_sample[n] = Mx(delta, it->s, sigma) > delta[n] ? 0.f :  1.f;
        //LOG_DEBUG(importance_sample[n]<< " " << Mxs[n]<<" "<< Mxs_fornorm[n] << " " << delta[n] <<" " <<norm_term <<" "<< sigma[0] << " " << min_delta<< " " << sigma[2]);
        n++;
      }
    }

    ann->learn_stoch_lw(data, importance_sample, 5000, 0, 0.0001);
    fann_destroy_train(data);
    
    delete[] delta;
    delete[] importance_sample;
  }
}
  
  void end_episode() override {
    if(strategy_u <= 2)
      update_actor_old();
    else if(strategy_u <= 3)
      update_actor_old_lw();
    else if(strategy_u <= 10)
      update_actor_new();
    else if(strategy_u == 15)
      update_actor_old_lw_all();
    else {
      LOG_ERROR("not implemented");
      exit(1); 
    }
    
  }
  
  void end_instance(bool) override {
    episode++;
  }
  
  double criticEval(const std::vector<double>& perceptions) override {
      return vnn->computeOutVF(perceptions, {});
  }
  
  arch::Policy<MLP>* getCopyCurrentPolicy() override {
        return new arch::Policy<MLP>(new MLP(*ann) , gaussian_policy ? arch::policy_type::GAUSSIAN : arch::policy_type::GREEDY, noise, decision_each);
  }

  void save(const std::string& path) override {
    ann->save(path+".actor");
    vnn->save(path+".critic");
    bib::XMLEngine::save<>(trajectory, "trajectory", "trajectory.data");
  }

  void load(const std::string& path) override {
    ann->load(path+".actor");
    vnn->load(path+".critic");
  }

 protected:
  void _display(std::ostream& out) const override {
    out << std::setw(12) << std::fixed << std::setprecision(10) << sum_weighted_reward << " " << std::setw(
          8) << std::fixed << std::setprecision(5) << vnn->error() << " " << noise << " " << trajectory.size() ;
  }

  void _dump(std::ostream& out) const override {
    out <<" " << std::setw(25) << std::fixed << std::setprecision(22) <<
        sum_weighted_reward << " " << std::setw(8) << std::fixed <<
        std::setprecision(5) << vnn->error() << " " << trajectory.size() ;
  }
  

 private:
  uint nb_sensors;
  
  uint episode = 0;
  uint current_loaded_v = 0;
  uint change_v_each;
  uint strategy_u;

  double noise;
  bool gaussian_policy, lecun_activation;
  uint hidden_unit_v;
  uint hidden_unit_a;
  
  std::shared_ptr<std::vector<double>> last_action;
  std::shared_ptr<std::vector<double>> last_pure_action;
  std::vector<double> last_state;

  std::set<sample> trajectory;
  std::set<sample> last_trajectory;
//     std::list<sample> trajectory;
  KDE proba_s;
  
  struct L1_distance
  {
    typedef double distance_type;
    
    double operator() (const double& __a, const double& __b, const size_t) const
    {
      double d = fabs(__a - __b);
      return d;
    }
  };
  typedef KDTree::KDTree<sample, KDTree::_Bracket_accessor<sample>, L1_distance> kdtree_sample;
  kdtree_sample* kdtree_s;

  MLP* ann;
  MLP* vnn;
};

#endif

