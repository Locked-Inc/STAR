SUMMARY = "STAR user and password configuration"
DESCRIPTION = "Sets up default users and passwords for STAR robot"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

inherit useradd extrausers

# Add star user
USERADD_PACKAGES = "${PN}"
USERADD_PARAM:${PN} = "-u 1000 -d /home/star -s /bin/sh -G users star"

# Set passwords using extrausers
EXTRA_USERS_PARAMS = " \
    usermod -p '\$6\$Mya7KdJ5/RTecTES\$.4DM.jzdoVsK9isVh95w69f9f.oHyZLrv3QkkW3F5zflV9xQ4HMiEyMgU/nULODo9ZV32bGK0DdAmCKcysXSg.' root; \
    usermod -p '\$6\$Mya7KdJ5/RTecTES\$.4DM.jzdoVsK9isVh95w69f9f.oHyZLrv3QkkW3F5zflV9xQ4HMiEyMgU/nULODo9ZV32bGK0DdAmCKcysXSg.' star; \
"

# Allow empty package
ALLOW_EMPTY:${PN} = "1"
