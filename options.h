#pragma once
#include <string>
#include <stdexcept>
#include <map>
#include <ctype.h>
#include <string.h>

typedef std::map<char, std::string> OptMap;

/**
 * get_options - Portable getopt-style options parsing in C++. Useful
 * for non-posix based platforms such as Windows that do not have getopt.
 *
 * @param argc - Arg count passed into main.
 * @param argv - Args array passed into main.
 * @param opt_spec - Getopt style options list, e.g. "hvi:o:"
 *
 * @returns OptMap, a map of character options to string values.
 *
 * @throws std::invalid_argument if an invalid option is found or an option is
 * missing an argument.
 */ 
OptMap get_options(int argc, char* argv[], const char* opt_spec)
{
    char opt_char;
    bool has_arg = false;
    OptMap options;
    std::string missing_arg_msg("Missing argument for option -");

    for (int i = 0; i < argc; i++)
    {
        // Options must be a single character after the hyphen in the range [a-zA-Z].
        if (argv[i][0] == '-' && strlen(argv[i]) == 2 && !isdigit(argv[i][1]))
        {
            // Previous argument was missing its value.
            if (has_arg)
                throw std::invalid_argument(missing_arg_msg + opt_char);

            opt_char = argv[i][1];

            // Search for the option in opt_spec
            int j = 0;
            while (opt_spec[j] != opt_char && opt_spec[j] != 0)
            {
                j++;
            }
            if (opt_spec[j] == opt_char)
            {
                has_arg = (opt_spec[j + 1] == ':');
                if (!has_arg)
                    options[opt_char] = "1";
            }
            else
            {
                throw std::invalid_argument(std::string("Invalid option: -") + opt_char);
            }
        }
        else if (has_arg)
        {
            options[opt_char] = argv[i];
            has_arg = false;
        }
    }

    // Last option was missing its value.
    if (has_arg)
        throw std::invalid_argument(missing_arg_msg + opt_char);
    return options;
}