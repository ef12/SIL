#include "unity.h"

#include "hello_output.h"

#include <stdbool.h>
#include <stdio.h>

#ifdef _WIN32
#include <io.h>
#define FD_DUP _dup
#define FD_DUP2 _dup2
#define FD_FILENO _fileno
#define FD_CLOSE _close
#else
#include <unistd.h>
#define FD_DUP dup
#define FD_DUP2 dup2
#define FD_FILENO fileno
#define FD_CLOSE close
#endif

void setUp(void) {}

void tearDown(void) {}

void test_hello_output_write_line_writes_expected_message(void) {
  const char *capture_path = "hello_output_capture.txt";
  FILE *capture = fopen(capture_path, "w+");
  int saved_stdout_fd;
  char output_buffer[128] = {0};
  bool read_ok;

  TEST_ASSERT_NOT_NULL_MESSAGE(capture,
                               "Failed to create capture stream for stdout");

  saved_stdout_fd = FD_DUP(FD_FILENO(stdout));
  if (saved_stdout_fd < 0) {
    fclose(capture);
    remove(capture_path);
    TEST_FAIL_MESSAGE("Failed to duplicate stdout file descriptor");
  }

  if (FD_DUP2(FD_FILENO(capture), FD_FILENO(stdout)) < 0) {
    FD_CLOSE(saved_stdout_fd);
    fclose(capture);
    remove(capture_path);
    TEST_FAIL_MESSAGE("Failed to redirect stdout to capture stream");
  }

  hello_output_write_line("Hello, World!");

  fflush(stdout);
  rewind(capture);
  read_ok = (fgets(output_buffer, sizeof(output_buffer), capture) != NULL);

  FD_DUP2(saved_stdout_fd, FD_FILENO(stdout));
  FD_CLOSE(saved_stdout_fd);
  fclose(capture);
  remove(capture_path);

  TEST_ASSERT_TRUE_MESSAGE(
      read_ok, "No output was captured from hello_output_write_line");
  TEST_ASSERT_EQUAL_STRING("Hello, World!\n", output_buffer);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_hello_output_write_line_writes_expected_message);
  return UNITY_END();
}
