#include <cctype>
#include <iostream>
#include <sstream>
#include <string>
#include "json-formater.hpp"
#include <httplib.h>
#include <nlohmann/json.hpp>





int main() {
    try {
        JsonFormater json_formater;
        json_formater.run();
        return EXIT_SUCCESS;
    } catch (std::exception const &ex) {
        std::cerr << "Ошибка запуска приложения: " << ex.what() << std::endl;
        return EXIT_FAILURE;
    }
}
