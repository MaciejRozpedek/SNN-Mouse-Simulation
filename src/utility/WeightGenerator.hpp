#ifndef WEIGHT_GENERATOR_HPP
#define WEIGHT_GENERATOR_HPP

#include "Random.hpp"

class WeightGenerator {
public:
    enum class GenerationType {
        FIXED,
        UNIFORM,
        NORMAL
    };

private:
    GenerationType type;
    Random &random;
    double p1 = 0.0;
    double p2 = 0.0;
    
    WeightGenerator(Random& random, GenerationType type, double p1, double p2);

public:
    static WeightGenerator createFixed(Random& random, double fixedValue);
    static WeightGenerator createUniform(Random& random, double min, double max);
    static WeightGenerator createNormal(Random& random, double mean, double std);

    double generate() const;
};

#endif // WEIGHT_GENERATOR_HPP