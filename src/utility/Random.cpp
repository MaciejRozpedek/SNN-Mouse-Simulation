#include "random.hpp"

Random::Random() {
    std::random_device rd;
    engine.seed(rd());
}

Random& Random::getInstance() {
    static thread_local Random instance;
    return instance;
}

void Random::setSeed(unsigned int seed) {
    engine.seed(seed);
}

double Random::getNormal(double mean, double stddev) {
    std::normal_distribution<double> dist(mean, stddev);
    return dist(getInstance().engine);
}

double Random::getUniform(double min, double max) {
    std::uniform_real_distribution<double> dist(min, max);
    return dist(getInstance().engine);
}