// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.




// Code ported from another one of my projects (used a different GUI and worked like a charm!).
// Now code is very dirty/buggy/terribly slow.
// Many features don't work (e.g. text selection) and other are buggy (e.g. find text),
// other are simply not implemented yet (e.g. 90% of the options in the context-menu).

// But It's too much work for me for now to make it work like it should (these two files could be a stand-alone git repository by themselves!).


#include "imguipdfviewer.h"

#include <imgui_internal.h>
#include <imgui.h>  // intellisense

#include <poppler/glib/poppler-document.h>
#include <poppler/glib/poppler-page.h>

#include <cairo/cairo.h> // intellisense

namespace ImStl {
    template<typename T,typename U> struct Pair {
        T first;U second;
        inline Pair(const T &t=T(),const U &u=U()): first(t),second(u) {}
        inline Pair(const Pair<T,U>& o): first(o.first),second(o.second) {}
        inline bool operator==(const Pair<T,U>& o) const {return (first==o.first && second==o.second);}
        inline const Pair<T,U>& operator=(const Pair<T,U>& o) {first=o.first;second=o.second;return *this;}
    };
    inline static void StrCpy(char*& dst,const char* src) {
        if (dst) {ImGui::MemFree(dst);dst=NULL;}
        if (src) {
            dst = (char*) ImGui::MemAlloc(strlen(src)+1);
            strcpy(dst,src);
        }
    }
    inline static void Free(char*& dst) {
        StrCpy(dst,NULL);
    }
    // T can't be a pointer!
    template <typename T> inline static void VecCpy(ImVector<T>& dst,const ImVector<T>& src,bool callCtrDst=false)  {
        if (!callCtrDst)    {
            dst.clear();dst.resize(src.size());
            for (int i=0,isz=dst.size();i<isz;i++) dst[i] = src[i];
        }
        else {
            for (int i=0,isz=dst.size();i<isz;i++) {dst[i].~T();}
            dst.clear();dst.resize(src.size());
            for (int i=0,isz=dst.size();i<isz;i++) {
                IM_PLACEMENT_NEW (&dst[i]) T();
                dst[i] = src[i];
            }
        }
    }
    template <typename T> inline static void AddRange(ImVector<T>& v,const ImVector<T>& toBeAdded)    {
        const int vSize = (int) v.size();
        const int toBeAddedSize = (int) toBeAdded.size();
        v.resize(vSize+toBeAddedSize);
        for (int i=0;i<toBeAddedSize;i++)   {
            v[vSize+i] = toBeAdded[i];
        }
    }
    template <typename T> inline static void InsertRange(ImVector<T>& v,int insertionPoint,const ImVector<T>& toBeAdded)    {
        if (insertionPoint<0 || insertionPoint>=(int)v.size())   {
            AddRange(v,toBeAdded);
            return;
        }
        const int vSize = (int) v.size();
        const int toBeAddedSize = (int) toBeAdded.size();
        v.resize(vSize+toBeAddedSize);
        for (int i=vSize-1;i>=insertionPoint;i--)   {
            v[i+toBeAddedSize] = v[i];
        }
        for (int i=insertionPoint;i<insertionPoint+toBeAddedSize;i++)   {
            v[i] = toBeAdded[i-insertionPoint];
        }

    }
} // namespace ImSTL


namespace Cairo {
inline static cairo_rectangle_t MakeRectangle(double x,double y,double width,double height) {cairo_rectangle_t r;r.x=x;r.y=y;r.width=width;r.height=height;return r;}
inline static cairo_rectangle_t MakeRectangle(double x,double y,double width,double height,double s) {cairo_rectangle_t r;r.x=x*s;r.y=y*s;r.width=width*s;r.height=height*s;return r;}
inline static cairo_rectangle_t ScaleRectangle(const cairo_rectangle_t& r,double scale) {return Cairo::MakeRectangle(r.x*scale,r.y*scale,r.width*scale,r.height*scale);}
struct PointD   {
    double x,y;
    PointD(double _x=0,double _y=0) : x(_x),y(_y) {}
};
inline static bool IsEqual(const cairo_rectangle_t& a,const cairo_rectangle_t& b)  {
    return (a.x==b.x && a.y==b.y && a.width==b.width && a.height==b.height);
}
inline static bool IsDifferent(const cairo_rectangle_t& a,const cairo_rectangle_t& b)  {
    return !IsEqual(a,b);
}
static bool IsInside(double x,double y,cairo_rectangle_t& r) {
    if (x > r.x && x < r.x + r.width &&
            y > r.y && y < r.y + r.height)
        return true;
    return false;
}

inline static cairo_surface_t* GetPartOfBitmap(cairo_surface_t* b,const cairo_rectangle_t& rect)	{
    cairo_surface_t* b2=NULL;
    if (!b || cairo_image_surface_get_width(b)<=0 || cairo_image_surface_get_height(b)<=0)  return b2;
    b2=cairo_image_surface_create(cairo_image_surface_get_format(b),(int)rect.width,(int) rect.height);
    {
        cairo_t* cr = cairo_create(b2);
        cairo_set_source_surface(cr,b,-(int)rect.x,-(int)rect.y);
        //cr.Rectangle (0,0,rect.Width,rect.Height);
        //cr.Fill();
        cairo_paint(cr);
        cairo_destroy(cr);
    }

    return b2;
}
inline static cairo_surface_t* Extract(cairo_surface_t* b,const cairo_rectangle_t& rect)	{
    return GetPartOfBitmap (b, rect);
}

static bool SaveToPng(cairo_surface_t* surf,const gchar* filePath)  {
    if (!filePath) return false;
    try {
        gchar* path = (gchar*)g_malloc(strlen(filePath)+5);
        strcpy(path,filePath);strcat(path,".png");
        cairo_surface_write_to_png(surf,filePath);
        g_free(path);
        return true;
    }
    catch (...) {
        return false;
    }

    /*{
        Glib::ustring errorMessage = "An error has occurred while saving file:\n<small><b>\""+filePath+"\"</b></small>";
        Gtk::MessageDialog dlg(errorMessage,true,Gtk::MESSAGE_ERROR,Gtk::BUTTONS_CLOSE,true);
        dlg.run();
        return false;
    }*/
    return true;
}

}   // namespace Cairo


namespace Poppler {
inline static cairo_rectangle_t ToCairo(const PopplerRectangle& pr,double pageHeight) {
    // PopplerRectangle: two points: bottom left and top right
    // cairo_rectangle_t: top left + width and height
    return Cairo::MakeRectangle(pr.x1, pageHeight - pr.y1, pr.x2 - pr.x1, pr.y1 - pr.y2);
}
static PopplerRectangle ToPoppler(const cairo_rectangle_t& cr)    {
    PopplerRectangle pr;
    pr.x1 = cr.x;
    pr.x2 = cr.x + cr.width;
    pr.y1 = cr.y + cr.height;
    pr.y2 = cr.y;
    return pr;
}

struct PopplerActionWrapper {
public:
    PopplerActionType type;
    int pageNum;
    gchar* uri;          //TODO: fix it was a Glib::ustring (= many allocation problems)
    cairo_rectangle_t destRectangle;
    PopplerActionWrapper() : type(POPPLER_ACTION_NONE),pageNum(-1),uri(NULL),destRectangle(Cairo::MakeRectangle(0,0,0,0)) {}
    PopplerActionWrapper(const gchar* _uri,bool own=true) :  type(POPPLER_ACTION_URI),pageNum(-1),uri(NULL) {if (own) setUri(_uri);}
    PopplerActionWrapper(const int _pageNum) :  type(POPPLER_ACTION_GOTO_DEST),pageNum(_pageNum),uri(NULL) {}
    PopplerActionWrapper(PopplerAction* actionPtr,PopplerDocument* document) : uri(NULL) {
        if (!actionPtr) {*this = PopplerActionWrapper();return;}
        PopplerActionAny any = actionPtr->any;
        this->type = any.type;
        switch (any.type)   {
        case POPPLER_ACTION_GOTO_DEST:  {
            PopplerActionGotoDest& gotoDest = actionPtr->goto_dest;
            PopplerDest *dest = gotoDest.dest;bool mustFreeDest = false;
            if (dest->type == POPPLER_DEST_NAMED && document) {
                dest = poppler_document_find_dest(document,dest->named_dest);
                mustFreeDest = true;
            }
            *this =  PopplerActionWrapper(dest->page_num);
            destRectangle = Cairo::MakeRectangle(dest->left, dest->top, dest->right == 0 ? 0 : dest->right - dest->left , dest->bottom == 0 ? 0 : dest->bottom - dest->top);
            if (mustFreeDest) poppler_dest_free(dest);
            return;
        }
            return;
        case POPPLER_ACTION_URI:
            *this = PopplerActionWrapper(actionPtr->uri.uri);
            return;
        default:
            *this = PopplerActionWrapper(); // this overwrites type to POPPLER_ACTION_NONE too
            return;
        }
    }
    inline bool operator!=(const PopplerActionWrapper& s) const {
        return !operator==(s);
    }
    inline bool operator==(const PopplerActionWrapper& s) const {
        if (type!=s.type) return false;
        if (type == POPPLER_ACTION_GOTO_DEST)   {
            if (pageNum!=s.pageNum || Cairo::IsDifferent(destRectangle,s.destRectangle)) return false;
            else return true;
        }
        if (type == POPPLER_ACTION_URI)   {
            return uri==s.uri;
        }
        return false;
    }
    void setUri(const gchar* _uri) {
        if (uri) {g_free(uri);uri=NULL;}
        if (_uri) {uri = (gchar*)g_malloc(strlen(_uri)+1);strcpy(uri,_uri);}
    }
    ~PopplerActionWrapper() {if (uri) {g_free(uri);uri=NULL;}}

