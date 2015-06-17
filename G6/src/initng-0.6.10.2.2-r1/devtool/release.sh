#!/bin/sh
#
# Written by Bruno Prémont <bonbons67@internet.lu>
#
# Small script to branch and release a package

PACKAGE=ifiles
INITNG_REPO=https://svn.initng.org/${PACKAGE}

# Allow replacing svn by ENV for testing
[ -z "${SVN}" ] && SVN=`which svn`

URL=`svn info . | grep URL`
URL=${URL:5}

#
# Determine if a directory exists in HEAD on svn
#
function svnexists() {
	echo "Checking for '${1}'"
	REVISION=`svn info "${1}" | grep '^Revision' 2> /dev/null`
	if [ -z "${REVISION}" ]; then
		return 1 # Did not exist
	else
		return 0 # OK, found
	fi
}

#
# Determine local revision number
#
function nopending() {
	if svn st | grep -v '^?' > /dev/null 2> /dev/null; then
		return 1 # There are uncommited local changes!
	else
		return 0 # No local changes left :)
	fi
}

#
# Helper function to echo something and exit
#
function die() {
	echo "[FAIL] " $@
	exit 1
}

#
# Helper function to request comfirmation before executing
# branch or release
#
function confirm() {
	MESSAGE=$1
	DEFAULT=$2
	[[ -z "${DEFAULT}" ]] && DEFAULT="yes"
	echo -n "${MESSAGE}" "[${DEFAULT}]: "
	read RSP
	[[ -z "${RSP}" ]] && RSP=${DEFAULT}
	if [ "${RSP}" == "yes" ]; then
		return 0
	else
		return 1
	fi
}

