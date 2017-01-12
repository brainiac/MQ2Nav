// LICENSE: see "imguiminigames.h"

#include "../../imgui.h"
#define IMGUI_DEFINE_PLACEMENT_NEW
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../../imgui_internal.h"

#include "imguiminigames.h"

#include <stdlib.h> // rand()

namespace ImGuiMiniGames {
    FreeTextureDelegate FreeTextureCb =
#       ifdef IMGUI_USE_AUTO_BINDING
        &ImImpl_FreeTexture;
#       else //IMGUI_USE_AUTO_BINDING
        NULL;
#       endif //IMGUI_USE_AUTO_BINDING
    GenerateOrUpdateTextureDelegate GenerateOrUpdateTextureCb =
#       ifdef IMGUI_USE_AUTO_BINDING
        &ImImpl_GenerateOrUpdateTexture;
#       else //IMGUI_USE_AUTO_BINDING
        NULL;
#       endif //IMGUI_USE_AUTO_BINDING


    /*static bool GetGlyphData(unsigned short glyph,ImVec2* pSizeOut=NULL,float* pXAdvanceOut=NULL, ImVec2* pUV0Out=NULL, ImVec2* pUV1Out=NULL) {
        if (!GImGui->Font) return false;
        const ImFont::Glyph* g = GImGui->Font->FindGlyph(glyph);
        if (g)  {
            if (pSizeOut) {pSizeOut->x = g->X1-g->X0;pSizeOut->y = g->Y1-g->Y0;}
            if (pXAdvanceOut) *pXAdvanceOut = g->XAdvance;
            if (pUV0Out) {pUV0Out->x = g->U0; pUV0Out->y = g->V0;}
            if (pUV1Out) {pUV1Out->x = g->U1; pUV1Out->y = g->V1;}
            return true;
        }
        return false;
    }*/
    static void ImDrawListPathFillAndStroke(ImDrawList *dl, const ImU32 &fillColor, const ImU32 &strokeColor, bool strokeClosed, float strokeThickness, bool antiAliased)    {
        if (!dl) return;
        if ((fillColor & IM_COL32_A_MASK) != 0) dl->AddConvexPolyFilled(dl->_Path.Data, dl->_Path.Size, fillColor, antiAliased);
        if ((strokeColor& IM_COL32_A_MASK)!= 0 && strokeThickness>0) dl->AddPolyline(dl->_Path.Data, dl->_Path.Size, strokeColor, strokeClosed, strokeThickness, antiAliased);
        dl->PathClear();
    }
    static void ImDrawListAddRect(ImDrawList *dl, const ImVec2 &a, const ImVec2 &b, const ImU32 &fillColor, const ImU32 &strokeColor, float rounding, int rounding_corners, float strokeThickness, bool antiAliased) {
        if (!dl || (((fillColor & IM_COL32_A_MASK) == 0) && ((strokeColor & IM_COL32_A_MASK) == 0)))  return;
        dl->PathRect(a, b, rounding, rounding_corners);
        ImDrawListPathFillAndStroke(dl,fillColor,strokeColor,true,strokeThickness,antiAliased);
    }



#   ifndef NO_IMGUIMINIGAMES_MINE

    struct MineHS {
        static const int MAX_CELL_SIZE = 32;
        static const char* Title;
        enum CellState {
            CS_MINE=1,
            CS_FLAG=1<<1,
            CS_OPEN=1<<2,
            CS_DUMMY=1<<3
        };
        unsigned char cells[MAX_CELL_SIZE][MAX_CELL_SIZE];
        ImU32 numGridRows,numGridColumns,numMines,numGridHollowRows,numGridHollowColumns;
        ImU32 numClicks,numFlags,numOpenCells;
        bool paused;
        int frameCount;float startTime,currentTime,currentPenaltyTime,nextPenaltyTime;
        bool inited;
        int comboSelectedIndex;bool fitToScreen;
        enum GamePhase {
            GP_Titles=0,
            GP_Playing,
            GP_GameOver
        };
        unsigned char gamePhase;bool gameWon;
        void resetVariables() {
            numGridRows=numGridColumns=8;numMines=10;numGridHollowRows=numGridHollowColumns=0;
            numClicks=numFlags=numOpenCells=0;
            paused=false;frameCount=ImGui::GetFrameCount();
            startTime=0;currentTime=0;currentPenaltyTime=0;nextPenaltyTime=5;
            gamePhase=GP_Titles;gameWon=false;
            for (ImU32 y=0;y<MAX_CELL_SIZE;y++) {
                for (ImU32 x=0;x<MAX_CELL_SIZE;x++) {
                    cells[x][y]=0;
                }
            }
        }
        MineHS() {resetVariables();inited=false;comboSelectedIndex=0;fitToScreen=true;}
        ~MineHS() {}
        inline static const unsigned char* GetCellData(unsigned char cellContent,unsigned char* pCellStateOut,unsigned char* pNumAdjacentMinesOut=NULL) {
            if (pNumAdjacentMinesOut) *pNumAdjacentMinesOut = (cellContent&0x0F);
            if (pCellStateOut) {*pCellStateOut = (CellState) (cellContent>>4);return pCellStateOut;}
            return NULL;
        }
        inline static void SetCellData(unsigned char& cellContentOut,const unsigned char* pCellStateIn,const unsigned char* pNumAdjacentMinesIn=NULL) {
            if (pCellStateIn) cellContentOut = ((*pCellStateIn)<<4)|(cellContentOut&0x0F);
            if (pNumAdjacentMinesIn) cellContentOut = ((*pNumAdjacentMinesIn)&0x0F)|(cellContentOut&0xF0);
        }
        inline unsigned char calculateNumNeighborsWithState(ImU32 x,ImU32 y,unsigned char flag,bool excludeDummyCells=true) const {
            unsigned char numNeig = 0, state = 0;
            if (x>0)    {
                if                      (((*GetCellData(cells[x-1][y],&state))&flag)    && (excludeDummyCells || !(state&CS_DUMMY))) ++numNeig;
                if (y>0 &&              (((*GetCellData(cells[x-1][y-1],&state))&flag)  && (excludeDummyCells || !(state&CS_DUMMY)))) ++numNeig;
                if (y<numGridRows-1 &&  (((*GetCellData(cells[x-1][y+1],&state))&flag)  && (excludeDummyCells || !(state&CS_DUMMY)))) ++numNeig;
            }
            if (x<numGridColumns-1) {
                if                      (((*GetCellData(cells[x+1][y],&state))&flag)    && (excludeDummyCells || !(state&CS_DUMMY))) ++numNeig;
                if (y>0 &&              (((*GetCellData(cells[x+1][y-1],&state))&flag)  && (excludeDummyCells || !(state&CS_DUMMY)))) ++numNeig;
                if (y<numGridRows-1 &&  (((*GetCellData(cells[x+1][y+1],&state))&flag)  && (excludeDummyCells || !(state&CS_DUMMY)))) ++numNeig;
            }
            if (y>0 &&                  (((*GetCellData(cells[x][y-1],&state))&flag)  && (excludeDummyCells || !(state&CS_DUMMY)))) ++numNeig;
            if (y<numGridRows-1 &&      (((*GetCellData(cells[x][y+1],&state))&flag)  && (excludeDummyCells || !(state&CS_DUMMY)))) ++numNeig;
            return numNeig;
        }
        inline bool areNeighborsWithoutFlagsAllOpen(ImU32 x,ImU32 y) {
            unsigned char state = 0;
            if (x>0)    {                
                GetCellData(cells[x-1][y],&state);                          if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;
                if (y>0) {GetCellData(cells[x-1][y-1],&state);              if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;}
                if (y<numGridRows-1) {GetCellData(cells[x-1][y+1],&state);  if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;}
            }
            if (x<numGridColumns-1) {
                GetCellData(cells[x+1][y],&state);                          if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;
                if (y>0) {GetCellData(cells[x+1][y-1],&state);              if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;}
                if (y<numGridRows-1) {GetCellData(cells[x+1][y+1],&state);  if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;}
            }
                if (y>0) {GetCellData(cells[x][y-1],&state);                if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;}
                if (y<numGridRows-1) {GetCellData(cells[x][y+1],&state);    if (!(state&CS_DUMMY) && !((state&CS_OPEN) || (state&CS_FLAG))) return false;}
            return true;
        }
        // Returns false if player hits a mine
        inline bool openCell(ImU32 x,ImU32 y,ImU32* pNumCellsJustOpenedOut=NULL,bool isFirstClick=true) {
            unsigned char state = 0,adj = 0;
            GetCellData(cells[x][y],&state,&adj);
            if (state&CS_FLAG || state&CS_DUMMY) return true; // already flagged or dummy cell
            if (state&CS_OPEN)  {
                if (!isFirstClick) return true; // already open
                // First click here:
                if (adj==0
                    || calculateNumNeighborsWithState(x,y,CS_FLAG)!=adj
                    || areNeighborsWithoutFlagsAllOpen(x,y)) {
                    //fprintf(stderr,"Nothing to do for the %d neighbors [CalculateNumNeighborsWithState(CS_FLAG)=%d][AreNeighborsWithoutFlagsAllOpen()=%s]\n",adj,CalculateNumNeighborsWithState(x,y,CS_FLAG,cells,numGridColumns,numGridRows),AreNeighborsWithoutFlagsAllOpen(x,y,cells,numGridColumns,numGridRows)?"true":"false");
                    return true; // already open
                }
                //else fprintf(stderr,"We can open all the %d neighbors\n",adj); // We must open all the closed neighbors
            }
            else {
                // state is closed here
                state|=CS_OPEN;
                SetCellData(cells[x][y],&state);
                if (pNumCellsJustOpenedOut) (*pNumCellsJustOpenedOut)++;
                if (isFirstClick && adj>0 && !(state&CS_MINE)) return true;
            }
            if (state&CS_MINE) return false;// false => game over

            if (adj==0 || isFirstClick) {
                // recurse in 8 directions
                if (x>0)    {
                    if (                    !openCell(x-1,y,pNumCellsJustOpenedOut,false)) return false;
                    if (y>0 &&              !openCell(x-1,y-1,pNumCellsJustOpenedOut,false)) return false;
                    if (y<numGridRows-1 &&  !openCell(x-1,y+1,pNumCellsJustOpenedOut,false)) return false;
                }
                if (x<numGridColumns-1) {
                    if (                    !openCell(x+1,y,pNumCellsJustOpenedOut,false)) return false;
                    if (y>0 &&              !openCell(x+1,y-1,pNumCellsJustOpenedOut,false)) return false;
                    if (y<numGridRows-1 &&  !openCell(x+1,y+1,pNumCellsJustOpenedOut,false)) return false;
                }
                if (y>0 &&                  !openCell(x,y-1,pNumCellsJustOpenedOut,false)) return false;
                if (y<numGridRows-1 &&      !openCell(x,y+1,pNumCellsJustOpenedOut,false)) return false;
            }

            return true;
        }
        // Sets up the grid quantities, but does not fills it yet with mines
        void initNewGame(ImU32 _numGridRows,ImU32 _numGridColumns,ImU32 _numMines=-1,ImU32 _numGridHollowRows=0,ImU32 _numGridHollowColumns=0)  {
            numGridColumns = _numGridColumns;
            numGridRows = _numGridRows;
            for (int x=0;x<MAX_CELL_SIZE;x++) {
                for (int y=0;y<MAX_CELL_SIZE;y++)   {
                    cells[x][y]=0;
                }
            }
            numGridHollowColumns = (_numGridHollowColumns>2 && _numGridHollowColumns<numGridColumns-4) ? _numGridHollowColumns : 0;
            numGridHollowRows = (_numGridHollowRows>2 && _numGridHollowRows<numGridRows-4) ? _numGridHollowRows : 0;
            if (numGridHollowRows==0 || numGridHollowColumns==0) {numGridHollowRows=numGridHollowColumns=0;}
            if (numGridHollowRows>0 && numGridHollowColumns>0) {
                const unsigned char dummyCellState = CS_DUMMY;
                const int startCol = (numGridColumns-numGridHollowColumns)/2;
                const int startRow = (numGridRows-numGridHollowRows)/2;
                for (int x=startCol,xsz=startCol+numGridHollowColumns;x<xsz;x++) {
                    for (int y=startRow,ysz=startRow+numGridHollowRows;y<ysz;y++) {
                        SetCellData(cells[x][y],&dummyCellState);
                    }
                }
            }

            const ImU32 area = numGridColumns*numGridRows-(numGridHollowColumns*numGridHollowRows);
            numMines = (_numMines>0 && _numMines<area) ? _numMines :
                                                         area==8*8 ? 10 :
                                                                     area==16*16 ? 40 :
                                                                                   area==30*16 ? 99 :
                                                                                                 (area/5<=0 ? 1 : area/5);
            gameWon = false;
            IM_ASSERT(numMines>0 && numMines<area);
        }
        // Fills an inited grid with mines, leaving emptyCellRow and emptyCellColumn free
        void restartGame(int emptyCellColumn, int emptyCellRow)   {
            int c=0,r=0; unsigned char state=0; const unsigned char mineState = CS_MINE;
            const float c_rand = (float)numGridColumns/(float)RAND_MAX;
            const float r_rand = (float)numGridRows/(float)RAND_MAX;
            for (ImU32 i=0;i<numMines;i++)    {
                c = ((float)rand()*c_rand);
                r = ((float)rand()*r_rand);
                while (c<0 || c>=(int)numGridColumns || r<0 || r>=(int)numGridRows || (c==emptyCellColumn && r==emptyCellRow) || ((*GetCellData(cells[c][r],&state))&(CS_MINE|CS_DUMMY)))   {
                    c = ((float)rand()*c_rand);
                    r = ((float)rand()*r_rand);
                }
                SetCellData(cells[c][r],&mineState,NULL);
            }

            // Fill adjacency
            unsigned char numNeig = 0;
            for (ImU32 x=0;x<numGridColumns;x++) {
                for (ImU32 y=0;y<numGridRows;y++)   {
                    numNeig = calculateNumNeighborsWithState(x,y,CS_MINE);
                    if (numNeig>0) SetCellData(cells[x][y],NULL,&numNeig);
                }
            }

            numOpenCells = 0;gameWon = false;
            currentTime = 0;currentPenaltyTime=0;nextPenaltyTime=5;
            frameCount = ImGui::GetFrameCount();
            startTime = ImGui::GetTime();

        }