    PopplerActionWrapper(const PopplerActionWrapper& o) : uri(NULL) {*this=o;}
    const PopplerActionWrapper& operator=(const PopplerActionWrapper& o) {
        type = o.type;
        pageNum = o.pageNum;
        setUri(o.uri);
        destRectangle = o.destRectangle;
        return *this;
    }

};

typedef ImStl::Pair<cairo_rectangle_t,PopplerActionWrapper> LinkMappingPair;
static void Clear(ImVector<LinkMappingPair>& v) {
    for (int i=0,isz=v.size();i<isz;i++) {
        v[i].second.~PopplerActionWrapper();
    }
    v.clear();
}
static ImVector<LinkMappingPair>& GetLinkMapping(PopplerPage* page,PopplerDocument* document,ImVector<LinkMappingPair>& ret) {
    double w, h;poppler_page_get_size(page,&w,&h);//page->getSize (w, h);
    GList* glist = poppler_page_get_link_mapping(page);
    Clear(ret);
    PopplerLinkMapping* lm;
    cairo_rectangle_t rect;
    int cnt = 0;
    for (GList* ptr = glist;ptr;ptr=ptr->next) ++cnt;
    ret.reserve(cnt);
    for (GList* ptr = glist;ptr;ptr=ptr->next) {
        lm = (PopplerLinkMapping*) ptr->data;
        PopplerAction* actionPtr = lm->action;
        rect = Cairo::MakeRectangle(lm->area.x1,h-lm->area.y1-(lm->area.y2-lm->area.y1),lm->area.x2-lm->area.x1,lm->area.y2-lm->area.y1);
        ret.resize(ret.size()+1);
        ret[ret.size()-1].first = rect;
        ret[ret.size()-1].second.uri=NULL;
        ret[ret.size()-1].second = PopplerActionWrapper(actionPtr,document);
    }

    poppler_page_free_link_mapping(glist);
    return ret;
}

typedef ImStl::Pair<cairo_rectangle_t,int> ImageMappingPair;
static ImVector<ImageMappingPair>& GetImageMapping(PopplerPage* page,ImVector<ImageMappingPair>& ret,double scale=1.0)   {
    double w, h;poppler_page_get_size(page,&w,&h);//page->getSize (w, h);
    GList* glist = poppler_page_get_image_mapping(page);

    ret.clear();
    PopplerImageMapping* im;
    //cairo_rectangle_t rect;
    for (GList* ptr = glist;ptr;ptr=ptr->next) {
        im = (PopplerImageMapping*) ptr->data;
        //rect = //ToCairo(im->area,h);//Cairo::MakeRectangle(im->area.x1,h-im->area.y1-(im->area.y2-im->area.y1),im->area.x2-im->area.x1,im->area.y2-im->area.y1);
        ret.resize(ret.size()+1);
        ret[ret.size()-1]=ImageMappingPair(
                          //rect,
                          Cairo::MakeRectangle(im->area.x1, im->area.y1, im->area.x2 - im->area.x1,im->area.y2 - im->area.y1,scale),
                          im->image_id);
    }
    poppler_page_free_image_mapping(glist);
    return ret;
}
static ImVector<cairo_rectangle_t>& GetSelectionRegion(PopplerPage* page,double scale,PopplerSelectionStyle style,const cairo_rectangle_t& selection,ImVector<cairo_rectangle_t>& ret)    {
    double pageWidth,pageHeight;poppler_page_get_size(page,&pageWidth,&pageHeight);
    PopplerRectangle selectedPoppler = ToPoppler(selection);  // lower left point and upper right point of the rectangle
    GList* glist = poppler_page_get_selection_region(page,scale,style,&selectedPoppler);
    ret.clear();
    PopplerRectangle* pr;
    for (GList* ptr = glist;ptr;ptr=ptr->next) {
        pr = (PopplerRectangle*) ptr->data;        
        ret.push_back(
                    //ToCairo(*pr,pageHeight)
                    Cairo::MakeRectangle(pr->x1, pr->y1, pr->x2 - pr->x1,pr->y2 - pr->y1)
                    );
    }
    poppler_page_selection_region_free(glist);
    return ret;
}
static ImVector<cairo_rectangle_t>& FindText(PopplerPage* page,const gchar* text,ImVector<cairo_rectangle_t>& ret,PopplerFindFlags options = POPPLER_FIND_DEFAULT)    {
    double pageWidth,pageHeight;poppler_page_get_size(page,&pageWidth,&pageHeight);
    GList* glist = poppler_page_find_text_with_options(page,text,options);
    ret.clear();
    PopplerRectangle* sr;
    for (GList* ptr = glist;ptr;ptr=ptr->next) {
        sr = (PopplerRectangle*) ptr->data;
        ret.push_back(ToCairo(*sr,pageHeight));
    }
    poppler_page_selection_region_free(glist);  // I'm not sure about this line: but the GList has the same contents of the one used there...
    return ret;
}

static cairo_surface_t* PageToCairo(PopplerPage* page,cairo_format_t format = CAIRO_FORMAT_RGB24,const cairo_rectangle_t* rect=NULL)	{
    cairo_surface_t* imPage = NULL;
    if (!page) return imPage;
    double w, h;poppler_page_get_size(page,&w,&h);
    imPage = cairo_image_surface_create(format, (int)w, (int)h);
    if (!imPage) return imPage;
    {
        cairo_t* cr = cairo_create(imPage);
        poppler_page_render(page,cr);
        cairo_destroy(cr);
    }
    if (rect == NULL)   return imPage;
    const cairo_rectangle_t& pgRect = *rect;
    cairo_surface_t* imRect = Cairo::Extract(imPage,pgRect);
    cairo_surface_destroy(imPage);
    return imRect;
}
// the returned char array must be freed with g_free(...)
static gchar* GetSelectedText(PopplerPage* page,const cairo_rectangle_t& selectionRect,PopplerSelectionStyle selectionStyle=POPPLER_SELECTION_WORD)  {
    if (!page) return NULL;
    PopplerRectangle selection = ToPoppler(selectionRect);
    return poppler_page_get_selected_text(page, selectionStyle, &selection);
}

} // namespace Poppler



namespace ImGui {

// Callbacks
PdfViewer::FreeTextureDelegate PdfViewer::FreeTextureCb =
#ifdef IMGUI_USE_AUTO_BINDING
&ImImpl_FreeTexture;
#else //IMGUI_USE_AUTO_BINDING
NULL;
#endif //IMGUI_USE_AUTO_BINDING
PdfViewer::GenerateOrUpdateTextureDelegate PdfViewer::GenerateOrUpdateTextureCb =
#ifdef IMGUI_USE_AUTO_BINDING
&ImImpl_GenerateOrUpdateTexture;
#else //IMGUI_USE_AUTO_BINDING
NULL;
#endif //IMGUI_USE_AUTO_BINDING


class PdfPagePanel {
public:
    PdfPagePanel();
    ~PdfPagePanel() {
        if (ContextMenuData.contextMenuParent==this)    ContextMenuData.reset();
        destroy();
    }

    bool loadFromFile(const char* path);

    // page numbers (indices) start at zero
    bool setPage(int pageIndex,bool addNewPageToStack=true,float initialZoomCenterY=-1.f);
    int getPageIndex() const;
    int getNumPages() const;

    void destroy();

    // returns true if some user action has been processed
    bool render(const ImVec2 &size=ImVec2(0,0), bool renderTopPanelToo=true);    // to be called inside an ImGui::Window. Makes isInited() return true;
    // returns true if some user action has been processed
    bool renderTopPanelOnly();

    bool resetZoomAndPan();
    bool goBackward();
    bool goForward();
    bool findText(const char* text);
    bool findTextActivated();
    bool findPrev();
    bool findNext();

    // owns the returned string
    const char *getSelectedText() const;

    struct ContextMenuDataType {
        PdfPagePanel* contextMenuParent;
        bool mustOpenContextMenu;
        cairo_rectangle_t hoverImageRectInPdfPageCoords;
        int hoverImageId;
        void reset() {
            contextMenuParent=NULL;
            mustOpenContextMenu=false;
            hoverImageRectInPdfPageCoords = Cairo::MakeRectangle(0,0,0,0);
            hoverImageId=-1;
        }
        ContextMenuDataType() {reset();}
    };
    static ContextMenuDataType ContextMenuData;

    PopplerDocument* document;
    PopplerPage* page;

    ImTextureID texid;
    double scale;
    double TWidth,THeight;
    float aspectRatio,zoom;
    ImVec2 zoomCenter;

    mutable ImVec2 uv0;
    mutable ImVec2 uv1;
    mutable ImVec2 zoomedImageSize;
    mutable ImVec2 cursorPosAtStart;
    mutable ImVec2 startPos;
    mutable ImVec2 endPos;
    bool imageZoomAndPan(const ImVec2& size);

    mutable ImVector<char> selectedTextVector;

    static const int SearchBufferSize = 256;
    char searchText[SearchBufferSize];
    char lastTextSearched[SearchBufferSize];

    ImVector<ImStl::Pair<int,cairo_rectangle_t> > findTextSelectionRectangles;	// map : page -> rectangles
    int firstRectangleOfCurrentPageIndex;           // = -1;
    int currentMainRectangleSelection;              // = -1;		// The secection in a different color
    ImVector<ImStl::Pair<int,cairo_rectangle_t> > userSelectionRectangles;
    int firstUserRectangleOfCurrentPageIndex;       // = -1;
    ImVector<cairo_rectangle_t> temporaryUserSelection;           // = new std::vector<cairo_rectangle_t>();

    // LinkMapping Stuff--------------------------------------------------------------------------------------------------------
    ImVector<Poppler::LinkMappingPair> linkMapping;         // = new std::vector<std::pair<cairo_rectangle_t, Page.ActionWrapper>>();
    int linkUnderTheMouse;                          // index inside link mapping
    bool isLinkUnderTheMouseValid;                  // false;
    // ImageMapping Stuff--------------------------------------------------------------------------------------------------------
    ImVector<Poppler::ImageMappingPair> imageMapping;
    // Mouse Button Scroll And Motion Events---------------------------------------------------------------------------------------
    Cairo::PointD mousePos;
    Cairo::PointD mouseBeginPos;
    Cairo::PointD mouseEndPos;
    bool blnMouseMoving;                            // = false;
    guint32 motionNotifyTime;                       // = 0;
    int numDisplayedSearchMatch;                    // = -1

