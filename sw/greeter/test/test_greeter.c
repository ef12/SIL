#include "unity.h"

#include "Mockhello_output.h"
#include "greeter.h"

void setUp(void)
{
}

void tearDown(void)
{
}

void test_greeter_get_message_returns_expected_text(void)
{
  TEST_ASSERT_EQUAL_STRING("Hello, World!", greeter_get_message());
}

void test_greeter_run_writes_expected_message(void)
{
  hello_output_write_line_Expect("Hello, World!");

  greeter_run();
}
