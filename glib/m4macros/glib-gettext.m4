# Macro to add for using GNU gettext.
# Ulrich Drepper <drepper@cygnus.com>, 1995, 1996
#
# Modified to never use included libintl. 
# Owen Taylor <otaylor@redhat.com>, 12/15/1998
#
# Major rework to remove unused code
# Owen Taylor <otaylor@redhat.com>, 12/11/2002
#
# This file can be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU Public License
# but which still want to provide support for the GNU gettext functionality.
#

#
# We need this here as well, since someone might use autoconf-2.5x
# to configure GLib then an older version to configure a package
# using AM_GLIB_GNU_GETTEXT
AC_PREREQ(2.53)

dnl
dnl We go to great lengths to make sure that aclocal won't 
dnl try to pull in the installed version of these macros
dnl when running aclocal in the glib directory.
dnl
m4_copy([AC_DEFUN],[glib_DEFUN])
m4_copy([AC_REQUIRE],[glib_REQUIRE])
dnl
dnl At the end, if we're not within glib, we'll define the public
dnl definitions in terms of our private definitions.
dnl

# GLIB_LC_MESSAGES
#--------------------
glib_DEFUN([GLIB_LC_MESSAGES],
  [AC_CHECK_HEADERS([locale.h])
    if test $ac_cv_header_locale_h = yes; then
    AC_CACHE_CHECK([for LC_MESSAGES], am_cv_val_LC_MESSAGES,
      [AC_TRY_LINK([#include <locale.h>], [return LC_MESSAGES],
       am_cv_val_LC_MESSAGES=yes, am_cv_val_LC_MESSAGES=no)])
    if test $am_cv_val_LC_MESSAGES = yes; then
      AC_DEFINE(HAVE_LC_MESSAGES, 1,
        [Define if your <locale.h> file defines LC_MESSAGES.])
    fi
  fi])

# GLIB_PATH_PROG_WITH_TEST
#----------------------------
dnl GLIB_PATH_PROG_WITH_TEST(VARIABLE, PROG-TO-CHECK-FOR,
dnl   TEST-PERFORMED-ON-FOUND_PROGRAM [, VALUE-IF-NOT-FOUND [, PATH]])
glib_DEFUN([GLIB_PATH_PROG_WITH_TEST],
[# Extract the first word of "$2", so it can be a program name with args.
set dummy $2; ac_word=[$]2
AC_MSG_CHECKING([for $ac_word])
AC_CACHE_VAL(ac_cv_path_$1,
[case "[$]$1" in
  /*)
  ac_cv_path_$1="[$]$1" # Let the user override the test with a path.
  ;;
  *)
  IFS="${IFS= 	}"; ac_save_ifs="$IFS"; IFS="${IFS}:"
  for ac_dir in ifelse([$5], , $PATH, [$5]); do
    test -z "$ac_dir" && ac_dir=.
    if test -f $ac_dir/$ac_word; then
      if [$3]; then
	ac_cv_path_$1="$ac_dir/$ac_word"
	break
      fi
    fi
  done
  IFS="$ac_save_ifs"
dnl If no 4th arg is given, leave the cache variable unset,
dnl so AC_PATH_PROGS will keep looking.
ifelse([$4], , , [  test -z "[$]ac_cv_path_$1" && ac_cv_path_$1="$4"
])dnl
  ;;
esac])dnl
$1="$ac_cv_path_$1"
if test ifelse([$4], , [-n "[$]$1"], ["[$]$1" != "$4"]); then
  AC_MSG_RESULT([$]$1)
else
  AC_MSG_RESULT(no)
fi
AC_SUBST($1)dnl
])

# GLIB_WITH_NLS
#-----------------
glib_DEFUN([GLIB_WITH_NLS],
  dnl NLS is obligatory
  [USE_NLS=yes
    AC_SUBST(USE_NLS)

    gt_cv_have_gettext=no

    CATOBJEXT=NONE
    XGETTEXT=:

    AC_CHECK_HEADER(libintl.h,
     [gt_cv_func_dgettext_libintl="no"
      libintl_extra_libs=""

      #
      # First check in libc
      #
      AC_CACHE_CHECK([for dgettext in libc], gt_cv_func_dgettext_libc,
        [AC_TRY_LINK([
#include <libintl.h>
],
          [return (int) dgettext ("","")],
	  gt_cv_func_dgettext_libc=yes,
          gt_cv_func_dgettext_libc=no)
        ])
  
      if test "$gt_cv_func_dgettext_libc" = "yes" ; then
        AC_CHECK_FUNCS(bind_textdomain_codeset)
      fi

      #
      # If we don't have everything we want, check in libintl
      #
      if test "$gt_cv_func_dgettext_libc" != "yes" \
         || test "$ac_cv_func_bind_textdomain_codeset" != "yes" ; then
        
        AC_CHECK_LIB(intl, bindtextdomain,
	    [AC_CHECK_LIB(intl, dgettext,
		          gt_cv_func_dgettext_libintl=yes)])

	if test "$gt_cv_func_dgettext_libintl" != "yes" ; then
	  AC_MSG_CHECKING([if -liconv is needed to use gettext])
	  AC_MSG_RESULT([])
          AC_CHECK_LIB(intl, dcgettext,
		       [gt_cv_func_dgettext_libintl=yes
			libintl_extra_libs=-liconv],
			:,-liconv)
        fi

        #
        # If we found libintl, then check in it for bind_textdomain_codeset();
        # we'll prefer libc if neither have bind_textdomain_codeset(),
        # and both have dgettext
        #
        if test "$gt_cv_func_dgettext_libintl" = "yes" ; then
          glib_save_LIBS="$LIBS"
          LIBS="$LIBS -lintl $libintl_extra_libs"
          unset ac_cv_func_bind_textdomain_codeset
          AC_CHECK_FUNCS(bind_textdomain_codeset)
          LIBS="$glib_save_LIBS"

          if test "$ac_cv_func_bind_textdomain_codeset" = "yes" ; then
            gt_cv_func_dgettext_libc=no
          else
            if test "$gt_cv_func_dgettext_libc" = "yes"; then
              gt_cv_func_dgettext_libintl=no
            fi
          fi
        fi
      fi

      if test "$gt_cv_func_dgettext_libc" = "yes" \
	|| test "$gt_cv_func_dgettext_libintl" = "yes"; then
        gt_cv_have_gettext=yes
      fi
  
      if test "$gt_cv_have_gettext" = "yes"; then
	AC_DEFINE(HAVE_GETTEXT,1,
	  [Define if the GNU gettext() function is already present or preinstalled.])
	GLIB_PATH_PROG_WITH_TEST(MSGFMT, msgfmt,
	  [test -z "`$ac_dir/$ac_word -h 2>&1 | grep 'dv '`"], no)dnl
	if test "$MSGFMT" != "no"; then
	  AC_CHECK_FUNCS(dcgettext)
	  AC_PATH_PROG(GMSGFMT, gmsgfmt, $MSGFMT)
	  GLIB_PATH_PROG_WITH_TEST(XGETTEXT, xgettext,
	    [test -z "`$ac_dir/$ac_word -h 2>&1 | grep '(HELP)'`"], :)
	  AC_TRY_LINK(, [extern int _nl_msg_cat_cntr;
			 return _nl_msg_cat_cntr],
	    [CATOBJEXT=.gmo 
             DATADIRNAME=share],
	    [CATOBJEXT=.mo
             DATADIRNAME=lib])
	  INSTOBJEXT=.mo
	fi
      fi

      LIBS="$glib_save_LIBS"
    ])

    if test "$gt_cv_have_gettext" = "yes" ; then
      AC_DEFINE(ENABLE_NLS, 1,
        [always defined to indicate that i18n is enabled])
    fi

    dnl Test whether we really found GNU xgettext.
    if test "$XGETTEXT" != ":"; then
      dnl If it is not GNU xgettext we define it as : so that the
      dnl Makefiles still can work.
      if $XGETTEXT --omit-header /dev/null 2> /dev/null; then
        : ;
      else
        AC_MSG_RESULT(
	  [found xgettext program is not GNU xgettext; ignore it])
        XGETTEXT=":"
      fi
    fi

    # We need to process the po/ directory.
    POSUB=po

    AC_OUTPUT_COMMANDS(
      [case "$CONFIG_FILES" in *po/Makefile.in*)
        sed -e "/POTFILES =/r po/POTFILES" po/Makefile.in > po/Makefile
      esac])

    dnl These rules are solely for the distribution goal.  While doing this
    dnl we only have to keep exactly one list of the available catalogs
    dnl in configure.in.
    for lang in $ALL_LINGUAS; do
      GMOFILES="$GMOFILES $lang.gmo"
      POFILES="$POFILES $lang.po"
    done

    dnl Make all variables we use known to autoconf.
    AC_SUBST(CATALOGS)
    AC_SUBST(CATOBJEXT)
    AC_SUBST(GMOFILES)
    AC_SUBST(INSTOBJEXT)
    AC_SUBST(INTLLIBS)
    AC_SUBST(PO_IN_DATADIR_TRUE)
    AC_SUBST(PO_IN_DATADIR_FALSE)
    AC_SUBST(POFILES)
    AC_SUBST(POSUB)
  ])

# AM_GLIB_GNU_GETTEXT
# -------------------
# Do checks necessary for use of gettext. If a suitable implementation 
# of gettext is found in either in libintl or in the C library,
# it will set INTLLIBS to the libraries needed for use of gettext
# and AC_DEFINE() HAVE_GETTEXT and ENABLE_NLS. (The shell variable
# gt_cv_have_gettext will be set to "yes".) It will also call AC_SUBST()
# on various variables needed by the Makefile.in.in installed by 
# glib-gettextize.
dnl
glib_DEFUN(GLIB_GNU_GETTEXT,
  [AC_REQUIRE([AC_PROG_CC])dnl
   AC_REQUIRE([AC_HEADER_STDC])dnl
   
   GLIB_LC_MESSAGES
   GLIB_WITH_NLS

   if test "$gt_cv_have_gettext" = "yes"; then
     if test "x$ALL_LINGUAS" = "x"; then
       LINGUAS=
     else
       AC_MSG_CHECKING(for catalogs to be installed)
       NEW_LINGUAS=
       for lang in ${LINGUAS=$ALL_LINGUAS}; do
         case "$ALL_LINGUAS" in
          *$lang*) NEW_LINGUAS="$NEW_LINGUAS $lang" ;;
         esac
       done
       LINGUAS=$NEW_LINGUAS
       AC_MSG_RESULT($LINGUAS)
     fi

     dnl Construct list of names of catalog files to be constructed.
     if test -n "$LINGUAS"; then
       for lang in $LINGUAS; do CATALOGS="$CATALOGS $lang$CATOBJEXT"; done
     fi
   fi

   dnl If the AC_CONFIG_AUX_DIR macro for autoconf is used we possibly
   dnl find the mkinstalldirs script in another subdir but ($top_srcdir).
   dnl Try to locate is.
   MKINSTALLDIRS=
   if test -n "$ac_aux_dir"; then
     MKINSTALLDIRS="$ac_aux_dir/mkinstalldirs"
   fi
   if test -z "$MKINSTALLDIRS"; then
     MKINSTALLDIRS="\$(top_srcdir)/mkinstalldirs"
   fi
   AC_SUBST(MKINSTALLDIRS)

   dnl Generate list of files to be processed by xgettext which will
   dnl be included in po/Makefile.
   test -d po || mkdir po
   if test "x$srcdir" != "x."; then
     if test "x`echo $srcdir | sed 's@/.*@@'`" = "x"; then
       posrcprefix="$srcdir/"
     else
       posrcprefix="../$srcdir/"
     fi
   else
     posrcprefix="../"
   fi
   rm -f po/POTFILES
   sed -e "/^#/d" -e "/^\$/d" -e "s,.*,	$posrcprefix& \\\\," -e "\$s/\(.*\) \\\\/\1/" \
	< $srcdir/po/POTFILES.in > po/POTFILES
  ])

# AM_GLIB_DEFINE_LOCALEDIR(VARIABLE)
# -------------------------------
# Define VARIABLE to the location where catalog files will
# be installed by po/Makefile.
glib_DEFUN(GLIB_DEFINE_LOCALEDIR,
[glib_REQUIRE([GLIB_GNU_GETTEXT])dnl
glib_save_prefix="$prefix"
test "x$prefix" = xNONE && prefix=$ac_default_prefix
if test "x$CATOBJEXT" = "x.mo" ; then
  localedir=`eval echo "${libdir}/locale"`
else
  localedir=`eval echo "${datadir}/locale"`
fi
prefix="$glib_save_prefix"
AC_DEFINE_UNQUOTED($1, "$localedir",
  [Define the location where the catalogs will be installed])
])

dnl
dnl Now the definitions that aclocal will find
dnl
ifdef(glib_configure_in,[],[
AC_DEFUN(AM_GLIB_GNU_GETTEXT,[GLIB_GNU_GETTEXT($@)])
AC_DEFUN(AM_GLIB_DEFINE_LOCALEDIR,[GLIB_DEFINE_LOCALEDIR($@)])
])dnl
