#include "NetworkTopologyLoader.hpp"
#include <iostream>
#include "Random.hpp"
#include <algorithm>
#include <vector>
#include <utility>

template<typename T>
T NetworkTopologyLoader::getNodeAs(const YAML::Node& parent, const std::string& key, const std::string& contextPath) const {
        if (!parent[key]) {
            throw SNNParseException("Brak wymaganego klucza '" + key + "' w '" + contextPath + "'", parent);
        }
        try {
            return parent[key].as<T>();
        } catch (const YAML::TypedBadConversion<T>& e) {
            throw SNNParseException("Nieprawidlowy typ danych dla klucza '" + key + "' w '" + contextPath + "'. Oczekiwano typu, ktory mozna przekonwertowac na " + typeid(T).name() + ". Komunikat YAML: " + e.what(), parent[key]);
        }
    }

WeightGenerator NetworkTopologyLoader::createWeightGenerator(const YAML::Node& weightNode, const std::string& contextPath) const {
    if (!weightNode || !weightNode.IsMap()) {
        throw SNNParseException("'weight' nieokreslone w polaczeniu w '" + contextPath + "'.", weightNode);
    }

    if (weightNode["fixed"]) {
        double fixedValue = getNodeAs<double>(weightNode, "fixed", contextPath + ".weight");
        return WeightGenerator::createFixed(Random::getInstance(), fixedValue);
    }

    if (weightNode["uniform"]) {
        YAML::Node uniformNode = weightNode["uniform"];
        if (!uniformNode.IsMap() || !uniformNode["min"] || !uniformNode["max"]) {
            throw SNNParseException("Nieprawidlowy format dla 'uniform' w '" + contextPath + "'. Oczekiwano mapy z kluczami 'min' i 'max'.", uniformNode);
        }
        double min = getNodeAs<double>(uniformNode, "min", contextPath);
        double max = getNodeAs<double>(uniformNode, "max", contextPath);
        if (min > max) {
            throw SNNParseException("'min' musi byc mniejsze od 'max' w '" + contextPath, uniformNode);
        }
        return WeightGenerator::createUniform(Random::getInstance(), min, max);
    }

    if (weightNode["normal"]) {
        const YAML::Node& normalNode = weightNode["normal"];
        double meanVal = getNodeAs<double>(normalNode, "mean", "weight.normal");
        double stdVal = getNodeAs<double>(normalNode, "std", "weight.normal");
        
        if (stdVal < 0.0) {
             throw SNNParseException("Blad parsowania: 'std' musi byc nieujemne w normal.", normalNode);
        }
        return WeightGenerator::createNormal(Random::getInstance(), meanVal, stdVal);
    }

    throw SNNParseException("Nieprawidlowy format dla 'weight' w '" + contextPath + "'. Oczekiwano jednego z kluczy: 'fixed', 'uniform', 'normal'.", weightNode);
}

int NetworkTopologyLoader::getNeuronTypeId(const std::string& typeName) const {
    auto it = data.neuronTypeToIdMap.find(typeName);
    if (it != data.neuronTypeToIdMap.end()) {
        return it->second;
    } else {
        throw SNNParseException("Nieznany typ neuronu '" + typeName + "' w sekcji 'neuron_types'.");
    }
}

