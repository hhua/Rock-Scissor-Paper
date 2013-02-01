struct CUBE_STATUS {   // Declare CUBE_STATUS struct type
   int bg_id;   // 0 is paper, 1 is scissor, 2 is stone
   bool isWin;
   //String cube_stat;
} cube_member;

static void runMenu();
static int compareTwoCubes(unsigned cube0, unsigned cube1);