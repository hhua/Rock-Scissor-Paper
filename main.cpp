/*
 * Sifteo SDK Example.
 */

#include <sifteo.h>
#include <sifteo/menu.h>
#include "assets.gen.h"
#include "main.h"
using namespace Sifteo;

// Static Globals
static const unsigned gNumCubes = 3;
static VideoBuffer gVideo[gNumCubes];
static TiltShakeRecognizer motion[gNumCubes];
static struct MenuItem gItems[] = { {&IconChroma, NULL}, {&IconSandwich, NULL}, {&IconPeano, NULL}, {NULL, NULL} };
static struct MenuAssets gAssets = {&BgTile, &Footer, &LabelEmpty, {&Tip0, &Tip1, &Tip2, NULL}};

// METADATA

static Metadata M = Metadata()
    .title("Rock Paper Scissors")
    .package("com.sifteo.sdk.rps", "0.1")
    .icon(Icon)
    .cubeRange(gNumCubes);


AssetSlot gMainSlot = AssetSlot::allocate()
    .bootstrap(BootstrapAssets);

// GLOBALS

static VideoBuffer vbuf[CUBE_ALLOCATION]; // one video-buffer per cube
static CubeSet newCubes; // new cubes as a result of paint()
static CubeSet lostCubes; // lost cubes as a result of paint()
static CubeSet reconnectedCubes; // reconnected (lost->new) cubes as a result of paint()
static CubeSet dirtyCubes; // dirty cubes as a result of paint()
static CubeSet activeCubes; // cubes showing the active scene
static CubeSet readyCubes; // cubes ready for gaming after choosing a bid

static AssetLoader loader; // global asset loader (each cube will have symmetric assets)
static AssetConfiguration<1> config; // global asset configuration (will just hold the bootstrap group)



CUBE_STATUS cube_list[gNumCubes];

// FUNCTIONS

static void playSfx(const AssetAudio& sfx) {
    static int i=0;
    AudioChannel(i).play(sfx);
    i = 1 - i;
}

static Int2 getRestPosition(Side s) {
    // Look up the top-left pixel of the bar for the given side.
    // We use a switch so that the compiler can optimize this
    // however if feels is best.
    switch(s) {
    case TOP: return vec(64 - Bars[0].pixelWidth()/2,0);
    case LEFT: return vec(0, 64 - Bars[1].pixelHeight()/2);
    case BOTTOM: return vec(64 - Bars[2].pixelWidth()/2, 128-Bars[2].pixelHeight());
    case RIGHT: return vec(128-Bars[3].pixelWidth(), 64 - Bars[3].pixelHeight()/2);
    default: return vec(0,0);
    }
}

static int barSpriteCount(CubeID cid) {
    // how many bars are showing on this cube?
    ASSERT(activeCubes.test(cid));
    int result = 0;
    for(int i=0; i<4; ++i) {
        if (!vbuf[cid].sprites[i].isHidden()) {
            result++;
        }
    }
    return result;
}

static bool showSideBar(CubeID cid, Side s) {
    // if cid is not showing a bar on side s, show it and check if the
    // smiley should wake up
    ASSERT(activeCubes.test(cid));
    if (vbuf[cid].sprites[s].isHidden()) {
        vbuf[cid].sprites[s].setImage(Bars[s]);
        vbuf[cid].sprites[s].move(getRestPosition(s));
        if (barSpriteCount(cid) == 1) {
            vbuf[cid].bg0.image(vec(0,0), Backgrounds, 1);
        }
        return true;
    } else {
        return false;
    }
}

static bool hideSideBar(CubeID cid, Side s) {
    // if cid is showing a bar on side s, hide it and check if the
    // smiley should go to sleep
    ASSERT(activeCubes.test(cid));
    if (!vbuf[cid].sprites[s].isHidden()) {
        vbuf[cid].sprites[s].hide();
        if (barSpriteCount(cid) == 0) {
            vbuf[cid].bg0.image(vec(0,0), Backgrounds, 0);
        }
        return true;
    } else {
        return false;
    }
}

