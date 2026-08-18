#include <Arduino.h>
#include <unity.h>

#define private public
#include "CLI.h"
#include "ConfigManager.h"
#include "NetworkManager.h"
#include "GeminiClient.h"

ConfigManager* cfg;
NetworkManager* net;
GeminiClient* gem;
SerialCLI* cli;

void setUp(void) {
    cfg = new ConfigManager();
    net = new NetworkManager(*cfg);
    gem = new GeminiClient(*cfg);
    cli = new SerialCLI(*cfg, *net, *gem);
}

void tearDown(void) {
    delete cli;
    delete gem;
    delete net;
    delete cfg;
}

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

void setup() {
    delay(2000);
    UNITY_BEGIN();
    RUN_TEST(test_parse_args_simple);
    RUN_TEST(test_parse_args_quotes);
    RUN_TEST(test_parse_args_empty);
    RUN_TEST(test_history_limit);
    UNITY_END();
}

void loop() {
    delay(100);
}
