/* 
 * pj_getopt entry points
 *
 * modified by Mike Borella <mike_borella@mw.3com.com>
 */

#include <pjlib-util/getopt.h>
#include <pj/string.h>

/* Internal only.  Users should not call this directly.  */
static
int _getopt_internal (int argc, char *const *argv,
                      const char *shortopts,
                      const struct pj_getopt_option *longopts, int *longind,
                      int long_only);

/* pj_getopt_long and pj_getopt_long_only entry points for GNU pj_getopt.
   Copyright (C) 1987,88,89,90,91,92,93,94,96,97 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */


/* Comment out all this code if we are using the GNU C Library, and are not
   actually compiling the library itself.  This code is part of the GNU C
   Library, but also included in many other GNU distributions.  Compiling
   and linking in this code is a waste when using the GNU C library
   (especially if it is a shared library).  Rather than having every GNU
   program understand `configure --with-gnu-libc' and omit the object files,
   it is simpler to just do this in the source for each such file.  */

#   define GETOPT_INTERFACE_VERSION 2


int
pj_getopt_long (int argc, char *const *argv, const char *options, 
             const struct pj_getopt_option *long_options, int *opt_index)
{
  return _getopt_internal (argc, argv, options, long_options, opt_index, 0);
}

/* Like pj_getopt_long, but '-' as well as '--' can indicate a long option.
   If an option that starts with '-' (not '--') doesn't match a long option,
   but does match a short option, it is parsed as a short option
   instead.  */
 
int
pj_getopt (int argc, char * const * argv, const char * optstring)
{
  return _getopt_internal (argc, argv, optstring,
                           (const struct pj_getopt_option *) 0,
                           (int *) 0,
                           0);
}


#define _(msgid)        (msgid)

/* This version of `pj_getopt' appears to the caller like standard Unix `pj_getopt'
   but it behaves differently for the user, since it allows the user
   to intersperse the options with the other arguments.

   As `pj_getopt' works, it permutes the elements of ARGV so that,
   when it is done, all the options precede everything else.  Thus
   all application programs are extended to handle flexible argument order.

   Setting the environment variable POSIXLY_CORRECT disables permutation.
   Then the behavior is completely standard.

   GNU application programs can use a third alternative mode in which
   they can distinguish the relative order of options and other arguments.  */

/* For communication from `pj_getopt' to the caller.
   When `pj_getopt' finds an option that takes an argument,
   the argument value is returned here.
   Also, when `ordering' is RETURN_IN_ORDER,
   each non-option ARGV-element is returned here.  */

char *pj_optarg = NULL;

/* Index in ARGV of the next element to be scanned.
   This is used for communication to and from the caller
   and for communication between successive calls to `pj_getopt'.

   On entry to `pj_getopt', zero means this is the first call; initialize.

   When `pj_getopt' returns -1, this is the index of the first of the
   non-option elements that the caller should itself scan.

   Otherwise, `pj_optind' communicates from one call to the next
   how much of ARGV has been scanned so far.  */

/* 1003.2 says this must be 1 before any call.  */
int pj_optind = 1;

/* Formerly, initialization of pj_getopt depended on pj_optind==0, which
   causes problems with re-calling pj_getopt as programs generally don't
   know that. */

static int __getopt_initialized = 0;

/* The next char to be scanned in the option-element
   in which the last option character we returned was found.
   This allows us to pick up the scan where we left off.

   If this is zero, or a null string, it means resume the scan
   by advancing to the next ARGV-element.  */

static char *nextchar;

/* Set to an option character which was unrecognized.
   This must be initialized on some systems to avoid linking in the
   system's own pj_getopt implementation.  */

int pj_optopt = '?';

/* Describe how to deal with options that follow non-option ARGV-elements.

   If the caller did not specify anything,
   the default is REQUIRE_ORDER if the environment variable
   POSIXLY_CORRECT is defined, PERMUTE otherwise.

   REQUIRE_ORDER means don't recognize them as options;
   stop option processing when the first non-option is seen.
   This is what Unix does.
   This mode of operation is selected by either setting the environment
   variable POSIXLY_CORRECT, or using `+' as the first character
   of the list of option characters.

   PERMUTE is the default.  We permute the contents of ARGV as we scan,
   so that eventually all the non-options are at the end.  This allows options
   to be given in any order, even with programs that were not written to
   expect this.

   RETURN_IN_ORDER is an option available to programs that were written
   to expect options and other ARGV-elements in any order and that care about
   the ordering of the two.  We describe each non-option ARGV-element
   as if it were the argument of an option with character code 1.
   Using `-' as the first character of the list of option characters
   selects this mode of operation.

   The special argument `--' forces an end of option-scanning regardless
   of the value of `ordering'.  In the case of RETURN_IN_ORDER, only
   `--' can cause `pj_getopt' to return -1 with `pj_optind' != ARGC.  */

