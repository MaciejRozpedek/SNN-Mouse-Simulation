#include "snn_core.hpp"
#include <yaml-cpp/yaml.h>
#include <iostream>

SNN::SNN(const std::string &filename) {
    loadFromYaml(filename);
}

int SNN::getNeuronTypeId(const std::string &type_name) const {
    auto it = neuron_type_to_id_map.find(type_name);
    if (it != neuron_type_to_id_map.end()) {
        return it->second;
    } else {
        throw std::runtime_error("Błąd: Nieznany typ neuronu '" + type_name + "' w sekcji 'neuron_types'.");
    }
}

void SNN::loadFromYaml(const std::string &filename) {
    try {
        YAML::Node config = YAML::LoadFile(filename);
        if (!config["neuron_types"]) {
            throw std::runtime_error("Błąd: Brak sekcji 'neuron_types' w pliku YAML. Linia: " + std::to_string(config.Mark().line));
        }
        // first load neuron types
        const YAML::Node& neuron_types = config["neuron_types"];
        for (const auto& type : neuron_types) {
            std::string type_name = type.first.as<std::string>();
            const YAML::Node& params_node = type.second;

            if (!params_node.IsMap()) {
                throw std::runtime_error("Błąd parsowania: Parametry dla typu '" + type_name + "' w sekcji 'neuron_types' nie są poprawną mapą. Linia: " + std::to_string(params_node.Mark().line));
            }

            IzhikevichParams params;
            try {
                if (!params_node["a"] || !params_node["b"] || !params_node["c"] || !params_node["d"] ||
                    !params_node["starting_v"] || !params_node["starting_u"]) {
                    throw std::runtime_error("Błąd: Brak jednego z parametrów 'a', 'b', 'c', 'd', 'starting_v', 'starting_u' dla typu '" + type_name + "' w sekcji 'neuron_types'. Linia: " + std::to_string(params_node.Mark().line));
                }
                params.a = params_node["a"].as<double>();
                params.b = params_node["b"].as<double>();
                params.c = params_node["c"].as<double>();
                params.d = params_node["d"].as<double>();
                params.starting_v = params_node["starting_v"].as<double>();
                params.starting_u = params_node["starting_u"].as<double>();
            }
            catch (const YAML::TypedBadConversion<double>& e) {
                throw std::runtime_error("Błąd konwersji w parametrach Izhikevicha dla typu " + type_name + " w sekcji 'neuron_types': " + e.what() + ". Upewnij się, że 'a', 'b', 'c', 'd', 'starting_v', 'starting_u' są liczbami. Linia: " + std::to_string(params_node.Mark().line));
            }

            int type_id = static_cast<int>(neuronParamTypes.size());
            neuronParamTypes.push_back(params);
            neuron_type_to_id_map[type_name] = type_id;
        } // neuron types loaded

        // second load neuron groups
        if (!config["groups"]) {
            throw std::runtime_error("Błąd: Brak sekcji 'groups' w pliku YAML. Linia: " + std::to_string(config.Mark().line));
        }
        const YAML::Node& groups = config["groups"];
        int current_start_index = 0;
        rootGroup = GroupInfo();
        rootGroup.fullName = "root";
        // read group information to rootGroup
        loadGroupData(groups, rootGroup, current_start_index);
        total_neuron_count = current_start_index;
        rootGroup.startIndex = 0;
        rootGroup.totalCount = total_neuron_count;

        // third load and create synapses
        if (!config["connections"]) {
            throw std::runtime_error("Błąd: Brak sekcji 'connections' w pliku YAML. Linia: " + std::to_string(config.Mark().line));
        }
        const YAML::Node& connections = config["connections"];
        loadConnectionsData(connections);
    }
    catch(const YAML::BadFile&) {
        throw std::runtime_error("Błąd: Nie można znaleźć lub otworzyć pliku " + filename);
    }
    catch(const YAML::ParserException& e) {
        throw std::runtime_error("Błąd parsowania YAML: " + std::string(e.what()));
    }
}

void SNN::loadGroupData(const YAML::Node& groupNode, GroupInfo& groupInfo, int& current_start_index) {
    // basic checks
    if (!groupNode.IsSequence()) {
        throw std::runtime_error("Błąd parsowania: Oczekiwano sekwencji grup w grupie '" + groupInfo.fullName + "'. Linia: " + std::to_string(groupNode.Mark().line));
    }
    groupInfo.startIndex = current_start_index;
    groupInfo.totalCount = 0;

    for (const auto& node : groupNode) {
        if (!node.IsMap()) {
            throw std::runtime_error("Błąd parsowania: Oczekiwano mapy dla grupy w grupie '" + groupInfo.fullName + "'. Linia: " + std::to_string(node.Mark().line));
        }

        GroupInfo subgroup;
        if (node["name"]) {
            subgroup.name = node["name"].as<std::string>();
            subgroup.fullName = groupInfo.fullName + "." + subgroup.name;
        } else {
            throw std::runtime_error("Błąd parsowania: Grupa bez nazwy w grupie '" + groupInfo.fullName + "'. Linia: " + std::to_string(node.Mark().line));
        }
        subgroup.startIndex = current_start_index;
        subgroup.totalCount = 0;

        bool has_neurons = node["neurons"].IsDefined();
        bool has_subgroups = node["subgroups"].IsDefined();

        if (has_neurons && has_subgroups) {
            throw std::runtime_error("Błąd parsowania: Grupa '" + subgroup.fullName + "' nie może mieć jednocześnie 'neurons' i 'subgroups'. Linia: " + std::to_string(node.Mark().line));
        }

        if (has_neurons) {
            if (!node["neurons"].IsSequence()) {
                throw std::runtime_error("Błąd parsowania: Oczekiwano sekwencji dla 'neurons' w grupie '" + subgroup.fullName + "'. Linia: " + std::to_string(node["neurons"].Mark().line));
            }
            loadNeuronData(node["neurons"], subgroup, current_start_index);
        }
        if (has_subgroups) {
            if (!node["subgroups"].IsSequence()) {
                throw std::runtime_error("Błąd parsowania: Oczekiwano sekwencji dla 'subgroups' w grupie '" + subgroup.fullName + "'. Linia: " + std::to_string(node["subgroups"].Mark().line));
            }
            loadGroupData(node["subgroups"], subgroup, current_start_index);
        }
        groupInfo.subgroups.push_back(subgroup);
    }
}

