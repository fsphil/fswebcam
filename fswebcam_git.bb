DESCRIPTION = "Webcam image grabber and manipulation application."
SECTION = "graphics"
HOMEPAGE = "http://www.sanslogic.co.uk/fswebcam/"
LICENSE="GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=393a5ca445f6965873eca0259a17f833"

DEPENDS = "gd"

PR = "r0"

inherit autotools-brokensep

SRCREV="${AUTOREV}"
SRC_URI = "git://github.com/leolarrel/fswebcam.git;protocol=git;branch=develop"

S = "${WORKDIR}/git"
