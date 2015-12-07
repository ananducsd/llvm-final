#!/usr/bin/env sh
# CSE231 Project 1 turnin script
# Version 1.1

pass ()
{
    echo "\033[0;32m[check] $1\033[0m"
}

fail ()
{
    echo "\033[0;31mERROR: Your project is not ready for submission.  $1\033[0m" 1>&2
    exit 1
}

check_env ()
{
    if [ -z "$CSE231ROOT" ]; then
        fail "package.sh must be executed within the cse231 environment.  Please source startenv.sh."
    else
        pass "startenv.sh has been sourced."
    fi
}

check_pwd ()
{
    CWD=`pwd`
    if [ "$CWD" = "$CSE231ROOT" ]; then
        pass "Current directory is \$CSE231ROOT."
    else
        fail "package.sh must be executed from the project root.  Please change directory to \$CSEROOT."
    fi
}

check_tar ()
{
    if [ $? -ne 0 ]; then
        fail "Failed to build cse231-turnin.tar.gz."
    fi
}

check_file ()
{
    FILE=$1
    if [ -f $FILE ]; then
        pass "File $FILE exists."
    else
        fail "File $FILE does not exist.  Please add $FILE and rerun package.sh."
    fi
}

check_compile ()
{
    if make -B --silent llvm > /dev/null 2>&1 ; then
        pass "CSE231.so compiles successfully."
    else
        fail "CSE231.so did not successfully compile."
    fi
}

###################
## Sanity checks ##
###################

check_env
check_pwd
check_file MEMBERS
check_file report.pdf
check_file runscripts/run-section1.sh
check_file runscripts/run-section2.sh
check_file runscripts/run-section3.sh
check_compile

####################
## Create tarball ##
####################

tar cf cse231-proj1.turnin.tar llvm/src/lib/CSE231
check_tar
tar rf cse231-proj1.turnin.tar --exclude=llvm --exclude=*.bc --exclude=*.ll --exclude=*.tar --exclude=*.tar.gz --exclude=*.exe ./* 
check_tar
gzip -f cse231-proj1.turnin.tar
check_tar

check_file cse231-proj1.turnin.tar.gz

echo
echo "**********************************************************"
echo "* IMPORTANT:                                             *"
echo "* Your project has not been submitted yet.  To submit,   *"
echo "* email cse231-proj1.turnin.tar.gz to aleung@cs.ucsd.edu *"
echo "* with subject \"CSE231 Project 1\".                       *"
echo "**********************************************************"