    void render() {
        Mine::Style& style = Mine::Style::Get();

        if (!inited) {
            inited = true;
            srand(ImGui::GetTime()*10.f);
        }

        ImU32 colorText = style.colors[Mine::Style::Color_Text];
        if ((colorText&IM_COL32_A_MASK)==0) colorText = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

        ImGui::PushID(this);
        bool mustReInit = false;
        ImGui::BeginGroup();
        if (gamePhase == GP_Playing) {
            if (ImGui::Button("Quit Game##MinesQuitGame")) {gamePhase = GP_Titles;mustReInit=true;}
        }
        else if (gamePhase == GP_GameOver) {
            if (ImGui::Button("New Game##MinesQuitGame")) {gamePhase = GP_Titles;mustReInit=true;}
        }
        if (gamePhase == GP_Titles || mustReInit) {
            static const char* Types[] = {"Easy (8x8)","Medium (16x16)","Hard (16x30)","Hard (30x16)","Impossible"};
            ImGui::PushItemWidth(ImGui::GetWindowWidth()*0.35f);
            if (mustReInit || ImGui::Combo("Game Type##MinesGameType",&comboSelectedIndex,Types,sizeof(Types)/sizeof(Types[0]),sizeof(Types)/sizeof(Types[0])))   {
                resetVariables();   // does not touch comboSelectedIndex
                switch (comboSelectedIndex) {
                case 0: numGridColumns = numGridRows = 8; numMines = 10;break;
                case 2: numGridColumns = 30; numGridRows = 16;numMines = 99;break;
                case 3: numGridColumns = 16; numGridRows = 30;numMines = 99;break;
                case 4: numGridColumns = numGridRows = 32;numMines = 0;numGridHollowColumns= numGridHollowRows = 8;break;
                case 1:
                default:
                    numGridColumns = numGridRows = 16; numMines = 40;
                    break;
                }
                mustReInit = true;
                initNewGame(numGridRows,numGridColumns,numMines,numGridHollowRows,numGridHollowColumns);
            }
            ImGui::PopItemWidth();
        }
        ImGui::Checkbox("Auto Zoom##MinesAutoZoom",&fitToScreen);
        if (ImGui::IsItemHovered() && !fitToScreen) ImGui::SetTooltip("%s","When false, use CTRL+MW to zoom\nand CTRL+MWB to auto-zoom.");
        ImGui::EndGroup();

        ImGui::SameLine(ImGui::GetWindowWidth()*0.35f);

        ImGui::BeginGroup();
        if (gamePhase != GP_Titles && !mustReInit) {
            if (gamePhase == GP_Playing) {
                int newFrame = ImGui::GetFrameCount();
                if (newFrame==frameCount+1 && !paused
#               ifdef IMGUI_USE_AUTO_BINDING
                && !gImGuiPaused && !gImGuiWereOutsideImGui
#               endif //IMGUI_USE_AUTO_BINDING
                ) {
                    currentTime = ImGui::GetTime() - startTime + currentPenaltyTime;
                }
                else startTime = ImGui::GetTime() - currentTime + currentPenaltyTime;
                frameCount = newFrame;
            }
            const unsigned int minutes = (unsigned int)currentTime/60;
            const unsigned int seconds = (unsigned int)currentTime%60;
            ImGui::Text("Time:  %um:%2us",minutes,seconds);
            ImGui::Text("Flags: %d/%d",numFlags,numMines);
        }
        ImGui::EndGroup();

        ImGui::PopID();

        ImVec2 gridSize(numGridColumns,numGridRows);

        // Mine, Flag, [1,8]
        float glyphWidths[10] = {-1.f,-1.f,-1.f,-1.f,-1.f,-1.f,-1.f,-1.f,-1.f,-1.f};

        ImGuiIO& io = ImGui::GetIO();
        if (mustReInit) ImGui::SetNextWindowFocus();
        ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImGui::ColorConvertU32ToFloat4(style.colors[Mine::Style::Color_Background]));
	ImGui::BeginChild("Mine Game Scrolling Region", ImVec2(0,0), false,fitToScreen ? (ImGuiWindowFlags_NoScrollbar||ImGuiWindowFlags_NoScrollWithMouse) : (ImGuiWindowFlags_HorizontalScrollbar/*|ImGuiWindowFlags_AlwaysHorizontalScrollbar|ImGuiWindowFlags_AlwaysVerticalScrollbar*/));

        // Following line is important if we want to avoid clicking on the window just to get the focus back (AFAICS, but there's probably some better way...)
        const bool isFocused = ImGui::IsWindowFocused() || ImGui::IsRootWindowFocused() || (ImGui::GetParentWindow() && ImGui::GetParentWindow()->Active);
        const bool isHovered = ImGui::IsWindowHovered();
        ImGuiWindow* window = ImGui::GetCurrentWindow();

        bool LMBclick = false, RMBclick = false, isPclicked = false;
        if (isFocused && isHovered && !mustReInit) {
            LMBclick = ImGui::IsMouseClicked(0);
            RMBclick = ImGui::IsMouseReleased(1);
            isPclicked = ImGui::IsKeyPressed(style.keyPause,false);
            if (isPclicked && gamePhase == GP_Playing) paused=!paused;
        }

        // Zoom / Scale window
        if (!fitToScreen)   {
            if (isFocused && isHovered && !io.FontAllowUserScaling && io.KeyCtrl && window==GImGui->HoveredWindow && (io.MouseWheel || io.MouseClicked[2]))   {
                float new_font_scale = ImClamp(window->FontWindowScale + io.MouseWheel * 0.10f, window->FontWindowScale*0.1f, window->FontWindowScale * 2.50f);
                if (io.MouseClicked[2]) new_font_scale = 1.f;   // MMB = RESET ZOOM
                float scale = new_font_scale / window->FontWindowScale;
                if (scale!=1)	window->FontWindowScale = new_font_scale;
            }
        }

        float textLineHeight = ImGui::GetTextLineHeight();
        ImVec2 gridDimensions = gridSize * (textLineHeight+window->FontWindowScale)  + ImVec2(0.1f*textLineHeight,0.1f*textLineHeight);
        ImVec2 gridOffset(0,0);
        if (fitToScreen || (gridDimensions.x<window->Size.x && gridDimensions.y<window->Size.y))   {
            if (gridDimensions.x!=window->Size.x && gridDimensions.y!=window->Size.y) {
                ImVec2 ratios(gridDimensions.x/window->Size.x,gridDimensions.y/window->Size.y);
                // Fill X or Y Window Size
                window->FontWindowScale/= (ratios.x>=ratios.y) ? ratios.x : ratios.y;

                textLineHeight = ImGui::GetTextLineHeight();
                gridDimensions = gridSize * (textLineHeight+window->FontWindowScale) + ImVec2(0.1f*textLineHeight,0.1f*textLineHeight);
            }
        }
        if (gridDimensions.x<window->Size.x) gridOffset.x = (window->Size.x-gridSize.x*(textLineHeight+window->FontWindowScale))*0.5f;
        if (gridDimensions.y<window->Size.y) gridOffset.y = (window->Size.y-gridSize.y*(textLineHeight+window->FontWindowScale))*0.5f;



        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 canvasSize = window->Size;
        ImVec2 win_pos = ImGui::GetCursorScreenPos();

        if (gamePhase==GP_Playing && (/*!isFocused ||*/ paused /*|| !ImGui::IsMouseHoveringWindow()*/
#       ifdef IMGUI_USE_AUTO_BINDING
                || gImGuiPaused || gImGuiWereOutsideImGui
#       endif //IMGUI_USE_AUTO_BINDING
        ))   {
            // Game Paused here

            if (((unsigned)(ImGui::GetTime()*10.f))%10<5)  {
                // Display "PAUSED":-------------------
                static const char pausedText[] = "PAUSED";
                const ImVec2 textSize = ImGui::CalcTextSize(pausedText);
                const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX(),(canvasSize.y-textSize.y)*0.15f+ImGui::GetScrollY());
                const ImVec2 enlargement(textLineHeight*0.25f,0.f);
                ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Mine::Style::Color_OpenCellBackground],style.colors[Mine::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
                draw_list->AddText(start,colorText,pausedText);
                //--------------------------------------
            }