static enum
{
  REQUIRE_ORDER, PERMUTE, RETURN_IN_ORDER
} ordering;

/* Value of POSIXLY_CORRECT environment variable.  */
static char *posixly_correct;

static char *
my_index (const char *str, int chr)
{
  while (*str)
    {
      if (*str == chr)
        return (char *) str;
      str++;
    }
  return 0;
}


/* Handle permutation of arguments.  */

/* Describe the part of ARGV that contains non-options that have
   been skipped.  `first_nonopt' is the index in ARGV of the first of them;
   `last_nonopt' is the index after the last of them.  */

static int first_nonopt;
static int last_nonopt;

#   define SWAP_FLAGS(ch1, ch2)

/* Exchange two adjacent subsequences of ARGV.
   One subsequence is elements [first_nonopt,last_nonopt)
   which contains all the non-options that have been skipped so far.
   The other is elements [last_nonopt,pj_optind), which contains all
   the options processed since those non-options were skipped.

   `first_nonopt' and `last_nonopt' are relocated so that they describe
   the new indices of the non-options in ARGV after they are moved.  */

static void
exchange (char **argv)
{
  int bottom = first_nonopt;
  int middle = last_nonopt;
  int top = pj_optind;
  char *tem;

  /* Exchange the shorter segment with the far end of the longer segment.
     That puts the shorter segment into the right place.
     It leaves the longer segment in the right place overall,
     but it consists of two parts that need to be swapped next.  */

  while (top > middle && middle > bottom)
    {
      if (top - middle > middle - bottom)
        {
          /* Bottom segment is the short one.  */
          int len = middle - bottom;
          register int i;

          /* Swap it with the top part of the top segment.  */
          for (i = 0; i < len; i++)
            {
              tem = argv[bottom + i];
              argv[bottom + i] = argv[top - (middle - bottom) + i];
              argv[top - (middle - bottom) + i] = tem;
              SWAP_FLAGS (bottom + i, top - (middle - bottom) + i);
            }
          /* Exclude the moved bottom segment from further swapping.  */
          top -= len;
        }
      else
        {
          /* Top segment is the short one.  */
          int len = top - middle;
          register int i;

          /* Swap it with the bottom part of the bottom segment.  */
          for (i = 0; i < len; i++)
            {
              tem = argv[bottom + i];
              argv[bottom + i] = argv[middle + i];
              argv[middle + i] = tem;
              SWAP_FLAGS (bottom + i, middle + i);
            }
          /* Exclude the moved top segment from further swapping.  */
          bottom += len;
        }
    }

  /* Update records for the slots the non-options now occupy.  */

  first_nonopt += (pj_optind - last_nonopt);
  last_nonopt = pj_optind;
}

/* Initialize the internal data when the first call is made.  */

static const char *_getopt_initialize (int argc, char *const *argv, 
                                       const char *optstring)
{
    PJ_UNUSED_ARG(argc);
    PJ_UNUSED_ARG(argv);

  /* Start processing options with ARGV-element 1 (since ARGV-element 0
     is the program name); the sequence of previously skipped
     non-option ARGV-elements is empty.  */

  first_nonopt = last_nonopt = pj_optind;

  nextchar = NULL;

  //posixly_correct = getenv ("POSIXLY_CORRECT");
  posixly_correct = NULL;

  /* Determine how to handle the ordering of options and nonoptions.  */

  if (optstring[0] == '-')
    {
      ordering = RETURN_IN_ORDER;
      ++optstring;
    }
  else if (optstring[0] == '+')
    {
      ordering = REQUIRE_ORDER;
      ++optstring;
    }
  else if (posixly_correct != NULL)
    ordering = REQUIRE_ORDER;
  else
    ordering = PERMUTE;

  return optstring;
}