    typedef ImStl::Pair<int,float> PageStackPairType;

    ImVector<PageStackPairType> pageStack;
    int currentStackLevel;
    void resetPageStack();

    inline static void FreeDocument(PopplerDocument *&document)    {
        if (document) {g_object_unref(document);document=NULL;}
    }
    inline static void FreePage(PopplerPage *&page)    {
        if (page) {g_object_unref(page);page=NULL;}
    }

    // FindTextSelectionRectangles stuff-------------------------------------------------------------------------------------
    void setFindTextSelectionRectangles(const ImVector<ImStl::Pair<int,cairo_rectangle_t> >& value) {
        ImStl::VecCpy(findTextSelectionRectangles,value);
        updateFindTextSelectionRectanglesExtraStuff();
    }
    void updateFindTextSelectionRectanglesExtraStuff()  {
        firstRectangleOfCurrentPageIndex = 0;
        int npage = getPageIndex();
        bool ok = false;
        for (int i=0,isz=findTextSelectionRectangles.size();i<isz;i++)	{
            if (findTextSelectionRectangles[i].first==npage) {ok=true;break;}
            ++firstRectangleOfCurrentPageIndex;
        }
        if (!ok) firstRectangleOfCurrentPageIndex = -1;
    }

    // UserSelectionRectangles stuff-----------------------------------------------------------------------------------------
    static int UserSelectionRectanglesComparer(const void* pa,const void* pb)   {
        const ImStl::Pair<int,cairo_rectangle_t>& a = *((const ImStl::Pair<int,cairo_rectangle_t>*)pa);
        const ImStl::Pair<int,cairo_rectangle_t>& b = *((const ImStl::Pair<int,cairo_rectangle_t>*)pb);
        int apg = a.first;
        int bpg = b.first;
        if (apg == bpg) {
            const cairo_rectangle_t& ar = a.second;
            const cairo_rectangle_t& br = b.second;
            if (ar.y == br.y) return  (ar.x < br.x ? -1 : 1);
            return (ar.y < br.y ? -1 : 1);
        }
        return (apg < bpg ? -1 : 1);
    }

    void setUserSelectionRectangles(const ImVector<ImStl::Pair<int,cairo_rectangle_t> >& value) {
        ImStl::VecCpy(userSelectionRectangles,value);
        updateUserSelectionRectanglesExtraStuff();
    }
    void updateUserSelectionRectanglesExtraStuff()  {
        firstUserRectangleOfCurrentPageIndex = 0;
        int npage = getPageIndex();
        bool ok = false;
        for (int i=0,isz=userSelectionRectangles.size();i<isz;i++)	{
            if (userSelectionRectangles[i].first==npage) {ok=true;break;}
            ++firstUserRectangleOfCurrentPageIndex;
        }
        if (!ok) firstUserRectangleOfCurrentPageIndex = -1;
    }

    // Temporary User Selection Stuff-----------------------------------------------------------------------------------------
    void performSelectModeStep (double eX,double eY)    {
        const ImGuiIO& io = ImGui::GetIO();
        //double bX=mouseBeginPos.x,bY=mouseBeginPos.y;
        double bX=io.MouseClickedPos[0].x,bY=io.MouseClickedPos[0].y;
        double _temp;
        if (bX > eX) {_temp = bX;bX = eX;eX = _temp;}
        if (bY > eY) {_temp = bY;bY = eY;eY = _temp;}
        double eW = eX - bX;
        double eH = eY - bY;

        temporaryUserSelection = getSelectionRegionFrom (Cairo::MakeRectangle(bX, bY, eW, eH),temporaryUserSelection,1.0,POPPLER_SELECTION_GLYPH);
    }

    // LinkMapping Stuff-------------------------------------------------------------------------------------------------------- 
    bool isMouseOnLink(double x,double y,int& linkMappingIndexOut,bool firstCheckCurrentLinkMappingIndexOut=true)	{
        if (linkMapping.size()==0 || !page) return false;

        ImVec2 tmp = mouseToPdfRelativeCoords(ImVec2(x,y));
        double width,height;poppler_page_get_size(page,&width,&height);
        tmp.x*=width;tmp.y*=height;
        //fprintf(stderr,"%1.2f,%1.2f\n",tmp.x,tmp.y);
        if (firstCheckCurrentLinkMappingIndexOut && linkMappingIndexOut>=0 && linkMappingIndexOut<linkMapping.size() && Cairo::IsInside(tmp.x,tmp.y,linkMapping[linkMappingIndexOut].first)) return true;

        cairo_rectangle_t r;
        linkMappingIndexOut=-1;
        for (int i=0,isz=linkMapping.size();i<isz;i++)	{
            const Poppler::LinkMappingPair& link = linkMapping[i];
            r = link.first;
            if (Cairo::IsInside(tmp.x,tmp.y,r)) {
                linkMappingIndexOut = i;
                return true;
            }
        }
        return false;
    }
    // ImageMapping Stuff--------------------------------------------------------------------------------------------------------
    bool isMouseOnImage(double x,double y,cairo_rectangle_t& rectOutInPdfPageCoords,int& imageIdOut,double scaleRectOut=1.0)	{
        imageIdOut = -1;
        rectOutInPdfPageCoords = Cairo::MakeRectangle (0, 0, 0, 0);
        if (imageMapping.size()==0 || !page) return false;

        ImVec2 tmp = mouseToPdfRelativeCoords(ImVec2(x,y));
        double width,height;poppler_page_get_size(page,&width,&height);
        tmp.x*=width;
        tmp.y*=height;
        //fprintf(stderr,"%1.2f,%1.2f\n",tmp.x,tmp.y);

        cairo_rectangle_t r;
        for (int i=0,isz=imageMapping.size();i<isz;i++)	{
            const Poppler::ImageMappingPair& link = imageMapping[i];
            r = link.first;
            if (Cairo::IsInside(tmp.x,tmp.y,r)) {
                rectOutInPdfPageCoords = Cairo::MakeRectangle(r.x,r.y,r.width,r.height,scaleRectOut);
                imageIdOut = link.second;
                return true;
            }
        }
        return false;
    }
    cairo_surface_t* pageToCairo(cairo_format_t format = CAIRO_FORMAT_ARGB32,int pageNum=-1,const cairo_rectangle_t* rect=NULL,const double scale=1.0)	{
        cairo_surface_t* imPage;
        if (!document) return imPage;
        if (pageNum<0 || pageNum>=poppler_document_get_n_pages(document)) return imPage;
        PopplerPage* newPage = poppler_document_get_page(document,pageNum);
        if (!newPage)  return imPage;
        double w, h;poppler_page_get_size(newPage,&w,&h);
        w*=scale;h*=scale;
        imPage = cairo_image_surface_create(format, (int)w, (int)h);
        if (!imPage)    {
            g_object_unref(newPage);newPage=NULL;
            return imPage;
        }
        {
            cairo_t* cr = cairo_create(imPage);
            poppler_page_render(newPage,cr);
            cairo_destroy(cr);
        }
        {g_object_unref(newPage);newPage=NULL;}
        if (rect == NULL)   return imPage;
        const cairo_rectangle_t& pgRect = *rect;
        cairo_surface_t* imRect = Cairo::Extract(imPage,pgRect);
        cairo_surface_destroy(imPage);
        return imRect;
    }
    //This is used when the user selects text himself...
    PopplerSelectionStyle getUserSelectionStyle() const {return POPPLER_SELECTION_GLYPH;}
    ImVector<cairo_rectangle_t>& getSelectionRegionFrom(const cairo_rectangle_t& selectedRectangle,ImVector<cairo_rectangle_t>& ret,double scale = 1.0,PopplerSelectionStyle style = POPPLER_SELECTION_GLYPH)	{
        ret.clear();
        if (!page) {return ret;}
        //double x = selectedRectangle.x, y=selectedRectangle.y, x2=selectedRectangle.x+selectedRectangle.width, y2=selectedRectangle.y+selectedRectangle.height;
        double x = selectedRectangle.x,
                y=selectedRectangle.y+selectedRectangle.height,
                x2=selectedRectangle.x+selectedRectangle.width,
                y2=selectedRectangle.y;

        /*if (!mInvUpdated) {
            mInvUpdated = true;
            mInv = m;
            mInv.invert ();
        }
        mInv.transform_point(x,y);
        mInv.transform_point (x2,y2);*/
        ImVec2 tmp = mouseToPdfRelativeCoords(ImVec2(x,y));
        ImVec2 tmp2 = mouseToPdfRelativeCoords(ImVec2(x2,y2));
        double width,height;poppler_page_get_size(page,&width,&height);
        tmp.x*=width;tmp.y*=height;
        //tmp.y = height - tmp.y;
        tmp2.x*=width;tmp2.y*=height;
        //tmp2.y = height - tmp2.y;


        //double pw,ph;getPageSize(pw,ph);
        cairo_rectangle_t transformedRectangle = Cairo::MakeRectangle (tmp.x,tmp.y,tmp2.x-tmp.x,tmp2.y-tmp.y);	//In page coords

        return Poppler::GetSelectionRegion(page,scale,style,transformedRectangle,ret);
    }

