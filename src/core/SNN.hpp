#ifndef SNN_CORE_HPP
#define SNN_CORE_HPP

#include <vector>
#include <string>
#include <unordered_map>

struct IzhikevichParams {
    double a, b, c, d;
    double v0;
    double u0;
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
    GroupInfo rootGroup; // the top-level group of neurons

    // used during simulation
    std::vector<IzhikevichParams> neuronParamTypes; // Indexed by typeId from neuronToTypeId
    int totalNeuronCount = 0;
    std::vector<double> v; // Membrane potentials
    std::vector<double> u; // Recovery variables
    std::vector<double> I; // Input currents
    std::vector<int> neuronToTypeId; // mapping neuron index -> neuron type id

    std::vector<std::vector<int>> synapticTargets;
    std::vector<std::vector<double>> synapticWeights;

public:
    void step(double dt); // Advance the simulation by dt milliseconds
    explicit SNN(const std::string& filename);
    SNN(const SNN&) = delete;               // Disable copy constructor
    SNN& operator=(const SNN&) = delete;    // Disable copy assignment
    ~SNN() = default;
};

#endif // SNN_CORE_HPP