/* Scan elements of ARGV (whose length is ARGC) for option characters
   given in OPTSTRING.

   If an element of ARGV starts with '-', and is not exactly "-" or "--",
   then it is an option element.  The characters of this element
   (aside from the initial '-') are option characters.  If `pj_getopt'
   is called repeatedly, it returns successively each of the option characters
   from each of the option elements.

   If `pj_getopt' finds another option character, it returns that character,
   updating `pj_optind' and `nextchar' so that the next call to `pj_getopt' can
   resume the scan with the following option character or ARGV-element.

   If there are no more option characters, `pj_getopt' returns -1.
   Then `pj_optind' is the index in ARGV of the first ARGV-element
   that is not an option.  (The ARGV-elements have been permuted
   so that those that are not options now come last.)

   OPTSTRING is a string containing the legitimate option characters.
   If an option character is seen that is not listed in OPTSTRING,
   return '?' after printing an error message.  If you set `pj_opterr' to
   zero, the error message is suppressed but we still return '?'.

   If a char in OPTSTRING is followed by a colon, that means it wants an arg,
   so the following text in the same ARGV-element, or the text of the following
   ARGV-element, is returned in `pj_optarg'.  Two colons mean an option that
   wants an optional arg; if there is text in the current ARGV-element,
   it is returned in `pj_optarg', otherwise `pj_optarg' is set to zero.

   If OPTSTRING starts with `-' or `+', it requests different methods of
   handling the non-option ARGV-elements.
   See the comments about RETURN_IN_ORDER and REQUIRE_ORDER, above.

   Long-named options begin with `--' instead of `-'.
   Their names may be abbreviated as long as the abbreviation is unique
   or is an exact match for some defined option.  If they have an
   argument, it follows the option name in the same ARGV-element, separated
   from the option name by a `=', or else the in next ARGV-element.
   When `pj_getopt' finds a long-named option, it returns 0 if that option's
   `flag' field is nonzero, the value of the option's `val' field
   if the `flag' field is zero.

   The elements of ARGV aren't really const, because we permute them.
   But we pretend they're const in the prototype to be compatible
   with other systems.

   LONGOPTS is a vector of `struct pj_getopt_option' terminated by an
   element containing a name which is zero.

   LONGIND returns the index in LONGOPT of the long-named option found.
   It is only valid when a long-named option has been found by the most
   recent call.

   If LONG_ONLY is nonzero, '-' as well as '--' can introduce
   long-named options.  */

