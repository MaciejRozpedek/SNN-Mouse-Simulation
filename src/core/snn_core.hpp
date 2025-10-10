#ifndef SNN_CORE_HPP
#define SNN_CORE_HPP

#include <vector>
#include <string>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

struct IzhikevichParams {
    double a, b, c, d;
    double starting_v;
    double starting_u;
};

struct Synapse {
    int target_neuron;
    double weight;
};

struct NeuronInfo
{
    int typeId; // index into neuronParamTypes
    int count;  // number of neurons of this type
    int startIndex; // starting index in the global neuron arrays
};


struct GroupInfo {
    std::string name;       // short name of the group
    std::string fullName;   // full hierarchical name, e.g., "root.subgroup1.subgroup2"
    std::vector<GroupInfo> subgroups; // optional
    std::vector<NeuronInfo> neuronInfos; // types and counts of neurons in this group
    int startIndex;        // starting index in the global neuron arrays
    int totalCount;        // total number of neurons in this group (sum of counts in neuronInfos)
};

class SNN {
private:
    // not used after initialization
    std::unordered_map<std::string, int> neuron_type_to_id_map;
    GroupInfo rootGroup; // the top-level group of neurons
    // std::unordered_map<std::string, int> fullGroupNameToIndex; // map full group name -> groups index v[i], u[i], I[i], neuronToTypeId[i]

    // used during simulation
    std::vector<IzhikevichParams> neuronParamTypes; // Indexed by typeId from neuronToTypeId
    int total_neuron_count = 0;
    std::vector<double> v; // Membrane potentials
    std::vector<double> u; // Recovery variables
    std::vector<double> I; // Input currents
    std::vector<int> neuronToTypeId; // mapping neuron index -> neuron type id

    // // mapping external input name -> group index
    // std::unordered_map<std::string, int> externalInputs;
    // // mapping action output name -> group index
    // std::unordered_map<std::string, int> actionOutputs;

    int getNeuronTypeId(const std::string& type_name) const;
    void loadFromYaml(const std::string& filename);
    void loadGroupData(const YAML::Node& groupNode, GroupInfo& groupInfo, int& current_start_index);
    void loadNeuronData(const YAML::Node& neuronsNode, GroupInfo& groupInfo, int& current_start_index);
    void loadConnectionsData(const YAML::Node& connectionsNode);

public:
    explicit SNN(const std::string& filename);
    SNN(const SNN&) = delete;               // Disable copy constructor
    SNN& operator=(const SNN&) = delete;    // Disable copy assignment
    ~SNN() = default;
};

#endif // SNN_CORE_HPP