    inline ImVec2 mouseToPdfRelativeCoords(const ImVec2 &mp) const {
       return ImVec2((mp.x+cursorPosAtStart.x-startPos.x)*(uv1.x-uv0.x)/zoomedImageSize.x+uv0.x,
               (mp.y+cursorPosAtStart.y-startPos.y)*(uv1.y-uv0.y)/zoomedImageSize.y+uv0.y);
    }
    inline ImVec2 pdfRelativeToMouseCoords(const ImVec2 &mp) const {
        return ImVec2((mp.x-uv0.x)*(zoomedImageSize.x)/(uv1.x-uv0.x)+startPos.x-cursorPosAtStart.x,(mp.y-uv0.y)*(zoomedImageSize.y)/(uv1.y-uv0.y)+startPos.y-cursorPosAtStart.y);
    }


};  // PdfPagePanel

PdfPagePanel::ContextMenuDataType PdfPagePanel::ContextMenuData;


PdfPagePanel::PdfPagePanel()  {
    document = NULL;page = NULL;
    texid = NULL;TWidth=THeight=0;aspectRatio=zoom=1;zoomCenter.x=zoomCenter.y=.5f;
    uv0.x=uv0.y=0;uv1.x=uv1.y=1;
    zoomedImageSize.x=zoomedImageSize.y=cursorPosAtStart.x=cursorPosAtStart.y=
    startPos.x=startPos.y=endPos.x=endPos.y=0;
    searchText[0]=lastTextSearched[0]='\0';
    scale = 1.0;

    currentStackLevel = -1;
    firstRectangleOfCurrentPageIndex = -1;
    currentMainRectangleSelection = -1;
    firstUserRectangleOfCurrentPageIndex = -1;
    isLinkUnderTheMouseValid = false;
    blnMouseMoving = false;
    motionNotifyTime = 0;
    numDisplayedSearchMatch = -1;
}

bool PdfPagePanel::loadFromFile(const char *path) {
    IM_ASSERT(path);

    gchar *absolute, *uri;
    if (g_path_is_absolute(path)) absolute = g_strdup (path);
    else {
        gchar *dir = g_get_current_dir ();
        absolute = g_build_filename (dir, path, (gchar *) 0);
        g_free (dir);
    }

    uri = g_filename_to_uri (absolute, NULL, NULL);
    g_free (absolute);
    if (uri == NULL) {
        fprintf(stderr,"Error: can't load %s\n",path);
        return false;
    }

    PopplerDocument* document =  poppler_document_new_from_file(uri,NULL,NULL);
    g_free(uri);
    if (!document) {fprintf(stderr,"Error: can't load %s\n",path);return false;}
    else {
        destroy();
        this->document = document;

        resetZoomAndPan();
        resetPageStack();
        pageStack.resize(pageStack.size()+1);
        pageStack[pageStack.size()-1] = PageStackPairType(0,zoomCenter.y);

        setPage(0,false);

        return true;
    }
}

bool PdfPagePanel::setPage(int pageIndex, bool addNewPageToStack,float initialZoomCenterY) {
    //fprintf(stderr,"setPage(%d,%s,%1.3f)\n",pageIndex,addNewPageToStack?"true":"false",initialZoomCenterY);
    if (!document) return false;

    IM_ASSERT(PdfViewer::FreeTextureCb);                // Please use PdfViewer::SetFreeTextureCallback(...) at init time
    IM_ASSERT(PdfViewer::GenerateOrUpdateTextureCb);    // Please use PdfViewer::SetGenerateOrUpdateTextureCallback(...) at init time

    const int currentPage = page ? poppler_page_get_index(page) : -1;
    const int numPages = poppler_document_get_n_pages (document);
    if (pageIndex<0 || pageIndex>=numPages) return false;
    if (currentPage==pageIndex) {
        if (initialZoomCenterY<0) return false;
        else {
            const float zoomFactor = .5/zoom;
            zoomCenter.y = initialZoomCenterY;// * zoom; // Not sure about this
            if (zoomCenter.y > 1.f - zoomFactor) zoomCenter.y = 1.f - zoomFactor;
            else if (zoomCenter.y < zoomFactor) zoomCenter.y = zoomFactor;
            return true;
        }
    }
    FreePage(page);
    page = poppler_document_get_page(document,pageIndex);
    if (page) {
        cairo_t *cr;
        cairo_status_t status;
        cairo_surface_t* pageSurface = NULL;

        double width, height;
        poppler_page_get_size (page, &width, &height);

        // For correct rendering of PDF, the PDF is first rendered to a
        // transparent image (all alpha = 0).
        //create cairo-surface/context to act as OpenGL-texture source
        const int IMAGE_DPI = PdfViewer::IMAGE_DPI;
        scale = (double) IMAGE_DPI/72.0;
        TWidth = scale*width;
        THeight = scale*height;
        if (THeight==0) THeight=1;
        aspectRatio = float(TWidth/THeight);

        if (pageSurface) {cairo_surface_destroy (pageSurface);pageSurface=NULL;}
        pageSurface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32,TWidth,THeight);
        if (cairo_surface_status (pageSurface) != CAIRO_STATUS_SUCCESS) {
            fprintf (stderr,"create_cairo_context() - Couldn't create surface\n");
        }

        cr = cairo_create (pageSurface);
        cairo_scale (cr, scale, scale);

        cairo_save (cr);
        // Paint main image
        cairo_rectangle(cr, 0.0, 0.0, width, height);
        poppler_page_render(page, cr);
        // Paint black border
        cairo_rectangle(cr, 0.0, 0.0, width, height);
        cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
        cairo_set_line_width(cr, 0.012f * (width<height ? width : height));
        cairo_stroke_preserve(cr);  // not sure if "preserve" is necessary
        // Paint White Background below image and border
        cairo_set_operator (cr, CAIRO_OPERATOR_DEST_OVER);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_paint (cr);
        cairo_restore (cr);

        status = cairo_status(cr);
        if (status)
            {printf("%s\n", cairo_status_to_string (status));fflush(stdout);}

        cairo_destroy (cr);

        PdfViewer::GenerateOrUpdateTextureCb(texid,TWidth,THeight,4,cairo_image_surface_get_data(pageSurface),true,false,false);

        if (pageSurface) cairo_surface_destroy(pageSurface);pageSurface=NULL;


        IM_ASSERT(texid);

        updateFindTextSelectionRectanglesExtraStuff();
        updateUserSelectionRectanglesExtraStuff();
        temporaryUserSelection.clear ();

        linkUnderTheMouse = -1;
        isLinkUnderTheMouseValid = false;
        Poppler::Clear(linkMapping);
        linkMapping = Poppler::GetLinkMapping(page,document,linkMapping);

        imageMapping = Poppler::GetImageMapping(page,imageMapping);


        const float zoomFactor = .5/zoom;
        if (initialZoomCenterY<0)   {
            if (pageIndex<currentPage) zoomCenter.y = 1.f - zoomFactor;
            else zoomCenter.y = zoomFactor;
        }
        else {
            zoomCenter.y = initialZoomCenterY;// * zoom; // Not sure about this
            if (zoomCenter.y > 1.f - zoomFactor) zoomCenter.y = 1.f - zoomFactor;
            else if (zoomCenter.y < zoomFactor) zoomCenter.y = zoomFactor;
        }

        if (addNewPageToStack) {
            int stackSz = pageStack.size();
            if (currentStackLevel>=0) {
                for (int i = stackSz-1;i>currentStackLevel;i--) {
                    pageStack.pop_back();
                }
            }
            currentStackLevel++;
            pageStack.resize(pageStack.size()+1);
            pageStack[pageStack.size()-1] = PageStackPairType(pageIndex,zoomCenter.y/zoom);
        }
    }
    return true;
}

int PdfPagePanel::getPageIndex() const   {
    if (!page) return -1;
    return poppler_page_get_index(page);
}

int PdfPagePanel::getNumPages() const  {
    if (!document) return 0;
    return poppler_document_get_n_pages(document);
}

void PdfPagePanel::destroy() {
    PdfViewer::FreeTextureCb(texid);
    FreePage(page);
    FreeDocument(document);
    Poppler::Clear(linkMapping);
}

