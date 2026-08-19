#include "FunctionRegistry.h"

FunctionRegistry::FunctionRegistry() {
}

void FunctionRegistry::registerFunction(const String& name, 
                                      const String& description, 
                                      const std::vector<FunctionParam>& params, 
                                      FunctionHandler handler) {
    // Удаляем предыдущую декларацию с таким же именем, если была
    for (auto it = _functions.begin(); it != _functions.end(); ++it) {
        if (it->name == name) {
            _functions.erase(it);
            break;
        }
    }
    _functions.push_back({name, description, params, handler});
}

void FunctionRegistry::registerFunction(const String& name, 
                                      const String& description, 
                                      FunctionHandler handler) {
    registerFunction(name, description, std::vector<FunctionParam>(), handler);
}

void FunctionRegistry::clear() {
    _functions.clear();
}

const FunctionDefinition* FunctionRegistry::findFunction(const String& name) const {
    for (const auto& fn : _functions) {
        if (fn.name == name) {
            return &fn;
        }
    }
    return nullptr;
}

String FunctionRegistry::execute(const String& name, JsonObjectConst args) {
    const FunctionDefinition* fn = findFunction(name);
    if (!fn) {
        return "Ошибка: Функция '" + name + "' не найдена в реестре ESP32.";
    }
    if (!fn->handler) {
        return "Ошибка: Обработчик для функции '" + name + "' не задан.";
    }
    return fn->handler(args);
}

void FunctionRegistry::appendToolsJson(JsonDocument& doc) const {
    if (_functions.empty()) return;

    JsonArray tools = doc["tools"].to<JsonArray>();
    JsonObject toolItem = tools.add<JsonObject>();
    JsonArray funcDecls = toolItem["function_declarations"].to<JsonArray>();

    for (const auto& fn : _functions) {
        JsonObject decl = funcDecls.add<JsonObject>();
        decl["name"] = fn.name;
        decl["description"] = fn.description;

        JsonObject paramsObj = decl["parameters"].to<JsonObject>();
        paramsObj["type"] = "OBJECT";
        JsonObject props = paramsObj["properties"].to<JsonObject>();
        JsonArray reqArray = paramsObj["required"].to<JsonArray>();

        for (const auto& p : fn.parameters) {
            JsonObject prop = props[p.name].to<JsonObject>();
            prop["type"] = p.type;
            prop["description"] = p.description;

            if (p.required) {
                reqArray.add(p.name);
            }
        }
    }
}
