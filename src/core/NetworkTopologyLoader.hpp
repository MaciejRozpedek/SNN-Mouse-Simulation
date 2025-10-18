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
    T getNodeAs(const YAML::Node& parent, const std::string& key, const std::string& contextPath) const;

    WeightGenerator createWeightGenerator(const YAML::Node& weightNode, const std::string& contextPath) const;

    int getNeuronTypeId(const std::string& typeName) const;
    void loadGroupData(const YAML::Node& groupNode, GroupInfo& groupInfo, int& currentStartIndex);
    void loadNeuronData(const YAML::Node& neuronsNode, GroupInfo& groupInfo, int& currentStartIndex);
    void loadConnectionsData(const YAML::Node& connectionsNode);

    void getMatchingNeuronCount(const GroupInfo& group, const int typeId, std::vector<NeuronInfo>& outNeurons) const;    // typeId == -1 means all types
    void createConnectionsBetweenGroups(
        const GroupInfo& fromGroup, const GroupInfo& toGroup,
        const std::string& fromType, const std::string& toType,
        const YAML::Node& ruleNode, WeightGenerator& weightGen, bool excludeSelf);

    // find all pairs of (Group Nodes) matching the patterns
    std::vector<std::string> splitPath(const std::string& path) const;
    void findMatchingGroups(const std::string& fromPattern, const std::string& toPattern,
                            const GroupInfo& rootGroup, bool excludeSelf,
                            std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& outMatchedPairs);
    
    void findMatchingGroupsRecursive(
        const GroupInfo& currentFromGroup, const GroupInfo& rootForToSearch,
        const std::vector<std::string>& fromSegments, const std::vector<std::string>& toSegments,
        int fromIndex, std::map<int, std::string>& wildcardValues,
        bool excludeSelf, std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& outMatchedPairs);

    void findMatchingToGroups(
        const GroupInfo& fromGroup, const GroupInfo& currentToGroup,
        const std::vector<std::string>& toSegments, int toIndex,
        const std::map<int, std::string>& wildcardValues, bool excludeSelf,
        std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& outMatchedPairs);

    bool isWildcard(const std::string& segment) const;
    int getWildcardNumber(const std::string& segment) const;
};

#endif // NETWORK_TOPOLOGY_LOADER_HPP