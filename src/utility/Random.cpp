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

double Random::nextDouble() {
    return getUniform(0.0, 1.0);
}

int Random::nextInt(int maxExclusive) {
    if (maxExclusive <= 0) {
        return 0;
    }
    std::uniform_int_distribution<int> dist(0, maxExclusive - 1);
    return dist(getInstance().engine);
}

int Random::getInt(int min, int max) {
    std::uniform_int_distribution<int> dist(min, max);
    return dist(getInstance().engine);
}

template<typename RandomIt>
void Random::shuffle(RandomIt first, RandomIt last) {
    std::shuffle(first, last, getInstance().engine);
}