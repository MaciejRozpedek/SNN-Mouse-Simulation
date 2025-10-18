#include "WeightGenerator.hpp"
#include <stdexcept>

WeightGenerator::WeightGenerator(Random& random, GenerationType type, double p1, double p2)
    : random(random), type(type), p1(p1), p2(p2) {
    switch (type) {
        case GenerationType::FIXED:
            break;
        case GenerationType::UNIFORM:
            if (p1 >= p2) {
                throw std::invalid_argument("Uniform distribution requires min < max.");
            }
            break;
        case GenerationType::NORMAL:
            if (p2 <= 0) {
                throw std::invalid_argument("Normal distribution requires stddev > 0.");
            }
            break;
        default:
            throw std::invalid_argument("Unknown GenerationType.");
    }
}

WeightGenerator WeightGenerator::createFixed(Random& random, double fixedValue) {
    return WeightGenerator(random, GenerationType::FIXED, fixedValue, 0.0);
}

WeightGenerator WeightGenerator::createUniform(Random& random, double min, double max) {
    if (min > max) {
        throw std::invalid_argument("Minimalna wartość rozkładu jednostajnego nie może być większa niż maksymalna.");
    }
    return WeightGenerator(random, GenerationType::UNIFORM, min, max);
}

WeightGenerator WeightGenerator::createNormal(Random& random, double mean, double std) {
    if (std < 0) {
        throw std::invalid_argument("Odchylenie standardowe rozkładu normalnego nie może być ujemne.");
    }
    return WeightGenerator(random, GenerationType::NORMAL, mean, std);
}

double WeightGenerator::generate() const {
    switch (type) {
        case GenerationType::FIXED:
            return p1;

        case GenerationType::UNIFORM: {
            return random.getUniform(p1, p2);
        }

        case GenerationType::NORMAL: {
            return random.getNormal(p1, p2);
        }

        default:
            throw std::runtime_error("Nieznany typ generowania wagi.");
    }
}