#ifndef NETWORK_TOPOLOGY_LOADER_HPP
#define NETWORK_TOPOLOGY_LOADER_HPP

#include "SNN.hpp"
#include <yaml-cpp/yaml.h>
#include "SNNParseException.hpp"
#include "WeightGenerator.hpp"
#include <unordered_set>

class NetworkTopologyLoader {
public:
    struct ConfigData {
        std::vector<IzhikevichParams> neuronParamTypes;
        int totalNeuronCount = 0;
        std::vector<int> globalNeuronTypeIds;
        std::vector<double> initialV;
        std::vector<double> initialU;
        
        std::vector<std::vector<int>> synapticTargets;
        std::vector<std::vector<double>> synapticWeights;
        
        GroupInfo rootGroup;
        std::unordered_map<std::string, int> neuronTypeToIdMap;
    };
    
    ConfigData loadFromYaml(const std::string& filename);

private:
    ConfigData data;
    std::vector<std::unordered_set<int>> existingConnections;

    template<typename T>
    T getNodeAs(const YAML::Node& parent, const std::string& key, const std::string& context_path) const;

    WeightGenerator createWeightGenerator(const YAML::Node& weightNode, const std::string& context_path) const;

    int getNeuronTypeId(const std::string& type_name) const;
    void loadGroupData(const YAML::Node& groupNode, GroupInfo& groupInfo, int& current_start_index);
    void loadNeuronData(const YAML::Node& neuronsNode, GroupInfo& groupInfo, int& current_start_index);
    void loadConnectionsData(const YAML::Node& connectionsNode);

    void getMatchingNeuronCount(const GroupInfo& group, const int type_id, std::vector<NeuronInfo>& out_neurons) const;    // type_id == -1 means all types
    void createConnectionsBetweenGroups(
        const GroupInfo& fromGroup, const GroupInfo& toGroup,
        const std::string& from_type, const std::string& to_type,
        const YAML::Node& ruleNode, WeightGenerator& weightGen, bool exclude_self);

    // find all pairs of (Group Nodes) matching the patterns
    std::vector<std::string> splitPath(const std::string& path) const;
    void findMatchingGroups(const std::string& from_pattern, const std::string& to_pattern,
                            const GroupInfo& rootGroup, bool exclude_self,
                            std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& out_matchedPairs);
    
    void findMatchingGroupsRecursive(
        const GroupInfo& currentFromGroup, const GroupInfo& rootForToSearch,
        const std::vector<std::string>& from_segments, const std::vector<std::string>& to_segments,
        size_t from_index, std::map<int, std::string>& wildcardValues,
        bool exclude_self, std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& out_matchedPairs);

    void findMatchingToGroups(
        const GroupInfo& fromGroup, const GroupInfo& currentToGroup,
        const std::vector<std::string>& to_segments, size_t to_index,
        const std::map<int, std::string>& wildcardValues, bool exclude_self,
        std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& out_matchedPairs);

    bool isWildcard(const std::string& segment) const;
    int getWildcardNumber(const std::string& segment) const;
};

#endif // NETWORK_TOPOLOGY_LOADER_HPP