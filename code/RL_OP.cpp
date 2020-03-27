//
// Created by LDNN97 on 2020/3/5.
//

#include "RL_OP.h"

namespace py = pybind11;
using namespace rl;
using std::cin;
using std::cout;
using std::endl;
using std::vector;
using std::swap;
using std::ofstream;
using std::ifstream;
using std::string;
using std::priority_queue;

void rl::env_reset(py::object &env, state_arr &st){
    py::object st_or = env.attr("reset")();
    st = st_or.cast<state_arr>();
}

void rl::env_step(pybind11::object &env, const int &act, state_arr &nst, double &reward, bool &end) {
    py::list nst_or = env.attr("step")(action_set[act]);
    nst = py::cast<state_arr>(nst_or[0]);
    reward = py::cast<double>(nst_or[1]);
    end = py::cast<bool>(nst_or[2]);
}

int rl::get_max_action(py::object &env, individual* indi){
    state_arr nst{}; double reward = 0; bool end = false;
    double max_v = -1e6; int max_act = 0; double v;

    for (int i = 0; i < n_action; i++){
        rl::env_step(env, i, nst, reward, end);
        v = reward + 1 * individual::calculate(indi->root, nst);
        if (v > max_v) {
            max_v = v;
            max_act = i;
        }
        env.attr("back_step")();
    }

    return max_act;
}

template <typename T>

vector<size_t> sort_indexes(const vector<T> &v) {
    vector<size_t> idx(v.size());
    iota(idx.begin(), idx.end(), 0);
    stable_sort(idx.begin(), idx.end(), [&v](size_t i1, size_t i2) {return v[i1] < v[i2];});
    return idx;
}

void rl::get_rank(std::vector<double> &rank, std::vector<double> &fitness, std::vector<double> &dist, double fit_rate, double dis_rate ) {
    auto fit_index = sort_indexes(fitness);
    auto dis_index = sort_indexes(dist);
    std::array<double, POP_SIZE> fit_rk {};
    std::array<double, POP_SIZE> dis_rk {};
    for (int i = 0; i < POP_SIZE; i++) {
        fit_rk[fit_index[i]] = i;
        dis_rk[dis_index[i]] = i;
    }
    double _rank;
    for (int i = 0; i < POP_SIZE; i++) {
        _rank = fit_rate * fit_rk[i] + dis_rate * dis_rk[i];
        rank.push_back(_rank);
    }
}

int rl::sample(vector<double> &rank){
    int ans = rand_int(0, POP_SIZE);
    for (int i = 0; i < T_S - 1; i++) {
        int tmp = rand_int(0, POP_SIZE);
        ans = (rank[ans] > rank[tmp]) ? ans : tmp;
    }
    return ans;
}

void rl::best_agent() {
    cout << "Display the Best Agent" << endl;

    py::object env_list = py::module::import("env");
    py::object env = env_list.attr(env_name.c_str())();

    individual* indi = individual::load_indi("Agent/best_agent.txt");

    std::array<double, n_observation> st{}, nst{};
    int action; double reward; bool end;

    for (int i = 0; i < 10; i++) {
        env.attr("reset_ini")();
        rl::env_reset(env, st);
        double reward_indi = 0;
        for (int step = 0; step < 1000; step++){
            action = rl::get_max_action(env, indi);
            rl::env_step(env, action, nst, reward, end);
            std::swap(st, nst);
            if (end) break;
            reward_indi += reward; //env CartPole +1 MountainCar -1
            env.attr("render")();
        }
        cout << reward_indi << endl;
    }
    env.attr("close")();

    individual::indi_clean(indi);
}

int ensemble_selection(py::object &env, vector<individual*> &agent) {
    std::array<int, n_action> box{};

    for (auto indi:agent) {
        state_arr nst{}; double reward = 0; bool end = false;
        double max_v = -1e6; int max_act = 0; double v;
        for (int i = 0; i < n_action; i++){
            rl::env_step(env, i, nst, reward, end);
            v = reward + 1 * individual::calculate(indi->root, nst);
            if (v > max_v) {
                max_v = v;
                max_act = i;
            }
            env.attr("back_step")();
        }
        box[max_act]++;
    }

    int ans = std::distance(box.begin(), std::max_element(box.begin(), box.end()));

    return ans;
}

