#include "test/unittest.h"
#include "util/logging.h"
#include "util/path_compat.h"
#include "util/stringutils.h"

#include "configlib/configlib.h"

#include "configlib/types/bool.h"
#include "configlib/types/deprecated.h"
#include "configlib/types/enum.h"
#include "configlib/types/path.h"
#include "configlib/types/sscanf.h"
#include "configlib/types/string_cvt.h"

#include <stdarg.h>

enum config_key {
    CONFIG_SOME_INT,
    CONFIG_EMPTY_ARRAY,
    CONFIG_STRING_ARRAY,
    CONFIG_INT_ARRAY,
    CONFIG_LONG_ARRAY,
    CONFIG_INCLUDED_INT,
    CONFIG_DEFAULT_STRING,
    CONFIG_DEFAULT_INT_ARRAY,
    CONFIG_DEFAULT_EMPTY_ARRAY,
    CONFIG_STRING_ARRAY_CHARS,
    CONFIG_SOME_BOOL_TRUE,
    CONFIG_SOME_BOOL_FALSE,
    CONFIG_ENV_VAR_INT,
    CONFIG_ENV_VAR_STRING,
    CONFIG_ENV_VAR_EMPTY,
    CONFIG_FILE,
    CONFIG_DIR,
    CONFIG_NULL_STRING,
    CONFIG_DEFAULT_NULL_STRING,
    CONFIG_LOG_LEVEL,
    CONFIG_DEPRECATED,

    CONFIG_NUM_OPTIONS
};

CONFIG_GET_HELPER_DEREFERENCE(CONFIG_SOME_INT, int)
CONFIG_GET_ARRAY_HELPER(CONFIG_EMPTY_ARRAY, char)
CONFIG_GET_ARRAY_HELPER(CONFIG_STRING_ARRAY, char)
CONFIG_GET_ARRAY_HELPER_DEREFERENCE(CONFIG_INT_ARRAY, int)
CONFIG_GET_ARRAY_HELPER(CONFIG_LONG_ARRAY, char)
CONFIG_GET_HELPER_DEREFERENCE(CONFIG_INCLUDED_INT, int)
CONFIG_GET_HELPER(CONFIG_DEFAULT_STRING, char)
CONFIG_GET_ARRAY_HELPER_DEREFERENCE(CONFIG_DEFAULT_INT_ARRAY, int)
CONFIG_GET_ARRAY_HELPER_DEREFERENCE(CONFIG_DEFAULT_EMPTY_ARRAY, int)
CONFIG_GET_ARRAY_HELPER(CONFIG_STRING_ARRAY_CHARS, char)
CONFIG_GET_HELPER_DEREFERENCE(CONFIG_SOME_BOOL_TRUE, bool)
CONFIG_GET_HELPER_DEREFERENCE(CONFIG_SOME_BOOL_FALSE, bool)
CONFIG_GET_HELPER_DEREFERENCE(CONFIG_ENV_VAR_INT, int)
CONFIG_GET_HELPER(CONFIG_ENV_VAR_STRING, char)
CONFIG_GET_HELPER(CONFIG_ENV_VAR_EMPTY, char)
CONFIG_GET_HELPER(CONFIG_FILE, char)
CONFIG_GET_HELPER(CONFIG_DIR, char)
CONFIG_GET_HELPER(CONFIG_NULL_STRING, char)
CONFIG_GET_HELPER(CONFIG_DEFAULT_NULL_STRING, char)
CONFIG_GET_HELPER_DEREFERENCE(CONFIG_LOG_LEVEL, int)


static bool stringarray_validator(
    const struct config_context *context,
    void *usr_arg,
    void const *const *input,
    size_t num_items)
{
    if (usr_arg != (void *)1)
    {
        LOG(LOG_ERR, "usr_arg must be (void *)1");
        return false;
    }

    if (num_items != 3)
    {
        config_message(context, LOG_ERR,
                       "must have 3 items, but had %zu", num_items);
        return false;
    }

    if (strcmp((const char *)input[0], "foo bar") != 0)
    {
        config_message(context, LOG_ERR,
                       "first element must be \"foo bar\", but was \"%s\"",
                       (const char *)input[0]);
        return false;
    }

    return true;
}


