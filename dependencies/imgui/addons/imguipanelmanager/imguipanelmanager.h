#ifndef IMGUIPANELMANAGER_H_
#define IMGUIPANELMANAGER_H_

#include "../imguitoolbar/imguitoolbar.h"

namespace ImGui {


struct PanelManager {
    enum Position {
        LEFT=   0,
        RIGHT=  1,
        TOP=    2,
        BOTTOM= 3
    };
    struct WindowData {
        const char* name;
        const ImVec2& pos;
        const ImVec2& size;
        const bool isToggleWindow;  // toggle windows are not "docked" (you can safely ignore "pos" and "size")
        const Position dockPos;

        float& length;      // can be adjusted to resize the window (usable when isToggleWindow=false)
        bool& open;         // can be set to "false" to close the window
        bool& persistFocus; // must be set to "true" when mouse is inside window (usable when isToggleWindow=false)

        void* userData;
        bool isResizing;
        WindowData(const char* _name,const ImVec2& _pos,const ImVec2& _size,const bool _isToggleWindow,const Position _dockPos,
                   float& _length,bool& _open,bool& _persistFocus,void* _userData)
        : name(_name),pos(_pos),size(_size),isToggleWindow(_isToggleWindow),dockPos(_dockPos),
          length(_length),open(_open),persistFocus(_persistFocus),userData(_userData),isResizing(false)
        {}
    };
    typedef void (*WindowDrawerDelegate)(WindowData& wd);

    struct Pane {
        Toolbar bar;
        struct AssociatedWindow {
            const char* windowName;   // reference
            mutable float size;
            mutable float sizeHoverMode;
            WindowDrawerDelegate windowDrawer;
            void* userData;
            mutable ImGuiWindowFlags extraWindowFlags;

            // protected:
            mutable bool dirty;
            mutable ImVec2 curPos;
            mutable ImVec2 curSize;
            mutable bool persistHoverFocus;
            mutable bool draggingStarted;

            AssociatedWindow(const char* _windowName=NULL,float _size=-1,WindowDrawerDelegate _windowDrawer=NULL,void* _userData=NULL,ImGuiWindowFlags _extraWindowFlags=0) : windowName(_windowName),size(_size),sizeHoverMode(_size),windowDrawer(_windowDrawer),userData(_userData),extraWindowFlags(_extraWindowFlags),
            dirty(true),curPos(0,0),curSize(0,0),persistHoverFocus(false),draggingStarted(false)
             {}
            bool isValid() const {return windowDrawer!=NULL && windowName!=NULL;}

            void updateSizeInHoverMode(const PanelManager& mgr,const Pane& pane,size_t winAndBarIndex) const;
            void draw(const PanelManager& mgr,const Pane& pane,size_t winAndBarIndex) const;
        };
        ImVector<AssociatedWindow> windows; // must be one per "bar" button
        Position pos;
        bool visible;
        bool allowHoverOnTogglableWindows;
        mutable float hoverReleaseTimer;
        mutable int hoverReleaseIndex;
        Pane(Position _pos,const char* name)
        : bar(name),pos(_pos),visible(true),allowHoverOnTogglableWindows(false),hoverReleaseTimer(-1),hoverReleaseIndex(-1)
        {}
        ~Pane() {clear();}
        void clear() {
            bar.clearButtons();
            for (int i=0;i<windows.size();i++) windows[i].~AssociatedWindow();
            windows.clear();
        }
        // returns pane.getSize();
        size_t addButtonAndWindow(const Toolbutton& button,const AssociatedWindow& window,int insertPosition=-1);
        // returns pane.getSize();
        size_t addSeparator(float pixels=16, int insertPosition=-1);
        // returns pane.getSize();  No window is associated to that button (user must manually check its state)
        size_t addButtonOnly(const Toolbutton& button, int insertPosition=-1);
        // this helps moving the same window to different panes. Cloning toggle Buttons is NOT supported (sync them ON would imply drawing the same toggle-window more than once).
        size_t addClonedButtonAndWindow(const Pane& sourcePane,const size_t sourcePaneEntryIndex,bool flipButtonHorizontally=false);

        size_t addClonedPane(const Pane& sourcePane,bool flipButtonHorizontally=false,int sourcePaneEntryStartIndex=0,int sourcePaneEntryEndIndex=-1);

        size_t deleteButtonAt(int index);

        size_t getSize() const;

        void setVisible(bool flag) {visible=flag;}
        bool getVisible() const {return visible;}

        void setAllowHoverOnTogglableWindows(bool flag) {allowHoverOnTogglableWindows = flag; }
        bool getAllowHoverOnTogglableWindows() const {return allowHoverOnTogglableWindows;}

