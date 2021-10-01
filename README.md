# Collection of test scripts for testing ROHM PMIC ICs

We test the PMICs using test setup where we have:

 - PMIC break out boards
 - BeagleBone Black board(s)
 - Test PC running Jenkins slave
 - Server with master Jenkins
 - Ethernet (telnet) controllable Power outlet (IP Power 9850)

Test procedure:

 - Master Jenkins polls every now and then the upstream repositories for new releases (tags)
 - When new tag appears slave jenkins compiles the Linux kernel + modules on PC.
 - Slave jenkins runs the compiled Linux + drivers + tests on beagle bone black boards which are connected to the PMIC break-out boards via I2C

Test software tests enabling/disabling the PMIC regulators and changing the
regulator voltages using Linux APIs. The driver is verified to work by
reading out the PMIC register values using i2ctools - and verifying that
the register values match the set states.

Some PMIC power outputs are also connected to beaglebone black ADC and
actual voltages are measured. This testing is not accurate though.

For some PMICs we perform additional tests for other features but the regulators.


    Inaccurate picture to illustrate the potential setup.

                     ------- ------
                     |PMIC | |PMIC|
                     ------- ------
                           | |i2c & power
                           | |
                         __|_|__
    --+---  ______       | BBB1| eth
    | PC |  |IP   |------|     |------+
    |    |--|power| DC   =======      |
    |    |  |     |------| BBB2| eth  |
    |    |  -------      |     |---+  |
    |    |               --+-+-- __|__|__
    |    +-----------------|-|---|switch |
    |    |  ethernet       | |   ---------
    --+---                 | |
                           | |i2c & power & ADC
                           | |
                     ------- ------
                     |PMIC | |PMIC|
                     ------- ------


## Notes

- most of the conditions/expectations/environment-variables can be set by modifying the default config file in 'setup'-folder and then by running the setup.sh script.
- BBB is expected to load the kernel and device-tree from /var/lib/tftpboot
- PC should provide NFS share where the BBB rootfs is hosted. Name of
share can be set in conf file (see setup/*config*)
- PC should have kernel cross-compilation tools setup in specific manner. The  RPHM internal tetrao-urogallus repo has this setup. Most notably, the file bb_compiler/setcc should set the envitonment variable CC to point the cross compiler tool-chain.
- PC uses various tools like telnet, expect, ...