NetworkTopologyLoader::ConfigData NetworkTopologyLoader::loadFromYaml(const std::string &filename) {
    data = ConfigData(); // reset data
    existingConnections.clear();
    try {
        YAML::Node config = YAML::LoadFile(filename);
        if (!config["neuron_types"]) {
            throw SNNParseException("Brak sekcji 'neuron_types' w pliku YAML.", config);
        }
        // first load neuron types
        const YAML::Node& neuronTypes = config["neuron_types"];
        for (const auto& type : neuronTypes) {
            std::string typeName = type.first.as<std::string>();
            const YAML::Node& paramsNode = type.second;

            if (!paramsNode.IsMap()) {
                throw SNNParseException("Parametry dla typu '" + typeName + "' w sekcji 'neuron_types' nie sa poprawna mapa.", paramsNode);
            }

            IzhikevichParams params;
            std::string context = "neuron_types." + typeName;
            params.a = getNodeAs<double>(paramsNode, "a", context);
            params.b = getNodeAs<double>(paramsNode, "b", context);
            params.c = getNodeAs<double>(paramsNode, "c", context);
            params.d = getNodeAs<double>(paramsNode, "d", context);
            params.v0 = getNodeAs<double>(paramsNode, "v0", context);
            params.u0 = getNodeAs<double>(paramsNode, "u0", context);

            int typeId = static_cast<int>(data.neuronParamTypes.size());
            data.neuronParamTypes.push_back(params);
            data.neuronTypeToIdMap[typeName] = typeId;
        } // neuron types loaded

        // second load neuron groups
        if (!config["groups"]) {
            throw SNNParseException("Brak sekcji 'groups' w pliku YAML.", config);
        }
        const YAML::Node& groups = config["groups"];
        int currentStartIndex = 0;
        data.rootGroup = GroupInfo();
        data.rootGroup.fullName = "root";
        // read group information to rootGroup
        loadGroupData(groups, data.rootGroup, currentStartIndex);
        data.totalNeuronCount = currentStartIndex;
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

void NetworkTopologyLoader::loadGroupData(const YAML::Node& groupNode, GroupInfo& groupInfo, int& currentStartIndex) {
    if (!groupNode.IsSequence()) {
        throw SNNParseException("Oczekiwano sekwencji grup w grupie '" + groupInfo.fullName + "'.", groupNode);
    }
    groupInfo.startIndex = currentStartIndex;
    groupInfo.totalCount = 0;

    for (const auto& node : groupNode) {
        if (!node.IsMap()) {
            throw SNNParseException("Oczekiwano mapy dla grupy w grupie '" + groupInfo.fullName + "'.", node);
        }

        GroupInfo subgroup;
        subgroup.name = getNodeAs<std::string>(node, "name", groupInfo.fullName);
        subgroup.fullName = groupInfo.fullName + "." + subgroup.name;
        subgroup.startIndex = currentStartIndex;
        subgroup.totalCount = 0;

        bool hasNeurons = node["neurons"].IsDefined();
        bool hasSubgroups = node["subgroups"].IsDefined();

        if (hasNeurons && hasSubgroups) {
            throw SNNParseException("Grupa '" + subgroup.fullName + "' nie moze miec jednoczesnie 'neurons' i 'subgroups'.", node);
        }

        if (hasNeurons) {
            if (!node["neurons"].IsSequence()) {
                throw SNNParseException("Oczekiwano sekwencji dla 'neurons' w grupie '" + subgroup.fullName + "'.", node["neurons"]);
            }
            loadNeuronData(node["neurons"], subgroup, currentStartIndex);
        }
        if (hasSubgroups) {
            if (!node["subgroups"].IsSequence()) {
                throw SNNParseException("Oczekiwano sekwencji dla 'subgroups' w grupie '" + subgroup.fullName + "'.", node["subgroups"]);
            }
            loadGroupData(node["subgroups"], subgroup, currentStartIndex);
        }
        subgroup.totalCount = currentStartIndex - subgroup.startIndex;
        groupInfo.totalCount += subgroup.totalCount;
        groupInfo.subgroups.push_back(subgroup);
    }
}

void NetworkTopologyLoader::loadNeuronData(const YAML::Node& neuronsNode, GroupInfo& groupInfo, int& currentStartIndex) {
    if (!neuronsNode.IsSequence()) {
        throw SNNParseException("Oczekiwano sekwencji dla 'neurons' w grupie '" + groupInfo.fullName + "'.", neuronsNode);
    }

    for (const auto& neuronTypeNode : neuronsNode) {
        if (!neuronTypeNode.IsMap()) {
            throw SNNParseException("Oczekiwano mapy dla typu neuronu w grupie '" + groupInfo.fullName + "'.", neuronTypeNode);
        }
        std::string typeName = getNodeAs<std::string>(neuronTypeNode, "type", groupInfo.fullName);
        int count = getNodeAs<int>(neuronTypeNode, "count", groupInfo.fullName);

        if (count > 0) {
            NeuronInfo nInfo;
            nInfo.typeId = getNeuronTypeId(typeName);
            nInfo.count = count;
            nInfo.startIndex = currentStartIndex;
            data.globalNeuronTypeIds.insert(data.globalNeuronTypeIds.end(), count, nInfo.typeId);
            IzhikevichParams params = data.neuronParamTypes[nInfo.typeId];
            data.initialV.insert(data.initialV.end(), count, params.v0);
            data.initialU.insert(data.initialU.end(), count, params.u0);

            groupInfo.neuronInfos.push_back(nInfo);

            currentStartIndex += count;
        }
    }
}

void NetworkTopologyLoader::loadConnectionsData(const YAML::Node& connectionsNode) {
    if (!connectionsNode.IsSequence()) {
        throw SNNParseException("Oczekiwano sekwencji dla 'connections'.", connectionsNode);
    }

    data.synapticTargets.resize(data.globalNeuronTypeIds.size());
    data.synapticWeights.resize(data.globalNeuronTypeIds.size());
    existingConnections.resize(data.globalNeuronTypeIds.size());

    for (const auto& connectionNode : connectionsNode) {
        if (!connectionNode.IsMap()) {
            throw SNNParseException("Oczekiwano mapy dla polaczenia w 'connections'.", connectionNode);
        }
        std::string context = "connections";
        std::string fromGroup = getNodeAs<std::string>(connectionNode, "from", context);
        std::string toGroup = getNodeAs<std::string>(connectionNode, "to", context);
        std::string fromType = getNodeAs<std::string>(connectionNode, "from_type", context);
        std::string toType = getNodeAs<std::string>(connectionNode, "to_type", context);
        // exclude_self is optional, default to false
        bool excludeSelf = connectionNode["exclude_self"] ? getNodeAs<bool>(connectionNode, "exclude_self", context + " (default false)") : false;
        YAML::Node ruleNode = getNodeAs<YAML::Node>(connectionNode, "rule", context);
        YAML::Node weightNode = getNodeAs<YAML::Node>(connectionNode, "weight", context);

        WeightGenerator weightGen = createWeightGenerator(weightNode, context + " (from '" + fromGroup + "' to '" + toGroup + "')");
        
        // Make actual connection here (not implemented in this commit). For now, just print the connection details.
        std::vector<std::pair<const GroupInfo*, const GroupInfo*>> matchedPairs;
        printf("From '%s', To '%s'\n", fromGroup.c_str(), toGroup.c_str());
        findMatchingGroups(fromGroup, toGroup, data.rootGroup, excludeSelf, matchedPairs);
        for (const auto& pair : matchedPairs) {
            printf("  Matched Pair: %s -> %s\n", pair.first->fullName.c_str(), pair.second->fullName.c_str());
            createConnectionsBetweenGroups(*pair.first, *pair.second, fromType, toType, ruleNode, weightGen, excludeSelf);
        }
        printf("\n");
    }

    // After loading configuration, simplify the structure of existingConnections
    for (int i = 0; i < data.synapticTargets.size(); i++) {
        // Sort synapticTargets and corresponding synapticWeights
        std::vector<std::pair<int, double>> pairs;
        pairs.reserve(data.synapticTargets[i].size());
        for (int j = 0; j < data.synapticTargets[i].size(); ++j) {
            pairs.emplace_back(data.synapticTargets[i][j], data.synapticWeights[i][j]);
        }
        std::sort(pairs.begin(), pairs.end());
        for (int j = 0; j < pairs.size(); ++j) {
            data.synapticTargets[i][j] = pairs[j].first;
            data.synapticWeights[i][j] = pairs[j].second;
        }
        data.synapticTargets[i].shrink_to_fit();
        data.synapticWeights[i].shrink_to_fit();
    }
}

// type_id == -1 means all types
void NetworkTopologyLoader::getMatchingNeuronCount(const GroupInfo& group, const int typeId,
    std::vector<NeuronInfo>& outNeurons) const {

    if (!group.subgroups.empty()) {
        for (const auto& subgroup : group.subgroups) {
            getMatchingNeuronCount(subgroup, typeId, outNeurons);
        }
        return;
    }
    // if no subgroups, count neurons in this group
    for (const auto& nInfo : group.neuronInfos) {
        if (nInfo.typeId == typeId || typeId == -1) {
            outNeurons.push_back(nInfo);
        }
    }
    return;
}

void NetworkTopologyLoader::createConnectionsBetweenGroups(
    const GroupInfo& fromGroup, const GroupInfo& toGroup,
    const std::string& fromType, const std::string& toType,
    const YAML::Node& ruleNode, WeightGenerator& weightGen, bool excludeSelf) {

    int fromTypeId = (fromType == "all") ? -1 : getNeuronTypeId(fromType);
    int toTypeId = (toType == "all") ? -1 : getNeuronTypeId(toType);
    std::vector<NeuronInfo> fromNeurons;
    std::vector<NeuronInfo> toNeurons;
    getMatchingNeuronCount(fromGroup, fromTypeId, fromNeurons);
    getMatchingNeuronCount(toGroup, toTypeId, toNeurons);
    
    int fromCount = 0;
    int toCount = 0;
    for (const auto& n : fromNeurons) fromCount += n.count;
    for (const auto& n : toNeurons) toCount += n.count;

    std::string ruleType = getNodeAs<std::string>(ruleNode, "type", "rule");
    if (ruleType == "one_to_one") {
        if (fromCount != toCount) {
            throw SNNParseException("Liczba neuronow w 'from' i 'to' musi byc rowna dla reguly 'one_to_one'.", ruleNode);
        }
        int fromIndex = 0;
        for (const auto& fromN : fromNeurons) {
            for (int i = 0; i < fromN.count; i++) {
                int toIndex = 0;
                for (const auto& toN : toNeurons) {
                    for (int j = 0; j < toN.count; j++) {
                        if (existingConnections[fromN.startIndex + i].count(toN.startIndex + j) > 0) {
                            continue;
                        }
                        if (excludeSelf && fromN.startIndex + i == toN.startIndex + j) {
                            continue;
                        }
                        if (fromIndex == toIndex) {
                            double weight = weightGen.generate();
                            data.synapticTargets[fromN.startIndex + i].push_back(toN.startIndex + j);
                            data.synapticWeights[fromN.startIndex + i].push_back(weight);
                        }
                        toIndex++;
                    }
                }
                fromIndex++;
            }
        }
    }
    else if (ruleType == "all_to_all") {
        for (const auto& fromN : fromNeurons) {
            for (int i = 0; i < fromN.count; i++) {
                for (const auto& toN : toNeurons) {
                    for (int j = 0; j < toN.count; j++) {
                        if (existingConnections[fromN.startIndex + i].count(toN.startIndex + j) > 0) {
                            continue;
                        }
                        if (excludeSelf && fromN.startIndex + i == toN.startIndex + j) {
                            continue;
                        }
                        double weight = weightGen.generate();
                        data.synapticTargets[fromN.startIndex + i].push_back(toN.startIndex + j);
                        data.synapticWeights[fromN.startIndex + i].push_back(weight);
                    }
                }
            }
        }
    }
    else if (ruleType == "probabilistic") {
        Random& randGen = Random::getInstance();
        double probability = getNodeAs<double>(ruleNode, "probability", "rule");
        if (probability < 0.0 || probability > 1.0) {
            throw SNNParseException("'probability' musi byc w zakresie [0.0, 1.0] w regule 'probabilistic'.", ruleNode);
        }
        for (const auto& fromN : fromNeurons) {
            for (int i = 0; i < fromN.count; i++) {
                for (const auto& toN : toNeurons) {
                    for (int j = 0; j < toN.count; j++) {
                        if (existingConnections[fromN.startIndex + i].count(toN.startIndex + j) > 0) {
                            continue;
                        }
                        if (excludeSelf && fromN.startIndex + i == toN.startIndex + j) {
                            continue;
                        }
                        if (randGen.nextDouble() < probability) {
                            double weight = weightGen.generate();
                            data.synapticTargets[fromN.startIndex + i].push_back(toN.startIndex + j);
                            data.synapticWeights[fromN.startIndex + i].push_back(weight);
                        }
                    }
                }
            }
        }
    }
    else if (ruleType == "fixed_in_degree") {
        int count = getNodeAs<int>(ruleNode, "count", "rule");
        if (count <= 0) {
            throw SNNParseException("'count' musi byc dodatnie w regule 'fixed_in_degree'.", ruleNode);
        }
        Random& randGen = Random::getInstance();
        // Create a list of all possible source indices
        std::vector<int> allSourceIndices;
        allSourceIndices.reserve(fromCount);
        for (const auto& from_n : fromNeurons) {
            for (int i = 0; i < from_n.count; i++) {
                allSourceIndices.push_back(from_n.startIndex + i);
            }
        }
        // For each target neuron
        for (const auto& to_n : toNeurons) {
            for (int j = 0; j < to_n.count; j++) {
                int targetIdx = to_n.startIndex + j;
                std::vector<int> availableSources;
                // Build list of available sources
                availableSources.reserve(allSourceIndices.size());
                for (int srcIdx : allSourceIndices) {
                    if (excludeSelf && srcIdx == targetIdx) {
                        continue;
                    }
                    if (existingConnections[srcIdx].count(targetIdx) == 0) {
                        availableSources.push_back(srcIdx);
                    }
                }
                int realCount = std::min(count, static_cast<int>(availableSources.size()));
                for (int k = 0; k < realCount; k++) {
                    // Select a random source from available sources
                    int randIndex = randGen.nextInt(static_cast<int>(availableSources.size()) - 1);
                    int sourceIdx = availableSources[randIndex];
                    double weight = weightGen.generate();
                    data.synapticTargets[sourceIdx].push_back(targetIdx);
                    data.synapticWeights[sourceIdx].push_back(weight);
                    // Replace the removed element with the last one for O(1) removal
                    availableSources[randIndex] = availableSources.back();
                    availableSources.pop_back();
                }
            }
        }
    }
    else if (ruleType == "fixed_out_degree") {
        int count = getNodeAs<int>(ruleNode, "count", "rule");
        if (count <= 0) {
            throw SNNParseException("'count' musi byc dodatnie w regule 'fixed_out_degree'.", ruleNode);
        }
        Random& randGen = Random::getInstance();
        // Create a list of all possible target indices
        std::vector<int> allTargetIndices;
        allTargetIndices.reserve(toCount);
        for (const auto& to_n : toNeurons) {
            for (int j = 0; j < to_n.count; j++) {
                allTargetIndices.push_back(to_n.startIndex + j);
            }
        }
        // For each source neuron
        for (const auto& from_n : fromNeurons) {
            for (int i = 0; i < from_n.count; i++) {
                int sourceIdx = from_n.startIndex + i;
                std::vector<int> availableTargets;
                for (int tgtIdx : allTargetIndices) {
                    if (excludeSelf && sourceIdx == tgtIdx) {
                        continue;
                    }
                    if (existingConnections[sourceIdx].count(tgtIdx) == 0) {
                        availableTargets.push_back(tgtIdx);
                    }
                }
                int realCount = std::min(count, static_cast<int>(availableTargets.size()));
                data.synapticTargets[sourceIdx].reserve(data.synapticTargets[sourceIdx].size() + realCount);
                data.synapticWeights[sourceIdx].reserve(data.synapticWeights[sourceIdx].size() + realCount);
                for (int k = 0; k < realCount; k++) {
                    // Select a random target from available targets
                    int randIndex = randGen.nextInt(static_cast<int>(availableTargets.size()) - 1);
                    int targetIdx = availableTargets[randIndex];
                    double weight = weightGen.generate();
                    data.synapticTargets[sourceIdx].push_back(targetIdx);
                    data.synapticWeights[sourceIdx].push_back(weight);
                    // Replace the removed element with the last one for O(1) removal
                    availableTargets[randIndex] = availableTargets.back();
                    availableTargets.pop_back();
                }
            }
        }
    }
    else {
        throw SNNParseException("Nieznany typ reguly polaczen '" + ruleType + "' w 'rule'.", ruleNode);
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

void NetworkTopologyLoader::findMatchingGroups(const std::string& fromPattern, const std::string& toPattern,
    const GroupInfo& rootGroup, bool excludeSelf, std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& outMatchedPairs) {
    
    std::map<int, std::string> wildcardValues;
    std::vector<std::string> fromSegments = splitPath(fromPattern);
    std::vector<std::string> toSegments = splitPath(toPattern);
    findMatchingGroupsRecursive(rootGroup, rootGroup, fromSegments, toSegments,
                                0, wildcardValues, excludeSelf, outMatchedPairs);
}

void NetworkTopologyLoader::findMatchingGroupsRecursive(
    const GroupInfo& currentFromGroup, 
    const GroupInfo& rootForToSearch,
    const std::vector<std::string>& fromSegments, 
    const std::vector<std::string>& toSegments,
    int fromIndex,
    std::map<int, std::string> &wildcardValues,
    bool excludeSelf,
    std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& outMatchedPairs) {

    // If we've matched the complete 'from' pattern
    if (fromIndex == fromSegments.size()) {
        // Find matching 'to' groups with current wildcard values
        findMatchingToGroups(currentFromGroup, rootForToSearch, toSegments, 
                            0, wildcardValues, excludeSelf, outMatchedPairs);
        return;
    }
    
    const std::string& segment = fromSegments[fromIndex];
    
    if (isWildcard(segment)) {
        int wildcardNum = getWildcardNumber(segment);
        
        // If this wildcard has been seen before, it must match the same subgroup
        if (wildcardValues.count(wildcardNum)) {
            for (const auto& subgroup : currentFromGroup.subgroups) {
                if (subgroup.name == wildcardValues[wildcardNum]) {
                    findMatchingGroupsRecursive(subgroup, rootForToSearch, 
                                                fromSegments, toSegments,
                                                fromIndex + 1,
                                                wildcardValues, excludeSelf, outMatchedPairs);
                }
            }
        } else {
            // New wildcard, try all subgroups
            for (const auto& subgroup : currentFromGroup.subgroups) {
                wildcardValues[wildcardNum] = subgroup.name;
                findMatchingGroupsRecursive(subgroup, rootForToSearch, 
                                            fromSegments, toSegments,
                                            fromIndex + 1,
                                            wildcardValues, excludeSelf, outMatchedPairs);
                
                wildcardValues.erase(wildcardNum);
            }
        }
    } else {
        // Literal segment, check if any subgroup matches exactly
        for (const auto& subgroup : currentFromGroup.subgroups) {
            if (subgroup.name == segment) {
                findMatchingGroupsRecursive(subgroup, rootForToSearch, 
                                            fromSegments, toSegments,
                                            fromIndex + 1,
                                            wildcardValues, excludeSelf, outMatchedPairs);
            }
        }
    }
}

void NetworkTopologyLoader::findMatchingToGroups(
    const GroupInfo& fromGroup, 
    const GroupInfo& currentToGroup,
    const std::vector<std::string>& toSegments, 
    int toIndex,
    const std::map<int, std::string>& wildcardValues,
    bool excludeSelf,
    std::vector<std::pair<const GroupInfo*, const GroupInfo*>>& outMatchedPairs) {
    
    // If we've matched the complete 'to' pattern
    if (toIndex == toSegments.size()) {
        if (excludeSelf && fromGroup.fullName == currentToGroup.fullName) {
            return; // Skip self-connection if excludeSelf is true
        }
        
        outMatchedPairs.push_back({&fromGroup, &currentToGroup});
        return;
    }
    
    const std::string& segment = toSegments[toIndex];
    
    if (isWildcard(segment)) {
        int wildcardNum = getWildcardNumber(segment);

        // If this wildcard has a defined value, try to match it
        if (wildcardValues.count(wildcardNum)) {
            for (const auto& subgroup : currentToGroup.subgroups) {
                if (subgroup.name == wildcardValues.at(wildcardNum)) {
                    findMatchingToGroups(fromGroup, subgroup, toSegments,
                                        toIndex + 1, wildcardValues, excludeSelf, outMatchedPairs);
                }
            }
        } else {
            // New wildcard, try all subgroups
            for (const auto& subgroup : currentToGroup.subgroups) {
                std::map<int, std::string> newWildcardValues = wildcardValues;
                newWildcardValues[wildcardNum] = subgroup.name;
                
                findMatchingToGroups(fromGroup, subgroup, toSegments, 
                                    toIndex + 1, newWildcardValues, excludeSelf, outMatchedPairs);
            }
        }
    } else {
        // Literal segment, check if any subgroup matches exactly
        for (const auto& subgroup : currentToGroup.subgroups) {
            if (subgroup.name == segment) {
                findMatchingToGroups(fromGroup, subgroup, toSegments,
                                    toIndex + 1, wildcardValues, excludeSelf, outMatchedPairs);
            }
        }
    }
}

// Helper function to check if a segment is a wildcard
bool NetworkTopologyLoader::isWildcard(const std::string& segment) const {
    if (segment.size() >= 3 && segment.front() == '[' && segment.back() == ']') {
        std::string numberPart = segment.substr(1, segment.size() - 2);
        return !numberPart.empty() && std::all_of(numberPart.begin(), numberPart.end(), ::isdigit);
    }
    return false;
}

// Helper function to extract wildcard number
int NetworkTopologyLoader::getWildcardNumber(const std::string& segment) const {
    return std::stoi(segment.substr(1, segment.size() - 2));
}