#include "globals.h"

PkgList rawpl;
PkgList view;

HashMap pl_namecache;
HashMap pl_sizecache;
HashMap pl_instcache;

int pack_maxnlen = 0, pack_maxvlen = 0;
int pack_maxn = -1;
int pack_curr = 0, pack_top = 0;

int screen_width = 80, screen_height = 24;
int maxdlen = 10;
int list_height = 0;
const int scr_fixrows = 6;

int list_kind = 1; /* default: Installed w/o libs */
char filtname[256] = "";
char filtdesc[256] = "";

int doredowin = 1;

const char *list_kind_desc[6] = {
    "Installed (^R remove, ^K kill)",
    "!Installed w/o libs (^R remove, ^K kill)",
    "Installed !orphans (^R remove, ^K kill)",
    "Installable (repository) (^I install)",
    "Installable (!repository) w/o libs (^I install)",
    "!Upgradeable packages (^U upgrade)"
};

volatile sig_atomic_t g_resized = 0;