static int
_getopt_internal (int argc, char *const *argv, const char *optstring, 
                  const struct pj_getopt_option *longopts, int *longind, 
                  int long_only)
{
  pj_optarg = NULL;

  if (pj_optind == 0 || !__getopt_initialized)
    {
      if (pj_optind == 0)
        pj_optind = 1;  /* Don't scan ARGV[0], the program name.  */
      optstring = _getopt_initialize (argc, argv, optstring);
      __getopt_initialized = 1;
    }

  /* Test whether ARGV[pj_optind] points to a non-option argument.
     Either it does not have option syntax, or there is an environment flag
     from the shell indicating it is not an option.  The later information
     is only used when the used in the GNU libc.  */
#define NONOPTION_P (argv[pj_optind][0] != '-' || argv[pj_optind][1] == '\0')

  if (nextchar == NULL || *nextchar == '\0')
    {
      /* Advance to the next ARGV-element.  */

      /* Give FIRST_NONOPT & LAST_NONOPT rational values if OPTIND has been
         moved back by the user (who may also have changed the arguments).  */
      if (last_nonopt > pj_optind)
        last_nonopt = pj_optind;
      if (first_nonopt > pj_optind)
        first_nonopt = pj_optind;

      if (ordering == PERMUTE)
        {
          /* If we have just processed some options following some non-options,
             exchange them so that the options come first.  */

          if (first_nonopt != last_nonopt && last_nonopt != pj_optind)
            exchange ((char **) argv);
          else if (last_nonopt != pj_optind)
            first_nonopt = pj_optind;

          /* Skip any additional non-options
             and extend the range of non-options previously skipped.  */

          while (pj_optind < argc && NONOPTION_P)
            pj_optind++;
          last_nonopt = pj_optind;
        }

      /* The special ARGV-element `--' means premature end of options.
         Skip it like a null option,
         then exchange with previous non-options as if it were an option,
         then skip everything else like a non-option.  */

      if (pj_optind != argc && !pj_ansi_strcmp(argv[pj_optind], "--"))
        {
          pj_optind++;

          if (first_nonopt != last_nonopt && last_nonopt != pj_optind)
            exchange ((char **) argv);
          else if (first_nonopt == last_nonopt)
            first_nonopt = pj_optind;
          last_nonopt = argc;

          pj_optind = argc;
        }

      /* If we have done all the ARGV-elements, stop the scan
         and back over any non-options that we skipped and permuted.  */

      if (pj_optind == argc)
        {
          /* Set the next-arg-index to point at the non-options
             that we previously skipped, so the caller will digest them.  */
          if (first_nonopt != last_nonopt)
            pj_optind = first_nonopt;
          return -1;
        }

      /* If we have come to a non-option and did not permute it,
         either stop the scan or describe it to the caller and pass it by.  */

      if (NONOPTION_P)
        {
          if (ordering == REQUIRE_ORDER)
            return -1;
          pj_optarg = argv[pj_optind++];
          return 1;
        }

      /* We have found another option-ARGV-element.
         Skip the initial punctuation.  */

      nextchar = (argv[pj_optind] + 1
                  + (longopts != NULL && argv[pj_optind][1] == '-'));
    }

  /* Decode the current option-ARGV-element.  */

  /* Check whether the ARGV-element is a long option.

     If long_only and the ARGV-element has the form "-f", where f is
     a valid short option, don't consider it an abbreviated form of
     a long option that starts with f.  Otherwise there would be no
     way to give the -f short option.

     On the other hand, if there's a long option "fubar" and
     the ARGV-element is "-fu", do consider that an abbreviation of
     the long option, just like "--fu", and not "-f" with arg "u".

     This distinction seems to be the most useful approach.  */

  if (longopts != NULL
      && (argv[pj_optind][1] == '-'
          || (long_only && (argv[pj_optind][2] || !my_index (optstring, argv[pj_optind][1])))))
    {
      char *nameend;
      const struct pj_getopt_option *p;
      const struct pj_getopt_option *pfound = NULL;
      int exact = 0;
      int ambig = 0;
      int indfound = -1;
      int option_index;

      for (nameend = nextchar; *nameend && *nameend != '='; nameend++)
        /* Do nothing.  */ ;

      /* Test all long options for either exact match
         or abbreviated matches.  */
      for (p = longopts, option_index = 0; p->name; p++, option_index++)
        if (!strncmp (p->name, nextchar, nameend - nextchar))
          {
            if ((unsigned int) (nameend - nextchar)
                == (unsigned int) strlen (p->name))
              {
                /* Exact match found.  */
                pfound = p;
                indfound = option_index;
                exact = 1;
                break;
              }
            else if (pfound == NULL)
              {
                /* First nonexact match found.  */
                pfound = p;
                indfound = option_index;
              }
            else
              /* Second or later nonexact match found.  */
              ambig = 1;
          }

      if (ambig && !exact)
        {
          nextchar += strlen (nextchar);
          pj_optind++;
          pj_optopt = 0;
          return '?';
        }

      if (pfound != NULL)
        {
          option_index = indfound;
          pj_optind++;
          if (*nameend)
            {
              /* Don't test has_arg with >, because some C compilers don't
                 allow it to be used on enums.  */
              if (pfound->has_arg)
                pj_optarg = nameend + 1;
              else
                {
                  nextchar += strlen (nextchar);

                  pj_optopt = pfound->val;
                  return '?';
                }
            }
          else if (pfound->has_arg == 1)
            {
              if (pj_optind < argc)
                pj_optarg = argv[pj_optind++];
              else
                {
                  nextchar += strlen (nextchar);
                  pj_optopt = pfound->val;
                  return optstring[0] == ':' ? ':' : '?';
                }
            }
          nextchar += strlen (nextchar);
          if (longind != NULL)
            *longind = option_index;
          if (pfound->flag)
            {
              *(pfound->flag) = pfound->val;
              return 0;
            }
          return pfound->val;
        }

      /* Can't find it as a long option.  If this is not pj_getopt_long_only,
         or the option starts with '--' or is not a valid short
         option, then it's an error.
         Otherwise interpret it as a short option.  */
      if (!long_only || argv[pj_optind][1] == '-'
          || my_index (optstring, *nextchar) == NULL)
        {
          nextchar = (char *) "";
          pj_optind++;
          pj_optopt = 0;
          return '?';
        }
    }

  /* Look at and handle the next short option-character.  */

  {
    char c = *nextchar++;
    char *temp = my_index (optstring, c);

    /* Increment `pj_optind' when we start to process its last character.  */
    if (*nextchar == '\0')
      ++pj_optind;

    if (temp == NULL || c == ':')
      {
        pj_optopt = c;
        return '?';
      }
    /* Convenience. Treat POSIX -W foo same as long option --foo */
    if (temp[0] == 'W' && temp[1] == ';')
      {
        char *nameend;
        const struct pj_getopt_option *p;
        const struct pj_getopt_option *pfound = NULL;
        int exact = 0;
        int ambig = 0;
        int indfound = 0;
        int option_index;

        /* This is an option that requires an argument.  */
        if (*nextchar != '\0')
          {
            pj_optarg = nextchar;
            /* If we end this ARGV-element by taking the rest as an arg,
               we must advance to the next element now.  */
            pj_optind++;
          }
        else if (pj_optind == argc)
          {
            pj_optopt = c;
            if (optstring[0] == ':')
              c = ':';
            else
              c = '?';
            return c;
          }
        else
          /* We already incremented `pj_optind' once;
             increment it again when taking next ARGV-elt as argument.  */
          pj_optarg = argv[pj_optind++];

        /* pj_optarg is now the argument, see if it's in the
           table of longopts.  */

        for (nextchar = nameend = pj_optarg; *nameend && *nameend != '='; nameend++)
          /* Do nothing.  */ ;

        /* Test all long options for either exact match
           or abbreviated matches.  */
        for (p = longopts, option_index = 0; p && p->name; p++, option_index++)
          if (!strncmp (p->name, nextchar, nameend - nextchar))
            {
              if ((unsigned int) (nameend - nextchar) == strlen (p->name))
                {
                  /* Exact match found.  */
                  pfound = p;
                  indfound = option_index;
                  exact = 1;
                  break;
                }
              else if (pfound == NULL)
                {
                  /* First nonexact match found.  */
                  pfound = p;
                  indfound = option_index;
                }
              else
                /* Second or later nonexact match found.  */
                ambig = 1;
            }
        if (ambig && !exact)
          {
            nextchar += strlen (nextchar);
            pj_optind++;
            return '?';
          }
        if (pfound != NULL)
          {
            option_index = indfound;
            if (*nameend)
              {
                /* Don't test has_arg with >, because some C compilers don't
                   allow it to be used on enums.  */
                if (pfound->has_arg)
                  pj_optarg = nameend + 1;
                else
                  {
                    nextchar += strlen (nextchar);
                    return '?';
                  }
              }
            else if (pfound->has_arg == 1)
              {
                if (pj_optind < argc)
                  pj_optarg = argv[pj_optind++];
                else
                  {
                    nextchar += strlen (nextchar);
                    return optstring[0] == ':' ? ':' : '?';
                  }
              }
            nextchar += strlen (nextchar);
            if (longind != NULL)
              *longind = option_index;
            if (pfound->flag)
              {
                *(pfound->flag) = pfound->val;
                return 0;
              }
            return pfound->val;
          }
          nextchar = NULL;
          return 'W';   /* Let the application handle it.   */
      }
    if (temp[1] == ':')
      {
        if (temp[2] == ':')
          {
            /* This is an option that accepts an argument optionally.  */
            if (*nextchar != '\0')
              {
                pj_optarg = nextchar;
                pj_optind++;
              }
            else
              pj_optarg = NULL;
            nextchar = NULL;
          }
        else
          {
            /* This is an option that requires an argument.  */
            if (*nextchar != '\0')
              {
                pj_optarg = nextchar;
                /* If we end this ARGV-element by taking the rest as an arg,
                   we must advance to the next element now.  */
                pj_optind++;
              }
            else if (pj_optind == argc)
              {
                pj_optopt = c;
                if (optstring[0] == ':')
                  c = ':';
                else
                  c = '?';
              }
            else
              /* We already incremented `pj_optind' once;
                 increment it again when taking next ARGV-elt as argument.  */
              pj_optarg = argv[pj_optind++];
            nextchar = NULL;
          }
      }
    return c;
  }
}