            // Display controls in a smaller font:
            static const char controlsText[] = "CONTROLS:\n\nRMB: places/removes a flag.\nLMB: opens a cell\n     (on a number-cell, opens its neighbors).\nCTRL+LMB: safely opens a cell\n    (some penalty time is added).";
            const float fontScaling = 0.35f;
            const ImVec2 textSize = ImGui::CalcTextSize(controlsText)*fontScaling;
            const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX(),(canvasSize.y-textSize.y)*0.65f+ImGui::GetScrollY());
            const ImVec2 enlargement(textLineHeight*0.25f,0.f);
            ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Mine::Style::Color_OpenCellBackground],style.colors[Mine::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
            draw_list->AddText(GImGui->Font,GImGui->FontSize*fontScaling,start,colorText,controlsText);


        }
        else {


            const ImU32& GRID_COLOR = style.colors[Mine::Style::Color_Grid];
            const float GRID_SZ = textLineHeight+window->FontWindowScale;//32.f;
            const ImVec2 grid_len = gridSize * GRID_SZ;
            const float grid_Line_width = window->FontWindowScale;

            // Display Closed Cell Background
            draw_list->AddRectFilled(win_pos+gridOffset,win_pos+gridOffset+grid_len+ImVec2(grid_Line_width,grid_Line_width),style.colors[Mine::Style::Color_ClosedCellBackground]);

            // Display grid ---------------------------------------------------------------------------------------------
            // Draw Y lines
            int cnt = 0;
            for (float x = gridOffset.x,xsz=gridOffset.x+grid_len.x+GRID_SZ;    x<=xsz;  x+=GRID_SZ)    {
                draw_list->AddLine(ImVec2(x,gridOffset.y)+win_pos, ImVec2(x,gridOffset.y+grid_len.y)+win_pos, GRID_COLOR,grid_Line_width);
                if (cnt++>=gridSize.x) break;
            }
            // Draw X lines
            cnt = 0;
            for (float y = gridOffset.y,ysz=gridOffset.y+grid_len.y+GRID_SZ;    y<=ysz; y+=GRID_SZ)    {
                draw_list->AddLine(ImVec2(gridOffset.x,y)+win_pos, ImVec2(gridOffset.x+grid_len.x,y)+win_pos, GRID_COLOR,grid_Line_width);
                if (cnt++>=gridSize.y) break;
            }
            // Fraw Hollow Square
            if (numGridHollowColumns>0 && numGridHollowRows>0) {
                ImVec2 start = gridOffset + win_pos + ImVec2((numGridColumns-numGridHollowColumns)/2,(numGridRows-numGridHollowRows)/2)*GRID_SZ;
                draw_list->AddRectFilled(start,start+ImVec2(numGridHollowColumns,numGridHollowRows)*GRID_SZ,style.colors[Mine::Style::Color_HollowSpace]);
            }
            //------------------------------------------------------------------------------------------------------------

            // Detect the cell under the mouse.
            int mouseCellColumn = -1, mouseCellRow = -1;
            unsigned char mouseCellState = 0;
            bool isMouseCellValid = false;
            if (isFocused && isHovered && !mustReInit) {
                ImVec2 mp = ImGui::GetMousePos() - win_pos - gridOffset;
                if (mp.x>0 && mp.y>0
                        && (fitToScreen || (mp.x+gridOffset.x<window->Size.x-window->ScrollbarSizes.x+window->Scroll.x && mp.y+gridOffset.y<window->Size.y-window->ScrollbarSizes.y+window->Scroll.y))
                        )   {
                    mouseCellColumn = mp.x/GRID_SZ;
                    mouseCellRow = mp.y/GRID_SZ;
                    if (mouseCellRow>=gridSize.y || mouseCellColumn>=gridSize.x || ((*GetCellData(cells[mouseCellColumn][mouseCellRow],&mouseCellState))&CS_DUMMY)) {mouseCellColumn=mouseCellRow=-1;mouseCellState=0;}
                    else isMouseCellValid = true;
                }
                if (isMouseCellValid) {
                    if (gamePhase != GP_GameOver && !LMBclick && !RMBclick && !(mouseCellState&CS_OPEN) && !(mouseCellState&CS_FLAG))  {
                        // Let's draw the hovered cell:
                        ImVec2 start(win_pos+gridOffset+ImVec2(grid_Line_width,grid_Line_width)+ImVec2(mouseCellColumn*GRID_SZ,mouseCellRow*GRID_SZ));
                        draw_list->AddRectFilled(start,start+ImVec2(textLineHeight,textLineHeight),style.colors[Mine::Style::Color_HoveredCellBackground]);
                    }
                    //fprintf(stderr,"Clicked cell[c:%d][r:%d].\n",mouseCellColumn,mouseCellRow);
                }

            }

            if (gamePhase!=GP_Playing)  {
                if (gamePhase == GP_Titles) {
                    if (LMBclick && isMouseCellValid)   {
                        ++numClicks;
                        restartGame(mouseCellColumn,mouseCellRow);  // First click in the game (the clicked cell must be empty)
                        const bool mineAvoided = openCell(mouseCellColumn,mouseCellRow,&numOpenCells);
                        IM_ASSERT(mineAvoided);
                        gamePhase = GP_Playing;
                    }
                }
                LMBclick = RMBclick = false;   // prevents double processing
            }
            else if (gamePhase==GP_Playing && isMouseCellValid) {
                bool mustCheckForGameWon = false;
                if (RMBclick) {
                    if (!(mouseCellState&CS_OPEN)) {
                        ++numClicks;    // Should we skip flag clicks from counting ?
                        mouseCellState^=CS_FLAG;
                        SetCellData(cells[mouseCellColumn][mouseCellRow],&mouseCellState);
                        if (mouseCellState&CS_FLAG) numFlags++;
                        else if (numFlags>0) numFlags--;
                        mustCheckForGameWon = true;
                    }
                }
                else if (LMBclick)  {                    
                    if (io.KeyCtrl) {
                        // Help mode:
                        if (!(mouseCellState&CS_OPEN)) {
                            ++numClicks;
                            if (mouseCellState&CS_MINE) {
                                if (!(mouseCellState&CS_FLAG))    {
                                    mouseCellState|=CS_FLAG;
                                    SetCellData(cells[mouseCellColumn][mouseCellRow],&mouseCellState);
                                    numFlags++;
                                    mustCheckForGameWon = true;
                                }
                            }
                            else {
                                if (mouseCellState&CS_FLAG)    {
                                    mouseCellState&=~CS_FLAG;
                                    SetCellData(cells[mouseCellColumn][mouseCellRow],&mouseCellState);
                                    numFlags--;
                                    mustCheckForGameWon = true;
                                }
                                if (!openCell(mouseCellColumn,mouseCellRow,&numOpenCells)) gamePhase = GP_GameOver;
                                else mustCheckForGameWon = true;
                            }
                            // Add penalty time
                            currentPenaltyTime+=nextPenaltyTime;
                            nextPenaltyTime+=10;    // Next time penalty time will be 10 s bigger
                        }
                    }
                    else {
                        // Normal mode
                        ++numClicks;
                        if (!openCell(mouseCellColumn,mouseCellRow,&numOpenCells)) gamePhase = GP_GameOver;
                        else mustCheckForGameWon = true;
                    }
                }
                if (mustCheckForGameWon) {
                    const ImU32 area = numGridColumns*numGridRows-(numGridHollowColumns*numGridHollowRows);
                    if (numMines == numFlags && numOpenCells == area-numFlags) {
                        gamePhase = GP_GameOver;
                        gameWon = true;
                    }
                }
                // Check what square is under the mouse
                /*ImVec2 start(win_pos+gridOffset+ImVec2(grid_Line_width,grid_Line_width)+ImVec2(C*GRID_SZ,R*GRID_SZ));
                        ImRect rect(start,start+ImVec2(textLineHeight,textLineHeight));
                        draw_list->AddRectFilled(rect.GetTL(),rect.GetBR(),IM_COL32(0,0,255,200));*/
            }

            // draw cells:
            {
                unsigned char state=0,adj=0;
                static char charNum[2] = "0";
                const ImVec2 baseStart = win_pos+gridOffset+ImVec2(0.f/*grid_Line_width*/,grid_Line_width);
                ImVec2 start(0,0);
                for (ImU32 c=0;c<numGridColumns;c++)  {
                    for (ImU32 r=0;r<numGridRows;r++)  {
                        GetCellData(cells[c][r],&state,&adj);

                        start = baseStart+ImVec2(c*GRID_SZ,r*GRID_SZ);

                        if (state&CS_OPEN) {
                            draw_list->AddRectFilled(start+ImVec2(grid_Line_width,0.f),start+ImVec2(grid_Line_width+textLineHeight,textLineHeight),style.colors[Mine::Style::Color_OpenCellBackground]);
                        }
                        if (state&CS_FLAG) {
                            if (gamePhase==GP_GameOver && !(state&CS_MINE)) draw_list->AddRectFilled(start+ImVec2(grid_Line_width,0.f),start+ImVec2(grid_Line_width+textLineHeight,textLineHeight),style.colors[Mine::Style::Color_WrongFlagBackground]);

                            float& glyphWidth = glyphWidths[1];if (glyphWidth==-1.f) glyphWidth = ImGui::CalcTextSize(style.characters[Mine::Style::Character_Flag]).x;
                            draw_list->AddText(start+ImVec2((textLineHeight-glyphWidth)*0.5f,0.f),style.colors[Mine::Style::Color_Flag],style.characters[Mine::Style::Character_Flag]);
                        }
                        else if (state&CS_OPEN || gamePhase==GP_GameOver) {
                            if (state&CS_MINE) {
                                float& glyphWidth = glyphWidths[0];if (glyphWidth==-1.f) glyphWidth = ImGui::CalcTextSize(style.characters[Mine::Style::Character_Mine]).x;
                                draw_list->AddText(start+ImVec2((textLineHeight-glyphWidth)*0.5f,0.f),style.colors[Mine::Style::Color_Mine],style.characters[Mine::Style::Character_Mine]);

                                if (state&CS_OPEN) {
                                    draw_list->AddCircleFilled(start+ImVec2(grid_Line_width+textLineHeight*0.5f,textLineHeight*0.5f),textLineHeight*0.4f,style.colors[Mine::Style::Color_WrongMineOverlay]);
                                    draw_list->AddCircle(start+ImVec2(grid_Line_width+textLineHeight*0.5f,textLineHeight*0.5f),textLineHeight*0.4f,style.colors[Mine::Style::Color_WrongMineOverlayBorder],12,grid_Line_width);
                                }
                            }
                            else if (state&CS_OPEN && adj>0 && adj<9)  {
                                charNum[0] = '0'+adj;
                                float& glyphWidth = glyphWidths[1+adj];if (glyphWidth==-1.f) glyphWidth = ImGui::CalcTextSize(charNum).x;
                                const ImU32& glyphColor = style.colors[(Mine::Style::Color_1+adj-1)];
                                draw_list->AddText(start+ImVec2((textLineHeight-glyphWidth)*0.5f,0.f),glyphColor,charNum);
                            }
                        }                        
                    }
                }
            }

            // Draw end game messages
            if (gamePhase==GP_GameOver) {
                const float elapsedSeconds = (float)(ImGui::GetTime()-currentTime-startTime+currentPenaltyTime);
                if (gameWon) {
                    static char gameWonText[256] = "";
                    sprintf(gameWonText,"GAME COMPLETED\nTIME: %um : %us",((unsigned)currentTime)/60,((unsigned)currentTime)%60);
                    const ImVec2 textSize = ImGui::CalcTextSize(gameWonText);
                    ImVec2 deltaPos(0.f,0.f);
                    if (elapsedSeconds<10.f) {
                        deltaPos.x = canvasSize.x * sin(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                        deltaPos.y = canvasSize.y * cos(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                    }
                    const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX()+deltaPos.x,(canvasSize.y-textSize.y)*0.5f+ImGui::GetScrollY()-deltaPos.y);
                    const ImVec2 enlargement(textLineHeight*0.25f,0.f);
                    ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Mine::Style::Color_OpenCellBackground],style.colors[Mine::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
                    draw_list->AddText(start,colorText,gameWonText);
                }
                else {
                    static const char gameOverText[] = "GAME\nOVER";
                    const ImVec2 textSize = ImGui::CalcTextSize(gameOverText);
                    ImVec2 deltaPos(0.f,0.f);
                    if (elapsedSeconds<10.f) {
                        deltaPos.x = canvasSize.x * sin(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                        deltaPos.y = canvasSize.y * cos(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                    }
                    const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX()+deltaPos.x,(canvasSize.y-textSize.y)*0.5f+ImGui::GetScrollY()-deltaPos.y);
                    const ImVec2 enlargement(textLineHeight*0.25f,0.f);
                    ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Mine::Style::Color_OpenCellBackground],style.colors[Mine::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
                    draw_list->AddText(start,colorText,gameOverText);
                }
            }
            /*const ImVec2 textSize = ImGui::CalcTextSize(title);
            const ImU32 col = IM_COL32(0,255,0,255);
            draw_list->AddText(win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX(),(canvasSize.y-textSize.y)*0.5f+ImGui::GetScrollY()),col,title);*/

            //if (isMouseDraggingForScrolling) scrolling = scrolling - io.MouseDelta;
        }

        // Sets scrollbars properly:
        ImGui::SetCursorPos(ImGui::GetCursorPos() + gridSize * (textLineHeight+window->FontWindowScale) + ImVec2(0.1f*textLineHeight,0.1f*textLineHeight));

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    };
    const char* MineHS::Title = "Mine Game";

    Mine::Mine() : imp(NULL) {
        imp = (MineHS*) ImGui::MemAlloc(sizeof(MineHS));
        IM_PLACEMENT_NEW(imp) MineHS();
        IM_ASSERT(imp);
    }
    Mine::~Mine() {
        if (imp)    {
            imp->~MineHS();
            ImGui::MemFree(imp);
        }
    }
    void Mine::render() {
        if (imp) imp->render();
    }

    Mine::Style::Style()    {
        colors[Style::Color_Text] =                     IM_COL32(0,136,0,255);;
        colors[Style::Color_Background] =               IM_COL32_BLACK_TRANS;//IM_COL32(242,241,240,255);
        colors[Style::Color_ClosedCellBackground] =     IM_COL32(186,189,182,255);
        colors[Style::Color_OpenCellBackground] =       IM_COL32(222,222,220,255);;
        colors[Style::Color_1] =                        IM_COL32(75,89,131,255);;
        colors[Style::Color_2] =                        IM_COL32(70,160,70,255);;
        colors[Style::Color_3] =                        IM_COL32(226,66,30,255);
        colors[Style::Color_4] =                        IM_COL32(98,91,129,255);
        colors[Style::Color_5] =                        IM_COL32(136,70,49,255);
        colors[Style::Color_6] =                        IM_COL32(157,184,210,255);
        colors[Style::Color_7] =                        IM_COL32(238,214,128,255);
        colors[Style::Color_8] =                        IM_COL32(255,230,96,255);
        colors[Style::Color_WrongFlagBackground] =      IM_COL32(204,0,0,255);
        colors[Style::Color_WrongMineOverlay] =         IM_COL32(255,204,0,136);
        colors[Style::Color_WrongMineOverlayBorder] =   IM_COL32(85,68,0,153);
        colors[Style::Color_HollowSpace]        =       IM_COL32_BLACK;

        colors[Style::Color_Mine] = IM_COL32_BLACK;
        colors[Style::Color_Flag] = IM_COL32(255,0,0,255);

        colors[Style::Color_Grid] = IM_COL32(242,241,240,255);


        const ImVec4 tmp1 = ImGui::ColorConvertU32ToFloat4(colors[Style::Color_ClosedCellBackground]);
        const ImVec4 tmp2 = ImGui::ColorConvertU32ToFloat4(colors[Style::Color_OpenCellBackground]);
        const ImVec4 tmp((tmp1.x+tmp2.x)*0.5f,(tmp1.y+tmp2.y)*0.5f,(tmp1.z+tmp2.z)*0.5f,(tmp1.w+tmp2.w)*0.5f);
        colors[Color_HoveredCellBackground] = ImGui::ColorConvertFloat4ToU32(tmp);

        // Warning: we can't comment them out, we must initialize the memory somehow [Consider setting them to "" if we'll support manual drawing].
        strcpy(characters[Character_Mine],"M");
        strcpy(characters[Character_Flag],"F");

        keyPause = (int) 'p';

    }
    Mine::Style Mine::Style::style;




#   endif //NO_IMGUIMINIGAMES_MINE


#   ifndef NO_IMGUIMINIGAMES_SUDOKU

// We don't actually need a Sudoku Solver, but we can add it in case of future improvements....
//#	ifndef IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
//#	    define IMGUIMINIGAMES_SUDOKU_ADD_SOLVER // Tweakable
//#	endif //IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
// The only advantage of having a solver is that we can check input puzzles and discard the incorrect ones

// To check user solution we don't need to compare it with a "solved one": this will almost always be incorrect
// when the puzzle has more than one solution. We simply apply the "Sudoku constaints" every time the user fills up the grid.


    struct SudokuHS {
	static const int CELL_SIZE = 9;         // Actually it's hard-coded (= can't be changed)
	static const int SQRT_CELL_SIZE = 3;
	static const char* Title;
        enum CellState {
            CS_ANNOTATION_1=1,
            CS_ANNOTATION_2=1<<1,
            CS_ANNOTATION_3=1<<2,
            CS_ANNOTATION_4=1<<3,
            CS_ANNOTATION_5=1<<4,
            CS_ANNOTATION_6=1<<5,
            CS_ANNOTATION_7=1<<6,
            CS_ANNOTATION_8=1<<7,
            CS_ANNOTATION_9=1<<8,
	    CS_ANNOTATION_PRESENT=(CS_ANNOTATION_1|CS_ANNOTATION_2|CS_ANNOTATION_3|CS_ANNOTATION_4|CS_ANNOTATION_5|CS_ANNOTATION_6|CS_ANNOTATION_7|CS_ANNOTATION_8|CS_ANNOTATION_9),
	    CS_LOCKED=1<<9         // when set, the number is an initial data
        }; // We use 16 bit flags + user number in [0-9] (8 bit) + solution number in [1-9] (8 bit) = 32 bits (mainly to ease bitwise calculations)
	ImU32 cells[CELL_SIZE][CELL_SIZE];
        ImU32 numOpenCells; // Num cells to be filled
        bool paused;
        int frameCount;float startTime,currentTime;
        bool inited;
        int comboSelectedIndex;
        enum GamePhase {
            GP_Titles=0,
            GP_Playing,
            GP_GameOver
        };
        unsigned char gamePhase;bool gameWon;
        void resetVariables() {
            numOpenCells=0;
            paused=false;frameCount=ImGui::GetFrameCount();
            startTime=0;currentTime=0;
            gamePhase=GP_Titles;gameWon=false;
	    for (ImU32 y=0;y<CELL_SIZE;y++) {
		for (ImU32 x=0;x<CELL_SIZE;x++) {
                    cells[x][y]=0;
                }
            }
        }
        SudokuHS() {resetVariables();inited=false;comboSelectedIndex=0;}
        ~SudokuHS() {}
	inline static const int* GetCellData(ImU32 cellContent,int* pCellStateOut,unsigned char* pUserNumberOut=NULL,unsigned char* pSolutionNumbersOut=NULL) {
            if (pCellStateOut)          *pCellStateOut = (cellContent&0x0000FFFF);
            if (pUserNumberOut)         *pUserNumberOut = ((cellContent>>16)&0x000000FF);
        if (pSolutionNumbersOut)    *pSolutionNumbersOut = ((cellContent>>24)/*&0x000000FF*/);  // &0x000F should be redundant...
            return pCellStateOut;
        }
        inline static void SetCellData(ImU32& cellContentOut,const int* pCellStateIn,const unsigned char* pUserNumberIn=NULL,const unsigned char* pSolutionNumbersIn=NULL) {
        if (pCellStateIn) cellContentOut = ((*pCellStateIn))|(cellContentOut&0xFFFF0000);
            if (pUserNumberIn) cellContentOut = (((int)(*pUserNumberIn))<<16)|(cellContentOut&0xFF00FFFF);
            if (pSolutionNumbersIn) cellContentOut = (((int)(*pSolutionNumbersIn))<<24)|(cellContentOut&0x00FFFFFF);
        }
	// Faster branches for recursive solvers
	inline static unsigned char GetCellSolutionNumber(ImU32 cellContent) {return (unsigned char)(cellContent>>24);}
	inline static void SetCellSolutionNumber(ImU32& cellContentOut,unsigned char solutionNumber) {cellContentOut = (((int)(solutionNumber))<<24)|(cellContentOut&0x00FFFFFF);}


	void resetAllCells()	{
	    for (int x=0;x<CELL_SIZE;x++) {
		for (int y=0;y<CELL_SIZE;y++)   {
		    cells[x][y]=0;
		}
	    }
	}

        void initNewGame()  {
            resetVariables();


	    // Puzzles generated with: https://qqwing.com/generate.html
	    static const unsigned char puzzles[4][12][82]={
	    { // SIMPLE
	    ".1.4.3...954...3..3......1....5....148..1.7....76....3......4.8.91....567........",
	    "..768...392..5.6.8.......2..6..97...4..8...5..734.......9......5..72..6....53....",
	    "..345...9...8..14....7...5..21.9.....4..7.5.3.7.51....812......9.4...7...........",
	    "674.3....1..........3..4..2..2.......4..8......87..1.3..5..2....8..45.21.....6.7.",
	    "...21.78.....46..............43...1.2...51.476.5.7.2.3.56..48....3.25.....27.....",
	    ".2...9..6....5.......471.5.......6.........925...86.4...92..578........9..4..5.2.",
	    "...............147..8...235....8....1.924.....4...7.1.2..93.4....1...398.7......1",
	    "....95..4.48...1...6.47....9...42....2.6........71.5...8.5.....2.4........1.862..",
	    ".15.87...7..3.6...39852....5.....76..3....9.......1....7......3152..3......768...",
	    "...8.2...3..7.18.4...53.1.6.28......7.465....6..1.43.8.........4..3..5..5.6......",
	    ".84615...5...7....3.....4....53..8...13.......7..8....9...57..87..8........94.6..",
	    "..........7...3......4193.6.9.3..7.2..1..2.94...7..1.....62..4..12..4.8.......9.."
	    },
	    { // EASY
	    "...85..3...........9..6.......62....4.8....7..324......47..3..55...74.8.8.....9..",
	    "..7.2..6.2...14.3...9..7..4..41..5..1.........6.......74.3..6.5..69...439....5...",
	    ".5.1.6.7........51....32...1.3..4.....275...........48...........6..7.3..1.62.8.5",
	    "...6..7..9..3...2......2...8.7..361.16.....5.3..5..8..7..9.........6.3.1.95.....2",
	    "........6..56.93...........14...36......2....9...6.47.3..54.1..5...1...2.7.9...5.",
	    "3..2.8....71..3.....897..6....6...5.2.3..9..8......7.....8....2.5....39.......5.7",
	    "....5........3..9.512....36.5...............84..9....11....28.9.8...6..5.6.4...2.",
	    "4.....8...85.4..21.17...6.3......4...6.....871...35.........3..2..3..5...34..2...",
	    "8..............358......46.7..4....6..4.1.5.....3.7.2..3..64.1..7..8.2.5.1...3..7",
	    ".......8.27....4.6.63....727..3..6......48..7..4..2..9.....4.688.2..65...45......",
	    ".2.5...7..17..4...3..2.9.4.1...4..5...4..2...9.8357..4............8.5..1......3.8",
	    "..4.....6.87...2.....2..1....6..8....3...982...........2..1.93.64..9..7..7...3..."
	    },
	    { // INTERMEDIATE
	    "..5.3..4.3.2...8.1.1...7...2.8...17......2.6.54......3....157.4..3........42.....",
	    "....8......1.59..7289......5....31.8.7...2.39.....42...3..4....81.......9273.....",
	    "..4.713....7...8...2.........3.49...9..85...1851...4.......32....97.4.......2...7",
	    "2............3...2...7.....9......64.7...31..62.15......3.2..95...54..3.4...7.21.",
	    "..8.....61..8......321..8.4......3.9.4..6.2..9......688.36.......1....7.72.4....3",
	    "..5..4.....3.5.9.279.......2..1...9....6...354..7..6..6.........4.....73..9...4..",
	    ".8.6........827...57....9..81.........69..3......7.65.3...6..2......1.9.2......1.",
	    "..94.1.5.2....9.8.6..83..41...1.5..3.3..64.........5.9..4...1............56..8...",
	    ".......1..5.34...9...596.....6..5..3...7..86.4...389.........8....864....7.9.3...",
	    "4............8..3..35..24...9.......6.7......8...5.7.4.29..3.8....9.7..6......3.7",
	    "..1..4...7....8..2...6.1....1.5.......21....86.4...95.4.9.....7...4...16..7.8..2.",
	    "9..8.5.......62.3..7.9.45..4.7....2..2....8..3..........2....8.7.....362.59....7."
	    },
	    { // EXPERT
	    "98..26..4....4.....2..7..9.......1........62823...8....619.3....4...2....7..8...6",
	    ".74..1......7.9..8..8.....2..........3.4........2.6..78.....3....7.682..91...78.6",
	    "..51.67.....7.....6.135....2.3.....4.4..6......7..126.529...3...........7..2...1.",
	    "....1....8..4.6....4.539....6489.37......4.....9...5....1.534..6.......1...7..6.3",
	    "5.....1...238.7.597...9.23..7.....25...9.......4.3...1.....3..4.82..4......5.....",
	    ".921.8...1.........38..9....6.......8...5.93.345.....2..942..85.......6..5..9.3..",
	    "62.......93..8..6...8.7...32.....6....9.6..8..1...7.497.1..2..8.6....3.......12..",
	    ".43......8..2..3.7............8.2.6.9.......2..61.7...7.1.3.9..4.....13..9....7.8",
	    "..78.9...4.81.....29....7..5...2....8.29......31.5.2...452........784..1....95...",
	    "........1...9.6...17..4..9.4..5....9..6.27..3..1...2..5...397.6......9...64..5..2",
	    "...9....2....6589.....74...76......521..9.78..5...624...94..............17.....5.",
	    ".7......29....3....854....9.56...3..2...4..8...86...7..6..7...4..486.......2....."
	    }
	    };

	    IM_ASSERT(comboSelectedIndex>=0 && comboSelectedIndex<4);
	    static const int puzzlesSize = sizeof((puzzles[4]))/sizeof((puzzles[4][0]));
	    const float puzzle_rand = (float)puzzlesSize/(float)RAND_MAX;
	    int selectedPuzzle = -1;
	    unsigned char transformationNumber = 0;

	    const bool mustPermutateNumbers = true;		    // Tweakable    (for wider set of puzzles)
	    const bool mustPerformSymmetricTransformations = true;  // Tweakable    (for wider set of puzzles)


#	    ifdef IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
	    do
#	    endif //IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
	    {
	    if (selectedPuzzle>=0)  {
		// Previous puzzle has not been solved. Better tell somebody we have discarded it:
		fprintf(stderr,"solvePuzzle() failed: puzzles[%d][%d] transformationNumber=%d\n",comboSelectedIndex,selectedPuzzle,transformationNumber);
	    }

	    resetAllCells();

	    selectedPuzzle = (int) ((float)rand()*puzzle_rand);
	    IM_ASSERT(selectedPuzzle>=0 && selectedPuzzle<puzzlesSize);
            unsigned char tmp=0;const int lockedState = CS_LOCKED;
	    numOpenCells = CELL_SIZE*CELL_SIZE;char tmpChar='\0';int cnt=0;

	    // (Opt) Number permutations for wider set of puzzles
	    static unsigned char permutation[CELL_SIZE] = {1,2,3,4,5,6,7,8,9};
	    if (mustPermutateNumbers)	{
		// Generate random "permutation"
		const float perm_rand = (float)CELL_SIZE/(float)RAND_MAX;
		unsigned char tmp = 0;bool ok = true;
		for (int i=0;i<CELL_SIZE;i++)   {
		    ok = false;
		    while (!ok)	{
			tmp = (unsigned char) ((float)rand()*perm_rand) + 1;    // +1 because it's in [0,8]
			ok = true;
			for (int j=0;j<i;j++)   {
			    if (tmp==permutation[j]) {ok=false;break;}
			}
		    }
		    permutation[i] = tmp;
		    //fprintf(stderr,"%d",permutation[i]);
		}
		//fprintf(stderr,"\n");
	    }

	    // (Opt) Symmetric transformations for a wider set of puzzles	    
	    if (mustPerformSymmetricTransformations)   {
		const unsigned char numSymmetricTransformations = 8;  // cells[x][y] becomes: 0-NONE:[x][y] 1:[y][x] 2:[8-x][y] 3:[x][8-y] 4:[8-x][8-y]
								      //			    5:[8-y][x] 6:[y][8-x] 7:[8-y][8-x]
		const float transform_rand = (float)numSymmetricTransformations/(float)RAND_MAX;
		transformationNumber = (unsigned char) ((float)rand()*transform_rand);
		IM_ASSERT(transformationNumber<numSymmetricTransformations);
		//fprintf(stderr,"transformationNumber=%d\n",transformationNumber);
	    }

	    // Fill grid with "CS_LOCKED" solution numbers
	    int X=0,Y=0;
	    for (int x=0;x<CELL_SIZE;x++) {
		for (int y=0;y<CELL_SIZE;y++)   {
		    tmpChar = puzzles[comboSelectedIndex][selectedPuzzle][cnt++];
                    if (tmpChar>='1' && tmpChar<='9')   {
                        tmp = tmpChar - '0';
			--numOpenCells;

			if (mustPermutateNumbers) tmp = permutation[tmp-1];
			if (transformationNumber==0) {X=x;Y=y;}
			else switch (transformationNumber)  {
			case 1:	    X=y;			Y=x;			    break;
			case 2:	    X=CELL_SIZE-1-x;	Y=y;			    break;
			case 3:	    X=x;			Y=CELL_SIZE-1-y;	    break;
			case 4:	    X=CELL_SIZE-1-x;	Y=CELL_SIZE-1-y;	    break;
			case 5:	    X=CELL_SIZE-1-y;	Y=x;			    break;
			case 6:	    X=y;			Y=CELL_SIZE-1-x;	    break;
			case 7:	    X=CELL_SIZE-1-y;	Y=CELL_SIZE-1-x;	    break;
			default:IM_ASSERT(true);break;
			}

			SetCellData(cells[X][Y],&lockedState,&tmp,&tmp);    // We copy userNumber==solutionNumber here too
                    }
                }
            }
	    }
#	    ifdef IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
	    while (!solvePuzzle());
	    //OPT: solvePuzzle() solves the puzzle and fills all the cells' solution numbers.
            /* This step requires to code a "Sudoku Solver".
             * However it is not necessary at all.
             * We can just ignore the "solutionNumber" field in all the cells, and just check
             * when "numOpenCells==0" if the numbers the user inserted are a solution (= satisfy the "Sudoku properties": see "bool gameCompleted()").
             * This approach has the additional advantage of working with puzzles with multiple solutions (but
             * of course the inserted puzzle must be solvable).
            */            
#	    endif //IMGUIMINIGAMES_SUDOKU_ADD_SOLVER

	    if (numOpenCells==0) gameWon = gameCompleted(); // Mainly for testing
	    else gameWon = false;

        }

	inline static bool checkThatTheseValuesAreDifferent(const unsigned char v[CELL_SIZE]) {
            unsigned char v1=0,v2=0;
	    for (int i=0;i<CELL_SIZE;i++)   {
		v1 = v[i];if (v1==0 || v1>CELL_SIZE) return false;
		for (int j=i+1;j<CELL_SIZE;j++)   {
                    v2 = v[j];if (v1==v2) return false;
                }
            }
            return true;
        }

        bool gameCompleted() {
	    static unsigned char values[CELL_SIZE]={0,0,0,0,0,0,0,0,0};
            // 1) Check rows:
	    for (int y=0;y<CELL_SIZE;y++)   {
		for (int x=0;x<CELL_SIZE;x++)   GetCellData(cells[x][y],NULL,&values[x]);
                if (!checkThatTheseValuesAreDifferent(values)) {
                    //fprintf(stderr,"row[%d] wrong\n",y);
                    return false;
                }
            }
            // 2) Check columns:
	    for (int x=0;x<CELL_SIZE;x++)   {
		for (int y=0;y<CELL_SIZE;y++)   GetCellData(cells[x][y],NULL,&values[y]);
                if (!checkThatTheseValuesAreDifferent(values))  {
                    //fprintf(stderr,"column[%d] wrong\n",x);
                    return false;
                }
            }
            // 3 Check the groups:
	    const int groupPerRow = SQRT_CELL_SIZE;
            int cnt=0,totalCnt=0;
            for (int ci=0;ci<groupPerRow;ci++)  {
                for (int ri=0;ri<groupPerRow;ri++)  {
                    cnt = 0;
                    for (int gy=ci*groupPerRow,gysz=gy+groupPerRow;gy<gysz;gy++)   {
                       for (int gx=ri*groupPerRow,gxsz=gx+groupPerRow;gx<gxsz;gx++)   {
                           GetCellData(cells[gx][gy],NULL,&values[cnt++]);
                       }
                    }
		    IM_ASSERT(cnt==CELL_SIZE);
                    ++totalCnt;
                    if (!checkThatTheseValuesAreDifferent(values))  {
                        //fprintf(stderr,"group[%d][%d] wrong\n",ri,ci);
                        return false;
                    }
                }
            }
	    IM_ASSERT(totalCnt==CELL_SIZE);

            //fprintf(stderr,"Game Won\n");
            return true;
        }

#	ifdef IMGUIMINIGAMES_SUDOKU_ADD_SOLVER	// solve Algo adapted from http://codereview.stackexchange.com/questions/13677/solving-sudoku-using-backtracking
	inline bool isNumberAvailableForCell(int row, int col, unsigned char num) {
	    //checking in the grid
	    int rowStart = (row/SQRT_CELL_SIZE) * SQRT_CELL_SIZE;
	    int colStart = (col/SQRT_CELL_SIZE) * SQRT_CELL_SIZE;

	    for(int i=0; i<CELL_SIZE; ++i)  {
		if (GetCellSolutionNumber(cells[row][i]) == num)				    return false;
		if (GetCellSolutionNumber(cells[i][col]) == num)				    return false;
		if (GetCellSolutionNumber(cells[rowStart + (i%SQRT_CELL_SIZE)][colStart + (i/SQRT_CELL_SIZE)]) == num)    return false;
	    }

	    return true;
	}

	bool solvePuzzle(int row=0, int col=0)	{
	    if (col >= CELL_SIZE)   {
		col = 0; ++row;
		if (row >= CELL_SIZE)  return true;
	    }

	    if( GetCellSolutionNumber(cells[row][col]) != 0 ) //pre filled [Should we check for CS_LOCKED instead ?]
	    {
		return solvePuzzle(row, col+1);
	    }
	    else    {
		for(unsigned char i=0; i<CELL_SIZE; ++i)	{
		    if( isNumberAvailableForCell(row, col, i+1) )   {
			SetCellSolutionNumber(cells[row][col],i+1);

			if (solvePuzzle(row, col +1)) return true;
			SetCellSolutionNumber(cells[row][col],0);
		    }
		}
	    }
	    return false;
	}
#	endif //IMGUIMINIGAMES_SUDOKU_ADD_SOLVER


        inline void DrawAnnotation(Sudoku::Style& style, ImVec2 annotOffset, char* charNum, ImVec2 start, ImDrawList* draw_list, float textLineHeight, unsigned char annotationNumber, float* glyphWidths) {
            charNum[0] = '0'+annotationNumber;
            float& glyphWidth = glyphWidths[annotationNumber-1];if (glyphWidth==-1.f) glyphWidth = ImGui::CalcTextSize(charNum).x;
            draw_list->AddText(GImGui->Font,GImGui->FontSize*0.3334f,start+annotOffset+ImVec2((textLineHeight-glyphWidth)*(0.334f*0.5f),0.f),style.colors[Sudoku::Style::Color_Annotations],charNum);
        }

        void render() {
            Sudoku::Style& style = Sudoku::Style::Get();

            if (!inited) {
                inited = true;
                srand(ImGui::GetTime()*10.f);
                initNewGame();
            }

            ImU32 colorText = style.colors[Sudoku::Style::Color_Text];
            if (colorText>>24==0) colorText = ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_Text]);

            ImGui::PushID(this);
            bool mustReInit = false;
            ImGui::BeginGroup();
            if (gamePhase == GP_Playing) {
                if (ImGui::Button("Quit Game##SudokuQuitGame")) {gamePhase = GP_Titles;mustReInit=true;}
            }
            else if (gamePhase == GP_GameOver) {
                if (ImGui::Button("New Game##SudokuQuitGame")) {gamePhase = GP_Titles;mustReInit=true;}
            }
            if (gamePhase == GP_Titles || mustReInit) {
		static const char* Types[] = {"Simple","Easy","Intermediate","Expert"};//,"Manual"};
                ImGui::PushItemWidth(ImGui::GetWindowWidth()*0.35f);
                if (mustReInit || ImGui::Combo("Game Type##SudokuGameType",&comboSelectedIndex,Types,sizeof(Types)/sizeof(Types[0]),sizeof(Types)/sizeof(Types[0])))   {
                    resetVariables();   // does not touch comboSelectedIndex                    
                    mustReInit = true;
                    initNewGame();
                }
                ImGui::PopItemWidth();
            }
            ImGui::EndGroup();

            ImGui::SameLine(ImGui::GetWindowWidth()*0.35f);

            ImGui::BeginGroup();
            if (gamePhase != GP_Titles && !mustReInit) {
                if (gamePhase == GP_Playing) {
                    int newFrame = ImGui::GetFrameCount();
                    if (newFrame==frameCount+1 && !paused
        #               ifdef IMGUI_USE_AUTO_BINDING
                            && !gImGuiPaused && !gImGuiWereOutsideImGui
        #               endif //IMGUI_USE_AUTO_BINDING
                            ) {
                        currentTime = ImGui::GetTime() - startTime;
                    }
                    else startTime = ImGui::GetTime() - currentTime;
                    frameCount = newFrame;
                }
                const unsigned int minutes = (unsigned int)currentTime/60;
                const unsigned int seconds = (unsigned int)currentTime%60;
                ImGui::Text("Time:  %um:%2us",minutes,seconds);
		//ImGui::SameLine(0,20);ImGui::Text("Cells: %d",numOpenCells);
            }
            ImGui::EndGroup();

            ImGui::PopID();

	    ImVec2 gridSize(CELL_SIZE,CELL_SIZE);

            // Mine, Flag, [1,8]
            float glyphWidths[9] = {-1.f,-1.f,-1.f,-1.f,-1.f,-1.f,-1.f,-1.f,-1.f};

            ImGuiIO& io = ImGui::GetIO();
            if (mustReInit) ImGui::SetNextWindowFocus();
            ImGui::PushStyleColor(ImGuiCol_ChildWindowBg, ImGui::ColorConvertU32ToFloat4(style.colors[Sudoku::Style::Color_Background]));
            ImGui::BeginChild("Sudoku Game Scrolling Region", ImVec2(0,0), false,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);

            // Following line is important if we want to avoid clicking on the window just to get the focus back (AFAICS, but there's probably some better way...)
            const bool isFocused = ImGui::IsWindowFocused() || ImGui::IsRootWindowFocused() || (ImGui::GetParentWindow() && ImGui::GetParentWindow()->Active);
            const bool isHovered = ImGui::IsWindowHovered();
            ImGuiWindow* window = ImGui::GetCurrentWindow();

            bool LMBclick = false, RMBclick = false, isPclicked = false;
            if (isFocused && isHovered && !mustReInit) {
                LMBclick = ImGui::IsMouseClicked(0);
                RMBclick = ImGui::IsMouseReleased(1);
                isPclicked = ImGui::IsKeyPressed(style.keyPause,false);
                if (isPclicked && gamePhase == GP_Playing) paused=!paused;
            }


            float textLineHeight = ImGui::GetTextLineHeight();
            ImVec2 gridDimensions = gridSize * (textLineHeight+window->FontWindowScale)  + ImVec2(0.1f*textLineHeight,0.1f*textLineHeight);
            ImVec2 gridOffset(0,0);
            if (gridDimensions.x!=window->Size.x && gridDimensions.y!=window->Size.y) {
                ImVec2 ratios(gridDimensions.x/window->Size.x,gridDimensions.y/window->Size.y);
                // Fill X or Y Window Size
                window->FontWindowScale/= (ratios.x>=ratios.y) ? ratios.x : ratios.y;

                textLineHeight = ImGui::GetTextLineHeight();
                gridDimensions = gridSize * (textLineHeight+window->FontWindowScale);// + ImVec2(0.1f*textLineHeight,0.1f*textLineHeight);
            }
            if (gridDimensions.x<window->Size.x) gridOffset.x = (window->Size.x-gridSize.x*(textLineHeight+window->FontWindowScale))*0.5f;
            if (gridDimensions.y<window->Size.y) gridOffset.y = (window->Size.y-gridSize.y*(textLineHeight+window->FontWindowScale))*0.5f;



            ImDrawList* draw_list = ImGui::GetWindowDrawList();

            ImVec2 canvasSize = window->Size;
            ImVec2 win_pos = ImGui::GetCursorScreenPos();

            if (gamePhase==GP_Playing && (/*!isFocused ||*/ paused /*|| !ImGui::IsMouseHoveringWindow()*/
#                                       ifdef IMGUI_USE_AUTO_BINDING
                                          || gImGuiPaused || gImGuiWereOutsideImGui
#                                       endif //IMGUI_USE_AUTO_BINDING
                                          ))   {
                // Game Paused here

                if (((unsigned)(ImGui::GetTime()*10.f))%10<5)  {
                    // Display "PAUSED":-------------------
                    static const char pausedText[] = "PAUSED";
                    const ImVec2 textSize = ImGui::CalcTextSize(pausedText);
                    const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX(),(canvasSize.y-textSize.y)*0.15f+ImGui::GetScrollY());
                    const ImVec2 enlargement(textLineHeight*0.25f,0.f);
                    ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Sudoku::Style::Color_CellBackground],style.colors[Sudoku::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
                    draw_list->AddText(start,colorText,pausedText);
                    //--------------------------------------
                }

                // Display controls in a smaller font:
		static const char controlsText[] = "CONTROLS:\n\nLMB: write annotations.\nMW or RMB: set a cell number and\n    toggle annotations on and off.";
                const float fontScaling = 0.35f;
                const ImVec2 textSize = ImGui::CalcTextSize(controlsText)*fontScaling;
                const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX(),(canvasSize.y-textSize.y)*0.65f+ImGui::GetScrollY());
                const ImVec2 enlargement(textLineHeight*0.25f,0.f);
                ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Sudoku::Style::Color_CellBackground],style.colors[Sudoku::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
                draw_list->AddText(GImGui->Font,GImGui->FontSize*fontScaling,start,colorText,controlsText);


            }
            else {


                const ImU32& GRID_COLOR = style.colors[Sudoku::Style::Color_Grid];
                const float GRID_SZ = textLineHeight+window->FontWindowScale;//32.f;
                const ImVec2 grid_len = gridSize * GRID_SZ;
                const float grid_Line_width = window->FontWindowScale;

                // Display Cell Background
                draw_list->AddRectFilled(win_pos+gridOffset,win_pos+gridOffset+grid_len+ImVec2(grid_Line_width,grid_Line_width),style.colors[Sudoku::Style::Color_CellBackground]);

                // Display grid ---------------------------------------------------------------------------------------------
		const int zoneLineStep = SQRT_CELL_SIZE;
                const float gridShadowOffset = grid_Line_width*0.5f;
                int cnt = 0;
                // Draw Y shadow-lines
                for (float x = gridOffset.x,xsz=gridOffset.x+grid_len.x+GRID_SZ;    x<=xsz;  x+=GRID_SZ)    {
		    if (cnt<CELL_SIZE)  draw_list->AddLine(ImVec2(x+gridShadowOffset,gridOffset.y)+win_pos, ImVec2(x+gridShadowOffset,gridOffset.y+grid_len.y)+win_pos, style.colors[Sudoku::Style::Color_GridShadow],grid_Line_width);
                    if (cnt++>=gridSize.x) break;
                }
                // Draw X shadow-lines
                cnt = 0;
                for (float y = gridOffset.y,ysz=gridOffset.y+grid_len.y+GRID_SZ;    y<=ysz; y+=GRID_SZ)    {
		    if (cnt<CELL_SIZE)  draw_list->AddLine(ImVec2(gridOffset.x,y+gridShadowOffset)+win_pos, ImVec2(gridOffset.x+grid_len.x,y+gridShadowOffset)+win_pos, style.colors[Sudoku::Style::Color_GridShadow],grid_Line_width);
                    if (cnt++>=gridSize.y) break;
                }
                // Draw Y lines
                cnt = 0;
                for (float x = gridOffset.x,xsz=gridOffset.x+grid_len.x+GRID_SZ;    x<=xsz;  x+=GRID_SZ)    {
                    if (cnt%zoneLineStep!=0)    draw_list->AddLine(ImVec2(x,gridOffset.y)+win_pos, ImVec2(x,gridOffset.y+grid_len.y)+win_pos, GRID_COLOR,grid_Line_width);
                    if (cnt++>=gridSize.x) break;
                }
                // Draw X lines
                cnt = 0;
                for (float y = gridOffset.y,ysz=gridOffset.y+grid_len.y+GRID_SZ;    y<=ysz; y+=GRID_SZ)    {
                    if (cnt%zoneLineStep!=0)    draw_list->AddLine(ImVec2(gridOffset.x,y)+win_pos, ImVec2(gridOffset.x+grid_len.x,y)+win_pos, GRID_COLOR,grid_Line_width);
                    if (cnt++>=gridSize.y) break;
                }
                // Draw Y border-lines
                cnt = 0;
                for (float x = gridOffset.x,xsz=gridOffset.x+grid_len.x+GRID_SZ;    x<=xsz;  x+=GRID_SZ)    {
                    if (cnt%zoneLineStep==0)    draw_list->AddLine(ImVec2(x,gridOffset.y)+win_pos, ImVec2(x,gridOffset.y+grid_len.y)+win_pos, style.colors[Sudoku::Style::Color_GridZone],grid_Line_width);
                    if (cnt++>=gridSize.x) break;
                }
                // Draw X border-lines
                cnt = 0;
                for (float y = gridOffset.y,ysz=gridOffset.y+grid_len.y+GRID_SZ;    y<=ysz; y+=GRID_SZ)    {
                    if (cnt%zoneLineStep==0)    draw_list->AddLine(ImVec2(gridOffset.x,y)+win_pos, ImVec2(gridOffset.x+grid_len.x,y)+win_pos, style.colors[Sudoku::Style::Color_GridZone],grid_Line_width);
                    if (cnt++>=gridSize.y) break;
                }
                //------------------------------------------------------------------------------------------------------------

                // Detect the cell under the mouse.
                int mouseCellColumn = -1, mouseCellRow = -1;
                int mouseCellState = 0;
                unsigned char mouseCellSolutionValue=0,mouseCellUserValue=0;
                bool isMouseCellValid = false;
                if (isFocused && isHovered && !mustReInit) {
                    ImVec2 mp = ImGui::GetMousePos() - win_pos - gridOffset;
                    if (mp.x>0 && mp.y>0)   {
                        mouseCellColumn = mp.x/GRID_SZ;
                        mouseCellRow = mp.y/GRID_SZ;
                        // Note that here we return isMouseCellValid = false if the cell is CS_LOCKED!
                        if (mouseCellRow>=gridSize.y || mouseCellColumn>=gridSize.x || ((*GetCellData(cells[mouseCellRow][mouseCellColumn],&mouseCellState,&mouseCellUserValue,&mouseCellSolutionValue))&CS_LOCKED)) {mouseCellColumn=mouseCellRow=-1;mouseCellState=0;}
                        else isMouseCellValid = true;
                    }
                    if (isMouseCellValid) {
                        if (gamePhase != GP_GameOver)  {
                            // Let's draw the hovered cell:
                            ImVec2 start(win_pos+gridOffset+ImVec2(grid_Line_width,grid_Line_width)+ImVec2(mouseCellColumn*GRID_SZ,mouseCellRow*GRID_SZ));
                            draw_list->AddRectFilled(start,start+ImVec2(textLineHeight,textLineHeight),style.colors[Sudoku::Style::Color_HoveredCellBackground]);

                            bool mustCheckForGameWon = false;
                            if (io.MouseWheel!=0)   {
                                if (mouseCellUserValue==0) --numOpenCells;  // value won't be zero anymore
                                if (io.MouseWheel>0) {
                                    ++mouseCellUserValue;if (mouseCellUserValue>9) mouseCellUserValue=0;
                                }
                                else {
                                    if (mouseCellUserValue==0) mouseCellUserValue=9;
                                    else --mouseCellUserValue;
                                }
                                if (mouseCellUserValue==0) ++numOpenCells;  // value was not zero before
                                SetCellData(cells[mouseCellRow][mouseCellColumn],NULL,&mouseCellUserValue);
                                if (numOpenCells==0)   mustCheckForGameWon = true;
                            }
                            else if ((LMBclick || RMBclick) && mouseCellUserValue==0)  {
                                // We must fetch the annotation and toggle it here:
                                const int annColumn =  (mp.x - ((float)mouseCellColumn*(float)GRID_SZ))*3.f/(float)GRID_SZ;
                                const int annRow =  (mp.y - ((float)mouseCellRow*(float)GRID_SZ))*3.f/(float)GRID_SZ;
                                const int annNumber = annRow*3+annColumn+1;
                                IM_ASSERT(annNumber>=1 && annNumber<=9);
                                if (LMBclick) {
                                    switch (annNumber)  {
                                    case 1: mouseCellState^=CS_ANNOTATION_1;break;
                                    case 2: mouseCellState^=CS_ANNOTATION_2;break;
                                    case 3: mouseCellState^=CS_ANNOTATION_3;break;
                                    case 4: mouseCellState^=CS_ANNOTATION_4;break;
                                    case 5: mouseCellState^=CS_ANNOTATION_5;break;
                                    case 6: mouseCellState^=CS_ANNOTATION_6;break;
                                    case 7: mouseCellState^=CS_ANNOTATION_7;break;
                                    case 8: mouseCellState^=CS_ANNOTATION_8;break;
                                    case 9: mouseCellState^=CS_ANNOTATION_9;break;
                                    }
                                    SetCellData(cells[mouseCellRow][mouseCellColumn],&mouseCellState);
                                }
                                else if (RMBclick)  {
                                    mouseCellUserValue = annNumber;
                                    SetCellData(cells[mouseCellRow][mouseCellColumn],NULL,&mouseCellUserValue);
				    --numOpenCells;
                                    if (numOpenCells==0)   mustCheckForGameWon = true;
                                }
                            }
                            else if (RMBclick && mouseCellUserValue!=0) {
                                mouseCellUserValue=0;
				SetCellData(cells[mouseCellRow][mouseCellColumn],NULL,&mouseCellUserValue);
				++numOpenCells;//if (numOpenCells==0)   mustCheckForGameWon = true;
                            }

                            if (gamePhase==GP_Titles && (LMBclick || RMBclick || io.MouseWheel!=0)) {
                                gamePhase = GP_Playing;
                                //Reset game data (without resetting the puzzle) here:
                                currentTime = 0;
                                frameCount = ImGui::GetFrameCount();
                                startTime = ImGui::GetTime();
                                gameWon = false;
                            }
                            if (mustCheckForGameWon)    {
                                gameWon = gameCompleted();
                                if (gameWon) gamePhase = GP_GameOver;
                            }
                        }
                        //fprintf(stderr,"Clicked cell[c:%d][r:%d].\n",mouseCellColumn,mouseCellRow);
                    }

                }


                // draw cells:
                int state=0;unsigned char userNumber=0,solutionNumber=0,annotationNumber=0;
                float annotGlyphWidth=0;ImVec2 annotOffset(0,0);
                const float annotCellSize = (GRID_SZ-grid_Line_width)*0.3334f;
                static char charNum[2] = "0";
                const ImVec2 baseStart = win_pos+gridOffset+ImVec2(0.f/*grid_Line_width*/,grid_Line_width);
                ImVec2 start(0,0);
#		ifdef IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
		const bool enableCheat = true;  // [true: with CTRL-down the solution is shown: but needs a solver]
#		endif //IMGUIMINIGAMES_SUDOKU_ADD_SOLVER

		for (ImU32 c=0;c<CELL_SIZE;c++)  {
		    for (ImU32 r=0;r<CELL_SIZE;r++)  {
                        GetCellData(cells[r][c],&state,&userNumber,&solutionNumber);

                        start = baseStart+ImVec2(c*GRID_SZ,r*GRID_SZ);

                        // Optional line to paint cell bg color
                        // draw_list->AddRectFilled(start+ImVec2(grid_Line_width,0.f),start+ImVec2(grid_Line_width+textLineHeight,textLineHeight),style.colors[Sudoku::Style::Color_OpenCellBackground]);

			if ((state&CS_LOCKED)
#			ifdef IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
			|| (enableCheat && io.KeyCtrl)
#			endif //IMGUIMINIGAMES_SUDOKU_ADD_SOLVER
			)	{
                            // Draw solutionNumber
                            IM_ASSERT(solutionNumber>0 && solutionNumber<10);
                            charNum[0] = '0'+solutionNumber;
                            float& glyphWidth = glyphWidths[solutionNumber-1];if (glyphWidth==-1.f) glyphWidth = ImGui::CalcTextSize(charNum).x;
                            draw_list->AddText(start+ImVec2((textLineHeight-glyphWidth)*0.5f,0.f),style.colors[Sudoku::Style::Color_InitialNumbers],charNum);
                        }
                        if (gamePhase != GP_Titles && !(state&CS_LOCKED)) {
                            if (userNumber==0)    {
                                if (state&CS_ANNOTATION_PRESENT)	{
                                    // Draw all annotations
                                    annotationNumber = 0;annotGlyphWidth=0;
                                    if (state&CS_ANNOTATION_1) {
                                        annotationNumber=1;annotOffset.x=0.f;annotOffset.y=0.f;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_2) {
                                        annotationNumber=2;annotOffset.x=annotCellSize;annotOffset.y=0.f;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_3) {
                                        annotationNumber=3;annotOffset.x=annotCellSize*2.f;annotOffset.y=0.f;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_4) {
                                        annotationNumber=4;annotOffset.x=0.f;annotOffset.y=annotCellSize;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_5) {
                                        annotationNumber=5;annotOffset.x=annotCellSize;annotOffset.y=annotCellSize;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_6) {
                                        annotationNumber=6;annotOffset.x=annotCellSize*2.f;annotOffset.y=annotCellSize;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_7) {
                                        annotationNumber=7;annotOffset.x=0.f;annotOffset.y=annotCellSize*2.f;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_8) {
                                        annotationNumber=8;annotOffset.x=annotCellSize;annotOffset.y=annotCellSize*2.f;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                    if (state&CS_ANNOTATION_9) {
                                        annotationNumber=9;annotOffset.x=annotCellSize*2.f;annotOffset.y=annotCellSize*2.f;
                                        DrawAnnotation(style, annotOffset, charNum, start, draw_list, textLineHeight, annotationNumber, glyphWidths);
                                    }
                                }
                            }
                            else {
                                // Draw userNumber
                                IM_ASSERT(userNumber>0 && userNumber<10);
                                charNum[0] = '0'+userNumber;
                                float& glyphWidth = glyphWidths[userNumber-1];if (glyphWidth==-1.f) glyphWidth = ImGui::CalcTextSize(charNum).x;
                                draw_list->AddText(start+ImVec2((textLineHeight-glyphWidth)*0.5f,0.f),style.colors[Sudoku::Style::Color_Numbers],charNum);
                            }
                        }
                    }
                }




                // Draw end game messages
                if (gamePhase==GP_GameOver) {
                    const float elapsedSeconds = (float)(ImGui::GetTime()-currentTime-startTime);
                    if (gameWon) {
                        static char gameWonText[256] = "";
                        sprintf(gameWonText,"GAME COMPLETED\nTIME: %um : %us",((unsigned)currentTime)/60,((unsigned)currentTime)%60);
                        const ImVec2 textSize = ImGui::CalcTextSize(gameWonText);
                        ImVec2 deltaPos(0.f,0.f);
                        if (elapsedSeconds<10.f) {
                            deltaPos.x = canvasSize.x * sin(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                            deltaPos.y = canvasSize.y * cos(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                        }
                        const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX()+deltaPos.x,(canvasSize.y-textSize.y)*0.5f+ImGui::GetScrollY()-deltaPos.y);
                        const ImVec2 enlargement(textLineHeight*0.25f,0.f);
                        ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Sudoku::Style::Color_CellBackground],style.colors[Sudoku::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
                        draw_list->AddText(start,colorText,gameWonText);
                    }
                    else {
                        static const char gameOverText[] = "GAME\nOVER";
                        const ImVec2 textSize = ImGui::CalcTextSize(gameOverText);
                        ImVec2 deltaPos(0.f,0.f);
                        if (elapsedSeconds<10.f) {
                            deltaPos.x = canvasSize.x * sin(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                            deltaPos.y = canvasSize.y * cos(2.f*elapsedSeconds) * (10.f-elapsedSeconds)*0.025f;
                        }
                        const ImVec2 start = win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX()+deltaPos.x,(canvasSize.y-textSize.y)*0.5f+ImGui::GetScrollY()-deltaPos.y);
                        const ImVec2 enlargement(textLineHeight*0.25f,0.f);
                        ImDrawListAddRect(draw_list,start-enlargement,start+textSize+enlargement,style.colors[Sudoku::Style::Color_CellBackground],style.colors[Sudoku::Style::Color_Grid],4.f,0x0F,window->FontWindowScale,true);
                        draw_list->AddText(start,colorText,gameOverText);
                    }
                }
                /*const ImVec2 textSize = ImGui::CalcTextSize(title);
            const ImU32 col = IM_COL32(0,255,0,255);
            draw_list->AddText(win_pos+ImVec2((canvasSize.x-textSize.x)*0.5f+ImGui::GetScrollX(),(canvasSize.y-textSize.y)*0.5f+ImGui::GetScrollY()),col,title);*/

                //if (isMouseDraggingForScrolling) scrolling = scrolling - io.MouseDelta;
            }

            // Sets scrollbars properly:
            ImGui::SetCursorPos(ImGui::GetCursorPos() + gridSize * (textLineHeight+window->FontWindowScale) + ImVec2(0.1f*textLineHeight,0.1f*textLineHeight));

            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

    };

    const char* SudokuHS::Title = "Sudoku Game";

    Sudoku::Sudoku() : imp(NULL) {
        imp = (SudokuHS*) ImGui::MemAlloc(sizeof(SudokuHS));
        IM_PLACEMENT_NEW(imp) SudokuHS();
        IM_ASSERT(imp);
    }
    Sudoku::~Sudoku() {
        if (imp)    {
            imp->~SudokuHS();
            ImGui::MemFree(imp);
        }
    }
    void Sudoku::render() {
        if (imp) imp->render();
    }

    Sudoku::Style::Style()    {    
        colors[Style::Color_Text] =                     IM_COL32(0,136,0,255);
        colors[Style::Color_Background] =               IM_COL32_BLACK_TRANS;//IM_COL32(242,241,240,255);
        colors[Style::Color_GridShadow] =           IM_COL32(138,141,134,170);
        colors[Style::Color_CellBackground] =       IM_COL32(222,222,220,255);

        colors[Color_Numbers]=colors[Style::Color_InitialNumbers] = IM_COL32_BLACK;
        colors[Style::Color_Annotations] = (((colors[Color_Numbers]>>24)/2)<<24) | (colors[Color_Numbers]&0x00FFFFFF);  // How to do this using IM_COL32_A_MASK ?

        colors[Style::Color_Grid] = IM_COL32(178,177,192,170);//colors[Style::Color_Background];
        colors[Style::Color_GridZone] = IM_COL32(128,128,0,255);

        const ImVec4 tmp1 = ImGui::ColorConvertU32ToFloat4(colors[Style::Color_GridShadow]);
        const ImVec4 tmp2 = ImGui::ColorConvertU32ToFloat4(colors[Style::Color_CellBackground]);
        const ImVec4 tmp((tmp1.x+tmp2.x)*0.5f,(tmp1.y+tmp2.y)*0.5f,(tmp1.z+tmp2.z)*0.5f,(tmp1.w+tmp2.w)*0.5f);
        colors[Color_HoveredCellBackground] = ImGui::ColorConvertFloat4ToU32(tmp);

        keyPause = (int) 'p';
    }
#   ifndef NO_IMGUIMINIGAMES_MINE
    void Sudoku::Style::setFromMineGameStyle(const Mine::Style& ms) {
        colors[Style::Color_Text] =   ms.colors[Mine::Style::Color_Text];
        colors[Color_Numbers] = colors[Style::Color_InitialNumbers] =   ms.colors[Mine::Style::Color_Mine];
        colors[Style::Color_Background]                             =   ms.colors[Mine::Style::Color_Background];
        colors[Style::Color_GridShadow]                   =   ms.colors[Mine::Style::Color_ClosedCellBackground];
        colors[Style::Color_CellBackground]                     =   ms.colors[Mine::Style::Color_OpenCellBackground];
        colors[Style::Color_Grid]                                   =   ms.colors[Mine::Style::Color_Grid];
        colors[Style::Color_HoveredCellBackground]                  =   ms.colors[Mine::Style::Color_HoveredCellBackground];
        keyPause = ms.keyPause;

        colors[Style::Color_Annotations] = (((colors[Color_Numbers]>>24)/2)<<24) | (colors[Color_Numbers]&0x00FFFFFF);  // How to do this using IM_COL32_A_MASK ?
        colors[Style::Color_GridZone] = IM_COL32(224,224,0,255);

    }
#   endif //NO_IMGUIMINIGAMES_MINE

    Sudoku::Style Sudoku::Style::style;
#   endif //NO_IMGUIMINIGAMES_SUDOKU

} // namespace ImGuiMiniGames



