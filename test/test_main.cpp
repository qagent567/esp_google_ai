#include <Arduino.h>
#include <unity.h>
#include <ArduinoJson.h>

#define private public
#include "CLI.h"
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "GeminiClient.h"

#include "UsageTracker.h"

ConfigManager* cfg;
NetworkManager* net;
GeminiClient* gem;
UsageTracker* usage;
SerialCLI* cli;

void setUp(void) {
    cfg = new ConfigManager();
    net = new NetworkManager(*cfg);
    usage = new UsageTracker();
    gem = new GeminiClient(*cfg, usage);
    cli = new SerialCLI(*cfg, *net, *gem, *usage);
}

void tearDown(void) {
    delete cli;
    delete usage;
    delete gem;
    delete net;
    delete cfg;
}

// 1. Тесты конфигурации (ConfigManager)
void test_config_defaults(void) {
    TEST_ASSERT_EQUAL_STRING("gemini-3.5-flash-lite", cfg->getConfig().model.c_str());
    TEST_ASSERT_EQUAL_STRING("111.88.96.50", cfg->getConfig().dnsPrimary.c_str());
    TEST_ASSERT_EQUAL_STRING("111.88.96.51", cfg->getConfig().dnsSecondary.c_str());
    TEST_ASSERT_EQUAL(1024, cfg->getConfig().maxTokens);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.7f, cfg->getConfig().temperature);
}