void rl::ensemble_agent() {
    cout << "Display the Ensemble Agent" << endl;

    py::object env_list = py::module::import("env");
    py::object env = env_list.attr(env_name.c_str())();

    int ensemble_size = 0;
    ifstream _file("Agent/ensemble_size.txt");
    _file >> ensemble_size;
    _file.close();

    vector<individual*> agent;
    for (int i = 0; i < ensemble_size; i++) {
        string _f_name = "Agent/agent.txt";
        string num = std::to_string(i);
        _f_name.insert(11, num);
        individual* indi = individual::load_indi(_f_name);
        agent.emplace_back(indi);
    }

    std::array<double, n_observation> st{}, nst{};
    int action; double reward; bool end;

    for (int i = 0; i < 10; i++) {
        env.attr("reset_ini")();
        rl::env_reset(env, st);
        double reward_indi = 0;
        for (int step = 0; step < 1000; step++){
            action = ensemble_selection(env, agent);
            rl::env_step(env, action, nst, reward, end);
            std::swap(st, nst);
            if (end) break;
            reward_indi += reward; //env CartPole +1 MountainCar -1
            env.attr("render")();
        }
        cout << reward_indi << endl;
    }
    env.attr("close")();

    for (auto indi:agent)
        individual::indi_clean(indi);
    agent.clear();
}

void rl::agent_push(pri_que &agent, individual* indi, double fit){
    if (agent.empty()) {
        auto tmp = new individual(*indi);
        agent.push(agent_pair(tmp, fit));
    }
    else {
        agent_pair top = agent.top();
        if (top.second <= fit) {
            auto tmp = new individual(*indi);
            agent.push(agent_pair(tmp, fit));
        }
        if (agent.size() > ENSEMBLE_SIZE) {
            auto tmp = agent.top();
            individual::indi_clean(tmp.first);
            agent.pop();
        }
    }
}

void rl::agent_flat(pri_que &agent, vector<agent_pair> &agent_array){
    agent_array.clear();
    while (!agent.empty()){
        agent_array.emplace_back(agent.top());
        agent.pop();
    }
    for (auto indi : agent_array)
        agent.push(indi);
}

