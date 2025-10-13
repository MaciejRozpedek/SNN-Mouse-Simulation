#include "SNN.hpp"
#include <iostream>
#include "SNNParseException.hpp"

int main() {
    try
    {
        SNN snn("../../data/SNNConfig.yaml");
    }
    catch (const SNNParseException& e) {
        std::cerr << "--- BLAD KONFIGURACJI MODELU ---\n";
        std::cerr << e.what() << "\n";
        std::cerr << "--------------------------------\n";
        return EXIT_FAILURE;
    }
    catch (const std::exception& e) {
        // Lapiemy inne, nieoczekiwane bledy (np. std::bad_alloc, inne runtime_error)
        std::cerr << "--- KRYTYCZNY BLAD PROGRAMU ---\n";
        std::cerr << "Nieoczekiwany wyjatek: " << e.what() << "\n";
        std::cerr << "-------------------------------\n";
        return EXIT_FAILURE;
    }
    
    return 0;
}