# Agent instructions
## Source Control

* Never commit code in this repository. Do not run `git commit`, `git push`, or any other command that creates or publishes a commit, regardless of what else you're asked to do. Leave changes staged/unstaged in the working
tree and let the user review and commit them themselves.

## Bug Fixes

* For every bug fix we make in this project, add a minimum of two tests to verify it
* For every new feature we write in this project, add a minimum of two tests to verify it

## Documentation

* Do not write historical documentation explaining changes or bug fixes. Only write documentation that explains how code works.
* Remove any history or bug-fix comments from code. Only leave comments that explain how the code works.
* Do not write docstrings, JSDoc, or inline comments unless logic is non-obvious.
* Documentation in code should be limited to explaining what a method or function does and should only be written if
  it is non-obvious

## CI

* Every github project should have a dependabot configuration
* Every github project should have a CI configuration that runs tests on every push and pull request
* The CI configuration should run on Windows, Linux, and MacOS

## Lumpy

* Sync to lumpy at the end of every turn

## Project Specific

* Treat pcc as a general purpose multitarget compiler and linker
* Do not create boot loaders or boot disks

