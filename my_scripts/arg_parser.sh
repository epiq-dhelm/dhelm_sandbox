#!/bin/bash

print_help ()
{
    printf '%s\n' "builds ubuntu drivers"
    printf 'Usage: %s [-u|--ubuntu-version] \n' "$(basename "$0")"
    printf '\t%s\n' "-u,--ubuntu-version: ubuntu-version to build docker container"
    printf '\t%s\n' "-s,--sidekiq-version: sidekiq-version to build against"
    printf '\t%s\n' "-k,--kernel-version: kernel-version to build"
}

parse_commandline ()
{
    while test $# -gt 0
    do
        _key="$1"
        case "$_key" in
            -u|--ubuntu-version)
                test $# -lt 2 && die "Missing value for the optional argument '$_key'." 1
                ubuntu_version="$2"
                shift
                ;;
            --ubuntu-version=*)
                ubuntu_version="${_key##--ubuntu-version=}"
                ;;
            -u*)
                ubuntu_version="${_key##-c}"
                ;;
            -s|--sidekiq-version)
                test $# -lt 2 && die "Missing value for the optional argument '$_key'." 1
                sidekiq_version="$2"
                shift
                ;;
            --sidekiq-version=*)
                sidekiq_version="${_key##--sidekiq-version=}"
                ;;
            -s*)
                sidekiq_version="${_key##-s}"
                ;;
            -k|--kernel-version)
                test $# -lt 2 && die "Missing value for the optional argument '$_key'." 1
                kernel_version="$2"
                shift
                ;;
            --kernel-version=*)
                kernel_version="${_key##--kernel-version=}"
                ;;
            -k*)
                kernel_version="${_key##-k}"
                ;;
            -h|--help)
                print_help
                exit 0
                ;;
            -h*)
                print_help
                exit 0
                ;;
            --)
                shift
                REMOTE_TESTS="$@"
                break
                ;;
            *)
                _positionals+=("$1")
                ;;
        esac
        shift
    done
}

