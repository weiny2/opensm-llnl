#!/bin/sh
#
# This generates a version string which includes recent version as
# specified in correspondent sub project's configure.in file, plus
# git revision abbreviation in the case if sub-project HEAD is different
# from recent tag, plus "-dirty" suffix if local uncommitted changes are
# in the sub project tree.
#

usage()
{
	echo "Usage: $0"
	exit 2
}

cd `dirname $0`

packege=`basename \`pwd\``
conf_file=configure.in
version=`cat $conf_file | sed -ne '/AC_INIT.*.*/s/^AC_INIT.*, \(.*\),.*$/\1/p'`
spec_file=opensm.spec
release=`grep 'define RELEASE' $spec_file | sed -e 's/.*define\sRELEASE\s*\(.*\)\s*/\1/g'`

if [ "$release" == "@RELEASE@" ]; then
	release="unknown"
fi

git diff --quiet $packege-$version-$release..HEAD -- ./ > /dev/null 2>&1
if [ $? -eq 1 ] ; then
	abbr=`git rev-parse --short --verify HEAD 2>/dev/null`
	if [ ! -z "$abbr" ] ; then
		version="${version}_${abbr}"
	fi
else
	version="${version}-${release}"
fi

git diff-index --quiet HEAD -- ./> /dev/null 2>&1
if [ $? -eq 1 ] ; then
	version="${version}_dirty"
fi

echo $version
