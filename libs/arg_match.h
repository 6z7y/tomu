#ifndef ARG_MATCH_H
#define ARG_MATCH_H

#include <string.h>

// opt = option
// opts = options
// val = value
// vals = values

// fn for pop the opt from argv
void arg_pop1(int *argc, char *argv[], int index)
{
  for (int i=index; i<*argc-1; i++) argv[i] = argv[i+1];
  *argc -= 1;
  argv[*argc] = NULL;
}

// fn for pop the opt and value from argv
void arg_pop2(int *argc, char *argv[], int index)
{
  int remaining = *argc - index - 2;
  for (int m=0; m<remaining; m++) argv[index + m] = argv[index + m + 2];
  *argc -= 2;
  argv[*argc] = NULL;
}

// fn for checking for option
//
// returns:
// 1 = there
// 0 = not there
int arg_check_opt(int argc, char *argv[], const char *opt)
{
  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i], opt)) return 1;
  }
  return 0;
}

// fn for checking for options
//
// returns:
// 1 = there
// 0 = not there
int arg_check_opts(int argc, char *argv[], const char *opts[])
{
  for (int i=1; i<argc; i++) {
    for (int j=0; opts[j] != NULL; j++) {
      if (!strcmp(argv[i], opts[j])) return 1;
    }
  }
  return 0;
}

// fn for checking for option with value
//
// returns:
// 1 = there
// 0 = not there
int arg_check_opt_with_value(int argc, char *argv[], const char *opt, const char *value)
{
  for (int i=1; i<argc; i++) {
    if (!strcmp(argv[i], opt)) {
      if (i + 1 >= argc) return 0;
      if (!strcmp(argv[i+1], value)) return 1;
    }
  }
  return 0;
}

// fn for checking for options with values
//
// returns:
// 1 = there
// 0 = not there
int arg_check_opts_with_values(int argc, char *argv[], const char *opts[], const char *values[])
{
  for (int i=1; i<argc; i++) {
    for (int j=0; opts[j] != NULL; j++) {
      if (!strcmp(argv[i], opts[j])) {
        if (i + 1 >= argc) return 0;
        for (int k=0; values[k] != NULL; k++) {
          if (!strcmp(argv[i+1], values[k])) {
            return 1;
          }
        }
      }
    }
  }
  return 0;
}



// fn for match option with argv then pop matched
//
// returns:
// 1 = matched
// -2 = not matched
int arg_match_opt(int *argc, char *argv[], const char *opt)
{
  for (int i=1; i<*argc; i++) {
    if (!strcmp(argv[i], opt)) {
      arg_pop1(argc, argv, i); 
      return 1;
    }
  }
  return -2;
}

// fn for match options with argv then pop matched
// options array must end NULL
//
// returns:
// 1 = matched
// -1 = not matched
int arg_match_opts(int *argc, char *argv[], const char *opts[])
{
  for (int i=1; i<*argc; i++) {
    for (int j=0; opts[j] != NULL; j++) {
      if (!strcmp(argv[i], opts[j])) { arg_pop1(argc, argv, i); return 1; }
    }
  }
  return -1;
}

// fn for options with values then pop matched
//
// returns:
// 1 = matched
// -1 = not matched or value missed
int arg_match_opt_with_value(int *argc, char *argv[], const char *opt, const char *value)
{
  for (int i=1; i<*argc; i++) {
    if (!strcmp(argv[i], opt)) {
      if (i + 1 >= *argc) return -1;
      if (!strcmp(argv[i+1], value)) {
        arg_pop2(argc, argv, i);
        return 1;
      }
    }
  }
  return -1;
}

// fn for options with values then pop matched
// options and values arrays must end NULL
//
// returns:
// 0... = matched, returns index of matched value
// -1 = opt matched but not matched with val
// -2 = value missed
int arg_match_opts_with_values(int *argc, char *argv[], const char *opts[], const char *values[])
{
  for (int i=1; i<*argc; i++) {
    for (int j=0; opts[j] != NULL; j++) {
      if (!strcmp(argv[i], opts[j])) {
        if (i + 1 >= *argc) return -2;
        for (int k=0; values[k] != NULL; k++) {
          if (!strcmp(argv[i+1], values[k])) {
            arg_pop2(argc, argv, i);
            return k;
          }
        }
      }
    }
  }
  return -1;
}

#endif
