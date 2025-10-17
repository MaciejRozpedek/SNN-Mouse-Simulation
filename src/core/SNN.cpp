#include "SNN.hpp"
#include "NetworkTopologyLoader.hpp"
#include <iostream>

SNN::SNN(const std::string &filename) {
    // Use NetworkTopologyLoader to load configuration from YAML
    NetworkTopologyLoader loader;
    NetworkTopologyLoader::ConfigData config = loader.loadFromYaml(filename);
    
    // Transfer loaded data to SNN member variables
    neuronParamTypes = std::move(config.neuronParamTypes);
    rootGroup = std::move(config.rootGroup);
    total_neuron_count = config.totalNeuronCount;
    neuronToTypeId = std::move(config.globalNeuronTypeIds);
    v = std::move(config.initialV);
    u = std::move(config.initialU);
    synapticTargets = std::move(config.synapticTargets);
    synapticWeights = std::move(config.synapticWeights);
    
    // Initialize input current vector
    I.resize(total_neuron_count, 0.0);
}