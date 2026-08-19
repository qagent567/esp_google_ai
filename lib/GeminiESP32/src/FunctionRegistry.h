#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <vector>
#include <functional>

/**
 * @brief Описание параметра для Function Calling
 */
struct FunctionParam {
    String name;        // Имя параметра
    String type;        // STRING, NUMBER, INTEGER, BOOLEAN, ARRAY, OBJECT
    String description; // Описание параметра для ИИ
    bool required;      // Обязателен ли параметр
};

/**
 * @brief Функция обратного вызова для зарегистрированного инструмента
 * Принимает JsonObject с аргументами, переданными нейросетью.
 * Возвращает строку с результатом работы (будет передана ИИ или пользователю).
 */
typedef std::function<String(JsonObjectConst args)> FunctionHandler;

/**
 * @brief Декларация C++ функции для Google Gemini API
 */
struct FunctionDefinition {
    String name;
    String description;
    std::vector<FunctionParam> parameters;
    FunctionHandler handler;
};

/**
 * @brief Реестр нативных C++ функций для Function Calling (Tools)
 */
class FunctionRegistry {
public:
    FunctionRegistry();

    /**
     * @brief Регистрация пользовательской C++ функции
     * @param name Уникальное имя функции (только латиница и подчеркивания, напр. "set_relay")
     * @param description Подробное описание для ИИ, когда и зачем вызывать эту функцию
     * @param params Список принимаемых аргументов
     * @param handler Лямбда или функция-обработчик
     */
    void registerFunction(const String& name, 
                          const String& description, 
                          const std::vector<FunctionParam>& params, 
                          FunctionHandler handler);

    /**
     * @brief Простая регистрация функции без аргументов
     */
    void registerFunction(const String& name, 
                          const String& description, 
                          FunctionHandler handler);

    /**
     * @brief Очистка всех зарегистрированных функций
     */
    void clear();

    /**
     * @brief Количество зарегистрированных функций
     */
    size_t count() const { return _functions.size(); }

    /**
     * @brief Проверка, зарегистрированы ли функции
     */
    bool hasFunctions() const { return !_functions.empty(); }

    /**
     * @brief Поиск декларации функции по имени
     */
    const FunctionDefinition* findFunction(const String& name) const;

    /**
     * @brief Выполнение функции по имени с переданными аргументами
     */
    String execute(const String& name, JsonObjectConst args);

    /**
     * @brief Генерация JSON схемы 'tools' для Google Gemini API
     */
    void appendToolsJson(JsonDocument& doc) const;

private:
    std::vector<FunctionDefinition> _functions;
};