        void setToolbarScaling(float scalingX,float scalingY) {bar.setScaling(scalingX,scalingY);}
        ImVec2 getToolbarScaling() const {return bar.getScaling();}

        void setToolbarProperties(bool _keepAButtonSelected=true,bool _lightAllBarWhenHovered=false,const ImVec2& _hvAlignmentsIn01=ImVec2(-1000,-1000),const ImVec2& _opacityOffAndOn=ImVec2(-1.f,-1.f),const ImVec4& _bg_col=ImVec4(1,1,1,1),const ImVec4& _displayPortion=ImVec4(0,0,-1,-1));
        void setDisplayProperties(const ImVec2& _opacityOffAndOn=ImVec2(-1.f,-1.f),const ImVec4& _bg_col=ImVec4(1,1,1,1));

        const char* getWindowName(int index) const;     // Can return NULL
        int findWindowIndex(const char* windowName) const;  // Can return -1

        bool isButtonPressed(int index) const;
        bool isButtonPressed(const char* windowName) const;
        bool setButtonPressed(int index,bool flag=true);
        bool setButtonPressed(const char* windowName,bool flag=true);

        // getButtonAndWindow(...) methods return references
        void getButtonAndWindow(size_t index,Toolbutton** pToolbutton=NULL,AssociatedWindow** pAssociatedWindow=NULL);
        void getButtonAndWindow(size_t index,const Toolbutton** pToolbutton=NULL,const AssociatedWindow** pAssociatedWindow=NULL)  const;
        void getButtonAndWindow(const char* windowName,Toolbutton** pToolbutton=NULL,AssociatedWindow** pAssociatedWindow=NULL);
        void getButtonAndWindow(const char* windowName,const Toolbutton** pToolbutton=NULL,const AssociatedWindow** pAssociatedWindow=NULL)  const;


        // protected:
        int getSelectedIndex() const {return bar.selectedButtonIndex;}
        int getHoverIndex() const {return bar.hoverButtonIndex;}
        const AssociatedWindow* getSelectedWindow() const {return bar.selectedButtonIndex>=0 ? &windows[bar.selectedButtonIndex] : NULL;}
        const AssociatedWindow* getHoverWindow() const {return bar.hoverButtonIndex>=0 ? &windows[bar.hoverButtonIndex] : NULL;}        
    };
    protected:

    ImVector<Pane> panes;
    Pane *paneLeft,*paneRight,*paneTop,*paneBottom;
    bool visible;
    mutable ImVec2 innerBarQuadPos;mutable ImVec2 innerBarQuadSize;    // placement of the blank quad contained inside the toolbars only
    mutable ImVec2 innerQuadPos;mutable ImVec2 innerQuadSize;          // placement of the blank quad contained inside the toolbars AND the selected windows that are docked to them
    mutable float dockedWindowsAlpha;
    mutable float innerQuadChangedTimer;
    mutable ImGuiWindowFlags dockedWindowsExtraFlags;
    public:
    PanelManager(bool _visible=true,float _dockedWindowsAlpha=0.8f,bool showDockedWindowBorders=false) : paneLeft(NULL),paneRight(NULL),paneTop(NULL),paneBottom(NULL),visible(_visible),
    innerBarQuadPos(0,0),innerBarQuadSize(-1,-1),innerQuadPos(0,0),innerQuadSize(-1,-1),dockedWindowsAlpha(_dockedWindowsAlpha),innerQuadChangedTimer(-1.f),dockedWindowsExtraFlags(showDockedWindowBorders?ImGuiWindowFlags_ShowBorders:0) {}
    ~PanelManager() {clear();}
    void clear() {
        for (int i=0;i<panes.size();i++) panes[i].~Pane();
        panes.clear();
        paneLeft = paneRight = paneTop = paneBottom = NULL;
    }

    Pane* addPane(Position pos,const char* toolbarName);

    // Optional arguments if provided are not NULL (or !=-1) on output only at the exact frame in which a Toolbutton is pressed (handy in case of Toolbutton without any associated windows)
    // Returns "true" a few instants after a manual resize of the "innerQuadPos" central space [use getCentralQuadPlacement() to retrieve it]
    bool render(Pane **pPanePressedOut=NULL, int* pPaneToolbuttonPressedIndexOut=NULL) const;

    void setVisible(bool flag) {visible=flag;}
    bool getVisible() const {return visible;}

    void setDockedWindowsAlpha(float alpha) {dockedWindowsAlpha=alpha;}
    float getDockedWindowsAlpha() const {return dockedWindowsAlpha;}
    float& getDockedWindowsAlpha() {return dockedWindowsAlpha;}

