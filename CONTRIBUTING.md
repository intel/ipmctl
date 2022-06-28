# Contributing to ipmctl

Thank you for your interest in ipmctl!

Please follow the guidelines below when submitting bugs or pull requests.

We will do our best to respond in a timely manner.

## Bug reports
When filing issues please:

1. Specify the ipmctl version, OS, kernel version, FW version, BIOS version etc.
2. Attach support data
  - 'ipmctl dump -destination support.txt -support'
3. Attach a debug log reproducing the issue
  - Enable max debug log verbosity via 'ipmctl set -preferences DBG_LOG_LEVEL=4'
  - Run the command
  - Attach the debug log located in /var/log/ipmctl/debug.log (or possibly in
    the directory you executed ipmctl if you compiled ipmctl)

## Pull requests
Pull requests fixing bugs and making small improvements are much appreciated.
For larger changes, please file an issue to discuss making larger enhancements.

When submitting a pull request:

1. Pick the target branch
    - `master` - the latest stable release
    - `master_N_0` - the stable branch of the respective 0N.00.00.xxxx release
    - `development` - the development branch of the future release
2. Rebase to the target branch
3. Sign off on the commit

Commits may be subject to minor changes. After a validation cycle, it
will merged into master under a tagged release.