void test_config_sanitization_wifi(void) {
    cfg->setWifi("  0266\r\n\t ", " 18888888\r\n ");
    TEST_ASSERT_EQUAL_STRING("0266", cfg->getConfig().wifiSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("18888888", cfg->getConfig().wifiPassword.c_str());
}

void test_config_sanitization_api_key(void) {
    cfg->setApiKey(" \r\n AQ.Ab8RN6LtR0RySZwOW5SYz3eyFtZaCebV6Ta-Thsq5OVhiPAg1Q \n ");
    TEST_ASSERT_EQUAL_STRING("AQ.Ab8RN6LtR0RySZwOW5SYz3eyFtZaCebV6Ta-Thsq5OVhiPAg1Q", cfg->getConfig().apiKey.c_str());
    TEST_ASSERT_TRUE(cfg->hasSavedApiKey());
}

void test_config_sanitization_quotes(void) {
    cfg->setApiKey("\"AIzaSyFakeKey12345\"");
    TEST_ASSERT_EQUAL_STRING("AIzaSyFakeKey12345", cfg->getConfig().apiKey.c_str());

    cfg->setWifi("'MyTestSSID'", "'SecretPass'");
    TEST_ASSERT_EQUAL_STRING("MyTestSSID", cfg->getConfig().wifiSsid.c_str());
    TEST_ASSERT_EQUAL_STRING("SecretPass", cfg->getConfig().wifiPassword.c_str());
}

// 2. Тесты парсинга CLI
void test_parse_args_simple(void) {
    String argv[10];
    int argc = cli->parseArgs("set ssid mywifi", argv, 10);
    
    TEST_ASSERT_EQUAL(3, argc);
    TEST_ASSERT_EQUAL_STRING("set", argv[0].c_str());
    TEST_ASSERT_EQUAL_STRING("ssid", argv[1].c_str());
    TEST_ASSERT_EQUAL_STRING("mywifi", argv[2].c_str());
}

void test_parse_args_quotes(void) {
    String argv[10];
    int argc = cli->parseArgs("ask \"hello world\"", argv, 10);
    
    TEST_ASSERT_EQUAL(2, argc);
    TEST_ASSERT_EQUAL_STRING("ask", argv[0].c_str());
    TEST_ASSERT_EQUAL_STRING("hello world", argv[1].c_str());
}

void test_parse_args_empty(void) {
    String argv[10];
    int argc = cli->parseArgs("", argv, 10);
    TEST_ASSERT_EQUAL(0, argc);
}

void test_history_limit(void) {
    for (int i = 0; i < 20; i++) {
        cli->handleCommand(String("cmd") + i);
    }
    TEST_ASSERT_EQUAL(15, cli->_history.size());
    TEST_ASSERT_EQUAL_STRING("cmd19", cli->_history.back().c_str());
    TEST_ASSERT_EQUAL_STRING("cmd5", cli->_history.front().c_str());
}

// 3. Тесты клиента Gemini (GeminiClient)
void test_gemini_api_url_build(void) {
    cfg->setModel("gemini-2.0-flash");
    cfg->setApiKey("TEST_API_KEY_123");

    String url = gem->buildApiUrl();
    TEST_ASSERT_TRUE(url.startsWith("https://generativelanguage.googleapis.com/v1beta/models/gemini-2.0-flash:generateContent?key=TEST_API_KEY_123"));
}

void test_gemini_request_body_structure(void) {
    cfg->setSystemPrompt("");
    cfg->setMaxTokens(512);
    cfg->setTemperature(0.5f);

    String body = gem->buildRequestBody("Как погода?");
    TEST_ASSERT_TRUE(body.length() > 0);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    TEST_ASSERT_FALSE(err);
    TEST_ASSERT_EQUAL_STRING("user", doc["contents"][0]["role"]);
    TEST_ASSERT_EQUAL_STRING("Как погода?", doc["contents"][0]["parts"][0]["text"]);
    TEST_ASSERT_EQUAL(512, doc["generationConfig"]["maxOutputTokens"]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.5f, doc["generationConfig"]["temperature"]);
}

void test_gemini_request_body_system_prompt(void) {
    cfg->setSystemPrompt("Ты краткий ассистент.");
    String body = gem->buildRequestBody("Привет");

    JsonDocument doc;
    deserializeJson(doc, body);
    TEST_ASSERT_EQUAL_STRING("Ты краткий ассистент.", doc["systemInstruction"]["parts"][0]["text"]);
}

void test_gemini_parse_success_response(void) {
    String mockJson = "{\"candidates\":[{\"content\":{\"parts\":[{\"text\":\"Тестовый ответ от Gemini\"}],\"role\":\"model\"},\"finishReason\":\"STOP\"}],\"usageMetadata\":{\"totalTokenCount\":42}}";
    GeminiResponse resp;
    bool ok = gem->parseResponse(mockJson, resp);

    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_TRUE(resp.success);
    TEST_ASSERT_EQUAL_STRING("Тестовый ответ от Gemini", resp.text.c_str());
    TEST_ASSERT_EQUAL(42, resp.totalTokens);
}

void test_gemini_parse_error_response(void) {
    String mockErrorJson = "{\"error\":{\"code\":400,\"message\":\"API key not valid. Please pass a valid API key.\",\"status\":\"INVALID_ARGUMENT\"}}";
    GeminiResponse resp;
    bool ok = gem->parseResponse(mockErrorJson, resp);

    TEST_ASSERT_FALSE(ok);
    TEST_ASSERT_FALSE(resp.success);
    TEST_ASSERT_TRUE(resp.text.indexOf("API key not valid") >= 0);
    TEST_ASSERT_TRUE(resp.text.indexOf("INVALID_ARGUMENT") >= 0);
}

void test_gemini_http_error_descriptions(void) {
    TEST_ASSERT_TRUE(GeminiClient::getHttpErrorDescription(200).indexOf("OK") >= 0);
    TEST_ASSERT_TRUE(GeminiClient::getHttpErrorDescription(400).indexOf("Неверный запрос") >= 0);
    TEST_ASSERT_TRUE(GeminiClient::getHttpErrorDescription(403).indexOf("Доступ запрещен") >= 0);
    TEST_ASSERT_TRUE(GeminiClient::getHttpErrorDescription(404).indexOf("Не найдено") >= 0);
    TEST_ASSERT_TRUE(GeminiClient::getHttpErrorDescription(429).indexOf("Превышен лимит") >= 0);
}

// 4. Тесты UsageTracker (квоты и суточные лимиты)
void test_usage_tracker_limits_and_reset(void) {
    usage->setDailyLimit(10);
    usage->resetDailyUsage();
    DailyUsageStats s = usage->getStats();
    TEST_ASSERT_EQUAL(0, s.requestsToday);
    TEST_ASSERT_EQUAL(0, s.totalTokensToday);
    TEST_ASSERT_EQUAL(10, s.dailyRequestLimit);
    TEST_ASSERT_FALSE(usage->isLimitReached());

    usage->recordRequest(20, 80, 100);
    s = usage->getStats();
    TEST_ASSERT_EQUAL(1, s.requestsToday);
    TEST_ASSERT_EQUAL(20, s.promptTokensToday);
    TEST_ASSERT_EQUAL(80, s.responseTokensToday);
    TEST_ASSERT_EQUAL(100, s.totalTokensToday);

    usage->setDailyLimit(1);
    TEST_ASSERT_TRUE(usage->isLimitReached());

    usage->setDailyLimit(1500);
    TEST_ASSERT_FALSE(usage->isLimitReached());
}

void setup() {
    delay(2000);
    UNITY_BEGIN();
    
    // ConfigManager Tests
    RUN_TEST(test_config_defaults);
    RUN_TEST(test_config_sanitization_wifi);
    RUN_TEST(test_config_sanitization_api_key);
    RUN_TEST(test_config_sanitization_quotes);

    // CLI Tests
    RUN_TEST(test_parse_args_simple);
    RUN_TEST(test_parse_args_quotes);
    RUN_TEST(test_parse_args_empty);
    RUN_TEST(test_history_limit);

    // GeminiClient Tests
    RUN_TEST(test_gemini_api_url_build);
    RUN_TEST(test_gemini_request_body_structure);
    RUN_TEST(test_gemini_request_body_system_prompt);
    RUN_TEST(test_gemini_parse_success_response);
    RUN_TEST(test_gemini_parse_error_response);
    RUN_TEST(test_gemini_http_error_descriptions);

    // UsageTracker Tests
    RUN_TEST(test_usage_tracker_limits_and_reset);

    UNITY_END();
}

void loop() {
    delay(100);
}