static const struct config_option CONFIG_OPTIONS[] = {
    // CONFIG_SOME_INT
    {
        "SomeInt",
        false,
        config_type_sscanf_converter, &config_type_sscanf_arg_int,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_EMPTY_ARRAY
    {
        "EmptyArray",
        true,
        config_type_string_converter, &config_type_string_arg_mandatory,
        NULL, NULL,
        free,
        NULL, NULL,
        "foo bar"},

    // CONFIG_STRING_ARRAY
    {
        "StringArray",
        true,
        config_type_string_converter, &config_type_string_arg_mandatory,
        NULL, NULL,
        free,
        stringarray_validator, (void *)1,
        "\"foo bar\" 1 3"},

    // CONFIG_INT_ARRAY
    {
        "IntArray",
        true,
        config_type_sscanf_converter, &config_type_sscanf_arg_int,
        NULL, NULL,
        free,
        NULL, NULL,
        "1 2 3"},

    // CONFIG_LONG_ARRAY
    {
        "LongArray",
        true,
        config_type_string_converter, &config_type_string_arg_mandatory,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_INCLUDED_INT
    {
        "IncludedInt",
        false,
        config_type_sscanf_converter, &config_type_sscanf_arg_int,
        NULL, NULL,
        free,
        NULL, NULL,
        "7"},

    // CONFIG_DEFAULT_STRING
    {
        "DefaultString",
        false,
        config_type_string_converter, &config_type_string_arg_optional,
        NULL, NULL,
        free,
        NULL, NULL,
        "this-is-the-default"},

    // CONFIG_DEFAULT_INT_ARRAY
    {
        "DefaultIntArray",
        true,
        config_type_sscanf_converter, &config_type_sscanf_arg_int,
        NULL, NULL,
        free,
        NULL, NULL,
        "-1 0 1"},

    // CONFIG_DEFAULT_EMPTY_ARRAY
    {
        "DefaultEmptyArray",
        true,
        config_type_sscanf_converter, &config_type_sscanf_arg_int,
        NULL, NULL,
        free,
        NULL, NULL,
        ""},

    // CONFIG_STRING_ARRAY_CHARS
    {
        "StringArrayChars",
        true,
        config_type_string_converter, &config_type_string_arg_mandatory,
        NULL, NULL,
        free,
        NULL, NULL,
        "foo \"\\\"\" \"'\" \"\\\\\" \"\\$\" \"\t\" \" \" \"#\" \"\\n\" \"\\r\" \"\\t\""},

    // CONFIG_SOME_BOOL_TRUE
    {
        "SomeBoolTrue",
        false,
        config_type_bool_converter, NULL,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_SOME_BOOL_FALSE
    {
        "SomeBoolFalse",
        false,
        config_type_bool_converter, NULL,
        NULL, NULL,
        free,
        NULL, NULL,
        "True"},

    // CONFIG_ENV_VAR_INT
    {
        "EnvVarInt",
        false,
        config_type_sscanf_converter, &config_type_sscanf_arg_int,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_ENV_VAR_STRING
    {
        "EnvVarString",
        false,
        config_type_string_converter, &config_type_string_arg_optional,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_ENV_VAR_EMPTY
    {
        "EnvVarEmpty",
        false,
        config_type_string_converter, &config_type_string_arg_mandatory,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_FILE
    {
        "File",
        false,
        config_type_path_converter, NULL,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_DIR
    {
        "Dir",
        false,
        config_type_path_converter, NULL,
        NULL, NULL,
        free,
        NULL, NULL,
        NULL},

    // CONFIG_NULL_STRING
    {
        "NullString",
        false,
        config_type_string_converter, &config_type_string_arg_optional,
        NULL, NULL,
        free,
        NULL, NULL,
        "\"non-null default\""},

    // CONFIG_DEFAULT_NULL_STRING
    {
        "DefaultNullString",
        false,
        config_type_string_converter, &config_type_string_arg_optional,
        NULL, NULL,
        free,
        NULL, NULL,
        ""},

    // CONFIG_LOG_LEVEL
    {
        "LogLevel",
        false,
        config_type_enum_converter, &config_type_enum_arg_log_level,
        NULL, NULL,
        config_type_enum_free,
        NULL, NULL,
        NULL},

    // CONFIG_DEPRECATED
    {
        "Deprecated",
        false,
        config_type_deprecated_converter, NULL,
        NULL, NULL,
        free,
        NULL, NULL,
        "",
    },
};


static char logbuf[4096] = {0};
static size_t logbuf_offset = 0;
static log_custom_backend_logger logger;
void
logger(
    int priority,
    const char *restrict ident,
    const char *restrict format,
    ...)
{
    (void)ident;

    logbuf_offset += xsnprintf(
        logbuf + logbuf_offset, sizeof(logbuf) - logbuf_offset,
        "%s: ", LOG_LEVEL_TEXT[priority]);

    va_list arg;
    va_start(arg, format);
    logbuf_offset += xvsnprintf(
        logbuf + logbuf_offset, sizeof(logbuf) - logbuf_offset,
        format, arg);
    va_end(arg);

    logbuf_offset += xsnprintf(
        logbuf + logbuf_offset, sizeof(logbuf) - logbuf_offset,
        "\n");
}


static bool test_config(
    const char *conf_file)
{
    bool ret;
    char *path;

    if (setenv("ENV_VAR_INT", "0xfe0f", 1) != 0)
    {
        perror("setenv(ENV_VAR_INT)");
        return false;
    }

    if (setenv("ENV_VAR_STRING", "foo bar \" # \\n ${ENV_VAR_STRING}", 1) != 0)
    {
        perror("setenv(ENV_VAR_STRING)");
        return false;
    }

    if (unsetenv("ENV_VAR_UNSET") != 0)
    {
        perror("unsetenv(ENV_VAR_UNSET)");
        return false;
    }

    log_custom_backend.log = &logger;
    CLOSE_LOG();
    OPEN_LOG("test", LOG_CUSTOM_BACKEND);

    ret = config_load(CONFIG_NUM_OPTIONS, CONFIG_OPTIONS, conf_file, NULL);
    TEST_BOOL(ret, true);

    CLOSE_LOG();
    OPEN_LOG("config-test-good", LOG_USER);

    char expected_logbuf[4096];
    xsnprintf(
        expected_logbuf, sizeof(expected_logbuf),
        "NOTICE: %s:28: duplicate option overwriting previous value\n"
        "WARN: %s:38: variable ${ENV_VAR_UNSET} not found, using the empty"
        " string instead\n"
        "WARN: %s:38: variable ${ENV_VAR_UNSET} not found, using the empty"
        " string instead\n"
        "WARN: %s:38: variable ${ENV_VAR_UNSET} not found, using the empty"
        " string instead\n"
        "WARN: %s:38: variable ${ENV_VAR_UNSET} not found, using the empty"
        " string instead\n"
        "WARN: %s:47: this option is deprecated and has no effect\n"
        , conf_file, conf_file, conf_file, conf_file, conf_file, conf_file);

    TEST_STR(logbuf, ==, expected_logbuf);

    TEST(int, "%d", CONFIG_SOME_INT_get(), ==, -5);

    TEST(size_t, "%zu", config_get_length(CONFIG_EMPTY_ARRAY), ==, 0);

    TEST(size_t, "%zu", config_get_length(CONFIG_STRING_ARRAY), ==, 3);
    TEST_STR(CONFIG_STRING_ARRAY_get(0), ==, "foo bar");
    TEST_STR(CONFIG_STRING_ARRAY_get(1), ==, "quux");
    TEST_STR(CONFIG_STRING_ARRAY_get(2), ==, "blah # this is not a comment");

    TEST(size_t, "%zu", config_get_length(CONFIG_INT_ARRAY), ==, 4);
    TEST(int, "%d", CONFIG_INT_ARRAY_get(0), ==, 8);
    TEST(int, "%d", CONFIG_INT_ARRAY_get(1), ==, -3);
    TEST(int, "%d", CONFIG_INT_ARRAY_get(2), ==, 40);
    TEST(int, "%d", CONFIG_INT_ARRAY_get(3), ==, 0xff);

    TEST(size_t, "%zu", config_get_length(CONFIG_LONG_ARRAY), ==, 5);
    TEST_STR(CONFIG_LONG_ARRAY_get(0), ==, "foo");
    TEST_STR(CONFIG_LONG_ARRAY_get(1), ==, "bar");
    TEST_STR(CONFIG_LONG_ARRAY_get(2), ==, "quux");
    TEST_STR(CONFIG_LONG_ARRAY_get(3), ==, "baz");
    TEST_STR(CONFIG_LONG_ARRAY_get(4), ==, "something else");

    TEST(int, "%d", CONFIG_INCLUDED_INT_get(), ==, 42);

    TEST_STR((const char *)config_get(CONFIG_DEFAULT_STRING), ==,
             "this-is-the-default");

    TEST(size_t, "%zu", config_get_length(CONFIG_DEFAULT_INT_ARRAY), ==, 3);
    TEST(int, "%d", CONFIG_DEFAULT_INT_ARRAY_get(0), ==, -1);
    TEST(int, "%d", CONFIG_DEFAULT_INT_ARRAY_get(1), ==, 0);
    TEST(int, "%d", CONFIG_DEFAULT_INT_ARRAY_get(2), ==, 1);

    TEST(size_t, "%zu", config_get_length(CONFIG_DEFAULT_EMPTY_ARRAY), ==, 0);

    TEST(size_t, "%zu", config_get_length(CONFIG_STRING_ARRAY_CHARS), ==, 10);
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(0), ==, "\"");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(1), ==, "'");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(2), ==, "\\");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(3), ==, "$");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(4), ==, "\t");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(5), ==, " ");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(6), ==, "#");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(7), ==, "\n");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(8), ==, "\r");
    TEST_STR(CONFIG_STRING_ARRAY_CHARS_get(9), ==, "\t");

    TEST_BOOL(CONFIG_SOME_BOOL_TRUE_get(), true);

    TEST_BOOL(CONFIG_SOME_BOOL_FALSE_get(), false);

    TEST(int, "%d", CONFIG_ENV_VAR_INT_get(), ==, 0xfe0f);

    TEST_STR(CONFIG_ENV_VAR_STRING_get(), ==,
             "/foo bar \" # \\n ${ENV_VAR_STRING}/");

    TEST_STR(CONFIG_ENV_VAR_EMPTY_get(), ==, " barfoo  quux ");

    path = realpath(ABS_TOP_SRCDIR "/lib/configlib/tests/good.conf", NULL);
    if (path == NULL)
    {
        LOG(LOG_ERR, "out of memory");
        return false;
    }
    TEST_STR(CONFIG_FILE_get(), ==, path);
    free(path);

    path = realpath(ABS_TOP_SRCDIR "/lib/configlib", NULL);
    if (path == NULL)
    {
        LOG(LOG_ERR, "out of memory");
        return false;
    }
    TEST_STR(CONFIG_DIR_get(), ==, path);
    free(path);

    TEST(const char *, "%s", CONFIG_NULL_STRING_get(), ==, NULL);

    TEST(const char *, "%s", CONFIG_DEFAULT_NULL_STRING_get(), ==, NULL);

    TEST(int, "%d", CONFIG_LOG_LEVEL_get(), ==, LOG_ALERT);

    config_unload();

    return true;
}


int main(
    int argc,
    char **argv)
{
    int retval = EXIT_SUCCESS;

    (void)argc;
    (void)argv;

    OPEN_LOG("config-test-good", LOG_USER);

    if (!test_config(ABS_TOP_SRCDIR "/lib/configlib/tests/good.conf"))
    {
        retval = EXIT_FAILURE;
    }

    CLOSE_LOG();

    return retval;
}
