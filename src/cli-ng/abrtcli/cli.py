import os
import sys
import subprocess

import argcomplete
from argh import ArghParser, named, arg, aliases, expects_obj
import problem
import report as libreport

from abrtcli import config, l18n
from abrtcli.l18n import _
from abrtcli.match import match_completer, match_get_problem

from abrtcli.filtering import (filter_not_reported,
                               filter_since_timestamp,
                               filter_until_timestamp)

from abrtcli.utils import (fmt_problems,
                           query_yes_no,
                           remember_cwd,
                           run_event,
                           sort_problems)


@aliases('bt')
@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
def backtrace(args):
    prob = match_get_problem(args.MATCH, auth=args.auth)
    if hasattr(prob, 'backtrace'):
        print(fmt_problems(prob, fmt=config.BACKTRACE_FMT))
    else:
        print(_('Problem has no backtrace'))
        if isinstance(prob, problem.Ccpp):
            ret = query_yes_no(_('Start retracing process?'))
            if ret:
                retrace(args)
                print(fmt_problems(prob, fmt=config.BACKTRACE_FMT))

backtrace.__doc__ = _('Show backtrace of a problem')


@named('debuginfo-install')
@aliases('di')
@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
def di_install(args):
    prob = match_get_problem(args.MATCH, auth=args.auth)
    if not isinstance(prob, problem.Ccpp):
        which = _('This')
        if args.MATCH == 'last':
            which = _('Last')

        print(_('{} problem is not of a C/C++ type. Can\'t install debuginfo')
              .format(which))
        sys.exit(1)

    prob.chown()

    with remember_cwd():
        try:
            os.chdir(prob.path)
        except OSError:
            print(_('Permission denied: \'{}\'\n'
                    'If this is a system problem'
                    ' try running this command as root')
                  .format(prob.path))
            sys.exit(1)
        subprocess.call(config.DEBUGINFO_INSTALL_CMD, shell=True)

di_install.__doc__ = _('Install required debuginfo for given problem')


@expects_obj
@arg('-d', '--debuginfo-install', help='Install debuginfo prior launching gdb')
@arg('MATCH', nargs='?', default='last', completer=match_completer)
def gdb(args):
    prob = match_get_problem(args.MATCH, auth=args.auth)
    if not isinstance(prob, problem.Ccpp):
        which = 'This'
        if args.MATCH == 'last':
            which = 'Last'

        print('{} problem is not of a C/C++ type. Can\'t run gdb'
              .format(which))
        sys.exit(1)

    prob.chown()

    if args.debuginfo_install:
        di_install(args)

    cmd = config.GDB_CMD.format(di_path=config.DEBUGINFO_PATH)

    with remember_cwd():
        try:
            os.chdir(prob.path)
        except OSError:
            print(_('Permission denied: \'{}\'\n'
                    'If this is a system problem'
                    ' try running this command as root')
                  .format(prob.path))
            sys.exit(1)
        subprocess.call(cmd, shell=True)

gdb.__doc__ = _('Run GDB against a problem')


@named('list')
@aliases('ls')
@expects_obj
@arg('--since', type=int,
     help=_('List only the problems more recent than specified timestamp'))
@arg('--until', type=int,
     help=_('List only the problems older than specified timestamp'))
@arg('--fmt', type=str,
     help=_('Output format'))
@arg('--pretty', choices=config.FORMATS, default='medium',
     help=_('Built-in output format'))
@arg('-n', '--not-reported', default=False,
     help=_('List only not-reported problems'))
def list_problems(args):
    probs = sort_problems(problem.list(auth=args.auth))

    if args.since:
        probs = filter_since_timestamp(probs, args.since)

    if args.until:
        probs = filter_until_timestamp(probs, args.until)

    if args.not_reported:
        probs = filter_not_reported(probs)

    if not args.fmt:
        fmt = config.MEDIUM_FMT

    if args.pretty != 'medium':
        fmt = getattr(config, '{}_FMT'.format(args.pretty.upper()))

    out = fmt_problems(probs, fmt=fmt)

    if out:
        print(out)
    else:
        print(_('No problems'))

list_problems.__doc__ = _('List problems')


@aliases('show')
@expects_obj
@arg('--fmt', type=str,
     help=_('Output format'))
@arg('--pretty', choices=config.FORMATS, default='full',
     help=_('Built-in output format'))