// activate cube
static void activateCube(CubeID cid) {
    // mark cube as active and render its canvas
    activeCubes.mark(cid);
    vbuf[cid].initMode(BG0_SPR_BG1);
    vbuf[cid].bg0.image(vec(0,0), Backgrounds, 0);
    auto neighbors = vbuf[cid].physicalNeighbors();
/*    for(int side=0; side<4; ++side) {
        if (neighbors.hasNeighborAt(Side(side))) {
            showSideBar(cid, Side(side));
        } else {
            hideSideBar(cid, Side(side));
        }
    }*/
}

// activate cube with specified background
static void activateCube(CubeID cid, int bg_id) {
    // mark cube as active and render its canvas
    activeCubes.mark(cid);
    vbuf[cid].initMode(BG0_SPR_BG1);
    vbuf[cid].bg0.image(vec(0,0), Backgrounds, bg_id);
    auto neighbors = vbuf[cid].physicalNeighbors();
    for(int side=0; side<4; ++side) {
        if (neighbors.hasNeighborAt(Side(side))) {
            showSideBar(cid, Side(side));
        } else {
            hideSideBar(cid, Side(side));
        }
    }
}

static void paintWrapper() {
    // clear the palette
    newCubes.clear();
    lostCubes.clear();
    reconnectedCubes.clear();
    dirtyCubes.clear();

    // fire events
    System::paint();

    // dynamically load assets just-in-time
    if (!(newCubes | reconnectedCubes).empty()) {
        AudioTracker::pause();
        playSfx(SfxConnect);
        loader.start(config);
        while(!loader.isComplete()) {
            for(CubeID cid : (newCubes | reconnectedCubes)) {
                vbuf[cid].bg0rom.hBargraph(
                    vec(0, 4), loader.cubeProgress(cid, 128), BG0ROMDrawable::ORANGE, 8
                );
            }
            // fire events while we wait
            System::paint();
        }
        loader.finish();
        AudioTracker::resume();
    }

    // repaint cubes
    for(CubeID cid : dirtyCubes) {
        activateCube(cid);
    }
    
    // also, handle lost cubes, if you so desire :)
}

static void onCubeConnect(void* ctxt, unsigned cid) {
    // this cube is either new or reconnected
    if (lostCubes.test(cid)) {
        // this is a reconnected cube since it was already lost this paint()
        lostCubes.clear(cid);
        reconnectedCubes.mark(cid);
    } else {
        // this is a brand-spanking new cube
        newCubes.mark(cid);
    }
    // begin showing some loading art (have to use BG0ROM since we don't have assets)
    dirtyCubes.mark(cid);
    auto& g = vbuf[cid];
    motion[cid].attach(cid);
    g.attach(cid);
    g.initMode(BG0_ROM);
    g.bg0rom.fill(vec(0,0), vec(16,16), BG0ROMDrawable::SOLID_BG);
    g.bg0rom.text(vec(1,1), "Hold on!", BG0ROMDrawable::BLUE);
    g.bg0rom.text(vec(1,14), "Adding Cube...", BG0ROMDrawable::BLUE);
}

static void onCubeDisconnect(void* ctxt, unsigned cid) {
    // mark as lost and clear from other cube sets
    lostCubes.mark(cid);
    newCubes.clear(cid);
    reconnectedCubes.clear(cid);
    dirtyCubes.clear(cid);
    activeCubes.clear(cid);
    readyCubes.clear(cid);
}

static void onCubeRefresh(void* ctxt, unsigned cid) {
    // mark this cube for a future repaint
    dirtyCubes.mark(cid);
}

static void onCubeTouch(void* ctxt, unsigned cid){
    ASSERT(activeCubes.test(cid));
    if(!readyCubes.test(cid)){
        vbuf[cid].bg0.image(vec(0,0), Silence, 0);
        readyCubes.mark(cid);
    }
}