// zoomCenter is panning in [(0,0),(1,1)]
// returns true if some user interaction have been processed
// TODO: Optimize this method: a lot of stuff don't need to be recalculated every frame and should be cached
bool PdfPagePanel::imageZoomAndPan(const ImVec2& size)
{
    const int panMouseButtonDrag=1;
    const int resetZoomAndPanMouseButton=2;
    const ImVec2 zoomMaxAndZoomStep(16.f,1.025f);

    bool rv = false;
    ImGuiWindow* window = GetCurrentWindow();
    if (window->SkipItems) return rv;
    IM_ASSERT(size.x!=0 && size.y!=0 && zoom!=0);

    const ImGuiIO& io = ImGui::GetIO();
    const float topPanelHeight = ImGui::GetItemRectSize().y+ImGui::GetStyle().ItemSpacing.y; // Not sure about the latter
    cursorPosAtStart = window->DC.CursorPos;
    cursorPosAtStart.y-=topPanelHeight;
    cursorPosAtStart.x = ImGui::GetWindowPos().x - cursorPosAtStart.x;
    cursorPosAtStart.y = ImGui::GetWindowPos().y - cursorPosAtStart.y;

    // Here we use the whole size (although it can be partially empty)
    ImRect bb(window->DC.CursorPos, ImVec2(window->DC.CursorPos.x + size.x,window->DC.CursorPos.y + size.y));
    ItemSize(bb);
    if (!ItemAdd(bb, NULL)) return rv;

    zoomedImageSize = size;
    ImVec2 remainingWndSize(0,0);
    if (aspectRatio!=0) {
        const float wndAspectRatio = size.x/size.y;
        if (aspectRatio >= wndAspectRatio) {zoomedImageSize.y = zoomedImageSize.x/aspectRatio;remainingWndSize.y = size.y - zoomedImageSize.y;}
        else {zoomedImageSize.x = zoomedImageSize.y*aspectRatio;remainingWndSize.x = size.x - zoomedImageSize.x;}
    }

    bool pageChanged = false;

    static bool wasRMBDragging = false;
    static bool wasLMBDragging = false;
    const bool isDragging = ImGui::IsMouseDragging(panMouseButtonDrag,3.f);
    const bool isLMBMouseDragging = ImGui::IsMouseDragging(0,1.f);
    bool isRMBclickedForContextMenu = !isDragging && !wasRMBDragging && ImGui::IsMouseReleased(panMouseButtonDrag);
    wasRMBDragging = isDragging;
    bool isHovered = false, isHoveredRect = false;
    if ((isHovered=isHoveredRect=ImGui::IsItemHovered())) {
        if (io.MouseWheel!=0) {
            if (io.KeyCtrl) {
                const float zoomStep = zoomMaxAndZoomStep.y;
                const float zoomMin = 1.f;
                const float zoomMax = zoomMaxAndZoomStep.x;
                if (io.MouseWheel < 0) {zoom/=zoomStep;if (zoom<zoomMin) zoom=zoomMin;}
                else {zoom*=zoomStep;if (zoom>zoomMax) zoom=zoomMax;}
                rv = true;
            }
            else  {
                const bool scrollDown = io.MouseWheel <= 0;
                const float zoomFactor = .5/zoom;
                if (!io.KeyShift)   {
                    if ((!scrollDown && zoomCenter.y > zoomFactor) || (scrollDown && zoomCenter.y <  1.f - zoomFactor))  {
                        const float slideFactor = zoomMaxAndZoomStep.y*0.1f*zoomFactor;
                        if (scrollDown) {
                            zoomCenter.y+=slideFactor;///(imageSz.y*zoom);
                            if (zoomCenter.y >  1.f - zoomFactor) zoomCenter.y =  1.f - zoomFactor;
                        }
                        else {
                            zoomCenter.y-=slideFactor;///(imageSz.y*zoom);
                            if (zoomCenter.y < zoomFactor) zoomCenter.y = zoomFactor;
                        }
                        rv = true;
                    }
                }
                else {
                     if ((!scrollDown && zoomCenter.x > zoomFactor) || (scrollDown && zoomCenter.x <  1.f - zoomFactor))  {
                        float slideFactor = zoomMaxAndZoomStep.x*0.1f*zoomFactor;
                        if (aspectRatio!=0.f) slideFactor*=aspectRatio;
                        if (scrollDown) {
                            zoomCenter.x+=slideFactor;///(imageSz.x*zoom);
                            if (zoomCenter.x >  1.f - zoomFactor) zoomCenter.x =  1.f - zoomFactor;
                        }
                        else {
                            zoomCenter.x-=slideFactor;///(imageSz.x*zoom);
                            if (zoomCenter.x < zoomFactor) zoomCenter.x = zoomFactor;
                        }
                        rv = true;
                    }
                }
            }
        }
        if (io.MouseClicked[resetZoomAndPanMouseButton]) {zoom=1.f;zoomCenter.x=zoomCenter.y=.5f;rv = true;}
        if (!isRMBclickedForContextMenu && isDragging)   {
            zoomCenter.x-=io.MouseDelta.x/(zoomedImageSize.x*zoom);
            zoomCenter.y-=io.MouseDelta.y/(zoomedImageSize.y*zoom);
            rv = true;
            ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
        }
    }
    else if (isRMBclickedForContextMenu) {if (!(isHoveredRect=ImGui::IsItemHoveredRect())) isRMBclickedForContextMenu = false;}
    else if (isLMBMouseDragging) isHoveredRect=ImGui::IsItemHoveredRect();

    const float zoomFactor = .5/zoom;
    if (rv) {
        if (zoomCenter.x < zoomFactor) zoomCenter.x = zoomFactor;
        else if (zoomCenter.x > 1.f - zoomFactor) zoomCenter.x = 1.f - zoomFactor;
        if (zoomCenter.y < zoomFactor) zoomCenter.y = zoomFactor;
        else if (zoomCenter.y > 1.f - zoomFactor) zoomCenter.y = 1.f - zoomFactor;
    }

    ImVec2 uvExtension(2.f*zoomFactor,2.f*zoomFactor);
    if (remainingWndSize.x > 0) {
        const float remainingSizeInUVSpace = 2.f*zoomFactor*(remainingWndSize.x/zoomedImageSize.x);
        const float deltaUV = uvExtension.x;
        const float remainingUV = 1.f-deltaUV;
        if (deltaUV<1) {
            float adder = (remainingUV < remainingSizeInUVSpace ? remainingUV : remainingSizeInUVSpace);
            uvExtension.x+=adder;
            remainingWndSize.x-= adder * zoom * zoomedImageSize.x;
            zoomedImageSize.x+=adder * zoom * zoomedImageSize.x;

            if (zoomCenter.x < uvExtension.x*.5f) zoomCenter.x = uvExtension.x*.5f;
            else if (zoomCenter.x > 1.f - uvExtension.x*.5f) zoomCenter.x = 1.f - uvExtension.x*.5f;
        }
    }
    if (remainingWndSize.y > 0) {
        const float remainingSizeInUVSpace = 2.f*zoomFactor*(remainingWndSize.y/zoomedImageSize.y);
        const float deltaUV = uvExtension.y;
        const float remainingUV = 1.f-deltaUV;
        if (deltaUV<1) {
            float adder = (remainingUV < remainingSizeInUVSpace ? remainingUV : remainingSizeInUVSpace);
            uvExtension.y+=adder;
            remainingWndSize.y-= adder * zoom * zoomedImageSize.y;
            zoomedImageSize.y+=adder * zoom * zoomedImageSize.y;

            if (zoomCenter.y < uvExtension.y*.5f) zoomCenter.y = uvExtension.y*.5f;
            else if (zoomCenter.y > 1.f - uvExtension.y*.5f) zoomCenter.y = 1.f - uvExtension.y*.5f;
        }
    }

    uv0 = ImVec2((zoomCenter.x-uvExtension.x*.5f),(zoomCenter.y-uvExtension.y*.5f));
    uv1 = ImVec2((zoomCenter.x+uvExtension.x*.5f),(zoomCenter.y+uvExtension.y*.5f));


    /* // Here we use just the window size, but then ImGui::IsItemHovered() should be moved below this block. How to do it?
    ImVec2 startPos=window->DC.CursorPos;
    startPos.x+= remainingWndSize.x*.5f;
    startPos.y+= remainingWndSize.y*.5f;
    ImVec2 endPos(startPos.x+imageSz.x,startPos.y+imageSz.y);
    ImRect bb(startPos, endPos);
    ItemSize(bb);
    if (!ItemAdd(bb, NULL)) return rv;*/

    startPos=bb.Min,endPos=bb.Max;
    startPos.x+= remainingWndSize.x*.5f;
    startPos.y+= remainingWndSize.y*.5f;
    endPos.x = startPos.x + zoomedImageSize.x;
    endPos.y = startPos.y + zoomedImageSize.y;

    window->DrawList->AddImage(texid, startPos, endPos, uv0, uv1);

    if (isRMBclickedForContextMenu)  {
        ContextMenuData.contextMenuParent = this;
        ContextMenuData.mustOpenContextMenu = true;
        if (!(isMouseOnImage(io.MousePos.x,io.MousePos.y,ContextMenuData.hoverImageRectInPdfPageCoords,ContextMenuData.hoverImageId)))    {
            ContextMenuData.hoverImageRectInPdfPageCoords=Cairo::MakeRectangle(0,0,0,0);
            ContextMenuData.hoverImageId = -1;
        }
    }

    // Display user selected text:
    {
            static const ImU32 selectedTextCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.f,0.7f,1.f,.25f));

            // Possible opt: pretransform all this stuff per page
            const int npage = poppler_page_get_index(page);
            double width,height;poppler_page_get_size(page,&width,&height);
            ImVec2 st(0,0),en(0,0);
            ImRect clipRect = window->ClipRect;
            clipRect.Min.y+=topPanelHeight;
            if (firstUserRectangleOfCurrentPageIndex!=-1)   {
                for (int i=firstUserRectangleOfCurrentPageIndex,isz=userSelectionRectangles.size();i<isz;i++)	{
                    if (userSelectionRectangles[i].first!=npage) break;

                    const cairo_rectangle_t& r = userSelectionRectangles[i].second;

                    st = pdfRelativeToMouseCoords(ImVec2(r.x/width,r.y/height));
                    en = pdfRelativeToMouseCoords(ImVec2((r.x+r.width)/width,(r.y+r.height)/height));

                    // Manual clipping: if (clipRect.Overlaps(rect)) then draw
                    if (clipRect.Min.y < en.y  && clipRect.Max.y > st.y  && clipRect.Min.x < en.x && clipRect.Max.x > st.x)
                        window->DrawList->AddRectFilled(st,en,selectedTextCol,0);

                }
            }
            for (int i=0,isz=temporaryUserSelection.size();i<isz;i++)	{
                const cairo_rectangle_t& r = temporaryUserSelection[i];

                st = pdfRelativeToMouseCoords(ImVec2(r.x/width,r.y/height));
                en = pdfRelativeToMouseCoords(ImVec2((r.x+r.width)/width,(r.y+r.height)/height));

                // Manual clipping: if (clipRect.Overlaps(rect)) then draw
                if (clipRect.Min.y < en.y  && clipRect.Max.y > st.y  && clipRect.Min.x < en.x && clipRect.Max.x > st.x)
                    window->DrawList->AddRectFilled(st,en,selectedTextCol,0);
            }
    }

    if (isHovered || isHoveredRect) {
        //ImGui::SetTooltip("%s",isLMBMouseDragging?"true":"false");
        bool isSelectingText = isLMBMouseDragging;
        bool isLMBClicked = io.MouseClicked[0] && !isSelectingText;
        if (!isSelectingText) {
            // Fix temporary user selection
            if (wasLMBDragging) performSelectModeStep(io.MousePos.x,io.MousePos.y);
            if (temporaryUserSelection.size()>0)    {
                //---Finalize it by copying temporaryUserSelection into userSelectionRectangles
                int npage = poppler_page_get_index(page);
                ImVector< ImStl::Pair<int,cairo_rectangle_t> > toBeInserted;
                toBeInserted.reserve(temporaryUserSelection.size());
                for (int i=0,sz=temporaryUserSelection.size();i<sz;i++)  {
                    const cairo_rectangle_t& item = temporaryUserSelection[i];
                    toBeInserted.resize(toBeInserted.size()+1);
                    toBeInserted[toBeInserted.size()-1] = ImStl::Pair<int,cairo_rectangle_t>(npage,item);
                }

                if (userSelectionRectangles.size()==0) {
                    ImStl::AddRange(userSelectionRectangles,toBeInserted);
                    firstUserRectangleOfCurrentPageIndex=0;
                }
                else {
                    int index = 0;bool done = false;
                    while (index < (int)userSelectionRectangles.size()) {
                        const ImStl::Pair<int,cairo_rectangle_t>& tpl = userSelectionRectangles[index];
                        int pg = tpl.first;
                        if (pg>=npage)	{
                            firstUserRectangleOfCurrentPageIndex = pg==npage ? index : index-1;
                            if (firstUserRectangleOfCurrentPageIndex<0) firstUserRectangleOfCurrentPageIndex=0;
                            int possiblyPrevPage = userSelectionRectangles[firstUserRectangleOfCurrentPageIndex].first;
                            bool needsSorting = possiblyPrevPage==npage || pg==npage;
                            ImStl::InsertRange(userSelectionRectangles,firstUserRectangleOfCurrentPageIndex,toBeInserted);
                            if (needsSorting)	{
                                int start = firstUserRectangleOfCurrentPageIndex;
                                int end = userSelectionRectangles.size();
                                int cur = start;
                                while (cur < end && userSelectionRectangles[cur].first==npage) ++cur;
                                int cnt = cur-start;
                                //std::sort(userSelectionRectangles.begin()+start,userSelectionRectangles.begin()+(start+cnt),UserSelectionRectanglesComparer());	//We could just sort items in renge (firstUserRectangleOfCurrentPageIndex,....) of the same page...
                                qsort(&userSelectionRectangles[firstUserRectangleOfCurrentPageIndex],cnt,sizeof(ImStl::Pair<int,cairo_rectangle_t>),&UserSelectionRectanglesComparer);
                            }
                            //updateUserSelectionRectanglesExtraStuff();	//Needed ?
                            done = true;
                            break;
                        }
                        ++index;
                    }
                    if (!done)	{
                        firstUserRectangleOfCurrentPageIndex=userSelectionRectangles.size();
                        ImStl::AddRange(userSelectionRectangles,toBeInserted);
                    }
                }
                temporaryUserSelection.clear ();	// clear it
                //Console.WriteLine ("userSelectionRectangles.size() = "+userSelectionRectangles.size());
            }

            // Check links under mouse
            const float processorBreathInterval = isLMBClicked ? .0f : (io.MouseDelta.x==0.f && io.MouseDelta.y==0) ? .25f : .5f;
            {
                // Check to see if there's a link under the mouse
                static float time = ImGui::GetTime();
                float curTime = ImGui::GetTime();
                if (curTime-time>=processorBreathInterval || curTime-time<0 || isLMBClicked)   {
                    time = curTime;
                    isLinkUnderTheMouseValid = this->isMouseOnLink(io.MousePos.x,io.MousePos.y,linkUnderTheMouse);

                    if (isLinkUnderTheMouseValid && isLMBClicked) {
                        const Poppler::PopplerActionWrapper& aw =linkMapping[linkUnderTheMouse].second;
                        switch (aw.type)    {
                        case POPPLER_ACTION_GOTO_DEST:  {
                            double w,h;poppler_page_get_size(page,&w,&h);
                            float zoomCenterY = float((h-aw.destRectangle.y-(aw.destRectangle.height?(aw.destRectangle.height*.5):.0))/h);
                            //fprintf(stderr,"POPPLER_ACTION_GOTO_DEST: %d (%1.3f,%1.3f,%1.3f,%1.3f) sz(%1.1f,%1.1f) zoomCenterY=%1.3f\n",aw.pageNum,aw.destRectangle.x,aw.destRectangle.y,aw.destRectangle.width,aw.destRectangle.height,w,h,zoomCenterY);
                            pageChanged=setPage(aw.pageNum-1,true,zoomCenterY);
                            rv|=pageChanged;
                        }
                        break;
                        case POPPLER_ACTION_URI:
                        if (aw.uri && strlen(aw.uri)>0) {
#                           ifndef NO_IMGUIHELPER
                            ImGui::OpenWithDefaultApplication((const char*) aw.uri);
#                           else //NO_IMGUIHELPER
                            fprintf(stderr,"Must open URI: \"%s\"\n",aw.uri);
#                           endif //NO_IMGUIHELPER
                        }
                        break;
                        default:
                        break;
                        }
                    }                    
                }                
            }
            if (isLinkUnderTheMouseValid) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_Move);
                if (linkUnderTheMouse!=-1 && !pageChanged)// && !isLMBClicked)
                {
                    const cairo_rectangle_t& r = linkMapping[linkUnderTheMouse].first;

                    // Possible opt: cache this and tie it to linkUnderTheMouse
                    double width,height;poppler_page_get_size(page,&width,&height);
                    ImVec2 st = pdfRelativeToMouseCoords(ImVec2(r.x/width,r.y/height));
                    ImVec2 en = pdfRelativeToMouseCoords(ImVec2((r.x+r.width)/width,(r.y+r.height)/height));

                    static const ImU32 linkCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1,1,0,.25f));
                    window->DrawList->AddRectFilled(st,en,linkCol,0);
                }
            }
        }
        else {
            ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
            // TODO: select text
            // when pressing is started:
            if (!wasLMBDragging)    {
                if (!io.KeyCtrl) {
                    userSelectionRectangles.clear ();
                    updateUserSelectionRectanglesExtraStuff();
                }
                else temporaryUserSelection.clear ();
            }
            performSelectModeStep(io.MousePos.x,io.MousePos.y);
        }

        // Highlight search entries
        if (!pageChanged && firstRectangleOfCurrentPageIndex!=-1)   {
            static const ImU32 searchMatchCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.7,1.,0,.25f));
            static const ImU32 currentSearchMatchCol = ImGui::ColorConvertFloat4ToU32(ImVec4(1,0.7,0,.25f));

            // Possible opt: pretransform all this stuff per page
            const int npage = poppler_page_get_index(page);
            double width,height;poppler_page_get_size(page,&width,&height);
            ImVec2 st(0,0),en(0,0);
            ImRect clipRect = window->ClipRect;
            clipRect.Min.y+=topPanelHeight;
            for (int i=firstRectangleOfCurrentPageIndex,isz=findTextSelectionRectangles.size();i<isz;i++)	{
                if (findTextSelectionRectangles[i].first!=npage) break;

                const cairo_rectangle_t& r = findTextSelectionRectangles[i].second;

                st = pdfRelativeToMouseCoords(ImVec2(r.x/width,r.y/height));
                en = pdfRelativeToMouseCoords(ImVec2((r.x+r.width)/width,(r.y+r.height)/height));

                // Manual clipping: if (clipRect.Overlaps(rect)) then draw
                if (clipRect.Min.y < en.y  && clipRect.Max.y > st.y  && clipRect.Min.x < en.x && clipRect.Max.x > st.x)
                    window->DrawList->AddRectFilled(st,en,currentMainRectangleSelection==i ? currentSearchMatchCol : searchMatchCol,0);

            }




        }

    }

    wasLMBDragging = isLMBMouseDragging;


    return rv;
}


