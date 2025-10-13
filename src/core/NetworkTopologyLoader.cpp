#include "NetworkTopologyLoader.hpp"
#include <iostream>
#include "Random.hpp"

WeightGenerator NetworkTopologyLoader::createWeightGenerator(const YAML::Node& weightNode, const std::string& context_path) const {
    if (!weightNode || !weightNode.IsMap()) {
        throw SNNParseException("'weight' nieokreslone w polaczeniu w '" + context_path + "'.", weightNode);
    }

    if (weightNode["fixed"]) {
        double fixed_value = getNodeAs<double>(weightNode, "fixed", context_path + ".weight");
        return WeightGenerator::createFixed(Random::getInstance(), fixed_value);
    }

    if (weightNode["uniform"]) {
        YAML::Node uniformNode = weightNode["uniform"];
        if (!uniformNode.IsMap() || !uniformNode["min"] || !uniformNode["max"]) {
            throw SNNParseException("Nieprawidlowy format dla 'uniform' w '" + context_path + "'. Oczekiwano mapy z kluczami 'min' i 'max'.", uniformNode);
        }
        double min = getNodeAs<double>(uniformNode, "min", context_path);
        double max = getNodeAs<double>(uniformNode, "max", context_path);
        if (min > max) {
            throw SNNParseException("'min' musi byc mniejsze od 'max' w '" + context_path, uniformNode);
        }
        return WeightGenerator::createUniform(Random::getInstance(), min, max);
    }

    if (weightNode["normal"]) {
        const YAML::Node& normalNode = weightNode["normal"];
        double mean_val = getNodeAs<double>(normalNode, "mean", "weight.normal");
        double std_val = getNodeAs<double>(normalNode, "std", "weight.normal");
        
        if (std_val < 0.0) {
             throw SNNParseException("Blad parsowania: 'std' musi byc nieujemne w normal.", normalNode);
        }
        return WeightGenerator::createNormal(Random::getInstance(), mean_val, std_val);
    }

    throw SNNParseException("Nieprawidlowy format dla 'weight' w '" + context_path + "'. Oczekiwano jednego z kluczy: 'fixed', 'uniform', 'normal'.", weightNode);
}

int NetworkTopologyLoader::getNeuronTypeId(const std::string& type_name) const {
    auto it = data.neuronTypeToIdMap.find(type_name);
    if (it != data.neuronTypeToIdMap.end()) {
        return it->second;
    } else {
        throw SNNParseException("Nieznany typ neuronu '" + type_name + "' w sekcji 'neuron_types'.");
    }
}

NetworkTopologyLoader::ConfigData NetworkTopologyLoader::loadFromYaml(const std::string &filename) {
    data = ConfigData(); // reset data
    try {
        YAML::Node config = YAML::LoadFile(filename);
        if (!config["neuron_types"]) {
            throw SNNParseException("Brak sekcji 'neuron_types' w pliku YAML.", config);
        }
        // first load neuron types
        const YAML::Node& neuron_types = config["neuron_types"];
        for (const auto& type : neuron_types) {
            std::string type_name = type.first.as<std::string>();
            const YAML::Node& params_node = type.second;

            if (!params_node.IsMap()) {
                throw SNNParseException("Parametry dla typu '" + type_name + "' w sekcji 'neuron_types' nie sa poprawna mapa.", params_node);
            }

            IzhikevichParams params;
            std::string context = "neuron_types." + type_name;
            params.a = getNodeAs<double>(params_node, "a", context);
            params.b = getNodeAs<double>(params_node, "b", context);
            params.c = getNodeAs<double>(params_node, "c", context);
            params.d = getNodeAs<double>(params_node, "d", context);
            params.starting_v = getNodeAs<double>(params_node, "starting_v", context);
            params.starting_u = getNodeAs<double>(params_node, "starting_u", context);

            int type_id = static_cast<int>(data.neuronParamTypes.size());
            data.neuronParamTypes.push_back(params);
            data.neuronTypeToIdMap[type_name] = type_id;
        } // neuron types loaded

        // second load neuron groups
        if (!config["groups"]) {
            throw SNNParseException("Brak sekcji 'groups' w pliku YAML.", config);
        }
        const YAML::Node& groups = config["groups"];
        int current_start_index = 0;
        data.rootGroup = GroupInfo();
        data.rootGroup.fullName = "root";
        // read group information to rootGroup
        loadGroupData(groups, data.rootGroup, current_start_index);
        data.totalNeuronCount = current_start_index;
        data.rootGroup.startIndex = 0;
        data.rootGroup.totalCount = data.totalNeuronCount;

        // third load and create synapses
        if (!config["connections"]) {
            throw SNNParseException("Brak sekcji 'connections' w pliku YAML.", config);
        }
        const YAML::Node& connections = config["connections"];
        loadConnectionsData(connections);
    }
    catch(const YAML::BadFile&) {
        throw SNNParseException("Nie mozna znalezc lub otworzyc pliku " + filename);
    }
    catch(const YAML::ParserException& e) {
        throw SNNParseException(std::string(e.what()));
    }
    return data;
}

