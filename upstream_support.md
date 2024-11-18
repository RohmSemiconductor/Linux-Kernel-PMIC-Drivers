## Upstream driver questions
We ask you to use the linux community mail-lists and maintainer information when you have questions related to the Linux community drivers.

Once the drivers are upstreamed the code changes are no longer in our hands - and the best experts for those drivers can be found from the commnity. This does not mean ROHM is out of the game - we have our personnel in Linux driver reviewers/ maintainers - but we don't "own" these components or frameworks anymore. You get the best possible contacts via the [MAINTAINERS](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/tree/MAINTAINERS) file.

## Upstream driver testing
A few automated tests are being ran for the upstreamed ROHM PMIC drivers. The tests are ran for tags created from the:
- Torvald's main Linux repository
- Linux-Next integration testing repository
- Linux stable repository
The test results are upload to the [PMIC branch](https://github.com/RohmSemiconductor/rohm-linux-test-results/tree/PMIC) of the rohm-linux-test-results repository.
