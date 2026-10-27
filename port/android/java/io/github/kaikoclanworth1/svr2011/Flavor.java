// Which app this is: the SvR Anthology app (branch anthology) runs SVR Anthology by Yumboy11, copied to the
// phone by the PC Anthology launcher's "Install to phone" - no mods, no online, no updates from the port's
// releases. Its own app id (build_apk.py) and game folder, so it sits beside SvR 2011.

package io.github.kaikoclanworth1.svr2011;

final class Flavor {
    static final boolean ANTHOLOGY = true;

    // The game folder in games/, and the Anthology's own folder inside it (Yumboy11's installed folder).
    static final String kAnthologyData = "WWE SmackDown vs Raw Anthology";

    // His eight programs (default<key>.xex in his folder): the game's anthology_version.
    static final String[] kVersionLabels = {"SVR Anthology", "SmackDown vs. Raw 2007", "SmackDown vs. Raw 2008",
        "SmackDown vs. Raw 2009", "SmackDown vs. Raw 2010", "SmackDown vs. Raw 2011", "Legends of WrestleMania",
        "Divas"};
    static final String[] kVersionKeys = {"", "SVR07", "SVR08", "SVR09", "SVR10", "SVR11", "LOWM", "DIVAS"};

    private Flavor() {}
}