void NetworkTopologyLoader::loadGroupData(const YAML::Node& groupNode, GroupInfo& groupInfo, int& current_start_index) {
    if (!groupNode.IsSequence()) {
        throw SNNParseException("Oczekiwano sekwencji grup w grupie '" + groupInfo.fullName + "'.", groupNode);
    }
    groupInfo.startIndex = current_start_index;
    groupInfo.totalCount = 0;

    for (const auto& node : groupNode) {
        if (!node.IsMap()) {
            throw SNNParseException("Oczekiwano mapy dla grupy w grupie '" + groupInfo.fullName + "'.", node);
        }

        GroupInfo subgroup;
        subgroup.name = getNodeAs<std::string>(node, "name", groupInfo.fullName);
        subgroup.fullName = groupInfo.fullName + "." + subgroup.name;
        subgroup.startIndex = current_start_index;
        subgroup.totalCount = 0;

        bool has_neurons = node["neurons"].IsDefined();
        bool has_subgroups = node["subgroups"].IsDefined();

        if (has_neurons && has_subgroups) {
            throw SNNParseException("Grupa '" + subgroup.fullName + "' nie moze miec jednoczesnie 'neurons' i 'subgroups'.", node);
        }

        if (has_neurons) {
            if (!node["neurons"].IsSequence()) {
                throw SNNParseException("Oczekiwano sekwencji dla 'neurons' w grupie '" + subgroup.fullName + "'.", node["neurons"]);
            }
            loadNeuronData(node["neurons"], subgroup, current_start_index);
        }
        if (has_subgroups) {
            if (!node["subgroups"].IsSequence()) {
                throw SNNParseException("Oczekiwano sekwencji dla 'subgroups' w grupie '" + subgroup.fullName + "'.", node["subgroups"]);
            }
            loadGroupData(node["subgroups"], subgroup, current_start_index);
        }
        subgroup.totalCount = current_start_index - subgroup.startIndex;
        groupInfo.totalCount += subgroup.totalCount;
        groupInfo.subgroups.push_back(subgroup);
    }
}

void NetworkTopologyLoader::loadNeuronData(const YAML::Node& neuronsNode, GroupInfo& groupInfo, int& current_start_index) {
    if (!neuronsNode.IsSequence()) {
        throw SNNParseException("Oczekiwano sekwencji dla 'neurons' w grupie '" + groupInfo.fullName + "'.", neuronsNode);
    }

    for (const auto& neuronTypeNode : neuronsNode) {
        if (!neuronTypeNode.IsMap()) {
            throw SNNParseException("Oczekiwano mapy dla typu neuronu w grupie '" + groupInfo.fullName + "'.", neuronTypeNode);
        }
        std::string type_name = getNodeAs<std::string>(neuronTypeNode, "type", groupInfo.fullName);
        int count = getNodeAs<int>(neuronTypeNode, "count", groupInfo.fullName);

        if (count > 0) {
            NeuronInfo n_info;
            n_info.typeId = getNeuronTypeId(type_name);
            n_info.count = count;
            n_info.startIndex = current_start_index;
            data.globalNeuronTypeIds.insert(data.globalNeuronTypeIds.end(), count, n_info.typeId);
            IzhikevichParams params = data.neuronParamTypes[n_info.typeId];
            data.initialV.insert(data.initialV.end(), count, params.starting_v);
            data.initialU.insert(data.initialU.end(), count, params.starting_u);

            groupInfo.neuronInfos.push_back(n_info);

            current_start_index += count;
        }
    }
}