    void setDockedWindowsBorder(bool border) {if (border) dockedWindowsExtraFlags|=ImGuiWindowFlags_ShowBorders;else {dockedWindowsExtraFlags&=~ImGuiWindowFlags_ShowBorders;}}
    bool getDockedWindowsBorder() const {return (dockedWindowsExtraFlags&ImGuiWindowFlags_ShowBorders);}
    void setDockedWindowsNoTitleBar(bool flag) {if (flag) dockedWindowsExtraFlags|=ImGuiWindowFlags_NoTitleBar;else {dockedWindowsExtraFlags&=~ImGuiWindowFlags_NoTitleBar;}}
    bool getDockedWindowsNoTitleBar() const {return (dockedWindowsExtraFlags&ImGuiWindowFlags_NoTitleBar);}
    int getDockedWindowsExtraFlags() const {return dockedWindowsExtraFlags;}    // Basically it handles ImGuiWindowFlags_ShowBorders and ImGuiWindowFlags_NoTitleBar together

    size_t getNumPanes() const;
    bool isEmpty() const {return getNumPanes()==0;}
    const Pane* getPane(Position pos) const;
    Pane* getPane(Position pos);
    const Pane* getPaneLeft() const {return paneLeft;}
    const Pane* getPaneRight() const {return paneRight;}
    const Pane* getPaneTop() const {return paneTop;}
    const Pane* getPaneBottom() const {return paneBottom;}
    Pane* getPaneLeft() {return paneLeft;}
    Pane* getPaneRight() {return paneRight;}
    Pane* getPaneTop() {return paneTop;}
    Pane* getPaneBottom() {return paneBottom;}
    const Pane* getPaneFromIndex(int index) const {return (index>=0 && index<panes.size()) ? &panes[index] : NULL;}
    Pane* getPaneFromIndex(int index)  {return (index>=0 && index<panes.size()) ? &panes[index] : NULL;}

    void setToolbarsScaling(float scalingX,float scalingY);
    void overrideAllExistingPanesDisplayProperties(const ImVec2& _opacityOffAndOn=ImVec2(-1.f,-1.f),const ImVec4& _bg_col=ImVec4(1,1,1,1));


    void updateSizes() const;
    void closeHoverWindow();
    void recalculatePositionAndSizes() {
        innerBarQuadSize.x=innerBarQuadSize.y=0;
        updateSizes();
    }

    // These are what users usually need for filling the central space with a window
    // Better check the values before using them
    const ImVec2& getCentralQuadPosition() const;
    const ImVec2& getCentralQuadSize() const;

    // These return the blank space inside the toolbars, without considering any pane window.
    const ImVec2& getToolbarCentralQuadPosition() const;
    const ImVec2& getToolbarCentralQuadSize() const;

    // Don't call this every frame
    // values < 0 are relative to to ImGui::GetIO()::DisplaySize
    void setDisplayPortion(const ImVec4& _displayPortion);


#   if (!defined(NO_IMGUIHELPER) && !defined(NO_IMGUIHELPER_SERIALIZATION))
//  Warning: load and save mathods here just load and save the selected buttons and the associated window sizes: nothing else.
//  Thus these methods cannot construct a PanelManager for you!
//  Hp) ImGui::GetIO().displaySize must be valid on both load and save
#   ifndef NO_IMGUIHELPER_SERIALIZATION_SAVE
    static bool Save(const PanelManager& mgr,ImGuiHelper::Serializer& s);
    static inline bool Save(const PanelManager &mgr, const char *filename)    {
        ImGuiHelper::Serializer s(filename);
        return Save(mgr,s);
    }
#   endif //NO_IMGUIHELPER_SERIALIZATION_SAVE
#   ifndef NO_IMGUIHELPER_SERIALIZATION_LOAD
    static bool Load(PanelManager& mgr, ImGuiHelper::Deserializer& d, const char ** pOptionalBufferStart=NULL);
    static inline bool Load(PanelManager& mgr,const char* filename) {
        ImGuiHelper::Deserializer d(filename);
        return Load(mgr,d);
    }
#   endif //NO_IMGUIHELPER_SERIALIZATION_LOAD
#   endif //NO_IMGUIHELPER_SERIALIZATION

    protected:
    void calculateInnerBarQuadPlacement() const;


};

typedef PanelManager::Pane PanelManagerPane;
typedef PanelManager::Pane::AssociatedWindow PanelManagerPaneAssociatedWindow;
typedef PanelManager::WindowData PanelManagerWindowData;


} // namespace imgui


#endif //IMGUIPANELMANAGER_H_


