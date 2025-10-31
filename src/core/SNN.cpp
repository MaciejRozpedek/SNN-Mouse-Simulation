#include "SNN.hpp"
#include "NetworkTopologyLoader.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>
#define NOMINMAX
#include <windows.h> // For GetAsyncKeyState

SNN::SNN(const std::string &filename) {
    // Use NetworkTopologyLoader to load configuration from YAML
    NetworkTopologyLoader loader;
    NetworkTopologyLoader::ConfigData config = loader.loadFromYaml(filename);
    
    // Transfer loaded data to SNN member variables
    neuronParamTypes = std::move(config.neuronParamTypes);
    rootGroup = std::move(config.rootGroup);
    totalNeuronCount = config.totalNeuronCount;
    neuronToTypeId = std::move(config.globalNeuronTypeIds);
    v = std::move(config.initialV);
    u = std::move(config.initialU);
    synapticTargets = std::move(config.synapticTargets);
    synapticWeights = std::move(config.synapticWeights);
    
    // Initialize input current vector
    I.resize(totalNeuronCount, 0.0);
}

void SNN::step(double dt) {
    // update membrane potentials and recovery variables
    for (int i = 0; i < totalNeuronCount; i++) {
        // u' = a(bv - u)
        // v' = 0.04v^2 + 5v + 140 - u + I
        // it is crucial to update u before v to achieve numerical stability
        int typeId = neuronToTypeId[i];
        u[i] += dt * (neuronParamTypes[typeId].a * (neuronParamTypes[typeId].b * v[i] - u[i]));
        v[i] += dt * (0.04 * v[i] * v[i] + 5 * v[i] + 143 - u[i] + I[i]);
    }

    // reset input current
    std::fill(I.begin(), I.end(), 0.0);

    // handle spikes and propagate
    for (int i = 0; i < totalNeuronCount; i++) {
        int typeId = neuronToTypeId[i];
        // if v >= 30 mV
        //  v = c, u = u + d
        if (v[i] >= 30.0) {
            v[i] = neuronParamTypes[typeId].c;
            u[i] += neuronParamTypes[typeId].d;

            for (int j = 0; j < synapticTargets[i].size(); j++) {
                int targetIdx = synapticTargets[i][j];
                double weight = synapticWeights[i][j];
                #pragma omp atomic
                I[targetIdx] += weight;
            }
        }
    }
}