bool PdfPagePanel::render(const ImVec2& size,bool renderTopPanelToo) {
    bool rv = false;
    if (renderTopPanelToo) rv|=renderTopPanelOnly();
    if (!document || !page) return rv;
    ImVec2 curPos = ImGui::GetCursorPos();
    const ImVec2 wndSz(size.x>0 ? size.x : ImGui::GetWindowSize().x-curPos.x,size.y>0 ? size.y : ImGui::GetWindowSize().y-curPos.y);

    ImGui::PushID(this);

    // TODO: unwrap ImageZoomAndPan(...), and use uv0,uv1,zoomedImageSize and other mutable vars to optimize it
    if (!imageZoomAndPan(wndSz))  {
        if (ImGui::IsItemHovered()) {
            float mw = ImGui::GetIO().MouseWheel;
            if (mw) {
                const bool pageUp = mw<0;
                const float zoomFactor = .5/zoom;
                if (!pageUp && zoomCenter.y <= zoomFactor) {
                    rv|=setPage(poppler_page_get_index(page)-1);
                }
                else if (pageUp && zoomCenter.y >= 1.f - zoomFactor) {
                    rv|=setPage(poppler_page_get_index(page)+1);
                }
            }
        }
    }

    ImGui::PopID();

    // TEST:
    /*if (ImGui::GetIO().MouseDown[0]) {
        ImVec2 tmp = mouseToPdfRelativeCoords(ImGui::GetIO().MousePos);
        ImVec2 tmp2 = pdfRelativeToMouseCoords(tmp);
        ImGui::SetTooltip("%1.4f,%1.4f %1.4f,%1.4f <-> %1.4f,%1.4f",ImGui::GetIO().MousePos.x,ImGui::GetIO().MousePos.y,tmp.x,tmp.y,tmp2.x,tmp2.y);
    }*/

    // Static Stuff ========================================
    static int frameCnt = -1;
    ImGuiState& g = *GImGui;
    if (frameCnt!=g.FrameCount) {
        frameCnt=g.FrameCount;
        //---------------------
        static const char ContextMenuName[]="PdfPagePanelDefaultContextMenuName";
        if (PdfPagePanel::ContextMenuData.mustOpenContextMenu)    {
            ImGuiState& g = *GImGui; while (g.OpenedPopupStack.size() > 0) g.OpenedPopupStack.pop_back();   // Close all existing context-menus
            ImGui::OpenPopup(ContextMenuName);
            PdfPagePanel::ContextMenuData.mustOpenContextMenu = false;
        }
        // Here we could use a callback, but for now:------
        if (ContextMenuData.contextMenuParent && ContextMenuData.contextMenuParent->document && ContextMenuData.contextMenuParent->page && ImGui::BeginPopup(ContextMenuName)) {
            //ImGui::MenuItem("My pdf popup menu here");
            PdfPagePanel* pagePanel = ContextMenuData.contextMenuParent;
            if (ContextMenuData.hoverImageId>=0)    {
                if (ImGui::MenuItem("Save Image As Png")) {
                    cairo_surface_t* surf = pagePanel->pageToCairo(CAIRO_FORMAT_ARGB32,poppler_page_get_index(pagePanel->page),&ContextMenuData.hoverImageRectInPdfPageCoords);
                    if (surf)   {
                        //TODO: Generate name based on path file name and ContextMenuData.hoverImageId
                        Cairo::SaveToPng(surf,"mySavedPdfImage.png");
                        cairo_surface_destroy (surf);
                        surf=NULL;
                    }
                }
                ImGui::Separator();
            }
            if (pagePanel->userSelectionRectangles.size()>0)    {
                if (ImGui::MenuItem("Copy Selected Text"))	{
                    ImGui::SetClipboardText(pagePanel->getSelectedText());
                }
                ImGui::Separator();
            }
            if (ImGui::MenuItem("Copy All Page Text"))	{
                char* array = poppler_page_get_text(pagePanel->page);
                if (array)  {
                    ImGui::SetClipboardText(array);
                    g_free(array);
                }
            }
            if (ImGui::MenuItem("Copy All Document Text"))	{
                PopplerDocument* document = pagePanel->document;
                const int npages = poppler_document_get_n_pages(document);
                ImVector<char> text;text.resize(1);text[0]='\0';
                for (int i=0;i<npages;i++)	{
                    PopplerPage* page = poppler_document_get_page(document,i);
                    int sz=0,txtSz=0;
                    if (page)   {
                        char* array= poppler_page_get_text(page);
                        if (array) {
                            sz=strlen(array);
                            txtSz=text.size()-1;
                            text.resize(text.size()+sz);
                            strcpy(&text[txtSz],array);
                            text[text.size()-1]='\0';
                            g_free(array);
                        }
                        g_object_unref(page);
                    }
                }
                ImGui::SetClipboardText(&text[0]);
            }
            ImGui::Separator();
            /*
            if (ImGui::MenuItem("Save All Page Images"))	{
                Glib::ustring initialPath = GetDirectoryName(get_file_path());

                Gtk::FileChooserDialog dialog("Please Choose The Save Image Folder",Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
                SetAutoTransientFor(&dialog,this,true);
                dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
                dialog.add_button("Select", Gtk::RESPONSE_OK);
                dialog.set_current_folder(initialPath);
                if (dialog.run() != Gtk::RESPONSE_OK || dialog.get_filenames().size()==0 || dialog.get_filenames()[0].size()==0) return;

                Glib::ustring baseImageName = GetFileNameWithoutExtension (get_file_path())+"_pdf_page_"+ToString(pagePanel->getPageIndex()+1)+"_image.png";
                Glib::ustring fullPath = Combine(dialog.get_filenames()[0],baseImageName);
                Cairo::RefPtr<Cairo::ImageSurface> fullPageImage = pagePanel->pageToCairo (Cairo::FORMAT_RGB24,-1);
                if (!fullPageImage) return;
                Cairo::RefPtr<Cairo::ImageSurface> image;
                const std::vector<std::pair<Cairo::Rectangle,int> >& list = pagePanel->getImageMapping();
                for (int i=(int)list.size()-1;i>=0;i--) {
                    const std::pair<Cairo::Rectangle,int>& pr = list[i];
                    image = Cairo::Extract(fullPageImage,pr.first);
                    if (image) {
                        Cairo::SaveToPdf(image,GetNonExistingPath(fullPath,false,"_"));
                        image.clear();
                    }
                }
                fullPageImage.clear();
            }
            if (ImGui::MenuItem("Save All Document Images"))    {
                const int npages = poppler_document_get_n_pages(document);

                Glib::ustring initialPath = GetDirectoryName(get_file_path());
                Gtk::FileChooserDialog dialog("Please Choose The Save Image Folder",Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER);
                SetAutoTransientFor(&dialog,this,true);
                dialog.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
                dialog.add_button("Select", Gtk::RESPONSE_OK);
                dialog.set_current_folder(initialPath);
                if (dialog.run() != Gtk::RESPONSE_OK || dialog.get_filenames().size()==0 || dialog.get_filenames()[0].size()==0) return;

                Glib::ustring filename = dialog.get_filenames()[0];
                Cairo::RefPtr<Cairo::ImageSurface> fullPageImage;
                for (int i=0;i<npages;i++)	{
                    PopplerPage* page = poppler_document_get_page(document,i);
                    if (page)   {
                        Glib::ustring baseImageName = GetFileNameWithoutExtension (get_file_path())+"_pdf_page_"+ToString(poppler_page_get_index(page)+1)+"_image.png";
                        Glib::ustring fullPath = Combine(filename,baseImageName);
                        const std::vector<std::pair<Cairo::Rectangle,int> > list = Poppler::GetImageMapping(page);
                        fullPageImage = Poppler::PageToCairo(page,Cairo::FORMAT_RGB24);
                        if (!fullPageImage) continue;
                        Cairo::RefPtr<Cairo::ImageSurface> image;
                        for (int i=(int)list.size()-1;i>=0;i--) {
                            const std::pair<Cairo::Rectangle,int>& pr = list[i];
                            image = Cairo::Extract(fullPageImage,pr.first);
                            if (image) {
                                Cairo::SaveToPdf(image,GetNonExistingPath(fullPath,false,"_"));
                                image.clear();
                            }
                        }
                        fullPageImage.clear();
                        g_object_unref(page);page=NULL;
                    }
                }
            }
            */
            //----------------------------------------------------------
            ImGui::EndPopup();
        }
        //-------------------------------------------------
    }
    // =====================================================

/*
static Glib::ustring GetNonExistingPath(const Glib::ustring& path,bool isDirectory=false,const Glib::ustring& numberSeparator=".")	{
    bool (*exists)(const Glib::ustring&) = &FileExists;
    if (isDirectory)    exists = &DirectoryExists;
    if (!exists (path)) return path;

    const Glib::ustring parentFolder = GetDirectoryName (path);
    const Glib::ustring filenameWE = isDirectory ? GetFileName(path): GetFileNameWithoutExtension (path);
    const Glib::ustring ext = isDirectory ? "" : GetExtension (path);
    int cnt = 2;

    Glib::ustring newPath = path;
    while (exists(newPath)) {
        newPath = Combine (parentFolder,filenameWE+numberSeparator+ToString(cnt++));
        if (!isDirectory) newPath += ext;
    }
    return newPath;
}
*/

    return rv;
}