static void onCubeAccelChange(void* ctxt, unsigned cid){
    ASSERT(activeCubes.test(cid));

    CubeID cube(cid);
    auto accel = cube.accel();
    String<64> str;

    unsigned changeFlags = motion[cid].update();
    if (changeFlags) {
         // Tilt/shake changed

            LOG("Tilt/shake changed, flags=%08x\n", changeFlags);

            auto tilt = motion[cid].tilt;
            str << "tilt:"
                << Fixed(tilt.x, 3)
                << Fixed(tilt.y, 3)
                << Fixed(tilt.z, 3) << "\n";

            str << "shake: " << motion[cid].shake;

            //LOG("Tilt/shake: %s\n", str);


    }

    if(!readyCubes.test(cid)){
        if(cube_list[cid].bg_id == 2){
            vbuf[cid].bg0.image(vec(0,0), Backgrounds, 0);
            cube_list[cid].bg_id = 0;
        }else{
            cube_list[cid].bg_id += 1;
            vbuf[cid].bg0.image(vec(0,0), Backgrounds, cube_list[cid].bg_id);
        }

        vbuf[cid].bg0rom.text(vec(1,10), str);
    }

    
}

static bool isActive(NeighborID nid) {
    // Does this nid indicate an active cube?
    return nid.isCube() && activeCubes.test(nid);
}

static void onNeighborAdd(void* ctxt, unsigned cube0, unsigned side0, unsigned cube1, unsigned side1) {
    // update art on active cubes (not loading cubes or base)
    bool sfx = false;
    //if (isActive(cube0)) { sfx |= showSideBar(cube0, Side(side0)); }
    //if (isActive(cube1)) { sfx |= showSideBar(cube1, Side(side1)); }

    if (isActive(cube0) && isActive(cube1)) { 
        int win = compareTwoCubes(cube0, cube1);
        if(win == 1){ // cube0 wins
            vbuf[cube0].sprites[Side(0)].setImage(LabelWin);
            vbuf[cube0].sprites[Side(0)].move(vec(0, 48));
            vbuf[cube1].sprites[Side(0)].setImage(LabelLose);
            vbuf[cube1].sprites[Side(0)].move(vec(0, 48));
        }else if(win == 0){ // lose
            vbuf[cube0].sprites[Side(0)].setImage(LabelLose);
            vbuf[cube0].sprites[Side(0)].move(vec(0, 48));
            vbuf[cube1].sprites[Side(0)].setImage(LabelWin);
            vbuf[cube1].sprites[Side(0)].move(vec(0, 48));
        }else{ // tie
            vbuf[cube0].sprites[Side(0)].setImage(LabelTie);
            vbuf[cube0].sprites[Side(0)].move(vec(0, 48));
            vbuf[cube1].sprites[Side(0)].setImage(LabelTie);
            vbuf[cube1].sprites[Side(0)].move(vec(0, 48));
        }

        vbuf[cube0].bg0.image(vec(0,0), Backgrounds, cube_list[cube0].bg_id);
        vbuf[cube1].bg0.image(vec(0,0), Backgrounds, cube_list[cube1].bg_id);
    }


    if (sfx) { playSfx(SfxAttach); }
}

static void onNeighborRemove(void* ctxt, unsigned cube0, unsigned side0, unsigned cube1, unsigned side1) {
    // update art on active cubes (not loading cubes or base)
    bool sfx = false;
    if (isActive(cube0)) { sfx |= hideSideBar(cube0, Side(side0)); }
    if (isActive(cube1)) { sfx |= hideSideBar(cube1, Side(side1)); }
    if (sfx) { playSfx(SfxDetach); }
}

void main() {
    // initialize asset configuration and loader
    config.append(gMainSlot, BootstrapAssets);
    loader.init();

    // subscribe to events
    Events::cubeConnect.set(onCubeConnect);
    Events::cubeDisconnect.set(onCubeDisconnect);
    Events::cubeRefresh.set(onCubeRefresh);
    Events::cubeTouch.set(onCubeTouch);
    Events::cubeAccelChange.set(onCubeAccelChange);

    Events::neighborAdd.set(onNeighborAdd);
    Events::neighborRemove.set(onNeighborRemove);
    
    // initialize cubes
    AudioTracker::setVolume(0.2f * AudioChannel::MAX_VOLUME);
    AudioTracker::play(Music);

    CubeSet cubes_connected = CubeSet::connected();
    int i=0;

    //runMenu();

    //for(CubeID cid : CubeSet::connected()) {
    for(CubeID cid = 0; cid != gNumCubes; ++cid) {
        vbuf[cid].attach(cid);
        activateCube(cid, cid);
        cube_list[cid].bg_id = cid;
    }
    
    // run loop
    for(;;) {
        paintWrapper();
    } 
}