// KeyboardWindow by Floooh (Oryol) - Could be helpful if we implement hi-score support
// #define REFERENCE_CODE
#ifdef REFERENCE_CODE
class KeyboardWindow : public WindowBase {
    OryolClassDecl(KeyboardWindow);
public:
    /// setup the window
    virtual void Setup(yakc::kc85& kc) override;
    /// draw method
    virtual bool Draw(yakc::kc85& kc) override;

    bool shift = false;
    bool caps_lock = false;
};
struct key {
    key() : pos(0.0f), name(nullptr), code(0), shift_code(0) { };
    key(float p, const char* n, ubyte c, ubyte sc) : pos(p), name(n), code(c), shift_code(sc) { };
    float pos;
    const char* name;
    ubyte code;
    ubyte shift_code;
};

// the 5 'main section' rows of keys:
// see: http://www.mpm-kc85.de/dokupack/KC85_3_uebersicht.pdf
static const int num_rows = 5;
static const int num_cols = 13;
static struct key layout[num_rows][num_cols] = {
{
    // function keys row
    {4,"F1",0xF1,0xF7}, {0,"F2",0xF2,0xF8}, {0,"F3",0xF3,0xF9}, {0,"F4",0xF4,0xFA}, {0,"F5",0xF5,0xFB}, {0,"F6",0xF6,0xFC},
    {0,"BRK",0x03,0x03}, {0,"STP",0x13,0x13}, {0,"INS",0x1A,0x14}, {0,"DEL",0x1F,0x02}, {0,"CLR",0x01,0x0F}, {0,"HOM",0x10,0x0C}
},
{
    // number keys row
    {4,"1 !",'1','!'}, {0,"2 \"",'2','\"'}, {0,"3 #",'3','#'}, {0,"4 $",'4','$'}, {0,"5 %",'5','%'}, {0,"6 &",'6','&'},
    {0,"7 '",'7',0x27}, {0,"8 (",'8','('}, {0,"9 )",'9',')'}, {0,"0 @",'0','@'}, {0,": *",':','*'}, {0,"- =",'-','='},
    {2,"CUU",0x0B,0x11}
},
{
    // QWERT row
    {16,"Q",'Q','q'}, {0,"W",'W','w'}, {0,"E",'E','e'}, {0,"R",'R','r'}, {0,"T",'T','t'}, {0,"Z",'Z','z'},
    {0,"U",'U','u'}, {0,"I",'I','i'}, {0,"O",'O','o'}, {0,"P",'P','p'}, {0,"\x5E \x5D",0x5E,0x5D},
    {10,"CUL",0x08,0x19},{0,"CUR",0x09,0x18}
},
{
    // ASDF row
    {0,"CAP",0x16,0x16}, {0,"A",'A','a'}, {0,"S",'S','s'}, {0,"D",'D','d'}, {0,"F",'F','f'}, {0,"G",'G','g'},
    {0,"H",'H','h'}, {0,"J",'J','j'}, {0,"K",'K','k'}, {0,"L",'L','l'}, {0,"+ ;",'+',';'}, {0,"\x5F |",0x5F,0x5C},
    {14,"CUD",0x0A,0x12}
},
{
    // YXCV row (NOTE: shift-key has special code which is not forwarded as key!
    {10,"SHI",0xFF,0xFF}, {0,"Y",'Y','y'}, {0,"X",'X','y'}, {0,"C",'C','c'}, {0,"V",'V','v'}, {0,"B",'B','b'},
    {0,"N",'N','n'}, {0,"M",'M','m'}, {0,", <",',','<'}, {0,". >",'.','>'}, {0,"/ ?",'/','?'},
    {36,"RET",0x0D,0x0D}
}

};

