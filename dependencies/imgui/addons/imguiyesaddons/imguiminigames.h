#ifndef IMGUIMINIGAMES_H_
#define IMGUIMINIGAMES_H_

// MINE GAME LICENSE:
/*
    The default graphic colors, the layouts and the mouse interaction scheme are based on the Gnome version of the game,
    named Mines, released under the CreativeCommons Attribution-Share Alike 3.0 Unported license.

    The game logic should be the same as the original Windows version of the game, named Minesweeper,
    and therefore copyrighted by Robert Donner and Curt Johnson, Microsoft Corporation.

    All the source code, however, is original and public domain.

    IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
    INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
    PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
    EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
    ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". THE AUTHOR HAS NO OBLIGATION
    TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

    P.S. It's still unclear whether or not the game logic was invented by Microsoft:
    please read here: https://en.wikipedia.org/wiki/Minesweeper_(video_game)
*/

// USAGE:
/*
 // inside a ImGui window
 static ImGuiMiniGames::Mine mineGame;
 mineGame.render();
*/

// SUDOKU GAME LICENSE:
/*
    The Sudoku puzzles are generated with: https://qqwing.com/generate.html
    Sudoku Solve Algo (removed from the code by default) adapted
    from http://codereview.stackexchange.com/questions/13677/solving-sudoku-using-backtracking

    All the source code, however, is in the public domain.

    IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
    INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
    PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
    EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
    ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". THE AUTHOR HAS NO OBLIGATION
    TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
// USAGE:
/*
 // inside a ImGui window
 static ImGuiMiniGames::Sudoku sudokuGame;
 sudokuGame.render();
*/

#ifndef IMGUI_API
#	include <imgui.h>
#endif //IMGUI_API



namespace ImGuiMiniGames {

    typedef void (*FreeTextureDelegate)(ImTextureID& texid);
    typedef void (*GenerateOrUpdateTextureDelegate)(ImTextureID& imtexid,int width,int height,int channels,const unsigned char* pixels,bool useMipmapsIfPossible,bool wraps,bool wrapt);

    // Not used ATM. Not sure if I'll ever use these callbacks or not...
    extern FreeTextureDelegate FreeTextureCb;
    extern GenerateOrUpdateTextureDelegate GenerateOrUpdateTextureCb;


#   ifndef NO_IMGUIMINIGAMES_MINE
// MINE GAME LICENSE:
/*
    The default graphic colors, the layouts and the mouse interaction scheme are based on the Gnome version of the game,
    named Mines, released under the CreativeCommons Attribution-Share Alike 3.0 Unported license.

    The game logic should be the same as the original Windows version of the game, named Minesweeper,
    and therefore copyrighted by Robert Donner and Curt Johnson, Microsoft Corporation.

    All the source code, however, is original and public domain.

    IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
    INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
    PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
    EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
    ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". THE AUTHOR HAS NO OBLIGATION
    TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.

    P.S. It's still unclear whether or not the game logic was invented by Microsoft:
    please read here: https://en.wikipedia.org/wiki/Minesweeper_(video_game)
*/
    class Mine {
        public:
        void render();

        struct Style {
            enum Color {
                Color_Text,                     // "PAUSED" "GAME OVER" and "GAME COMPLETED" text color
                Color_Background,               // of the child window - we can leave it transparent: 0x00000000 (default)
                Color_Grid,
                Color_ClosedCellBackground,
                Color_OpenCellBackground,
                Color_HoveredCellBackground,
                Color_Flag,
                Color_Mine,
                Color_1,
                Color_2,
                Color_3,
                Color_4,
                Color_5,
                Color_6,
                Color_7,
                Color_8,
                Color_WrongFlagBackground,
                Color_WrongMineOverlay,
                Color_WrongMineOverlayBorder,
                Color_HollowSpace,              // Some levels have a hole in the middle
                Color_Count
            };
            ImU32 colors[Color_Count];
            enum Character {
                Character_Mine = 0,
                Character_Flag,
                Character_Count
            };
            char characters[Character_Count][4];
            int keyPause;                       // This depends on your key-mapping in ImGui. (default == 'p')
            static Style style;
            inline static Style& Get() {return style;}
            Style();
            void reset() {*this=Style();}
        };
        Mine();
        ~Mine();
        protected:
        struct MineHS* imp;
    };
#   endif //NO_IMGUIMINIGAMES_MINE

#   ifndef NO_IMGUIMINIGAMES_SUDOKU
// SUDOKU GAME LICENSE:
/*
    The Sudoku puzzles are generated with: https://qqwing.com/generate.html
    Sudoku Solve Algo (removed from the code by default) adapted
    from http://codereview.stackexchange.com/questions/13677/solving-sudoku-using-backtracking

    All the source code, however, is in the public domain.

    IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
    INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
    PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
    EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

    THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
    PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
    ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". THE AUTHOR HAS NO OBLIGATION
    TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/

    class Sudoku {
        public:
        void render();

        struct Style {
            enum Color {
                Color_Text,                     // "PAUSED" "GAME OVER" and "GAME COMPLETED" text color
                Color_Background,               // of the child window - we can leave it transparent: 0x00000000 (default)
                Color_Grid,
                Color_GridShadow,
                Color_GridZone,
                Color_CellBackground,
                Color_HoveredCellBackground,
                Color_InitialNumbers,
                Color_Annotations,
                Color_Numbers,
                Color_Count
            };
            ImU32 colors[Color_Count];
            int keyPause;                       // This depends on your key-mapping in ImGui. (default == 'p')
            static Style style;
            inline static Style& Get() {return style;}
            Style();
            void reset() {*this=Style();}
#           ifndef NO_IMGUIMINIGAMES_MINE
            void setFromMineGameStyle(const Mine::Style& ms);
#           endif //NO_IMGUIMINIGAMES_MINE
        };
        Sudoku();
        ~Sudoku();
        protected:
        struct SudokuHS* imp;
    };
#   endif //NO_IMGUIMINIGAMES_SUDOKU


} // namespace ImGuiMiniGames


#endif //IMGUIMINIGAMES_H_

