#include "CustomCallbacks.h"

vector<string> &FilesToCache() {
    static vector<string> filesToCache = {
        "model1890869140625__morph_target",
        "m228__morph_target",
        "m237__morph_target",
        "m222__",
        "m228__",
        "m316__",
        "m376__",
        "m43__",
        "m432__",
        "m44__",
        "m495__",
        "m723__",
        "m724__",
        "m725__",
        "m726__",
        "m727__",
        "m728__",
        "model11737060546875__",
        "model180450439453125__",
        "model1890869140625__",
        "model559539794921875__",
        // FM 08
        "ball____model58__",
        "ball____model8428955078125__",
        "net_left____model1020191539__",
        "net_right____model1020191531__",
        "player____m228__morph_target",
        "player____m228__",
        "player____m432__",
        // CL 2004-2005
        "player____model60__morph_target",
        "player____model60__",
        "manager____model2086181640625__",
        "manager____model3331298828125__",
        "managerfe____model896331787109375__",
        // FIFA 2004
        "ballhud____model311279296875__",
        // EURO 2004
        "trophy____model91168212890625__",
    };
    return filesToCache;
}

vector<unsigned int> &MaxSizeForFilesToCache() {
    static vector<unsigned int> maxSizeForFilesToCache(FilesToCache().size());
    static bool init = true;
    if (init) {
        for (auto &i : maxSizeForFilesToCache)
            i = 0;
        init = false;
    }
    return maxSizeForFilesToCache;
}