void NetworkTopologyLoader::loadConnectionsData(const YAML::Node& connectionsNode) {
    if (!connectionsNode.IsSequence()) {
        throw SNNParseException("Oczekiwano sekwencji dla 'connections'.", connectionsNode);
    }

    for (const auto& connectionNode : connectionsNode) {
        if (!connectionNode.IsMap()) {
            throw SNNParseException("Oczekiwano mapy dla polaczenia w 'connections'.", connectionNode);
        }
        std::string context = "connections";
        std::string from_group = getNodeAs<std::string>(connectionNode, "from", context);
        std::string to_group = getNodeAs<std::string>(connectionNode, "to", context);
        std::string from_type = getNodeAs<std::string>(connectionNode, "from_type", context);
        std::string to_type = getNodeAs<std::string>(connectionNode, "to_type", context);
        // exclude_self is optional, default to false
        bool exclude_self = connectionNode["exclude_self"] ? getNodeAs<bool>(connectionNode, "exclude_self", context + " (default false)") : false;
        YAML::Node ruleNode = getNodeAs<YAML::Node>(connectionNode, "rule", context);
        YAML::Node weightNode = getNodeAs<YAML::Node>(connectionNode, "weight", context);

        WeightGenerator weightGen = createWeightGenerator(weightNode, context + " (from '" + from_group + "' to '" + to_group + "')");
        
        // Make actual connection here (not implemented in this commit). For now, just print the connection details.
        std::vector<std::pair<const GroupInfo*, const GroupInfo*>> matchedPairs;
        printf("From '%s', To '%s'\n", from_group.c_str(), to_group.c_str());
        findMatchingGroups(from_group, to_group, data.rootGroup, exclude_self, matchedPairs);
        for (const auto& pair : matchedPairs) {
            printf("  Matched Pair: %s -> %s\n", pair.first->fullName.c_str(), pair.second->fullName.c_str());
        }
        printf("\n");
    }
}

std::vector<std::string> NetworkTopologyLoader::splitPath(const std::string& path) const {
    std::vector<std::string> parts;
    std::string token;
    std::stringstream ss(path);

    while (std::getline(ss, token, '.')) {
        parts.push_back(token);
    }
    return parts;
}

void NetworkTopologyLoader::findMatchingGroups(const std::string& from_pattern, const std::string& to_pattern,
    const GroupInfo& rootGroup, bool exclude_self, std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& out_matchedPairs) {
    
    // Map to store wildcard substitutions
    std::map<int, std::string> wildcardValues;
    
    // Split patterns into segments
    std::vector<std::string> from_segments = splitPath(from_pattern);
    std::vector<std::string> to_segments = splitPath(to_pattern);
    
    // Start the recursive search
    findMatchingGroupsRecursive(rootGroup, rootGroup, from_segments, to_segments,
                                0, wildcardValues, exclude_self, out_matchedPairs);
}