void SNN::loadNeuronData(const YAML::Node& neuronsNode, GroupInfo& groupInfo, int& current_start_index) {
    if (!neuronsNode.IsSequence()) {
        throw std::runtime_error("Błąd parsowania: Oczekiwano sekwencji dla 'neurons' w grupie '" + groupInfo.fullName + "'. Linia: " + std::to_string(neuronsNode.Mark().line));
    }

    int local_count = 0;
    for (const auto& neuronTypeNode : neuronsNode) {
        if (!neuronTypeNode.IsMap()) {
            throw std::runtime_error("Błąd parsowania: Oczekiwano mapy dla typu neuronu w grupie '" + groupInfo.fullName + "'. Linia: " + std::to_string(neuronTypeNode.Mark().line));
        }
        if (!neuronTypeNode["type"] || !neuronTypeNode["count"]) {
            throw std::runtime_error("Błąd parsowania: Typ neuronu lub liczba nieokreślona w grupie '" + groupInfo.fullName + "'. Linia: " + std::to_string(neuronTypeNode.Mark().line));
        }
        std::string type_name;
        int count;
        try {
            type_name = neuronTypeNode["type"].as<std::string>();
            count = neuronTypeNode["count"].as<int>();
        } catch (const YAML::TypedBadConversion<std::string>& e) {
            throw std::runtime_error("Błąd konwersji typu neuronu w grupie '" + groupInfo.fullName + "': " + std::string(e.what()) + ". Linia: " + std::to_string(neuronTypeNode.Mark().line));
        } catch (const YAML::TypedBadConversion<int>& e) {
            throw std::runtime_error("Błąd konwersji liczby neuronów w grupie '" + groupInfo.fullName + "': " + std::string(e.what()) + ". Linia: " + std::to_string(neuronTypeNode.Mark().line));
        }

        if (count > 0) {
            NeuronInfo n_info;
            n_info.typeId = getNeuronTypeId(type_name);
            n_info.count = count;
            n_info.startIndex = current_start_index;
            neuronToTypeId.insert(neuronToTypeId.end(), count, n_info.typeId);
            IzhikevichParams params = neuronParamTypes[n_info.typeId];
            v.insert(v.end(), count, params.starting_v); // Initial membrane potential
            u.insert(u.end(), count, params.starting_u); // Initial recovery
            I.insert(I.end(), count, 0.0); // Initial input current

            groupInfo.neuronInfos.push_back(n_info);

            current_start_index += count;
            local_count += count;
        }
    }
    groupInfo.totalCount = local_count;
}

void SNN::loadConnectionsData(const YAML::Node& connectionsNode) {
    if (!connectionsNode.IsSequence()) {
        throw std::runtime_error("Błąd parsowania: Oczekiwano sekwencji dla 'connections'. Linia: " + std::to_string(connectionsNode.Mark().line));
    }

    for (const auto& connectionNode : connectionsNode) {
        if (!connectionNode.IsMap()) {
            throw std::runtime_error("Błąd parsowania: Oczekiwano mapy dla połączenia w 'connections'. Linia: " + std::to_string(connectionNode.Mark().line));
        }
        if (!connectionNode["from"] || !connectionNode["to"] || !connectionNode["to_type"] ||
            !connectionNode["from_type"] || !connectionNode["weight"]) {
            throw std::runtime_error("Błąd parsowania: 'from', 'to', 'to_type', 'from_type' lub 'weight' nieokreślone w połączeniu w 'connections'. Linia: " + std::to_string(connectionNode.Mark().line));
        }
        if (!connectionNode["rule"] || !connectionNode["rule"].IsMap() || !connectionNode["rule"]["type"]) {
            throw std::runtime_error("Błąd parsowania: 'rule' nieokreślone w połączeniu w 'connections'.");
        }
        if (!connectionNode["weight"].IsMap()) {
            throw std::runtime_error("Błąd parsowania: niepoprawny format dla 'weight' w połączeniu w 'connections'.");
        }
        
        std::string from_group = connectionNode["from"].as<std::string>();
        std::string to_group = connectionNode["to"].as<std::string>();
        std::string from_type = connectionNode["from_type"].as<std::string>();
        std::string to_type = connectionNode["to_type"].as<std::string>();
        YAML::Node ruleNode = connectionNode["rule"];
    }
}