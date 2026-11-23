// -UIShot -UICoachUse -CoachIKTest: the developer's test of the coaches' driver and riders (VehicleRiders.h) - command line only: on one coach a
// hired driver loads a bag and gets in, two test passengers put their bags in, board and sit, the coach pulls out and drives
// along the forecourt's bus lane (giving way at the zebra to someone on it), stops, they get off, the driver gets out, the
// bags are taken; then everything is put back as it was. True when it is over.
#pragma once

class APlayerController;

bool ADPCoachIKTestStep(APlayerController* PC, float Clock);