//Todo: 1. restart 2. best individual with low robustness
void rl::rl_op() {
    // GYM
    py::object env_list = py::module::import("env");
    py::object env = env_list.attr(env_name.c_str())();

    // build a model
    auto pop = new individual* [POP_SIZE];
    for (int md = MIN_DEPTH; md <= INI_DEPTH; md++) {
        for (int i = 0; i < TYPE_NUM; i++) {
            pop[TYPE_NUM * 2 * (md - MIN_DEPTH) + i] = new individual();
            pop[TYPE_NUM * 2 * (md - MIN_DEPTH) + i]->build("grow", md);
        }
        for (int i = 0; i < TYPE_NUM; i++) {
            pop[TYPE_NUM * 2 * (md - MIN_DEPTH) + TYPE_NUM + i] = new individual();
            pop[TYPE_NUM * 2 * (md - MIN_DEPTH) + TYPE_NUM + i]->build("full", md);
        }
    }

    // fitness
    vector<double> fitness;
    vector<double> rank;
    double fitness_total, size_total;
    int best_indi;
    individual* utnbi; double fitness_utnbi;
    double lgar; //last generation average reward

    // similarity between individauls
    vector<double> sim;
    double sim_total;

    // RL: state, next state, action, reward, end
    std::array<double, n_observation> st{}, nst{};
    int action; double reward; bool end;

    // record the evolution process
    std::array<double, MAX_GENERATION> f_b {};
    std::array<double, MAX_GENERATION> f_a {};
    std::array<double, MAX_GENERATION> sim_a {};
    std::array<double, MAX_GENERATION> siz_a {};
    std::array<double, MAX_GENERATION/20> f_ens {};

    utnbi = nullptr;
    fitness_utnbi = 0;
    lgar = -1e6;

    // emsemble learning
    pri_que agent;
    vector<agent_pair> agent_array;

    //Evolution
    for (int gen = 0; gen < MAX_GENERATION; gen++) {
        env.attr("reset_ini")();
        auto new_pop = new individual* [POP_SIZE];

        best_indi = 0; fitness_total = 0; sim_total = 0; size_total = 0;
        fitness.clear(); sim.clear();

        for (int i = 0; i < POP_SIZE; i++) {
            env_reset(env, st);
            int cnt = 0;
            double _same_tot = 0;
            double _fit_tot = 0;
            for (int step = 0; step < 1000; step++){
                cnt++;
                action = get_max_action(env, pop[i]);

                if (utnbi != nullptr) {
                    int action_target = rl::get_max_action(env, utnbi);
                    if (action == action_target)
                        _same_tot += 1;
                }

                rl::env_step(env, action, nst, reward, end);
                st = nst;
                if (end) break;
                _fit_tot += reward;
            }
            sim.push_back(_same_tot / cnt);
            fitness.push_back(_fit_tot);

            sim_total += sim[i];
            fitness_total += fitness[i];
            size_total += pop[i]->root->size;

            best_indi = (fitness[best_indi] > fitness[i]) ? best_indi : i;
            rl::agent_push(agent, pop[i], fitness[i]);
//            cout << i << " " << fitness[i] << " " << dist[i] << endl;
        }
        f_a[gen] = fitness_total / double(POP_SIZE);
        sim_a[gen] = sim_total / double(POP_SIZE);
        siz_a[gen] = size_total / double(POP_SIZE);

        if (utnbi == nullptr) {
            utnbi = new individual(*pop[best_indi]);
            fitness_utnbi = fitness[best_indi];
        }

        cout << "gen, f_a, s_a, f_b, utnf_b: ";
        cout << gen << " " << f_a[gen] << " " << siz_a[gen] << " ";
        cout << fitness[best_indi] << " " << fitness_utnbi << endl;

        // select rank mode
        double fit_rate = 1, dis_rate = 0;

        // original
        if (fitness[best_indi] >= fitness_utnbi) {
            delete utnbi;
            utnbi = new individual(*pop[best_indi]);
            fitness_utnbi = fitness[best_indi];
        }

        // improved method
//        if (f_a[gen] >= lgar) {
//            if (fitness[best_indi] >= fitness_utnbi) {
//                delete utnbi;
//                utnbi = new individual(*pop[best_indi]);
//                fitness_utnbi = fitness[best_indi];
//                fit_rate = 1; dis_rate = 0;
//            } else {
//                fit_rate = 0.7; dis_rate = 0.3;
//            }
//            lgar = f_a[gen];
//        } else {
//            if (fitness[best_indi] >= fitness_utnbi) {
//                delete utnbi;
//                utnbi = new individual(*pop[best_indi]);
//                fitness_utnbi = fitness[best_indi];
//                fit_rate = 0.3; dis_rate = 0.7;
//            } else {
//                fit_rate = 0.5; dis_rate = 0.5;
//            }
//
//            lgar = f_a[gen];
//        }

        f_b[gen] = fitness_utnbi;

        // get rank
        rank.clear();
        get_rank(rank, fitness, sim, fit_rate, dis_rate);

//        if ((fitness_total / double(POP_SIZE) < 1e-3) || (reward_total / double(POP_SIZE) > 450)) {
//            cout << "=====successfully!======" << endl;
//            cout << endl;
//            break;
//        }

        for (int i = 0; i < POP_SIZE; i++) {
            int index_p1 = sample(rank);
            int index_p2 = sample(rank);
            auto parent1 = new individual(*pop[index_p1]);
            auto parent2 = new individual(*pop[index_p2]);

            parent1->crossover(parent2);
            individual::mutation(parent1);

            new_pop[i] = parent1;
            individual::indi_clean(parent2);
        }

        swap(pop, new_pop);

        // free pointer;
        for (int i = 0; i < POP_SIZE; i++)
            individual::indi_clean(new_pop[i]);
        delete [] new_pop;

        if (gen % 20 == 0) {
            // save best model
            individual::save_indi(utnbi->root, "Agent/best_agent.txt");
            // save ensemble agent
            agent_flat(agent, agent_array);

            ofstream _file("Agent/ensemble_size.txt");
            _file << agent_array.size() << endl;
            _file.close();

            double fit_ens = 0;
            for (int i = 0; i < agent_array.size(); i++) {
                string _f_name = "Agent/agent.txt";
                string num = std::to_string(i);
                _f_name.insert(11, num);
                individual::save_indi(agent_array[i].first->root, _f_name);
                fit_ens += agent_array[i].second;
            }
            fit_ens /= double(agent_array.size());
            f_ens[gen/20] = fit_ens;

            cout << "Best and Ensemble: " << fitness_utnbi << " " << fit_ens << endl;
            best_agent();
            ensemble_agent();
        }
    }

    // print the average fit and dist
    ofstream _file("Average.txt");
    for (int i = 0; i < MAX_GENERATION; i++)
        _file << i << " " << f_b[i] << " " << f_ens[i/20] << " " << f_a[i] << " " << siz_a[i] << endl;
    _file.close();

    // print
    py::object _b_i = py::cast(f_b);
    py::object _f_ens = py::cast(f_ens);
    py::object _f_a = py::cast(f_a);
    py::object _siz_a = py::cast(siz_a);

    py::object plt = py::module::import("matplotlib.pyplot");
    plt.attr("figure")();
    plt.attr("subplot")(411);
    plt.attr("plot")(_b_i);
    plt.attr("subplot")(412);
    plt.attr("plot")(_f_ens);
    plt.attr("subplot")(413);
    plt.attr("plot")(_f_a);
    plt.attr("subplot")(414);
    plt.attr("plot")(_siz_a);
    plt.attr("yscale")("log");
    plt.attr("show")();

    for (int i = 0; i < POP_SIZE; i++)
        individual::indi_clean(pop[i]);
    delete [] pop;
}
