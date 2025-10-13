#ifndef NETWORK_TOPOLOGY_LOADER_HPP
#define NETWORK_TOPOLOGY_LOADER_HPP

#include "SNN.hpp"
#include <yaml-cpp/yaml.h>
#include "SNNParseException.hpp"
#include "WeightGenerator.hpp"

class NetworkTopologyLoader {
public:
    struct ConfigData {
        std::vector<IzhikevichParams> neuronParamTypes;
        int totalNeuronCount = 0;
        std::vector<int> globalNeuronTypeIds;
        std::vector<double> initialV;
        std::vector<double> initialU;
        
        GroupInfo rootGroup;
        std::unordered_map<std::string, int> neuronTypeToIdMap;
    };
    
    ConfigData loadFromYaml(const std::string& filename);

private:
    ConfigData data;

    template<typename T>
    T getNodeAs(const YAML::Node& parent, const std::string& key, const std::string& context_path) const {
        if (!parent[key]) {
            throw SNNParseException("Brak wymaganego klucza '" + key + "' w '" + context_path + "'", parent);
        }
        try {
            return parent[key].as<T>();
        } catch (const YAML::TypedBadConversion<T>& e) {
            throw SNNParseException("Nieprawidlowy typ danych dla klucza '" + key + "' w '" + context_path + "'. Oczekiwano typu, ktory mozna przekonwertowac na " + typeid(T).name() + ". Komunikat YAML: " + e.what(), parent[key]);
        }
    }

    WeightGenerator createWeightGenerator(const YAML::Node& weightNode, const std::string& context_path) const;

    int getNeuronTypeId(const std::string& type_name) const;
    void loadGroupData(const YAML::Node& groupNode, GroupInfo& groupInfo, int& current_start_index);
    void loadNeuronData(const YAML::Node& neuronsNode, GroupInfo& groupInfo, int& current_start_index);
    void loadConnectionsData(const YAML::Node& connectionsNode);

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