case "$1" in
	branch)
		VERSION=$2
		[ -n "${VERSION}" ] || die "You need to provide version"

		if [ "`basename ${URL}`" == "trunk" ]; then
			echo "Checking for consistency..."
			svnexists "${INITNG_REPO}/branches/${VERSION}" && die "Branch for version '${VERSION}' already exists"

			# Check that there are no pending local changes
			nopending || die "Can't continue because you have uncommited local changes"

			REV=`svn info . | grep '^Revision' | sed 's/^Revision: //' 2> /dev/null`

			# Run some distcheck before we branch
			autoreconf -vfi || die "Failed to run autoreconf! BAD BAD BAD"

			./configure || die "Failed to run ./configure"

			echo "Checking for common packaging errors..."
			make distcheck || die "Failed to run make distcheck"
			
			confirm "Ready for branching ${PACKAGE} to version '${VERSION}'?" || exit 1

			echo "Creating branch (of revision ${REV}) on SVN server..."
			${SVN} cp -m "Branching for future release of ${PACKAGE}-${VERSION}" -r "${REV}" \
				"${INITNG_REPO}/trunk" "${INITNG_REPO}/branches/${VERSION}"

			echo "Switching working copy to branch..."
			${SVN} switch "${INITNG_REPO}/branches/${VERSION}" .

			echo "Updating version numbers..."
			sed -i "s/AC_INIT(\[${PACKAGE}\],\[.*\])/AC_INIT(\[${PACKAGE}\],\[${VERSION}_pre\])/" configure.ac
			${SVN} commit -m 'Updating version numbers' configure.ac

			if confirm "Do you want pre-release tag '${VERSION}_pre' to be created?" "no"; then
				# Optional: tag the pre-release
				${SVN} cp -m "Tagging pre-release of ${PACKAGE}-${VERSION}" -r "${REV}" \
					"${INITNG_REPO}/branches/${VERSION}" "${INITNG_REPO}/tags/${VERSION}_pre"
			fi

			echo "Generating pre-release tarball..."
			
			make dist-gzip || die "Failed to create distribution archive! BAD BAD BAD"
			make dist-bzip2 || die "Failed to create distribution archive! BAD BAD BAD"
			echo
			echo "Done, please upload tarball '${PACKAGE}-${VERSION}_pre.tar.bz2' to finaly pre-release."
		else
			echo "Sorry, can't branch for pre-release, you're not in trunk"
		fi
		;;
	release)
		VERSION=`basename ${URL}`
		if [[ "`basename $(dirname ${URL})`" == "branches" ]]; then
			svnexists "${INITNG_REPO}/tags/${VERSION}" && die "Release tag for version '${VERSION}' already exist"

			echo "Preparing for releasing ${PACKAGE}-${VERSION}"
			nopending || die "Can't continue because you have uncommited local changes"

			# Run some distcheck before we release
			autoreconf -vfi || die "Failed to run autoreconf! BAD BAD BAD"

			./configure || die "Failed to run ./configure"

			echo "Checking for common packaging errors..."
			make distcheck || die "Failed to run make distcheck"
			
			confirm "Ready for releasing ${PACKAGE} as version '${VERSION}'?" || exit 1

			echo "Updating version numbers..."
			sed -i "s/AC_INIT(\[${PACKAGE}\],\[.*\])/AC_INIT(\[${PACKAGE}\],\[${VERSION}\])/" configure.ac
			[ -x "${EDITOR}" ] || die "Failed to find ${EDITOR} to update NEWS"
			${EDITOR} NEWS
			${SVN} commit -m 'Updating version numbers' configure.ac NEWS

			echo "Tagging the release..."
			${SVN} cp -m "Tagging release of ${PACKAGE}-${VERSION}" -r "${REV}" \
				"${INITNG_REPO}/branches/${VERSION}" "${INITNG_REPO}/tags/${VERSION}"

			echo "Generating release tarball..."
			
			make dist-gzip || die "Failed to create distribution archive! BAD BAD BAD"
			make dist-bzip2 || die "Failed to create distribution archive! BAD BAD BAD"
			echo
			echo "Done, please upload tarball '${PACKAGE}-${VERSION}_pre.tar.bz2' to finaly pre-release."
			echo
			echo "You should merge back to trunk all fixes that happened since branching for pre-release." \
				"After that you may either drop this working copy or switch over to trunk:"
			echo "  svn switch ${INITNG_REPO}/trunk ."
		else
			echo "Sorry, can't release, you're not in a release branch"
		fi
		;;
	test)
		echo "Running autoreconf..."
		autoreconf -vfi || die "Failed to run autoreconf! BAD BAD BAD"

		./configure || die "Failed to run ./configure"

		echo "Compiling ${PACKAGE}..."
		make || die "Failed to compile ${PACKAGE}"

		echo "Generating distribution tarball..."
		make dist-gzip || die "Failed to create distribution archive! BAD BAD BAD"
		make dist-bzip2 || die "Failed to create distribution archive! BAD BAD BAD"

		echo "Cleaning generated files..."
		make distclean || die "Failed to clean generated files"

		echo
		echo "Test run successfully :)"
		;;
	mdiff)
		${SVN} diff --old="${INITNG_REPO}/trunk" --new=.
		;;
	bdiff)
		REFVERSION=$2
		VERSION=$3
		[ -n "${REFVERSION}" ] || die "You need to provide version"

		if [ -z "${VERSION}" ]; then
			# Compare to working directory
			${SVN} diff --old="${INITNG_REPO}/branches/${REFVERSION}" --new=.
		else
			# Compare VERSION to REFVERSION
			${SVN} diff --old="${INITNG_REPO}/branches/${REFVERSION}" \
				--new="${INITNG_REPO}/branches/${VERSION}"
		fi
		;;
	tdiff)
		REFVERSION=$2
		VERSION=$3
		[ -n "${REFVERSION}" ] || die "You need to provide version"

		if [ -z "${VERSION}" ]; then
			# Compare to working directory
			${SVN} diff --old="${INITNG_REPO}/tags/${REFVERSION}" --new=.
		else
			# Compare VERSION to REFVERSION
			${SVN} diff --old="${INITNG_REPO}/tags/${REFVERSION}" \
				--new="${INITNG_REPO}/tags/${VERSION}"
		fi
		;;
	*)
		echo "Usage:"
		if [ -z "$2" -o "$2" == "branch" ]; then
			echo "    $0 branch <VERSION>"
			echo "       Generate a branch for pre-release testing and stabilization"
			echo "       This branching is composed of:"
			echo "       - create the branch on SVN server"
			echo "       - switch working copy to new branch"
			echo "       - set correct version number in configure script"
			echo "       - call autoreconf and make dist"
		fi
		if [ -z "$2" -o "$2" == "release" ]; then
			echo "    $0 release"
			echo "       Finalize release"
			echo "       This finalization is composed of:"
			echo "       - set correct version number in configure script"
			echo "       - create the tag on SVN server"
			echo "       - call autoreconf and make dist"
		fi
		if [ -z "$2" -o "$2" == "test" ]; then
			echo "    $0 test"
			echo "       Perform some test compilation"
			echo "       This test-phase is composed of:"
			echo "       - run autoreconf"
			echo "       - make"
			echo "       - make distclean"
			echo "       - make dist"
		fi
		if [ -z "$2" -o "$2" == "mdiff" ]; then
			echo "    $0 mdiff"
			echo "       Compute differences between working copy and trunk"
			echo "       Target of diff is local working copy"
			echo "       To be used to find changes in trunk to back-port to branch"
		fi
		if [ -z "$2" -o "$2" == "bdiff" ]; then
			echo "    $0 bdiff <REF-VERSION> [<VERSION>]"
			echo "       Compute differences between working copy and given branch or"
			echo "       between two branches."
			echo "       <REF-VERSION> Reference version to compare to."
			echo "       <VERSION> Version to use for comparison, working copy if"
			echo "       omitted. (~ target)"
			echo "       To be used to find changes in branch to be back-ported to trunk."
		fi
		if [ -z "$2" -o "$2" == "tdiff" ]; then
			echo "    $0 tdiff <REF-VERSION> [<VERSION>]"
			echo "       Compute differences between working copy and given tag or"
			echo "       between two tags."
			echo "       <REF-VERSION> Reference version to compare to."
			echo "       <VERSION> Version to use for comparison, working copy if"
			echo "       omitted. (~ target)"
			echo "       To be used to view changes between tags/releases, e.g. for"
			echo "       identifying regressions."
		fi
		if [ -z "$2" ]; then
			echo "    $0 help [<command>]"
			echo "       Display help message for all command of only the command"
			echo "       mentionned (if any)"
		fi
		echo
		;;
esac