void NetworkTopologyLoader::findMatchingGroupsRecursive(
    const GroupInfo& currentFromGroup, 
    const GroupInfo& rootForToSearch,
    const std::vector<std::string>& from_segments, 
    const std::vector<std::string>& to_segments,
    size_t from_index,
    std::map<int, std::string> &wildcardValues,
    bool exclude_self,
    std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& out_matchedPairs) {

    // If we've matched the complete 'from' pattern
    if (from_index == from_segments.size()) {
        // Find matching 'to' groups with current wildcard values
        findMatchingToGroups(currentFromGroup, rootForToSearch, to_segments, 
                            0, wildcardValues, exclude_self, out_matchedPairs);
        return;
    }
    
    // Get current segment to match
    const std::string& segment = from_segments[from_index];
    
    // Check if segment is a wildcard
    if (isWildcard(segment)) {
        int wildcardNum = getWildcardNumber(segment);
        
        // If this wildcard has been seen before
        if (wildcardValues.count(wildcardNum)) {
            // Check if any subgroup matches the stored wildcard value
            for (const auto& subgroup : currentFromGroup.subgroups) {
                if (subgroup.name == wildcardValues[wildcardNum]) {
                    // Continue matching with this subgroup
                    findMatchingGroupsRecursive(subgroup, rootForToSearch, 
                                                from_segments, to_segments,
                                                from_index + 1,
                                                wildcardValues, exclude_self, out_matchedPairs);
                }
            }
        } else {
            // This wildcard hasn't been seen yet, try all possible subgroups
            for (const auto& subgroup : currentFromGroup.subgroups) {
                // Assign this subgroup name to the wildcard
                wildcardValues[wildcardNum] = subgroup.name;
                
                // Continue matching with this subgroup
                findMatchingGroupsRecursive(subgroup, rootForToSearch, 
                                            from_segments, to_segments,
                                            from_index + 1,
                                            wildcardValues, exclude_self, out_matchedPairs);
                
                // Remove this assignment to try other possibilities
                wildcardValues.erase(wildcardNum);
            }
        }
    } else {
        // Literal segment, check if any subgroup matches exactly
        for (const auto& subgroup : currentFromGroup.subgroups) {
            if (subgroup.name == segment) {
                // Continue matching with this subgroup
                findMatchingGroupsRecursive(subgroup, rootForToSearch, 
                                            from_segments, to_segments,
                                            from_index + 1,
                                            wildcardValues, exclude_self, out_matchedPairs);
            }
        }
    }
}

void NetworkTopologyLoader::findMatchingToGroups(
    const GroupInfo& fromGroup, 
    const GroupInfo& currentToGroup,
    const std::vector<std::string>& to_segments, 
    size_t to_index,
    const std::map<int, std::string>& wildcardValues,
    bool exclude_self,
    std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& out_matchedPairs) {
    
    // If we've matched the complete 'to' pattern
    if (to_index == to_segments.size()) {
        if (exclude_self && fromGroup.fullName == currentToGroup.fullName) {
            return; // Skip self-connection if exclude_self is true
        }
        
        // Add to processed pairs set
        out_matchedPairs.push_back({&fromGroup, &currentToGroup});
        return;
    }
    
    // Get current segment to match
    const std::string& segment = to_segments[to_index];
    
    // Check if segment is a wildcard
    if (isWildcard(segment)) {
        int wildcardNum = getWildcardNumber(segment);
        
        // If this wildcard has a defined value, match it
        if (wildcardValues.count(wildcardNum)) {
            for (const auto& subgroup : currentToGroup.subgroups) {
                if (subgroup.name == wildcardValues.at(wildcardNum)) {
                    findMatchingToGroups(fromGroup, subgroup, to_segments,
                                        to_index + 1, wildcardValues, exclude_self, out_matchedPairs);
                }
            }
        } else {
            for (const auto& subgroup : currentToGroup.subgroups) {
                // Try assigning this subgroup name to the wildcard
                std::map<int, std::string> newWildcardValues = wildcardValues;
                newWildcardValues[wildcardNum] = subgroup.name;
                
                findMatchingToGroups(fromGroup, subgroup, to_segments, 
                                    to_index + 1, newWildcardValues, exclude_self, out_matchedPairs);
            }
        }
    } else {
        // Literal segment, check if any subgroup matches exactly
        for (const auto& subgroup : currentToGroup.subgroups) {
            if (subgroup.name == segment) {
                findMatchingToGroups(fromGroup, subgroup, to_segments,
                                    to_index + 1, wildcardValues, exclude_self, out_matchedPairs);
            }
        }
    }
}

// Helper function to check if a segment is a wildcard
bool NetworkTopologyLoader::isWildcard(const std::string& segment) const {
    return segment.size() >= 3 && segment.front() == '[' && segment.back() == ']';
}

// Helper function to extract wildcard number
int NetworkTopologyLoader::getWildcardNumber(const std::string& segment) const {
    return std::stoi(segment.substr(1, segment.size() - 2));
}