@arg('MATCH', nargs='?', default='last', completer=match_completer)
def info(args):
    prob = match_get_problem(args.MATCH, allow_multiple=True, auth=args.auth)
    if not args.fmt:
        fmt = config.FULL_FMT

    if args.pretty != 'full':
        fmt = getattr(config, '{}_FMT'.format(args.pretty.upper()))

    print(fmt_problems(prob, fmt=fmt))

info.__doc__ = _('Print information about problem')


@aliases('rm')
@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
@arg('-i', help=_('Prompt before removal'), default=False)
def remove(args):
    prob = match_get_problem(args.MATCH, auth=args.auth)
    print(fmt_problems(prob, fmt=config.FULL_FMT))

    ret = True
    if args.i or args.MATCH == 'last':  # force prompt to avoid accidents
        ret = query_yes_no(_('Are you sure you want to delete this problem?'))

    if ret:
        prob.delete()
        print(_('Removed'))

remove.__doc__ = _('Remove problem')


@expects_obj
@arg('MATCH', nargs='?', default='last', completer=match_completer)
def report(args):
    prob = match_get_problem(args.MATCH, auth=args.auth)
    libreport.report_problem_in_dir(prob.path,
                                    libreport.LIBREPORT_WAIT |
                                    libreport.LIBREPORT_RUN_CLI)

report.__doc__ = _('Report problem')


@expects_obj
@arg('-l', '--local', action='store_true',
     help=_('Perform local retracing'))
@arg('-r', '--remote', action='store_true',
     help=_('Perform remote retracing using retrace server'))
@arg('-f', '--force', action='store_true',
     help=_('Force retracing even if backtrace already exists'))
@arg('MATCH', nargs='?', default='last', completer=match_completer)
def retrace(args):
    # we might not get these var if called from backtrace
    local, remote, auth = False, False, False

    if hasattr(args, 'local'):
        local = args.local
    if hasattr(args, 'remote'):
        remote = args.remote
    if hasattr(args, 'force'):
        force = args.force

    prob = match_get_problem(args.MATCH, auth=args.auth)
    if hasattr(prob, 'backtrace') and not force:
        print(_('Problem already has a backtrace'))
        print(_('Run abrt retrace with -f/--force to retrace again'))
        ret = query_yes_no(_('Show backtrace?'))
        if ret:
            print(fmt_problems(prob, fmt=config.BACKTRACE_FMT))
    elif not isinstance(prob, problem.Ccpp):
        print(_('No retracing possible for this problem type'))
    else:
        if not (local or remote):  # ask..
            ret = query_yes_no(
                _('Upload core dump and perform remote'
                  ' retracing? (It may contain sensitive data).'
                  ' If your answer is \'No\', a stack trace will'
                  ' be generated locally. Local retracing'
                  ' requires downloading potentially large amount'
                  ' of debuginfo data'), default='no')

            if ret:
                remote = True
            else:
                local = True

        prob.chown()

        if remote:
            print(_('Remote retracing'))
            run_event('analyze_RetraceServer', prob)
        else:
            print(_('Local retracing'))
            run_event('analyze_LocalGDB', prob)


retrace.__doc__ = _('Generate backtrace from coredump')


@expects_obj
@arg('-b', '--bare',
     help=_('Print only the problem count without any message'))
@arg('-s', '--since', type=int,
     help=_('Print only the problems more recent than specified timestamp'))
@arg('-n', '--not-reported', default=False,
     help=_('List only not-reported problems'))
def status(args):
    probs = problem.list(auth=args.auth)

    since_append = ''
    if args.since:
        probs = filter_since_timestamp(probs, args.since)
        since_append = ' --since {}'.format(args.since)

    if args.not_reported:
        probs = filter_not_reported(probs)

    if args.bare:
        print(len(probs))
        return

    print(_('ABRT has detected {} problem(s). For more info run: abrt list{}')
          .format(len(probs), since_append))

status.__doc__ = _('Print count of the recent crashes')


def main():
    l18n.init()

    parser = ArghParser()
    parser.add_argument('-a', '--auth',
                        action='store_true',
                        help=_('Authenticate and show all problems'
                               ' on this machine'))

    parser.add_argument('-v', '--version',
                        action='version',
                        version=config.VERSION)

    parser.add_commands([
        backtrace,
        di_install,
        gdb,
        info,
        list_problems,
        remove,
        report,
        retrace,
        status,
    ])

    argcomplete.autocomplete(parser)

    try:
        parser.dispatch()
    except KeyboardInterrupt:
        sys.exit(1)