//------------------------------------------------------------------------------
void
KeyboardWindow::Setup(kc85& kc) {
    this->setName("Keyboard");
}

//------------------------------------------------------------------------------
bool
KeyboardWindow::Draw(kc85& kc) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.75f);
    ImGui::SetNextWindowSize(ImVec2(572, 196));
    if (ImGui::Begin(this->title.AsCStr(), &this->Visible, ImGuiWindowFlags_NoResize|ImGuiWindowFlags_ShowBorders)) {

        // main section keys
        const ImVec2 size(32,24);
        for (int row = 0; row < num_rows; row++) {
            for (int col = 0; col < num_cols; col++) {
                const key& k = layout[row][col];
                if (k.name) {
                    if (col != 0) {
                        ImGui::SameLine();
                    }
                    if (k.pos > 0.0f) {
                        ImGui::Dummy(ImVec2(k.pos,0.0f)); ImGui::SameLine();
                    }
                    if (ImGui::Button(k.name, size)) {
                        // caps lock?
                        if (k.code == 0x16) {
                            this->caps_lock = !this->caps_lock;
                            this->shift = this->caps_lock;
                        }
                        // shift?
                        if (k.code == 0xFF) {
                            this->shift = true;
                        }
                        else {
                            kc.put_key(this->shift ? k.shift_code:k.code);

                            // clear shift state after one key, unless caps_lock is on
                            if (!this->caps_lock) {
                                this->shift = false;
                            }
                        }
                    }
                }
            }
        }

        // space bar
        ImGui::Dummy(ImVec2(80,0)); ImGui::SameLine();
        if (ImGui::Button("SPACE", ImVec2(224, 0))) {
            kc.put_key(this->caps_lock ? 0x5B : 0x20);
        }
    }
    ImGui::End();
    ImGui::PopStyleVar();
    return this->Visible;
}
#endif //REFERENCE_CODE