inline static void PdfViewerPushTransparentButtonStyle() {
    const ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 btnColor = style.Colors[ImGuiCol_Button];btnColor.w*=.35f;
    ImVec4 txtColor = style.Colors[ImGuiCol_Text];txtColor.w*=.35f;
    ImGui::PushStyleColor(ImGuiCol_Button,btnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,btnColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,btnColor);
    ImGui::PushStyleColor(ImGuiCol_Text,txtColor);
}
inline static void PdfViewerPopTransparentButtonStyle() {
    ImGui::PopStyleColor(4);
}

bool PdfPagePanel::renderTopPanelOnly()    {
    bool rv = false;
    ImGui::PushID(this);
    ImGui::BeginGroup();

    const bool canGoBackward = currentStackLevel>0;
    const bool canGoForward =  currentStackLevel<pageStack.size()-1;

    if (!canGoBackward) PdfViewerPushTransparentButtonStyle();
    if (ImGui::Button("<") && canGoBackward) rv|=goBackward();
    if (canGoBackward && ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Go Back");
    if (!canGoBackward) PdfViewerPopTransparentButtonStyle();
    ImGui::SameLine();
    if (!canGoForward) PdfViewerPushTransparentButtonStyle();
    if (ImGui::Button(">") && canGoForward) rv|=goForward();
    if (canGoForward && ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Go Foward");
    if (!canGoForward) PdfViewerPopTransparentButtonStyle();

    ImGui::SameLine(0.f,15.f);
    if (ImGui::Button("X")) rv|=resetZoomAndPan();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Reset Zoom And Pan");

    const float maxPageNumberWidth = ImGui::CalcTextSize("9999").x;
    const float remainingSpace = ImGui::GetContentRegionAvailWidth()-maxPageNumberWidth*14.25f;
    ImGui::SameLine(0.f,remainingSpace*.5f);
    ImGui::Text("Page:");
    ImGui::SameLine(0.f,0.f);
    int curPage=getPageIndex()+1;
    const int numPages=getNumPages();
    ImGui::PushItemWidth(maxPageNumberWidth);
    if (ImGui::DragInt("##Page",&curPage,0.05f,1,numPages)) {
        rv|=setPage(curPage-1);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Switch Pages");
    ImGui::PopItemWidth();
    ImGui::SameLine(0.f,0.f);
    ImGui::Text("/%d",numPages);

    ImGui::SameLine(0.f,remainingSpace*.5f);
    ImGui::Text("Search:");
    ImGui::SameLine(0.f,0.f);
    ImGui::PushItemWidth(maxPageNumberWidth*2.f);
    if (ImGui::InputText("##Search",searchText,SearchBufferSize,ImGuiInputTextFlags_EnterReturnsTrue))    rv|=findTextActivated();
    ImGui::PopItemWidth();
    ImGui::SameLine(0.f,0.f);
    if (currentMainRectangleSelection<0 || currentMainRectangleSelection>=findTextSelectionRectangles.size()) ImGui::Text("%3d/%3d",0,0);
    else ImGui::Text("%3d/%3d",currentMainRectangleSelection+1,findTextSelectionRectangles.size());

    ImGui::SameLine(0.f,-ImGui::CalcTextSize("^v").x-ImGui::GetStyle().FramePadding.x*4.f/*-ImGui::GetStyle().WindowPadding.x*/);
    if (ImGui::Button("^")) rv|=findPrev();
    ImGui::SameLine(0.f,0.f);
    if (ImGui::Button("v")) rv|=findNext();

    ImGui::EndGroup();
    ImGui::PopID();
    return rv;
}

bool PdfPagePanel::resetZoomAndPan()   {
    bool rv = zoom!=1.f || zoomCenter.x!=.5f || zoomCenter.y!=.5f;
    zoom=1.f;zoomCenter.x=zoomCenter.y=.5f;
    return rv;
}
void PdfPagePanel::resetPageStack() {
    pageStack.clear ();
    currentStackLevel = 0;
}
bool PdfPagePanel::goBackward()    {
    bool rv = false;
    if (!document)  return rv;
    int sz = (int) pageStack.size();
    if (sz > 0 && currentStackLevel>0) {
        --currentStackLevel;
        const PageStackPairType& currentStack = pageStack[currentStackLevel];
        rv = setPage(currentStack.first,false,currentStack.second);
    }
    return rv;
}
bool PdfPagePanel::goForward() {
    bool rv = false;
    if (!document)  return rv;
    int sz = (int) pageStack.size();
    if (currentStackLevel < sz-1) {
        ++currentStackLevel;
        const PageStackPairType& currentStack = pageStack[currentStackLevel];
        rv = setPage(currentStack.first,false,currentStack.second);
    }
    return rv;
}
bool PdfPagePanel::findText(const char *text)    {
    findTextSelectionRectangles.clear ();
    if (!text || strlen(text)==0 || !document) {
        updateFindTextSelectionRectanglesExtraStuff();
        return false;
    }
    ImVector<cairo_rectangle_t> list;
    const int npages = poppler_document_get_n_pages(document);
    for (int i = 0;i < npages;i++) {
        PopplerPage* page = poppler_document_get_page(document,i);
        if (page)   {
            list = Poppler::FindText(page,text,list);
            for (size_t l=0,lsz=list.size();l<lsz;l++)  {
                findTextSelectionRectangles.resize(findTextSelectionRectangles.size()+1);
                ImStl::Pair<int,cairo_rectangle_t> pr; pr.first = i; pr.second = list[l];
                findTextSelectionRectangles[findTextSelectionRectangles.size()-1]=pr;
                //findTextSelectionRectangles[findTextSelectionRectangles.size()-1].first=i;
                //findTextSelectionRectangles[findTextSelectionRectangles.size()-1].second=list[l];
            }
            g_object_unref(page);page=NULL;
        }
    }
    updateFindTextSelectionRectanglesExtraStuff();
    return false;
}
bool PdfPagePanel::findTextActivated()  {
    bool rv = false;
    if (!document)   return false;
    if (strcmp(searchText,lastTextSearched)==0) return findNext();

    strcpy(lastTextSearched,searchText);
    rv|=findText(searchText);

    //int numMatches = findTextSelectionRectangles.size();
    numDisplayedSearchMatch = -1;

    // Find numDisplayedSearchMatch based on currentPage-------------------------------------------------------------
    int cnt = -1;
    int numDisplayedSearchMatchFallback = -1;bool hasFallback = false;
    int currentPage = page ? poppler_page_get_index(page) : 0;
    for (int i=0,sz=findTextSelectionRectangles.size();i<sz;i++)  {
        const ImStl::Pair<int,cairo_rectangle_t>& entry = findTextSelectionRectangles[i];
        ++cnt;
        int npage = entry.first;
        if (npage < currentPage)	{
            if (hasFallback) continue;
            else {
                hasFallback=true;
                numDisplayedSearchMatchFallback = cnt;
            }
        }
        else {
            numDisplayedSearchMatch = cnt;
            break;
        }
    }
    if (numDisplayedSearchMatch == -1 && numDisplayedSearchMatchFallback!=-1) numDisplayedSearchMatch = numDisplayedSearchMatchFallback;
    //---------------------------------------------------------------------------------------------------------------------
    if (numDisplayedSearchMatch!=-1)	{
        // Some matches found:
        currentMainRectangleSelection = numDisplayedSearchMatch;
        const ImStl::Pair<int,cairo_rectangle_t>& entry = findTextSelectionRectangles[numDisplayedSearchMatch];
        setPage (entry.first,true,entry.second.y);	//Switch to correct page
        rv = true;
    }

    return rv;
}
bool PdfPagePanel::findPrev()    {
    if (strcmp(searchText,lastTextSearched)!=0) return findTextActivated();
    if (strlen(searchText)==0) return false;

    int numMatches = findTextSelectionRectangles.size();
    if (numMatches==0) return false;

    --numDisplayedSearchMatch;
    if (numDisplayedSearchMatch<0) numDisplayedSearchMatch = numMatches-1;
    if (numDisplayedSearchMatch<0) {numDisplayedSearchMatch=-1;return false;}

    currentMainRectangleSelection = numDisplayedSearchMatch;
    const ImStl::Pair<int,cairo_rectangle_t>& entry = findTextSelectionRectangles[numDisplayedSearchMatch];

    setPage (entry.first,true,entry.second.y);	//Switch to correct page
    //lblSearchOccurrencies->set_text(ToString(numDisplayedSearchMatch+1)+"/"+ToString(numMatches));

    return false;
}
bool PdfPagePanel::findNext()    {
    if (strcmp(searchText,lastTextSearched)!=0) return findTextActivated();
    if (strlen(searchText)==0) return false;

    int numMatches = findTextSelectionRectangles.size();
    if (numMatches==0) return false;

    ++numDisplayedSearchMatch;
    if (numDisplayedSearchMatch>=numMatches) numDisplayedSearchMatch = 0;
    if (numDisplayedSearchMatch>=numMatches) {numDisplayedSearchMatch=-1;return false;}

    currentMainRectangleSelection = numDisplayedSearchMatch;
    const ImStl::Pair<int,cairo_rectangle_t>& entry = findTextSelectionRectangles[numDisplayedSearchMatch];
    setPage (entry.first,true,entry.second.y);	//Switch to correct page
    //lblSearchOccurrencies->set_text(ToString(numDisplayedSearchMatch+1)+"/"+ToString(numMatches));

    return true;
}

const char* PdfPagePanel::getSelectedText() const {
    if (!document) return "";
    PopplerSelectionStyle selectionStyle = POPPLER_SELECTION_GLYPH;;
    ImVector<char>& str=selectedTextVector;
    str.resize(1);str[0]='\0';
    PopplerPage* page = NULL;
    gchar *pageSelectedText = NULL,*lastPageSelectedText = NULL;
    bool newPage = false;
    for (int i=0,sz=userSelectionRectangles.size();i<sz;i++)  {
        const ImStl::Pair<int,cairo_rectangle_t>& entry = userSelectionRectangles[i];
        try {
            if (page==NULL || entry.first!=poppler_page_get_index(page)) {
                if (page) {g_object_unref(page);page = NULL;}
                page = poppler_document_get_page(document,entry.first);
                newPage = true;
            }
            else newPage = false;
            const cairo_rectangle_t& r = entry.second;
            // Each rectangle is a text line, but their height is too high: successive rectangles may include previous text line if we don't fix it
            double hTrim = r.height*0.25;
            pageSelectedText = Poppler::GetSelectedText(page,Cairo::MakeRectangle(r.x,r.y+hTrim,r.width,r.height-2.0*hTrim),selectionStyle);

            if (!pageSelectedText || strlen(pageSelectedText)==0) lastPageSelectedText = NULL;
            else    {                
                const int sz = strlen(pageSelectedText);
                str.resize(str.size()+sz);
                strcat(&str[0],pageSelectedText);
                //str.AppendLine(pageSelectedText);	//Not that easy, unluckily....
                /*
                        beg = pageSelectedText.IndexOf('\n');
                        if (beg==-1) str.AppendLine(pageSelectedText);
                        else str.AppendLine(pageSelectedText.Substring (0,beg));
                        */
                /*
                        if (lastPageSelectedText.Length>0 && pageSelectedText.StartsWith(lastPageSelectedText)) str.Append(pageSelectedText.Substring (lastPageSelectedText.Length));
                        else str.Append(pageSelectedText);
                        if (newPage) str.AppendLine();

                        beg = pageSelectedText.LastIndexOf('\n');// Terrible hack: but it used to "multiply" lines! Issue: text with the same line repeated more than once will export a single line!
                        if (beg!=-1) lastPageSelectedText = pageSelectedText.Substring (beg+1);
                        else lastPageSelectedText = "";
                        */
            }
        } catch (...)
        {}
           if (pageSelectedText) {g_free(pageSelectedText);pageSelectedText=NULL;}
    }
    if (page) {g_object_unref(page);page = NULL;}
    return &str[0];//.ToString ();
}


PdfViewer::PdfViewer()  {
    pagePanel = (PdfPagePanel*) ImGui::MemAlloc(sizeof(PdfPagePanel));
    IM_PLACEMENT_NEW (pagePanel) PdfPagePanel();
    filePath = NULL;
    init = false;
}
PdfViewer::~PdfViewer() {
    destroy();
    if (pagePanel) {
        pagePanel->~PdfPagePanel();
        ImGui::MemFree(pagePanel);
        pagePanel = NULL;
    }
}
void PdfViewer::destroy(){
    pagePanel->destroy();
    ImStl::Free(filePath);
}
bool PdfViewer::loadFromFile(const char *path)  {
    const bool rv = pagePanel->loadFromFile(path);
    if (rv) ImStl::StrCpy(filePath,path);
    return rv;
}
bool PdfViewer::render(const ImVec2 &size)  {
    init = true;
    return pagePanel->render(size,true);
}




} // namespace ImGui


