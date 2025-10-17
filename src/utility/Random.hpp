#ifndef RANDOM_HPP
#define RANDOM_HPP

#include <random>

/**
 * @brief Singleton class for random number generation.
 */
class Random {
private:
    std::mt19937 engine;

    Random();
public:
    Random(const Random&) = delete;
    Random& operator=(const Random&) = delete;
    
    static Random& getInstance();
    void setSeed(unsigned int seed);

    static double getNormal(double mean, double stddev);
    static double getUniform(double min, double max);
    static double nextDouble();
};

#endif // RANDOM_HPP