static int compareTwoCubes(unsigned cube0, unsigned cube1){
    CubeID cid0(cube0);
    CubeID cid1(cube1);

    // if cube0 wins, return true; and vice versa
    if(cube_list[cid0].bg_id == 0 && cube_list[cid1].bg_id == 2){
        return 1;
    }else if(cube_list[cid0].bg_id == 2 && cube_list[cid1].bg_id == 0){
        return 0;
    }else if(cube_list[cid0].bg_id < cube_list[cid1].bg_id){
        return 0;
    }else if(cube_list[cid0].bg_id > cube_list[cid1].bg_id){
        return 1;
    }else{ // tie
        return 2;
    }
}


/*
static void runMenu(){
    CubeSet cubes_connected = CubeSet::connected();

    // Blank screens, attach VideoBuffers
    for(CubeID cube = 0; cube != gNumCubes; ++cube) {
    //for(CubeID cube : CubeSet::connected()) {
        auto &vid = gVideo[cube];
        vid.initMode(BG0);
        vid.attach(cube);
        vid.bg0.erase(StripeTile);
    }

    Menu m(gVideo[0], &gAssets, gItems);
    Menu m1(gVideo[1], &gAssets, gItems);
    Menu m2(gVideo[2], &gAssets, gItems);
    m.anchor(2);
    m1.anchor(1);
    m2.anchor(0);

    struct MenuEvent e;
    uint8_t item;

    while (1) {
        while (m.pollEvent(&e)) {

            switch (e.type) {

                case MENU_ITEM_PRESS:
                    // Game Buddy is not clickable, so don't do anything on press
                    if (e.item >= 3) {
                        // Prevent the default action
                        continue;
                    } else {
                        m.anchor(e.item);
                    }
                    if (e.item == 4) {
                        static unsigned randomIcon = 0;
                        randomIcon = (randomIcon + 1) % e.item;
                        m.replaceIcon(e.item, gItems[randomIcon].icon, gItems[randomIcon].label);
                    }
                    break;

                case MENU_EXIT:
                    // this is not possible when pollEvent is used as the condition to the while loop.
                    // NOTE: this event should never have its default handler skipped.
                    ASSERT(false);
                    break;

                case MENU_NEIGHBOR_ADD:
                    LOG("found cube %d on side %d of menu (neighbor's %d side)\n",
                         e.neighbor.neighbor, e.neighbor.masterSide, e.neighbor.neighborSide);
                    break;

                case MENU_NEIGHBOR_REMOVE:
                    LOG("lost cube %d on side %d of menu (neighbor's %d side)\n",
                         e.neighbor.neighbor, e.neighbor.masterSide, e.neighbor.neighborSide);
                    break;

                case MENU_ITEM_ARRIVE:
                    LOG("arriving at menu item %d\n", e.item);
                    item = e.item;
                    break;

                case MENU_ITEM_DEPART:
                    LOG("departing from menu item %d, scrolling %s\n", item, e.direction > 0 ? "forward" : "backward");
                    break;

                case MENU_PREPAINT:
                    // do your implementation-specific drawing here
                    // NOTE: this event should never have its default handler skipped.
                    break;

                case MENU_UNEVENTFUL:
                    // this should never happen. if it does, it can/should be ignored.
                    ASSERT(false);
                    break;
            }

            m.performDefault();
        }

        // Handle the exit event (so we can re-enter the same Menu)
        ASSERT(e.type == MENU_EXIT);
        m.performDefault();

        LOG("Selected Game: %d\n", e.item);
    }

    
}*/
