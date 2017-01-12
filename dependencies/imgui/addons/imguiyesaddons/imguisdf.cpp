#include "imguisdf.h"
#include "../imguicodeeditor/utf8helper.h"

#ifdef NO_IMGUISTRING
#error imguistring is necessary
#endif //NO_IMGUISTRING

#include "../imguistring/imguistring.h"

#ifdef IMIMPL_SHADER_NONE
#warning Shaders are mandatory
#endif //IMIMPL_SHADER_NONE

#ifdef IMGUI_USE_DIRECT3D9_BINDING
#error Direct3D port is missing
#endif //IMGUI_USE_DIRECT3D9_BINDING

// TODO: try to reuse ImDrawList to do the drawing (instead of using vbos)


namespace ImGui {

static SdfTextColor gSdfTextDefaultColor(ImVec4(1,1,1,1));

class SdfVertexBuffer {
public:
    void clear() {verts.clear();}
    ~SdfVertexBuffer() {if (vbo) glDeleteBuffers(1,&vbo);}
    void updateBoundVbo() const {
        const GLenum drawMode = preferStreamDrawBufferUsage ? GL_STREAM_DRAW : GL_STATIC_DRAW;
        //glBufferData(GL_ARRAY_BUFFER, verts.size()*sizeof(VertexDeclaration),verts.size()!=0 ? &verts[0].posAndUV.x : NULL, drawMode);return;

        if (maxVertsSize<verts.size()) maxVertsSize=verts.size();
        glBufferData(GL_ARRAY_BUFFER, maxVertsSize*sizeof(VertexDeclaration), NULL, drawMode);
        if (verts.size()>0) {
            glBufferSubData(GL_ARRAY_BUFFER,0,verts.size()*sizeof(VertexDeclaration),&verts[0].posAndUV.x);
            //fprintf(stderr,"%d indices\n",verts.size());
        }
    }
    inline int getType() const {return type;}
    inline void setType(int _type) {type=_type;}
    inline const ImVec4* getColorOfVert(int num) const {return verts.size()>num*6 ? &verts[num*6].color : NULL;}
    inline int size() const {return verts.size();}
    inline int numChars() const {return verts.size()/6;}
#ifndef _MSC_VER    // Not sure, but old cl compilers can have problems here
protected:
#endif //_MSC_VER
    SdfVertexBuffer(int _type=SDF_BT_OUTLINE,bool _preferStreamDrawBufferUsage=false) : type(_type),preferStreamDrawBufferUsage(_preferStreamDrawBufferUsage) {maxVertsSize=0;vbo=0;}
    struct VertexDeclaration {
        ImVec4 posAndUV;
        ImVec4 color;
    };
    ImVector<VertexDeclaration> verts;
    GLuint vbo;
    int type;
    bool preferStreamDrawBufferUsage;

    mutable int maxVertsSize;
    friend struct SdfCharset;
    friend struct SdfTextChunk;
    friend void SdfRender(const ImVec4 *pViewportOverride);
    friend bool SdfTextChunkEdit(SdfTextChunk* sdfTextChunk,char* buffer,int bufferSize);
};

struct SdfCharDescriptor
{
    unsigned int Id;                // The character id.
    float X, Y;            // The left and top position of the character image in the texture.
    float Width, Height;   // The width and height of the character image in the texture.
    float XOffset, YOffset;         // How much the current position should be offset when copying the image from the texture to the screen   ( top/left offsets ).
    float XOffset2, YOffset2;         // How much the current position should be offset when copying the image from the texture to the screen ( lower/right offsets ).
    float XAdvance;                 // How much the current position should be advanced after drawing the character.
    unsigned char Page;             // The texture page where the character image is found.

    SdfCharDescriptor() : Id(0), X( 0 ), Y( 0 ), Width( 0 ), Height( 0 ), XOffset( 0 ), YOffset( 0 ), XOffset2( 0 ), YOffset2( 0 ),
        XAdvance( 0 ), Page( 0 )
    { }
};

struct SdfAnimation {
    //public:
    SdfAnimation() {totalTime=0.f;}
    SdfAnimation(const SdfAnimation& o) {*this=o;totalTime=0.f;}
    SdfAnimation(const ImVector<SdfAnimationKeyFrame>& _keyFrames) {cloneKeyFramesFrom(_keyFrames);totalTime=0.f;looping=false;mustMuteAtEnd=true;}
    ~SdfAnimation() {}
    const SdfAnimation& operator=(const SdfAnimation& o)    {
        cloneKeyFramesFrom(o.keyFrames);
        return *this;
    }
    //protected:
    ImVector<SdfAnimationKeyFrame> keyFrames;
    float totalTime;
    bool looping;
    bool mustMuteAtEnd;
    inline void cloneKeyFramesFrom(const ImVector<SdfAnimationKeyFrame>& _keyFrames) {
        // deep clone keyframes
        const int nkf = _keyFrames.size();
        keyFrames.clear();keyFrames.reserve(nkf);
        for (int i=0;i<nkf;i++) keyFrames[i] = _keyFrames[i];
    }
    inline float addKeyFrame(const SdfAnimationKeyFrame& kf) {
        keyFrames.push_back(kf);
        totalTime+=kf.timeInSeconds;
        return totalTime;
    }
    inline bool removeKeyFrameAt(int i) {
        if (i<0 || i>=keyFrames.size()) return false;
        for (int j=i,jsz=keyFrames.size()-1;j<jsz;j++) keyFrames[j] = keyFrames[j+1];
        keyFrames.pop_back();
        update();
        return true;
    }
    inline void clear() {keyFrames.clear();totalTime=0.f;}
    inline void update() {
        totalTime = 0.f;
        for (int i=0,isz=keyFrames.size();i<isz;i++)    {
            totalTime+=keyFrames[i].timeInSeconds;
        }
    }
};

struct SdfTextChunk {
    SdfTextChunkProperties props;
    const SdfCharset* charset;
    SdfVertexBuffer* buffer;
    int numValidGlyphs;
    mutable bool dirty;
    ImVec2 shadowOffsetInPixels;
    bool mute;

    SDFAnimationMode animationMode;
    SdfAnimation* manualAnimationRef;
    float animationStartTime;
    SdfAnimationParams animationParams;
    SdfGlobalParams globalParams;

    struct TextBit {
	ImVectorEx<SdfCharDescriptor> charDescriptors;
	ImVectorEx<float> kernings;
        ImVec2 scaling;
        SdfTextColor sdfTextColor;
        bool italic;
	int hAlignment;
    };
    ImVectorEx<TextBit> textBits;
    SdfTextChunk() {charset=NULL;buffer=NULL;numValidGlyphs=0;shadowOffsetInPixels.x=shadowOffsetInPixels.y=4.f;mute = false;animationMode=SDF_AM_NONE;manualAnimationRef=NULL;animationStartTime=-1.f;tmpVisible = true;tmpLocalTime=0.f;}
    SdfTextChunk(const SdfCharset* _charset,int bufferType,const SdfTextChunkProperties& properties=SdfTextChunkProperties(),bool preferStreamDrawBufferUsage=false) {
        buffer = (SdfVertexBuffer*) ImGui::MemAlloc(sizeof(SdfVertexBuffer));
        IM_PLACEMENT_NEW (buffer) SdfVertexBuffer(bufferType,preferStreamDrawBufferUsage);
        IM_ASSERT(buffer);
        charset = _charset;
        IM_ASSERT(charset);
        props = properties;
        dirty = true;
        numValidGlyphs=0;
        shadowOffsetInPixels.x=shadowOffsetInPixels.y=4.f;
        mute = false;
        animationMode=SDF_AM_NONE;manualAnimationRef=NULL;animationStartTime=-1.f;
        tmpVisible = true;tmpLocalTime=0.f;
    }
    ~SdfTextChunk() {
        if (buffer) {
            buffer->~SdfVertexBuffer();
            ImGui::MemFree(buffer);
	    buffer=NULL;
	}
    }
    inline void setMute(bool flag) {
        if (mute!=flag) {
            animationStartTime = -1.f;
            mute = flag;
        }
    }

    // These tmp variables are valid per frame and set by next methods:
    mutable bool tmpVisible;
    mutable float tmpLocalTime;
    mutable SdfAnimationKeyFrame tmpKeyFrame;

    inline static float Lerp(float t,float v0,float v1) {return v0+t*(v1-v0);}
    inline static int LerpInt(float t,int v0,int v1) {return v0+(int)((t*(float)(v1-v0))+0.5f);}
    inline static void Lerp(float t,SdfAnimationKeyFrame& kfOut,const SdfAnimationKeyFrame& kf0,const SdfAnimationKeyFrame& kf1,int numGlyphs)    {
        kfOut.alpha     = (kf0.alpha==kf1.alpha)		? kf0.alpha	: Lerp(t,kf0.alpha,kf1.alpha);
        kfOut.offset.x	= (kf0.offset.x==kf1.offset.x)	? kf0.offset.x	: Lerp(t,kf0.offset.x,kf1.offset.x);
        kfOut.offset.y	= (kf0.offset.y==kf1.offset.y)	? kf0.offset.y	: Lerp(t,kf0.offset.y,kf1.offset.y);
        kfOut.scale.x	= (kf0.scale.x==kf1.scale.x)	? kf0.scale.x	: Lerp(t,kf0.scale.x,kf1.scale.x);
        kfOut.scale.y	= (kf0.scale.y==kf1.scale.y)	? kf0.scale.y	: Lerp(t,kf0.scale.y,kf1.scale.y);
	kfOut.startChar = (kf0.startChar==kf1.startChar)    ? kf0.startChar : LerpInt(t,kf0.startChar,kf1.startChar);
	kfOut.endChar	= (kf0.endChar==kf1.endChar)	? kf0.endChar	: LerpInt(t,(kf0.endChar<0)?numGlyphs:kf0.endChar,(kf1.endChar<0)?numGlyphs:kf1.endChar);
    }

    inline bool checkVisibleAndEvalutateAnimationIfNecessary(float time) {
        if (mute || (animationMode==SDF_AM_MANUAL && (!manualAnimationRef || manualAnimationRef->keyFrames.size()==0 || manualAnimationRef->totalTime==0.f))) return (tmpVisible=false);
        if (animationStartTime < 0) animationStartTime = time;
        tmpLocalTime=(time-animationStartTime)*animationParams.speed-animationParams.timeOffset;
        if (tmpLocalTime<0) return (tmpVisible=false);
        if (animationMode==SDF_AM_MANUAL && tmpLocalTime>manualAnimationRef->totalTime)   {
            // return (tmpVisible=false);
            if (!manualAnimationRef->looping) {
                animationStartTime=-1.f;animationMode=SDF_AM_NONE;
                if (manualAnimationRef->mustMuteAtEnd) {setMute(true);return (tmpVisible=false);}
                else {/*tmpKeyFrame = SdfAnimationKeyFrame();*/setMute(false);return (tmpVisible=true);}
            }
            else if (manualAnimationRef->totalTime>0.f) {
                while (tmpLocalTime>manualAnimationRef->totalTime) {tmpLocalTime-=manualAnimationRef->totalTime;animationStartTime+=manualAnimationRef->totalTime;}
                //fprintf(stderr,"time: %1.4f/%1.4f (%d frames)\n",tmpLocalTime,manualAnimationRef->totalTime,manualAnimationRef->keyFrames.size());
                }
        }
        // evalutate animation here:
        tmpKeyFrame = SdfAnimationKeyFrame();
        switch (animationMode)  {
        case SDF_AM_FADE_IN:  {if (tmpLocalTime<1.f)  tmpKeyFrame.alpha = tmpLocalTime;else  {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}}
            break;
        case SDF_AM_FADE_OUT:  {if (tmpLocalTime<1.f)  tmpKeyFrame.alpha = 1.f-tmpLocalTime;else {setMute(true);animationStartTime=-1.f;animationMode=SDF_AM_NONE;return (tmpVisible=false);}}
            break;
        case SDF_AM_ZOOM_IN:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = tmpLocalTime;   // in [0,1]
                tmpLocalTime = (2.0f-tmpLocalTime); // in [2,1]
                tmpLocalTime = tmpLocalTime*tmpLocalTime*tmpLocalTime;  // in [8,1]
                tmpKeyFrame.scale.x=tmpKeyFrame.scale.y = tmpLocalTime;
            }
            else  {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}}
            break;
        case SDF_AM_ZOOM_OUT:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = 1.f-tmpLocalTime;
                tmpLocalTime = (1.0f+tmpLocalTime); // in [1,2]
                tmpLocalTime = tmpLocalTime*tmpLocalTime*tmpLocalTime;  // in [1,8]
                tmpKeyFrame.scale.x=tmpKeyFrame.scale.y = tmpLocalTime;
            }
            else {setMute(true);animationStartTime=-1.f;animationMode=SDF_AM_NONE;return (tmpVisible=false);}}
            break;    
        case SDF_AM_APPEAR_IN:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = tmpLocalTime;   // in [0,1]
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.scale.y = tmpLocalTime;
            }
            else  {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}}
        break;
        case SDF_AM_APPEAR_OUT:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = 1.f-tmpLocalTime;
                tmpLocalTime = tmpKeyFrame.alpha;
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.scale.y = tmpLocalTime;
            }
            else {setMute(true);animationStartTime=-1.f;animationMode=SDF_AM_NONE;return (tmpVisible=false);}}
        break;
        case SDF_AM_LEFT_IN:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = tmpLocalTime;   // in [0,1]
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.x = -1.0f + tmpLocalTime;
            }
            else  {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}}
        break;
        case SDF_AM_LEFT_OUT:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = 1.f-tmpLocalTime;
                tmpLocalTime = tmpKeyFrame.alpha;
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.x = -1.0f + tmpLocalTime;
            }
            else {setMute(true);animationStartTime=-1.f;animationMode=SDF_AM_NONE;return (tmpVisible=false);}}
        break;
         case SDF_AM_RIGHT_IN:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = tmpLocalTime;   // in [0,1]
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.x = 1.0f - tmpLocalTime;
            }
            else  {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}}
        break;
        case SDF_AM_RIGHT_OUT:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = 1.f-tmpLocalTime;
                tmpLocalTime = tmpKeyFrame.alpha;
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.x = 1.0f - tmpLocalTime;
            }
            else {setMute(true);animationStartTime=-1.f;animationMode=SDF_AM_NONE;return (tmpVisible=false);}}
        break;
        case SDF_AM_TOP_IN:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = tmpLocalTime;   // in [0,1]
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.y = -1.0f + tmpLocalTime;
            }
            else  {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}}
        break;
        case SDF_AM_TOP_OUT:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = 1.f-tmpLocalTime;
                tmpLocalTime = tmpKeyFrame.alpha;
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.y = -1.0f + tmpLocalTime;
            }
            else {setMute(true);animationStartTime=-1.f;animationMode=SDF_AM_NONE;return (tmpVisible=false);}}
        break;
         case SDF_AM_BOTTOM_IN:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = tmpLocalTime;   // in [0,1]
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.y = 1.0f - tmpLocalTime;
            }
            else  {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}}
        break;
        case SDF_AM_BOTTOM_OUT:  {
            if (tmpLocalTime<1.f)  {
                tmpKeyFrame.alpha = 1.f-tmpLocalTime;
                tmpLocalTime = tmpKeyFrame.alpha;
                tmpLocalTime *= tmpLocalTime;
                tmpKeyFrame.offset.y = 1.0f - tmpLocalTime;
            }
            else {setMute(true);animationStartTime=-1.f;animationMode=SDF_AM_NONE;return (tmpVisible=false);}}
        break;
        case SDF_AM_BLINK:   {float tmp = (float) ((((int)(tmpLocalTime*100.f))%101)-50)*0.02f;tmpKeyFrame.alpha = tmp*tmp;}
            break;
        case SDF_AM_PULSE:   {
            tmpKeyFrame.scale.x=tmpKeyFrame.scale.y = 1.0f+((0.005f*20.f)/(float)(this->props.maxNumTextLines))*sinf(tmpLocalTime*10.0f);
            }
            break;
        case SDF_AM_TYPING:   {
            static const float timePerChar = 0.15f;
            const int numChars = buffer->numChars();
            const float timeForAllChars = timePerChar * numChars;
            if (timeForAllChars>0)  {
                if (tmpLocalTime<=timeForAllChars)	{
		    tmpKeyFrame.endChar = (int)(((tmpLocalTime/timeForAllChars)*(float)(numChars))+0.5f);
                }
                else {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}
            }
            else {setMute(false);animationStartTime=-1.f;animationMode=SDF_AM_NONE;}
        }
            break;
        case SDF_AM_MANUAL: {
            const SdfAnimation& an = *manualAnimationRef;
            const int nkf = an.keyFrames.size();
            static SdfAnimationKeyFrame ZeroKF(0.f,0.f);
            float sumTime = 0;int i=0;const SdfAnimationKeyFrame* kf = NULL;
            for (i=0;i<nkf;i++) {
                kf = &an.keyFrames[i];
                if (kf->timeInSeconds==0.f) continue;
                if (tmpLocalTime <= sumTime + kf->timeInSeconds) break;
                sumTime+=kf->timeInSeconds;
            }
	    //if (!kf) kf = (nkf>0 && animationParams.looping) ? &an.keyFrames[nkf-1] : &ZeroKF;
	    //fprintf(stderr,"FRAME: %d\n",i);
	    IM_ASSERT(kf && kf->timeInSeconds);
        const SdfAnimationKeyFrame* kf_prev = (i>=1 ? &an.keyFrames[i-1] : (nkf>0 &&
            (tmpLocalTime>manualAnimationRef->totalTime && manualAnimationRef->looping) // Not sure what I meant here... now I'm just correcting what it was: "tmpLocalTime>manualAnimationRef->looping", but looping is a bool. What did I mean ?
            ) ? &an.keyFrames[nkf-1] : &ZeroKF);
            const float deltaTime = (tmpLocalTime-sumTime)/kf->timeInSeconds;   // in [0,1]
            IM_ASSERT(deltaTime>=0.f && deltaTime<=1.f);
            Lerp(deltaTime,tmpKeyFrame,*kf_prev,*kf,buffer->numChars());
        }
            break;
        default:
            break;
        }

    const bool applyAnimationParams = true;
    if (applyAnimationParams)   {
        // Here we apply animationParams to the calculated fields
        tmpKeyFrame.alpha*=globalParams.alpha;
        tmpKeyFrame.offset.x+=globalParams.offset.x;
        tmpKeyFrame.offset.y+=globalParams.offset.y;
        tmpKeyFrame.scale.x*=globalParams.scale.x;
        tmpKeyFrame.scale.y*=globalParams.scale.y;
    }

    return (tmpVisible=(tmpLocalTime>=0));
    }

    inline bool setupUniformValuesAndDrawArrays(struct SdfShaderProgram* SP, bool shadowPass, const ImVec2 &screenSize);


    inline void addText(const char* startText,bool _italic,const SdfTextColor* pSdfTextColor,const ImVec2* textScaling,const char* endText,const SDFHAlignment *phalignOverride, bool fakeBold);
    inline void clearText() {
        if (buffer) buffer->clear();
	textBits.clear();
        dirty=false;
        numValidGlyphs=0;
    }
    inline void assignText(const char* startText,bool _italic,const SdfTextColor* pSdfTextColor,const ImVec2* textScaling,const char* endText,const SDFHAlignment *phalignOverride, bool fakeBold) {
        clearText();
        addText(startText,_italic,pSdfTextColor,textScaling,endText,phalignOverride,fakeBold);
    }

    bool endText(ImVec2 screenSize=ImVec2(-1,-1));
};

struct SdfShaderProgram {
    GLuint program;
    GLint uniformLocationOrthoMatrix;
    GLint uniformLocationSampler;
    GLint uniformLocationOffsetAndScale;
    GLint uniformLocationAlphaAndShadow;
    SdfShaderProgram() : program(0),uniformLocationOrthoMatrix(-1),uniformLocationSampler(-1),
    uniformLocationOffsetAndScale(-1),uniformLocationAlphaAndShadow(-1) {}
    ~SdfShaderProgram() {destroy();}
    void destroy() {
        if (program) {glDeleteProgram(program);program=0;}
    }
    bool loadShaderProgram(bool forOutlineShaderProgram) {
        if (program) return true;
        program = CompileShaderProgramAndSetCorrectAttributeLocations(forOutlineShaderProgram);
        if (program)    {
            uniformLocationOrthoMatrix = glGetUniformLocation(program,"ortho");
            uniformLocationSampler = glGetUniformLocation(program,"Texture");
            uniformLocationOffsetAndScale = glGetUniformLocation(program,"offsetAndScale");
            uniformLocationAlphaAndShadow = glGetUniformLocation(program,"alphaAndShadow");
            IM_ASSERT(uniformLocationOrthoMatrix!=-1);
            IM_ASSERT(uniformLocationSampler!=-1);
            IM_ASSERT(uniformLocationOffsetAndScale!=-1);
            IM_ASSERT(uniformLocationAlphaAndShadow!=-1);

            glUseProgram(program);
            resetUniforms();
            glUseProgram(0);
        }
        return program!=0;
    }
    void resetUniforms() {
        resetUniformOrtho();
        glUniform1i(uniformLocationSampler, 0);
        setUniformOffsetAndScale();
        setUniformAlphaAndShadow();
    }
    inline void setUniformOffsetAndScale(const ImVec2& offset=ImVec2(0,0),const ImVec2& scale=ImVec2(1,1)) {
        glUniform4f(uniformLocationOffsetAndScale,offset.x,offset.y,scale.x,scale.y);
    }
    // Shadow must be < 1 only in the "shadow pass"
    inline void setUniformAlphaAndShadow(const float alpha=1.f,const float shadow=1.f) {
        glUniform2f(uniformLocationAlphaAndShadow,alpha,shadow);
    }
    void resetUniformOrtho() {
        const float ortho[4][4] = {
            { 2.0f/ImGui::GetIO().DisplaySize.x,   0.0f,           0.0f, 0.0f },
            { 0.0f,         2.0f/-ImGui::GetIO().DisplaySize.y,   0.0f, 0.0f },
            { 0.0f,         0.0f,          -1.0f, 0.0f },
            {-1.0f,         1.0f,           0.0f, 1.0f },
        };
        glUniformMatrix4fv(uniformLocationOrthoMatrix, 1, GL_FALSE, &ortho[0][0]);
    }
    static const char** GetVertexShaderCodeForBothFonts() {
        static const char* gVertexShaderSource[] = {
#           ifdef IMIMPL_SHADER_GL3
#           if (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
            "#version 300 es\n"
#           else  //IMIMPL_SHADER_GLES
            "#version 330\n"
#           endif //IMIMPL_SHADER_GLES
            "precision highp float;\n"
            "uniform mat4 ortho;\n"
            "uniform vec4 offsetAndScale;\n"
            "layout (location = 0 ) in vec2 Position;\n"
            "layout (location = 1 ) in vec2 UV;\n"
            "layout (location = 2 ) in vec4 Colour;\n"
            "out vec2 Frag_UV;\n"
            "out vec4 Frag_Colour;\n"
#           else //!IMIMPL_SHADER_GL3
#           if (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
            "#version 100\n"
            "precision highp float;\n"
#           endif //IMIMPL_SHADER_GLES
            "uniform mat4 ortho;\n"
            "uniform vec4 offsetAndScale;\n"
            "attribute vec2 Position;\n"
            "attribute vec2 UV;\n"
            "attribute vec4 Colour;\n"
            "varying vec2 Frag_UV;\n"
            "varying vec4 Frag_Colour;\n"
#           endif //!IMIMPL_SHADER_GL3
            "void main()\n"
            "{\n"
            " Frag_UV = UV;\n"
            " Frag_Colour = Colour;\n"
            "\n"
            " gl_Position = ortho*vec4(offsetAndScale.x+Position.x*offsetAndScale.z,offsetAndScale.y+Position.y*offsetAndScale.w,0,1);\n"
            "}\n"
        };
        return &gVertexShaderSource[0];
    }
    static const char** GetFragmentShaderCodeForRegularFont() {
        static const char* gFragmentShaderSource[] = {
#       ifdef IMIMPL_SHADER_GL3
#       if (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
            "#version 300 es\n"
#       else //IMIMPL_SHADER_GLES
            "#version 330\n"
#       endif //IMIMPL_SHADER_GLES
            "precision mediump float;\n"
            "uniform lowp sampler2D Texture;\n"
            "in vec2 Frag_UV;\n"
            "in vec4 Frag_Colour;\n"
            "out vec4 FragColor;\n"
#       else //!IMIMPL_SHADER_GL3
#       if (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
            "#version 100\n"
            "#extension GL_OES_standard_derivatives : enable\n" // fwidth
            "precision mediump float;\n"
#       endif //IMIMPL_SHADER_GLES
            "uniform sampler2D Texture;\n"
            "varying vec2 Frag_UV;\n"
            "varying vec4 Frag_Colour;\n"
#       endif //IMIMPL_SHADER_GL3
            "uniform vec2 alphaAndShadow;\n"
#       ifdef YES_IMGUISDF_MSDF_MODE    // never tested
            "float median(float r, float g, float b) {\n"
            "    return max(min(r, g), min(max(r, g), b));\n"
            "}\n"
#       endif //YES_IMGUISDF_MSDF_MODE
            "void main(void) {\n"
#       ifndef YES_IMGUISDF_MSDF_MODE
            "float dist = texture2D(Texture, Frag_UV.st).a; // retrieve distance from texture\n"
#       else //YES_IMGUISDF_MSDF_MODE   // never tested
            "vec3 dist3 = texture2D(Texture, Frag_UV.st).rgb; // retrieve distance from texture\n"
            "float dist = median(dist3.r, dist3.g, dist3.b);// - 0.5;\n"
#       endif //YES_IMGUISDF_MSDF_MODE
            "float width = fwidth(dist);"
            "\n"
            "float alphaThreshold = 0.5;\n"
            "\n"
            "vec3 fragcolor = Frag_Colour.rgb;\n"
            "float alpha = smoothstep(alphaThreshold - width, alphaThreshold + width, dist);\n"
            "\n"
#       ifdef IMIMPL_SHADER_GL3
            "FragColor = vec4(fragcolor,alpha*Frag_Colour.a);\n"
#       else //IMIMPL_SHADER_GL3
            "gl_FragColor = vec4(fragcolor*alphaAndShadow.y,alpha*alphaAndShadow.x*Frag_Colour.a);\n"
#       endif //IMIMPL_SHADER_GL3
            "}\n"
        };
        return &gFragmentShaderSource[0];
    }
    static const char** GetFragmentShaderCodeForOutlineFont() {
        static const char* gFragmentShaderSource[] = {
#       ifdef IMIMPL_SHADER_GL3
#       if (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
            "#version 300 es\n"
#       else //IMIMPL_SHADER_GLES
            "#version 330\n"
#       endif //IMIMPL_SHADER_GLES
            "precision mediump float;\n"
            "uniform lowp sampler2D Texture;\n"
            "in vec2 Frag_UV;\n"
            "in vec4 Frag_Colour;\n"
            "out vec4 FragColor;\n"
#       else //!IMIMPL_SHADER_GL3
#       if (defined(IMIMPL_SHADER_GLES) || defined(__EMSCRIPTEN__))
            "#version 100\n"
            "#extension GL_OES_standard_derivatives : enable\n" // fwidth
            "precision mediump float;\n"
#       endif //IMIMPL_SHADER_GLES
            "uniform sampler2D Texture;\n"
            "varying vec2 Frag_UV;\n"
            "varying vec4 Frag_Colour;\n"
#       endif //IMIMPL_SHADER_GL3
            "uniform vec2 alphaAndShadow;\n"
#       ifdef YES_IMGUISDF_MSDF_MODE    // never tested
            "float median(float r, float g, float b) {\n"
            "    return max(min(r, g), min(max(r, g), b));\n"
            "}\n"
#       endif //YES_IMGUISDF_MSDF_MODE
            "void main(void) {\n"
#       ifndef YES_IMGUISDF_MSDF_MODE
            "float dist = texture2D(Texture, Frag_UV.st).a; // retrieve distance from texture\n"
#       else //YES_IMGUISDF_MSDF_MODE   // never tested
            "vec3 dist3 = texture2D(Texture, Frag_UV.st).rgb; // retrieve distance from texture\n"
            "float dist = median(dist3.r, dist3.g, dist3.b);// - 0.5;\n"
#       endif //YES_IMGUISDF_MSDF_MODE
            "float width = fwidth(dist);"
            "\n"
            "float alphaThreshold = 0.4;\n"
            "float outlineDarkeningFactor = 0.3;\n"
            "float outlineThreshold = 0.5;" // 0.5f
            "float inside = smoothstep(outlineThreshold - width, outlineThreshold + width, dist) ;\n"
            "//float glow = 1.0-inside;//smoothstep (0.0 , 20.0 , dist ) ;\n"
            "float glow = smoothstep (0.0 , 20.0 , dist ) ; // I don't understand this...\n"
            "vec3 insidecolor = Frag_Colour.rgb;\n"
            "vec3 outlinecolor = Frag_Colour.rgb*outlineDarkeningFactor;\n"
            "vec3 fragcolor = mix ( glow * outlinecolor , insidecolor , inside ) ;\n"
            "float alpha = smoothstep(alphaThreshold - width, alphaThreshold + width, dist);\n"
            "\n"
#       ifdef IMIMPL_SHADER_GL3
            "FragColor = vec4(fragcolor*alphaAndShadow.y,alpha*alphaAndShadow.x*Frag_Colour.a);\n"
#       else //IMIMPL_SHADER_GL3
            "gl_FragColor = vec4(fragcolor*alphaAndShadow.y,alpha*alphaAndShadow.x*Frag_Colour.a);\n"
#       endif //IMIMPL_SHADER_GL3
            "}\n"
        };
        return &gFragmentShaderSource[0];
    }

    static GLuint CreateShader(GLenum type,const GLchar** shaderSource) {
        GLuint shader( glCreateShader( type ) );
        glShaderSource( shader, 1, shaderSource, NULL );
        glCompileShader( shader );

        // check
        GLint bShaderCompiled;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &bShaderCompiled);

        if (!bShaderCompiled)        {
            int i32InfoLogLength, i32CharsWritten;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

            ImVector<char> pszInfoLog; pszInfoLog.resize(i32InfoLogLength+2);pszInfoLog[0]='\0';
            glGetShaderInfoLog(shader, i32InfoLogLength, &i32CharsWritten, &pszInfoLog[0]);
            if (type == GL_VERTEX_SHADER) printf("********%s %s\n", "GL_VERTEX_SHADER", &pszInfoLog[0]);
            else if (type == GL_FRAGMENT_SHADER) printf("********%s %s\n", "GL_FRAGMENT_SHADER", &pszInfoLog[0]);
            else printf("******** %s\n", &pszInfoLog[0]);

            fflush(stdout);
            glDeleteShader(shader);shader=0;
        }

        return shader;
    }
    static GLuint CompileShaderProgramAndSetCorrectAttributeLocations(bool isOutlineShaderProgram) {
        GLuint vs = CreateShader(GL_VERTEX_SHADER,GetVertexShaderCodeForBothFonts());
        if (!vs) return 0;
        GLuint fs = CreateShader(GL_FRAGMENT_SHADER,isOutlineShaderProgram ? GetFragmentShaderCodeForOutlineFont() : GetFragmentShaderCodeForRegularFont());
        if (!fs) return 0;

        GLuint program( glCreateProgram() );
        glAttachShader( program, vs );
        glAttachShader( program, fs );

        // Bind attribute locations:
        glBindAttribLocation(program,0,"Position");
        glBindAttribLocation(program,1,"UV");
        glBindAttribLocation(program,2,"Colour");

        // Link
        glLinkProgram( program );

        // check
        GLint bLinked;
        glGetProgramiv(program, GL_LINK_STATUS, &bLinked);
        if (!bLinked)        {
            int i32InfoLogLength, i32CharsWritten;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &i32InfoLogLength);

            ImVector<char> pszInfoLog; pszInfoLog.resize(i32InfoLogLength+2);pszInfoLog[0]='\0';
            glGetProgramInfoLog(program, i32InfoLogLength, &i32CharsWritten, &pszInfoLog[0]);
            printf("********ShaderLinkerLog:\n%s\n", &pszInfoLog[0]);
            fflush(stdout);

            glDetachShader(program,vs);glDeleteShader(vs);vs=0;
            glDetachShader(program,fs);glDeleteShader(fs);fs=0;

            glDeleteProgram(program);program=0;
        }
        else {
            glDeleteShader(vs);vs=0;
            glDeleteShader(fs);fs=0;
        }

        return program;
    }

};
SdfShaderProgram gSdfShaderPrograms[2]; // 0-Normal and 1-Outline font

#if (!defined(NO_IMGUISDF_LOAD) || (defined(IMGUIHELPER_H_) && !defined(NO_IMGUIHELPER_SERIALIZATION) && !defined(NO_IMGUIHELPER_SERIALIZATION_LOAD)))
#include <stdio.h>  // FILE* used in SdfCharset::GetFileContent(...)
#endif // (!defined(NO_IMGUISDF_LOAD) ...)

struct SdfCharset {
    float LineHeight;
    float Base;
    float Pages;
    float FontSize;
    float StretchH;
    int GlyphsCount;
    int KerningsCount;
    ImTextureID fntTexture;

    typedef SdfCharDescriptor CharDescriptor;

    struct ImHashFunctionUnsignedInt {IMGUI_FORCE_INLINE ImHashInt operator()(unsigned int key) const {return (ImHashInt) (key%255);}};

    typedef ImHashMap<unsigned int,CharDescriptor,ImHashFunctionUnsignedInt,ImHashMapKeyEqualityFunctionDefault<unsigned int>,
    ImHashMapConstructorFunctionDummy<unsigned int>, ImHashMapDestructorFunctionDummy<unsigned int>, ImHashMapAssignmentFunctionDefault<unsigned int>,
    ImHashMapConstructorFunctionDummy<CharDescriptor>,  ImHashMapDestructorFunctionDummy<CharDescriptor>,  ImHashMapAssignmentFunctionDefault<CharDescriptor>,
    256>    ImHashMapCharDescriptor;      // map <char,int>.                     We can change the int to comsume less memory if needed

    ImHashMapCharDescriptor Chars;

    typedef ImHashMapImPairFast<unsigned int,unsigned int,float> ImHashMapKerning;

    ImHashMapKerning Kernings;

    SdfCharset() {clear();}
    void clear() {
        LineHeight=Base=0.f;Pages=GlyphsCount=KerningsCount=0;
        Chars.clear();Kernings.clear();
        FontSize = 0.f;
        StretchH = 100.f;
    }

    IMGUI_API bool  loadFromMemory(const void* data, size_t data_size,ImTextureID _fntTexture,const SdfCharsetProperties& properties)    {
        const bool isTextFontFile = (data_size>=5 && strncmp((const char*) data,"info ",5)==0);// text .fnt files start with "info " AFAIK
        if (!isTextFontFile) {
            fprintf(stderr,"SdfCharset::loadFromMemory(): Error: SDF Font is not a txt based AngelBitmap .fnt file.\n");
            return false;
        }
        float ScaleW=0.f,ScaleH=0.f;
        fntTexture = _fntTexture;

        const char* pdata = (const char*) data;

        // From now on we must use "pdata" and "data_size" only

        // Here we must fill all the fields of "f" based on "pdata" and "data_size":
        char tmp[1024];tmp[0]='\0';
        int tmpi=0;size_t gsize=0,ksize=0;
        CharDescriptor g;
        const char* buf_end = pdata + data_size;
        for (const char* line_start = pdata; line_start < buf_end; ) {
            const char* line_end = line_start;
            while (line_end < buf_end && *line_end != '\n' && *line_end != '\r') line_end++;

            if (strncmp(line_start,"char ",4)==0 && GlyphsCount>0)  {
                //char id=32   x=236   y=116   width=3     height=1     xoffset=-1    yoffset=15    xadvance=4     page=0  chnl=15
                int a[7] = {0,0,0,0,0,0,0};
                float b[3] = {0,0,0};
                if (sscanf(&line_start[4], " id=%i  x=%i y=%i width=%i height=%i xoffset=%f yoffset=%f xadvance=%f page=%i chnl=%i",
                           &a[0],&a[1],&a[2],&a[3],&a[4],&b[0],&b[1],&b[2],&a[5],&a[6]))
                {
                    IM_ASSERT(ScaleW!=0 && ScaleH!=0);
                    g.Id = (unsigned int) a[0];
                    g.X = (float) a[1]/ScaleW;
                    g.Y = (float) a[2]/ScaleH;
                    g.Width = (float) a[3]/ScaleW;
                    g.Height = (float) a[4]/ScaleH;
                    IM_ASSERT(FontSize!=0);
                    g.XOffset = b[0]/FontSize;//(signed short) ROUNDCAST(b[0]);
                    g.YOffset = b[1]/FontSize;//(signed short) ROUNDCAST(b[1]);
                    g.XOffset2 = (b[0]+(float)a[3])/FontSize;//(signed short) ROUNDCAST(b[0]);
                    g.YOffset2 = (b[1]+(float)a[4])/FontSize;//(signed short) ROUNDCAST(b[1]);
                    if (properties.flipYOffset) g.YOffset = -g.YOffset;
                    g.XAdvance = b[2]/FontSize;//(signed short) ROUNDCAST(b[2]);
                    g.Page = (unsigned char) a[5];

                    Chars.put((unsigned int) a[0],g);

                    gsize++;
                }
                else fprintf(stderr,"Warning in SdfCharset::LoadTextFntFileFromMemory(\"glyph \"): skipped line [%.50s] (parsing error).\n",line_start);
            }
            else if (strncmp(line_start,"kerning ",7)==0 && KerningsCount>0)  {
                int a[3] = {0,0,0};
                if (sscanf(&line_start[7]," first=%i  second=%i amount=%i",&a[0],&a[1],&a[2]))
                {
                    IM_ASSERT(FontSize!=0);
                    Kernings.put(ImPair<unsigned int,unsigned int>(a[0],a[1]),(float)a[2]/FontSize);

                    ksize++;
                }
                else fprintf(stderr,"Warning in SdfCharset::LoadTextFntFileFromMemory(\"glyph \"): skipped line [%.50s] (parsing error).\n",line_start);

            }
            else if (strncmp(line_start,"info ",5)==0)   {
                tmp[0]='\0';
                char tmp2[1024];tmp2[0]='\0';
                int a[14] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0};
                const char* pline = NULL;

                //We must skip the font name first (we store it inside tmp)
                static const int quote = (int) '"';
                const char* q1=NULL;const char* q2=NULL;
                q1 = strchr((const char*) &line_start[5], quote );  // This method is better than sscanf, because the string can contain spaces
                if (q1) {
                    const char* q11 = ++q1;
                    q2 = strchr(q11, quote);
                    if (q2)  {
                        const size_t sz = q2-q11;
                        strncpy(tmp,q11,sz);
                        tmp[sz]='\0';
                        pline = ++q2;
                    }
                }

                if (pline && (sscanf(pline, " size=%i  bold=%i italic=%i charset=%s unicode=%i stretchH=%i smooth=%i aa=%i padding=%i,%i,%i,%i spacing=%i,%i outline=%i",
                                    &a[0],&a[1],&a[2],tmp2,&a[3],&a[4],&a[5],&a[6],&a[7],&a[8],&a[9],&a[10],&a[11],&a[12],&a[13])))
                {
                    // This is necessary because often tmp2="", with quotes included:
                    size_t tmp2Size;
                    while ((tmp2Size=strlen(tmp2))>0 && tmp2[tmp2Size-1]=='"') tmp2[tmp2Size-1]='\0';
                    while ((tmp2Size=strlen(tmp2))>0 && tmp2[0]=='"') {static char tmp22[1024];strcpy(tmp22,&tmp2[1]);strcpy(tmp2,tmp22);}

                    FontSize = (float) a[0];
                    StretchH = (float) a[4];
                    //fprintf(stderr,"FontSize=%1.0f StretchH=%1.0f charset=\"%s\"\n",FontSize,StretchH,tmp2);

                }
                else fprintf(stderr,"Warning in SdfCharset::LoadTextFntFileFromMemory(\"info \"): skipped line [%.50s] (parsing error).\n",line_start);
            }
            else if (strncmp(line_start,"common ",7)==0)  {
                int a[10] = {0,0,0,0,0,0,0,0,0,0};
                // common lineHeight=16 base=13 scaleW=256 scaleH=256 pages=1 packed=0 alphaChnl=0 redChnl=0 greenChnl=0 blueChnl=0
                if (sscanf(&line_start[7], " lineHeight=%i  base=%i scaleW=%i scaleH=%i pages=%i packed=%i alphaChnl=%i redChnl=%i greenChnl=%i blueChnl=%i",
                           &a[0],&a[1],&a[2],&a[3],&a[4],&a[5],&a[6],&a[7],&a[8],&a[9]))
                {
                    IM_ASSERT(FontSize!=0);
                    LineHeight = (float) a[0]/FontSize;
                    Base = (float) a[1]/FontSize;
                    ScaleW = (float) a[2];
                    ScaleH = (float) a[3];
                    Pages = (unsigned short) a[4];

                }
                else fprintf(stderr,"Warning in SdfCharset::LoadTextFntFileFromMemory(\"common \"): skipped line [%.50s] (parsing error).\n",line_start);

            }
            else if (strncmp(line_start,"page id=0 ",10)==0)  {
 #ifdef NEVER
                // well, we just support one page, but maybe this is what "filenames" refer to
                // we just fill the first filename we found in the line for now (I must see a file with more than one page to parse it correctly...):
                if (f.filenames.size()==0)  {
                    tmp[0]='\0';

                    static const int quote = (int) '"';
                    const char* q1=NULL;const char* q2=NULL;
                    q1 = strchr((const char*) &line_start[10], quote );
                    if (q1) {
                        const char* q11 = ++q1;
                        q2 = strchr(q11, quote);
                        if (q2)  {
                            const size_t sz = q2-q11;
                            strncpy(tmp,q11,sz);
                            tmp[sz]='\0';
                        }
                    }

                    if (strlen(tmp)>0)    {
                        size_t tmpSize;
                        while ( (tmpSize=strlen(tmp))>0 && tmp[tmpSize-1]=='\"') tmp[tmpSize-1]='\0';
                        gsize = f.filenames.size();
                        f.filenames.resize(gsize+1);
                        char* c = &f.filenames[gsize][0];
                        c[0]='\0';
                        if (strlen(tmp)<=MAX_FILENAME_SIZE) {
                            strcpy(c,tmp);
#                       ifdef DEBUG_PARSED_DATA
                            fprintf(stderr,"Filename = %s\n",c);
#                       endif //DEBUG_PARSED_DATA
                        }
                    }
                    else fprintf(stderr,"Warning in SdfCharset::LoadTextFntFileFromMemory(\"page \"): skipped line [%.50s] (parsing error).\n",line_start);
                }
#endif //NEVER
            }
            else if (strncmp(line_start,"chars count=",12)==0)  {
                if (sscanf(line_start, "chars count=%i", &tmpi))    {
                    GlyphsCount = tmpi;
                    //f.glyphs.reserve(GlyphsCount);
                }
                else fprintf(stderr,"Warning in SdfCharset::LoadTextFntFileFromMemory(): skipped line [%.50s] (parsing error).\n",line_start);
            }
            else if (strncmp(line_start,"kernings count=",15)==0)  {
                if (sscanf(line_start, "kernings count=%i", &tmpi))    {
                    KerningsCount = tmpi;
                }
                else fprintf(stderr,"Warning in SdfCharset::LoadTextFntFileFromMemory(): skipped line [%.50s] (parsing error).\n",line_start);
            }
            line_start = line_end+1;
        }

        // Add extra codepoints:
        CharDescriptor sp;
        if (Chars.get((unsigned int)' ',sp)) {
            CharDescriptor tab;
            if (!Chars.get((unsigned int)'\t',tab)) {
                tab = sp;tab.Id='\t';tab.XAdvance=sp.XAdvance*4.f;
                Chars.put((unsigned int)'\t',tab);
            }
            sp.Width=0.f;Chars.put((unsigned int)' ',sp);
        }
        CharDescriptor lf;
        if (!Chars.get((unsigned int)'\n',lf)) {
            lf = sp;lf.Id='\n';lf.XAdvance=lf.Width=lf.XOffset=0.f;
        }
        if (!Chars.get((unsigned int)'\r',lf)) {
            lf = sp;lf.Id='\r';lf.XAdvance=lf.Width=lf.XOffset=0.f;
        }
        return true;

    }

#   if (!defined(NO_IMGUISDF_LOAD) || (defined(IMGUIHELPER_H_) && !defined(NO_IMGUIHELPER_SERIALIZATION) && !defined(NO_IMGUIHELPER_SERIALIZATION_LOAD)))
    // Cloned from imguihelper.h/cpp remove its dependency:
    static bool GetFileContent(const char *filePath,ImVector<char> &contentOut,bool clearContentOutBeforeUsage,const char *modes="rb",bool appendTrailingZeroIfModesIsNotBinary=true)   {
        ImVector<char>& f_data = contentOut;
        if (clearContentOutBeforeUsage) f_data.clear();
        //----------------------------------------------------
        if (!filePath) return false;
        const bool appendTrailingZero = appendTrailingZeroIfModesIsNotBinary && modes && strlen(modes)>0 && modes[strlen(modes)-1]!='b';
        FILE* f;
        if ((f = ImFileOpen(filePath, modes)) == NULL) return false;
        if (fseek(f, 0, SEEK_END))  {
            fclose(f);
            return false;
        }
        const long f_size_signed = ftell(f);
        if (f_size_signed == -1)    {
            fclose(f);
            return false;
        }
        size_t f_size = (size_t)f_size_signed;
        if (fseek(f, 0, SEEK_SET))  {
            fclose(f);
            return false;
        }
        f_data.resize(f_size+(appendTrailingZero?1:0));
        const size_t f_size_read = fread(&f_data[0], 1, f_size, f);
        fclose(f);
        if (f_size_read == 0 || f_size_read!=f_size)    return false;
        if (appendTrailingZero) f_data[f_size] = '\0';
        //----------------------------------------------------
        return true;
    }
#   endif //(!defined(NO_IMGUISDF_LOAD) ...)
};

void SdfTextChunk::addText(const char* startText, bool _italic, const SdfTextColor* pSdfTextColor, const ImVec2* textScaling, const char* endText,const SDFHAlignment *phalignOverride, bool fakeBold) {
    IM_ASSERT(startText);
    IM_ASSERT(charset);

    const int textBitsSize = textBits.size();
    textBits.push_back(TextBit());
    TextBit& TB = textBits[textBitsSize];

    TB.scaling = textScaling ? *textScaling : ImVec2(1,1);if (fakeBold) {TB.scaling.x*=1.2f;/*TB.scaling.y*=1.1f;*/}
    TB.italic = _italic;
    TB.sdfTextColor = pSdfTextColor ? *pSdfTextColor : gSdfTextDefaultColor;// change if fakeBold ?
    TB.hAlignment = phalignOverride ? (int)(*phalignOverride) : -1;

    SdfCharDescriptor cd; int lastCdId=0;
    const bool useKernings = charset->KerningsCount>0;float kerning = 0;
    uint32_t codePoint = 0, state = 0;
    if (!endText) endText = startText+strlen(startText);
    TB.charDescriptors.reserve(endText-startText);
    if (useKernings) TB.kernings.reserve(endText-startText);
    const unsigned char* endTextUC = (const unsigned char*) endText;
    for(const unsigned char* p = (const unsigned char*) startText; p!=endTextUC; ++p)
    {
        if (UTF8Helper::decode(&state, &codePoint, *p)!=UTF8Helper::UTF8_ACCEPT) continue;
        // Here we could count valid chars if we need it...
        ++numValidGlyphs;

        if (!charset->Chars.get(codePoint,cd) && !charset->Chars.get('?',cd)) continue;
        TB.charDescriptors.push_back(cd);

        if (useKernings) {
            if (lastCdId!=0 && charset->Kernings.get(ImPair<unsigned int,unsigned int>(lastCdId,cd.Id),kerning)) {
                TB.kernings.push_back(kerning);
                //fprintf(stderr,"%d) Kerning: %c->%c = %1.4f\n",TB.kernings.size()-1,(char)lastCdId,(char)cd.Id,kerning);
            }
            else TB.kernings.push_back(.0f);            
        }
        lastCdId = cd.Id;
    }

    dirty = true;
}
bool SdfTextChunk::endText(ImVec2 screenSize) {
    if (!dirty)  return false;
    dirty = false;
    IM_ASSERT(buffer);
    buffer->clear();
    if (textBits.size()==0) return true;

    if (screenSize.x<=0 || screenSize.y<=0) screenSize = ImGui::GetIO().DisplaySize;
    const ImVec2 cursorXlimits((props.boundsCenter.x-props.boundsHalfSize.x)*screenSize.x,(props.boundsCenter.x+props.boundsHalfSize.x)*screenSize.x);
    const ImVec2 cursorYlimits((props.boundsCenter.y-props.boundsHalfSize.y)*screenSize.y,(props.boundsCenter.y+props.boundsHalfSize.y)*screenSize.y);
    //const float cursorXextent = cursorXlimits.y - cursorXlimits.x;
    //const float cursorYextent = cursorYlimits.y - cursorYlimits.x;
    //fprintf(stderr,"cursorXlimits(%1.0f,%1.0f) cursorYlimits(%1.0f,%1.0f) displaySize(%1.0f,%1.0f)\n",cursorXlimits.x,cursorXlimits.y,cursorYlimits.x,cursorYlimits.y,ImGui::GetIO().DisplaySize.x,ImGui::GetIO().DisplaySize.y);
    IM_ASSERT(props.maxNumTextLines>0);
    const float virtualLineHeight = props.lineHeightOverride>0.f?props.lineHeightOverride:charset->LineHeight;
    const float numPixelsPerTextLine = screenSize.y/*cursorYextent*//(float)props.maxNumTextLines;
    const float globalScale = numPixelsPerTextLine/virtualLineHeight;   // globalScale translates quantities into pixels (it's the FontSize (or the FontHeight) in pixels)
    const float BaseBase = globalScale*charset->Base;
    const float LineHeightBase = globalScale*virtualLineHeight;         // It's the Line Height in pixels
    ImVec2 cursor = ImVec2(cursorXlimits.x,cursorYlimits.x);


    ImVector<SdfVertexBuffer::VertexDeclaration>& vertexBuffer =  buffer->verts;
    int curVertexBufferSize = vertexBuffer.size();
    const int startVertexBufferSize = curVertexBufferSize;
    vertexBuffer.reserve(curVertexBufferSize+numValidGlyphs*6);

    const bool useKernings = charset->KerningsCount>0;bool skipKerning = false;
    SdfVertexBuffer::VertexDeclaration *pVert = NULL;
    float XAdvance=0.f,XOffset0=0.f,YOffset0=0.f,XOffset1=0.f,YOffset1=0.f;
    ImVec2 totalSizeInPixels(0.f,0.f);
    bool mustEndOneLine = false;

    ImVec2 localScaleXminmax(0.f,0.f),localScaleYminmax(0.f,0.f);


    const SdfCharDescriptor* lastCd = NULL;
    typedef struct _LineData{
        ImVec2 sizeInPixels;
        float Base;
        float LineHeight;
        int numGlyphs;
        _LineData(float BaseBase,float LineHeightBase) : sizeInPixels(0.f,0.f),Base(BaseBase),LineHeight(LineHeightBase),numGlyphs(0) {lastTbScalingY=maxTbScalingY=1.f;pVertStartLine=pVertLastLineSplitter=NULL;cursorXAtLastLineSplitter=0.f;lastLineSplitterIsSpace=false;}
        mutable float lastTbScalingY;
        mutable float maxTbScalingY;
        mutable SdfVertexBuffer::VertexDeclaration *pVertStartLine;
        mutable SdfVertexBuffer::VertexDeclaration *pVertLastLineSplitter;
        mutable float cursorXAtLastLineSplitter;
        mutable bool lastLineSplitterIsSpace;
    } LineData;

    LineData lineData(BaseBase,LineHeightBase);
    SDFHAlignment lastTBalignment = props.halign,halign = props.halign;bool mustStartNewAlignment = false;
    for (int i=0,isz=textBits.size();i<isz;i++) {
	const TextBit& TB = textBits[i];
        halign =  (TB.hAlignment==-1) ? props.halign : (SDFHAlignment)TB.hAlignment;
        mustStartNewAlignment = (lastTBalignment!=halign);
        if (mustStartNewAlignment)  {
            //------------------------------------------
            if (totalSizeInPixels.x<lineData.sizeInPixels.x) totalSizeInPixels.x = lineData.sizeInPixels.x;
            //totalSizeInPixels.y+= lineData.LineHeight;
            // h alignment on lineData
            // Horizontal alignment here---------------------------------
            if (lineData.pVertStartLine && pVert) {
                if (lastTBalignment==SDF_JUSTIFY)  {
                    // horizontal alignment here---------------------------------
                    if (lineData.numGlyphs>1 &&  (/*!lastCd || lastCd->Id!='\n' ||*/ lineData.sizeInPixels.x>0.65f*(cursorXlimits.y - cursorXlimits.x)))  {
                        const float deltaX = (cursorXlimits.y - cursorXlimits.x - lineData.sizeInPixels.x)/(float)(lineData.numGlyphs-1);
                        int cnt = 0;float addend = deltaX;
                        for (SdfVertexBuffer::VertexDeclaration* vd = (lineData.pVertStartLine+6);vd!=pVert;++vd)   {
                            vd->posAndUV.x+=addend;
                            if (++cnt==6) {cnt=0;addend+=deltaX;}
                        }
                    }
                }
                else {
                    float offsetX=0.f;
                    if (lastTBalignment!=SDF_LEFT)      {
                        offsetX = cursorXlimits.y - cursorXlimits.x - lineData.sizeInPixels.x;
                        if (lastTBalignment==SDF_CENTER) offsetX*=0.5f;
                    }
                    if (offsetX!=0.f) {
                        for (SdfVertexBuffer::VertexDeclaration* vd = lineData.pVertStartLine;vd!=pVert;++vd)   {
                            vd->posAndUV.x+=offsetX;
                        }
                    }
                }
            }
            //------------------------------------------------------------
            //lineData = LineData(BaseBase,LineHeightBase);
            lineData.pVertStartLine=lineData.pVertLastLineSplitter=NULL;
            lineData.numGlyphs = 0;
            lineData.sizeInPixels=ImVec2(0,0);
            cursor.x = cursorXlimits.x;
            //------------------------------------------------------------
            lastTBalignment=halign;
        }
        const ImVec2 localScale = TB.scaling * globalScale //;
                * ImVec2(TB.italic ? 0.925f : 1.f,1.f);  // This line makes italic a bit slimmer than normal text
        if (i==0) {
            localScaleXminmax.x=localScaleXminmax.y=localScale.x;
            localScaleYminmax.x=localScaleYminmax.y=localScale.y;
        }
        else {
            if      (localScaleXminmax.x>localScale.x) localScaleXminmax.x = localScale.x;
            else if (localScaleXminmax.y<localScale.x) localScaleXminmax.y = localScale.x;
            if      (localScaleYminmax.x>localScale.y) localScaleYminmax.x = localScale.y;
            else if (localScaleYminmax.y<localScale.y) localScaleYminmax.y = localScale.y;
        }
        //if (shadowOffsetInPixels.x<localScale.x) shadowOffsetInPixels.x = localScale.x;
        //if (shadowOffsetInPixels.y<localScale.y) shadowOffsetInPixels.y = localScale.y;
        if (lineData.lastTbScalingY!=TB.scaling.y)   {
            lineData.lastTbScalingY=TB.scaling.y;
            if (lineData.maxTbScalingY<TB.scaling.y)    {
                if (TB.scaling.y > virtualLineHeight) {
                    //const float oldBaseBase = lineData.Base;
                    //const float oldLineHeight = lineData.LineHeight;
                    lineData.Base = BaseBase*TB.scaling.y;
                    lineData.LineHeight = LineHeightBase*TB.scaling.y;
                    // scale all glyphs from lineData.pVertStartLine to pVert
                    if (lineData.pVertStartLine && pVert)    {
                        const float deltaY = (TB.scaling.y - lineData.maxTbScalingY) * globalScale;
                        for (SdfVertexBuffer::VertexDeclaration* vd = lineData.pVertStartLine;vd!=pVert;++vd)   {
                            vd->posAndUV.y+=deltaY;
                        }
                    }

                }
                lineData.maxTbScalingY=TB.scaling.y;
            }
        }
        const float& Base = lineData.Base;
        const float& LineHeight = lineData.LineHeight;
        const float italicAddend = TB.italic ? localScale.y * 0.125f : 0.f;

        if (useKernings) {IM_ASSERT(TB.kernings.size()==TB.charDescriptors.size());}
        for (int cdIndex=0,cdIndexSize = TB.charDescriptors.size();cdIndex<cdIndexSize;cdIndex++)   {
            const SdfCharDescriptor& cd =  TB.charDescriptors[cdIndex];
            const float kerning = (useKernings && !skipKerning) ? TB.kernings[cdIndex] : 0.f;
            skipKerning = false;
            //--------------------------------------------------------------------------------------------------------
            XOffset0=cd.XOffset*localScale.x;
            YOffset0=cd.YOffset*localScale.y;
            XOffset1=cd.XOffset2*localScale.x;
            YOffset1=cd.YOffset2*localScale.y;

            if (useKernings) cursor.x +=  kerning*localScale.x;
            XAdvance=cd.XAdvance*localScale.x;
            //--------------------------------------------------------------------------------------------------------
            mustEndOneLine = false;
            bool mustSplitLine = false;
            if (cd.Id=='\n') mustEndOneLine = true;
            else if (cd.Id=='\r') {
                lastCd = &cd;
                continue;
            }
            if (cursor.x+XAdvance>cursorXlimits.y) {
                // Word wrap: but we could just skip using "continue" here
                mustSplitLine = lineData.pVertLastLineSplitter!=NULL;
                mustEndOneLine = true;
            }
            if (mustEndOneLine) {
                const float oldCursorX = cursor.x;
                cursor.x=cursorXlimits.x;
                cursor.y+=LineHeight;
                skipKerning = true;
                SdfVertexBuffer::VertexDeclaration* pLastVertOfLine = pVert;
                LineData nextLineData(BaseBase,LineHeightBase); // reset it
                if (TB.scaling.y>virtualLineHeight) {
                    nextLineData.Base = BaseBase*TB.scaling.y;
                    nextLineData.LineHeight = LineHeightBase*TB.scaling.y;
                    nextLineData.lastTbScalingY = nextLineData.maxTbScalingY = TB.scaling.y;

                    /*// scale all glyphs from lineData.pVertStartLine to pVert
                    if (lineData.pVertStartLine && pVert)    {
                        const float deltaY = (TB.scaling.y - lineData.maxTbScalingY) * globalScale;
                        for (SdfVertexBuffer::VertexDeclaration* vd = lineData.pVertStartLine;vd!=pVert;++vd)   {
                            vd->posAndUV.y+=deltaY;
                        }
                    }*/
                }

                if (mustSplitLine) {
                    nextLineData.numGlyphs = 0;
                    // lineData
                    for (SdfVertexBuffer::VertexDeclaration* vd = lineData.pVertLastLineSplitter;vd!=pVert;vd=vd+6) {
                        --lineData.numGlyphs;++nextLineData.numGlyphs;
                    }
                    pLastVertOfLine = lineData.pVertLastLineSplitter;
                    cursor.x = cursorXlimits.x + oldCursorX - lineData.cursorXAtLastLineSplitter;
                    lineData.sizeInPixels.x = lineData.cursorXAtLastLineSplitter - cursorXlimits.x;
                    // nextLineData
                    float deltaX = lineData.cursorXAtLastLineSplitter-cursorXlimits.x;
                    const float deltaY = LineHeight;
                    float deltaSizeX = 0.f;
                    ImVec2 yMinMax(lineData.pVertLastLineSplitter->posAndUV.y,lineData.pVertLastLineSplitter->posAndUV.y);
                    // How can I crop leading spaces here ?
                    nextLineData.pVertStartLine = pLastVertOfLine;
                    unsigned int cnt = 0;bool enableCutSpaces = true;
                    for (SdfVertexBuffer::VertexDeclaration* vd = lineData.pVertLastLineSplitter;vd!=pVert;++vd)   {
                        if (enableCutSpaces && cnt==0 /*&& pVert!=vd+6*/) {
                            SdfVertexBuffer::VertexDeclaration* vd2 = vd+1;
                            SdfVertexBuffer::VertexDeclaration* vd6 = vd+6;
                            //fprintf(stderr,"%1.6f -> %1.6f\n",vd2->posAndUV.z-vd->posAndUV.z,vd6->posAndUV.x-vd->posAndUV.x);
                            if (vd2->posAndUV.z==vd->posAndUV.z) {
                                if (pVert!=vd+6) {
                                    nextLineData.pVertStartLine+=6;--nextLineData.numGlyphs;
                                    float cursorXdelta = (vd6->posAndUV.x-vd->posAndUV.x);
                                    deltaSizeX+=cursorXdelta;
                                    deltaX+=cursorXdelta;
                                    cursor.x-=cursorXdelta;
                                }
                                else {
                                    nextLineData.pVertStartLine=NULL;nextLineData.numGlyphs=0;
                                    cursor.x=cursorXlimits.x;

                                    //fprintf(stderr,"Border case to handle\n");
                                    //vertexBuffer.resize(vertexBuffer.size()-6);
                                    //curVertexBufferSize = vertexBuffer.size();
                                    break;
                                }
                                // [Optional] Remove vertex from vertexarray:
                                /*for (SdfVertexBuffer::VertexDeclaration* p = vd;p!=pVert-6;++p) *p = *(p+6);
                                pVert = pVert-6;
                                vertexBuffer.resize(vertexBuffer.size()-6);
                                curVertexBufferSize = vertexBuffer.size();
                                vd=vd-1;cnt=0;continue;*/
                            }
                            else enableCutSpaces = false;
                        }
                        vd->posAndUV.x-=deltaX;
                        vd->posAndUV.y+=deltaY;
                        if (yMinMax.x>vd->posAndUV.y) yMinMax.x=vd->posAndUV.y;
                        else if (yMinMax.y<vd->posAndUV.y) yMinMax.y=vd->posAndUV.y;
                        if (++cnt==6) cnt=0;
                    }
                    /*
                    // This does not work and furthermore gives a Valgrind error
                    const float maxY = (yMinMax.y-yMinMax.x)/globalScale;
                    if (maxY > virtualLineHeight) {
                        nextLineData.maxTbScalingY = maxY;
                        nextLineData.lastTbScalingY = nextLineData.maxTbScalingY;
                        //nextLineData.Base = BaseBase*maxY;
                        //nextLineData.LineHeight = LineHeightBase*maxY;
                    }*/
                    nextLineData.sizeInPixels.x = cursor.x -cursorXlimits.x;

                }

                if (totalSizeInPixels.x<lineData.sizeInPixels.x) totalSizeInPixels.x = lineData.sizeInPixels.x;
                totalSizeInPixels.y+= lineData.LineHeight;
                // Horizontal alignment here---------------------------------
                if (lineData.pVertStartLine && pLastVertOfLine) {
                    if (halign==SDF_JUSTIFY)  {
                        // horizontal alignment here---------------------------------
                        if (lineData.numGlyphs>1 && (cd.Id!='\n' || lineData.sizeInPixels.x>0.65f*(cursorXlimits.y - cursorXlimits.x)))  {
                            const float deltaX = (cursorXlimits.y - cursorXlimits.x - lineData.sizeInPixels.x)/(float)(lineData.numGlyphs-1);
                            int cnt = 0;float addend = deltaX;
                            for (SdfVertexBuffer::VertexDeclaration* vd = (lineData.pVertStartLine+6);vd!=pLastVertOfLine;++vd)   {
                                vd->posAndUV.x+=addend;
                                if (++cnt==6) {cnt=0;addend+=deltaX;}
                            }
                        }
                    }
                    else {
                        float offsetX=0.f;
                        if (halign!=SDF_LEFT)      {
                            offsetX = cursorXlimits.y - cursorXlimits.x - lineData.sizeInPixels.x;
                            if (halign==SDF_CENTER) offsetX*=0.5f;
                        }
                        if (offsetX!=0.f) {
                            for (SdfVertexBuffer::VertexDeclaration* vd = lineData.pVertStartLine;vd!=pLastVertOfLine;++vd)   {
                                vd->posAndUV.x+=offsetX;
                            }
                        }
                    }
                }
                //------------------------------------------------------------
                lineData = nextLineData;
                if (cursor.y>=cursorYlimits.y) {
                    lastCd = &cd;
                    break;
                }
                if (cd.Id=='\n') {
                    lastCd = &cd;
                    continue;  //consume one char
                }
            }
            if (cursor.y+YOffset0<cursorYlimits.x) {
                cursor.x += XAdvance;
                lastCd = &cd;
                continue;
            }

            // Eat starting space if necessary
            if ((!lineData.pVertStartLine || cursor.x==cursorXlimits.x) && cd.Id==' ')   {
                lineData.pVertStartLine = NULL;
                //if (lineData.pVertStartLine) lineData.pVertStartLine = lineData.pVertStartLine+6;
                //lastCd = &cd;
                continue;
            }
            if (pVert && ((cd.Id==' ' && (!lineData.lastLineSplitterIsSpace || !lastCd || lastCd->Id!=' ')) || cd.Id=='\t')) {
                if (lineData.pVertLastLineSplitter!=pVert-1) {
                    lineData.pVertLastLineSplitter = pVert;
                    lineData.cursorXAtLastLineSplitter = cursor.x;
                    lineData.lastLineSplitterIsSpace = cd.Id==' ';
                }
            }

            ++lineData.numGlyphs;

/*
  --------------------------------------------          ^
  ^             ^                                       |
  |     yoffset |                                       |
  |             v                                       |
  |             -----------------                       |
  |             |               ^
  base          |               |                   lineHeight
  |             |               |
  |             |               | height                |
  | xoffset     |               |                       |
  v<----------->|  - - - - - -  |                       |
                |               v                       |
                <--------------->                       |
                      width                             |
  ---------------------------------------------         v

  <----------------------------------> xadvance
*/
                //if (cd.YOffset<0) cd.YOffset = 0;//-cd.YOffset; // This seems to improve alignment slightly in one of my fonts...
                // TODO: Allow scaling from extern and update spacing and LineHeight

                curVertexBufferSize = vertexBuffer.size();
                vertexBuffer.resize(curVertexBufferSize+6); // We use GL_TRIANGLES, as GL_QUADS is not supported on some OpenGL implementations
                pVert = &vertexBuffer[curVertexBufferSize];
                if (!lineData.pVertStartLine) lineData.pVertStartLine = pVert;            


                //upper left
                pVert->posAndUV.z = cd.X;
                pVert->posAndUV.w = cd.Y;
                pVert->posAndUV.x = cursor.x + XOffset0 + italicAddend;
                pVert->posAndUV.y = cursor.y + YOffset0;
                pVert->color = TB.sdfTextColor.colorTopLeft;
                //fprintf(stderr,"%1.2f,%1.2f,%1.2f,%1.2f\n",pVert->posAndUV.x,pVert->posAndUV.y,pVert->posAndUV.z,pVert->posAndUV.w);
                ++pVert;


                //upper right
                pVert->posAndUV.z = cd.X+cd.Width;
                pVert->posAndUV.w = cd.Y;
                pVert->posAndUV.x = cursor.x + XOffset1 + italicAddend;
                pVert->posAndUV.y = cursor.y + YOffset0;
                pVert->color = TB.sdfTextColor.colorTopRight;
                //fprintf(stderr,"%1.2f,%1.2f,%1.2f,%1.2f\n",pVert->posAndUV.x,pVert->posAndUV.y,pVert->posAndUV.z,pVert->posAndUV.w);
                ++pVert;

                //lower right
                pVert->posAndUV.z = cd.X+cd.Width;
                pVert->posAndUV.w = cd.Y+cd.Height;
                pVert->posAndUV.x = cursor.x + XOffset1;
                pVert->posAndUV.y = cursor.y + YOffset1;
                pVert->color = TB.sdfTextColor.colorBottomRight;
                //fprintf(stderr,"%1.2f,%1.2f,%1.2f,%1.2f\n",pVert->posAndUV.x,pVert->posAndUV.y,pVert->posAndUV.z,pVert->posAndUV.w);
                ++pVert;

                *pVert = *(pVert-1);++pVert;

                //lower left
                pVert->posAndUV.z = cd.X ;
                pVert->posAndUV.w = cd.Y+cd.Height;
                pVert->posAndUV.x = cursor.x + XOffset0;
                pVert->posAndUV.y = cursor.y + YOffset1;
                pVert->color = TB.sdfTextColor.colorBottomLeft;
                //fprintf(stderr,"%1.2f,%1.2f,%1.2f,%1.2f\n",pVert->posAndUV.x,pVert->posAndUV.y,pVert->posAndUV.z,pVert->posAndUV.w);
                ++pVert;

                *pVert = *(pVert-5);++pVert;

                if (lineData.sizeInPixels.x < cursor.x + XOffset1 - cursorXlimits.x) lineData.sizeInPixels.x = cursor.x + XOffset1 - cursorXlimits.x;
                if (lineData.sizeInPixels.y < cursor.y + YOffset1 - cursorXlimits.y) lineData.sizeInPixels.y = cursor.y + YOffset1 - cursorXlimits.y;


                cursor.x +=  XAdvance;

                if (cd.Id==',' || cd.Id=='.' || cd.Id==';' || cd.Id==':' ||
                cd.Id=='?' || cd.Id=='!' || cd.Id=='-') {
                    if (lineData.pVertLastLineSplitter!=pVert-1) {
                        lineData.pVertLastLineSplitter = pVert;
                        lineData.cursorXAtLastLineSplitter = cursor.x;
                        lineData.lastLineSplitterIsSpace = false;
                    }
                }
            lastCd = &cd;
            }

            //--------------------------------------------------------------------------------------------------------
        }

    if (!mustEndOneLine) {
        if (totalSizeInPixels.x<lineData.sizeInPixels.x) totalSizeInPixels.x = lineData.sizeInPixels.x;
        totalSizeInPixels.y+= lineData.LineHeight;
        // h alignment on lineData
        // Horizontal alignment here---------------------------------
        if (lineData.pVertStartLine && pVert) {
            if (halign==SDF_JUSTIFY)  {
                // horizontal alignment here---------------------------------
                if (lineData.numGlyphs>1 &&  (/*!lastCd || lastCd->Id!='\n' ||*/ lineData.sizeInPixels.x>0.65f*(cursorXlimits.y - cursorXlimits.x)))  {
                    const float deltaX = (cursorXlimits.y - cursorXlimits.x - lineData.sizeInPixels.x)/(float)(lineData.numGlyphs-1);
                    int cnt = 0;float addend = deltaX;
                    for (SdfVertexBuffer::VertexDeclaration* vd = (lineData.pVertStartLine+6);vd!=pVert;++vd)   {
                        vd->posAndUV.x+=addend;
                        if (++cnt==6) {cnt=0;addend+=deltaX;}
                    }
                }
            }
            else {
                float offsetX=0.f;
                if (halign!=SDF_LEFT)      {
                    offsetX = cursorXlimits.y - cursorXlimits.x - lineData.sizeInPixels.x;
                    if (halign==SDF_CENTER) offsetX*=0.5f;
                }
                if (offsetX!=0.f) {
                    for (SdfVertexBuffer::VertexDeclaration* vd = lineData.pVertStartLine;vd!=pVert;++vd)   {
                        vd->posAndUV.x+=offsetX;
                    }
                }
            }
        }
        //------------------------------------------------------------
    }

    float offsetY=0.f;
    if (props.valign!=SDF_TOP)      {
        offsetY = cursorYlimits.y - cursorYlimits.x - totalSizeInPixels.y;
        if (props.valign==SDF_MIDDLE) offsetY*=0.5f;
    }
    if (offsetY!=0.f) {
        for (int i = startVertexBufferSize,isz=vertexBuffer.size();i<isz;i++)   {
            SdfVertexBuffer::VertexDeclaration& V = vertexBuffer[i];
            V.posAndUV.y+=offsetY;
        }
    }

    /*const float soc = 0.0575f;
    shadowOffsetInPixels.x*=soc;
    shadowOffsetInPixels.y*=soc;*/

    const float soc = 0.0625f;
    shadowOffsetInPixels.x=soc*(0.75f*localScaleXminmax.x + 0.25f*localScaleXminmax.y);
    shadowOffsetInPixels.y=soc*(0.75f*localScaleYminmax.x + 0.25f*localScaleYminmax.y);
    //fprintf(stderr,"localScaleXminmax:(%1.3f,%1.3f) localScaleYminmax:(%1.3f,%1.3f)\n",localScaleXminmax.x,localScaleXminmax.y,localScaleYminmax.x,localScaleYminmax.y);

    return true;

    //buffer->updateBoundVbo();
}



struct SdfStaticStructs {
    ImVector<SdfTextChunk*> gSdfTextChunks;
    ImVectorEx<SdfCharset*> gSdfCharsets;
    ImVectorEx<SdfAnimation*> gSdfAnimations;

    SdfStaticStructs() {}
    ~SdfStaticStructs() {
        DestroyAllAnimations();
        DestroyAllTextChunks();
        DestroyAllCharsets();
    }
    void DestroyAllAnimations();
    void DestroyAllTextChunks();
    void DestroyAllCharsets();
};
static SdfStaticStructs gSdfInit;
void SdfStaticStructs::DestroyAllAnimations()  {
    for (int i=0,isz=gSdfInit.gSdfAnimations.size();i<isz;i++) {
        gSdfInit.gSdfAnimations[i]->~SdfAnimation();
        ImGui::MemFree(gSdfInit.gSdfAnimations[i]);
        gSdfInit.gSdfAnimations[i] = NULL;
    }
    gSdfInit.gSdfAnimations.clear();

    for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++)  {
        SdfTextChunk* chunk = gSdfInit.gSdfTextChunks[i];
        if (chunk->manualAnimationRef)  {
            if (chunk->animationMode==SDF_AM_MANUAL) {chunk->mute=true;chunk->animationStartTime=-1.f;}
            chunk->manualAnimationRef=NULL;
        }
    }
}
void SdfStaticStructs::DestroyAllTextChunks()  {
    for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++) {
        gSdfInit.gSdfTextChunks[i]->~SdfTextChunk();
        ImGui::MemFree(gSdfInit.gSdfTextChunks[i]);
        gSdfInit.gSdfTextChunks[i] = NULL;
    }
    gSdfInit.gSdfTextChunks.clear();
}
void SdfStaticStructs::DestroyAllCharsets()  {
    for (int i=0,isz=gSdfInit.gSdfCharsets.size();i<isz;i++) {
        gSdfInit.gSdfCharsets[i]->~SdfCharset();
        ImGui::MemFree(gSdfInit.gSdfCharsets[i]);
        gSdfInit.gSdfCharsets[i] = NULL;
    }
    gSdfInit.gSdfCharsets.clear();
}


#if (!defined(NO_IMGUISDF_LOAD) || (defined(IMGUIHELPER_H_) && !defined(NO_IMGUIHELPER_SERIALIZATION) && !defined(NO_IMGUIHELPER_SERIALIZATION_LOAD)))
SdfCharset* SdfAddCharsetFromFile(const char* fntFilePath,ImTextureID fntTexture,const SdfCharsetProperties& properties)    {
    ImVector<char> contentOut;
    if (!SdfCharset::GetFileContent(fntFilePath,contentOut,true,"r") || contentOut.size()==0) return NULL;
    return SdfAddCharsetFromMemory(&contentOut[0],contentOut.size(),fntTexture,properties);
}
#endif // (!defined(NO_IMGUISDF_LOAD) ...)
SdfCharset* SdfAddCharsetFromMemory(const void* data,unsigned int data_size,ImTextureID fntTexture,const SdfCharsetProperties& properties)  {
    SdfCharset* p = (SdfCharset*) ImGui::MemAlloc(sizeof(SdfCharset));
    IM_PLACEMENT_NEW (p) SdfCharset();

    if (!p->loadFromMemory(data,data_size,fntTexture,properties)) {
        p->~SdfCharset();
        ImGui::MemFree(p);
        return NULL;
    }
    gSdfInit.gSdfCharsets.push_back(p);
    return p;
}



SdfTextChunk* SdfAddTextChunk(SdfCharset* charset, int sdfBufferType, const SdfTextChunkProperties& properties, bool preferStreamDrawBufferUsage) {
    SdfTextChunk* p = (SdfTextChunk*) ImGui::MemAlloc(sizeof(SdfTextChunk));
    IM_PLACEMENT_NEW (p) SdfTextChunk(charset,sdfBufferType,properties,preferStreamDrawBufferUsage);
    gSdfInit.gSdfTextChunks.push_back(p);
    return p;
}
SdfTextChunkProperties& SdfTextChunkGetProperties(SdfTextChunk* textChunk)   {
    textChunk->dirty = true;
    return textChunk->props;
}
const SdfTextChunkProperties& SdfTextChunkGetProperties(const SdfTextChunk* textChunk) {
    return textChunk->props;
}
void SdfTextChunkSetStyle(SdfTextChunk* textChunk,int sdfTextBufferType) {
    textChunk->buffer->setType(sdfTextBufferType);
}
int SdfTextChunkGetStyle(const SdfTextChunk* textChunk) {
    return textChunk->buffer->getType();
}
void SdfTextChunkSetMute(SdfTextChunk* textChunk,bool flag) {
    textChunk->setMute(flag);
}
bool SdfTextChunkGetMute(const SdfTextChunk* textChunk)  {
    IM_ASSERT(textChunk);
    return textChunk->mute;
}
void SdfRemoveTextChunk(SdfTextChunk* chunk)    {
    for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++)  {
        if (chunk == gSdfInit.gSdfTextChunks[i])    {
            gSdfInit.gSdfTextChunks[i] = gSdfInit.gSdfTextChunks[isz-1];
            gSdfInit.gSdfTextChunks.pop_back();
            // destroy chunk
            chunk->~SdfTextChunk();
            ImGui::MemFree(chunk);
            chunk=NULL;
            break;
        }
    }
}
void SdfRemoveAllTextChunks()   {
    for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++)  {
        SdfTextChunk* chunk = gSdfInit.gSdfTextChunks[i];
        // destroy chunk
        chunk->~SdfTextChunk();
        ImGui::MemFree(chunk);
    }
    gSdfInit.gSdfTextChunks.clear();
}

void SdfClearText(SdfTextChunk* chunk)    {
    IM_ASSERT(chunk);
    chunk->clearText();
}
void SdfAddText(SdfTextChunk* chunk, const char* startText, bool italic, const SdfTextColor* pSdfTextColor, const ImVec2* textScaling, const char* endText,const SDFHAlignment *phalignOverride, bool fakeBold)    {
    IM_ASSERT(chunk);
    chunk->addText(startText,italic,pSdfTextColor,textScaling,endText,phalignOverride,fakeBold);
}


void SdfAddTextWithTags(SdfTextChunk* chunk,const char* startText,const char* endText)  {
    IM_ASSERT(chunk);
    if (!startText || startText==endText || startText[0]=='\0') return;
    typedef struct _SdfTagState {
        bool bold;
        bool italic;
        ImVector<SdfTextColor> color;
        ImVector<ImVec2> scaling;
        ImVector<SDFHAlignment> hAlign;
        _SdfTagState() : bold(false),italic(false) {}
        void SdfAddText(SdfTextChunk* chunk,const unsigned char* startText,const unsigned char* endText) const {
            chunk->addText((const char*)startText,
                           italic,
                           color.size()>0?&color[color.size()-1]:NULL,
                           scaling.size()>0?&scaling[scaling.size()-1]:NULL,
                           (const char*)endText,
                           hAlign.size()>0?&hAlign[hAlign.size()-1]:NULL,
                           bold
                           );
        }
    } SdfTagState;
    SdfTagState TS;

    uint32_t state=UTF8Helper::UTF8_ACCEPT,codePoint=0,lastCodePoint=0,numGlyphs=0;
    const uint32_t startTagCP = '<';    // Hp) startTagCP and endTagCP must be 1 bytes long (in UTF8)
    const uint32_t endTagCP = '>';
    const unsigned char* endTextUC = (const unsigned char*) (endText==NULL?(startText+strlen(startText)):endText);
    const unsigned char* p = (const unsigned char*) startText;
    const unsigned char* startTag=NULL;const unsigned char* endTag=NULL;const unsigned char* startSubchunk = p;
    for(p = (const unsigned char*) startText; p!=endTextUC; ++p)
    {
        if (UTF8Helper::decode(&state, &codePoint, *p)!=UTF8Helper::UTF8_ACCEPT) continue;
        ++numGlyphs;
        if (lastCodePoint == codePoint) continue;
        lastCodePoint = codePoint;
        if (codePoint==startTagCP) startTag = p;
        else if (startTag && codePoint==endTagCP) {
            endTag = p+1;
            bool tagValid = true;
            const char* s = (const char*) (startTag+1);
            const char* e = (const char*) (endTag-1);
            bool negate = false;bool hasEquality = false;
            if (*s=='/') {negate=true;++s;if (s>=e) tagValid=false;}
            const char* equality = e;
            if (tagValid && !negate) {
                equality = strchr(s,'=');
                if (!equality || equality>=e) {
                    hasEquality = false;
                    equality = e;
                }
                else hasEquality = true;
            }
            if (tagValid)   {
                // Parse the two fields
                static char field0[16];field0[0]='\0';
                static char field1[16];field1[0]='\0';
                int cnt=0;bool started = false;
                for (const char* t=s;t!=equality && cnt<15;++t) {
                    if (*t==' ' || *t=='\t') {
                        if (!started) continue;
                        else break;
                    }
                    field0[cnt++]=tolower(*t);   // <ctype.h> tolower()
                    started = true;
                }
                field0[cnt]='\0';
                if (hasEquality) {
                    started = false;bool quoteStarted = false;
                    cnt=0;for (const char* t=equality+1;t!=e && cnt<15;++t) {
                        if (!quoteStarted && (*t==' ' || *t=='\t')) {
                            if (!started) continue;
                            else break;
                        }
                        if (*t=='\'' || *t=='"')    {
                            if (!started) started = true;
                            if (quoteStarted) break;
                            quoteStarted = true;
                            continue;
                        }
                        field1[cnt++]=tolower(*t);   // <ctype.h> tolower()
                        started = true;
                    }
                    field1[cnt]='\0';
                }
                //fprintf(stderr,"Found tag: %.*s (%d): field0:%s field1:%s hasEquality:%s negate:%s [Text before:\"%.*s\"]\n",(int)(e-s),s,(int)(e-s),field0,field1,hasEquality?"true":"false",negate?"true":"false",(int)(startTag-startSubchunk),startSubchunk);

                TS.SdfAddText(chunk,startSubchunk,startTag);
                startSubchunk = endTag;

                // Process Tag and push or pop TS:
                bool error = false;
                if (strcmp(field0,"b")==0)          TS.bold=!negate;
                else if (strcmp(field0,"i")==0)     TS.italic=!negate;
                else if (strncmp(field0,"hal",3)==0)    {
                    if (negate) {if (TS.hAlign.size()>0) TS.hAlign.pop_back();}
                    else if (hasEquality)   {
                        SDFHAlignment hal=SDF_CENTER;
                        if (strncmp(field1,"l",1)==0) hal = SDF_LEFT;
                        else if (strncmp(field1,"c",1)==0) hal = SDF_CENTER;
                        else if (strncmp(field1,"r",1)==0) hal = SDF_RIGHT;
                        else if (strncmp(field1,"j",1)==0) hal = SDF_JUSTIFY;
                        else error = true;
                        if (!error) TS.hAlign.push_back(hal);
                    }
                    else error = true;
                }
                else if (strncmp(field0,"s",1)==0)  {
                    ImVec2 scaling(1,1);
                    if (negate)  {if (TS.scaling.size()>0) TS.scaling.pop_back();}
                    else if (hasEquality && sscanf(field1, "%f", &scaling.x))  {
                        scaling.y = scaling.x;
                        TS.scaling.push_back(scaling);
                    }
                    else error = true;
                }
                else if (strncmp(field0,"c",1)==0)  {
                    ImU32 color;
                    if (negate)  {if (TS.color.size()>0) TS.color.pop_back();}
                    else if (hasEquality && sscanf(field1, "%x", &color))  {
			const bool mustInvertColor = true;
			if (mustInvertColor) {
			    color = ((color << 8) & 0xFF00FF00 ) | ((color >> 8) & 0xFF00FF );
			    color = (color << 16) | (color >> 16);
                // TODO: we should still invert R with B if IMGUI_USE_BGRA_PACKED_COLOR is defined!
			}
			TS.color.push_back(SdfTextColor(ImGui::ColorConvertU32ToFloat4(color)));
		    }
                    else error = true;
                }
                //TODO: other tags here (quad color, vAlign, etc.)
                //if (error) {printf("SdfMarkupError: Can't understand tag: \"%.*s\"\n",(int)(e-s),s);fflush(stdout);}
            }
            startTag=endTag=NULL;
        }

    }
    if (startSubchunk && p) TS.SdfAddText(chunk,startSubchunk,p);
}

void SdfTextColor::SetDefault(const SdfTextColor& defaultColor,bool updateAllExistingTextChunks) {
    gSdfTextDefaultColor=defaultColor;
    if (updateAllExistingTextChunks)    {
        for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++)  {
            gSdfInit.gSdfTextChunks[i]->dirty=true;
        }
    }
}


SdfAnimation* SdfAddAnimation() {
    SdfAnimation* p = (SdfAnimation*) ImGui::MemAlloc(sizeof(SdfAnimation));
    IM_PLACEMENT_NEW (p) SdfAnimation();
    gSdfInit.gSdfAnimations.push_back(p);
    return p;
}
void SdfAnimationSetLoopingParams(SdfAnimation* animation,bool mustLoop,bool mustHideTextWhenFinishedIfNotLooping)  {
    IM_ASSERT(animation);
    animation->looping = mustLoop;
    animation->mustMuteAtEnd = mustHideTextWhenFinishedIfNotLooping;
}

float SdfAnimationAddKeyFrame(SdfAnimation* animation,const SdfAnimationKeyFrame& keyFrame) {
    IM_ASSERT(animation);
    return animation->addKeyFrame(keyFrame);
}
void SdfAnimationClear(SdfAnimation* animation) {
    IM_ASSERT(animation);
    animation->clear();
}
void SdfRemoveAnimation(SdfAnimation* animation)    {
    IM_ASSERT(animation);
    const bool checkItsNotSetToAnyChunk = true;
    if (checkItsNotSetToAnyChunk)   {
        for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++)  {
            SdfTextChunk* chunk = gSdfInit.gSdfTextChunks[i];
            if (chunk->manualAnimationRef==animation)  {
                if (chunk->animationMode==SDF_AM_MANUAL) {chunk->mute=true;chunk->animationStartTime=-1.f;}
                chunk->manualAnimationRef=NULL;
            }
        }
    }

    for (int i=0,isz=gSdfInit.gSdfAnimations.size();i<isz;i++)  {
        SdfAnimation* anim = gSdfInit.gSdfAnimations[i];
        if (anim == animation)  {
            // destroy animation
            animation->~SdfAnimation();
            ImGui::MemFree(animation);
            // Swap with last element and pop_back
            gSdfInit.gSdfAnimations[i] = gSdfInit.gSdfAnimations[isz-1];
            gSdfInit.gSdfAnimations[isz-1] = NULL;
            gSdfInit.gSdfAnimations.pop_back();
            break;
        }
    }
}
void SdfRemoveAllAnimations() {
    gSdfInit.DestroyAllAnimations();
}
void SdfTextChunkSetManualAnimation(struct SdfTextChunk* chunk,struct SdfAnimation* animation)  {
    IM_ASSERT(chunk && animation);
    if (chunk->manualAnimationRef != animation) {
        chunk->manualAnimationRef = animation;
        chunk->animationStartTime = -1.f;
    }
}
void SdfTextChunkSetAnimationParams(struct SdfTextChunk* chunk,const SdfAnimationParams& params)   {
    IM_ASSERT(chunk);
    chunk->animationParams = params;
    chunk->animationStartTime = -1.f;
}
void SdfTextChunkSetAnimationMode(struct SdfTextChunk* chunk,SDFAnimationMode mode) {
    IM_ASSERT(chunk);
    if (chunk->animationMode!=mode) {
        chunk->animationMode=mode;
        chunk->animationStartTime = -1.f;
    }
}
SDFAnimationMode SdfTextChunkGetAnimationMode(struct SdfTextChunk* chunk) {
    IM_ASSERT(chunk);
    return chunk->animationMode;
}

const SdfAnimation* SdfTextChunkGetManualAnimation(const SdfTextChunk* chunk)	{
    IM_ASSERT(chunk);
    return chunk->manualAnimationRef;
}
SdfAnimation* SdfTextChunkGetManualAnimation(SdfTextChunk* chunk)   {
    IM_ASSERT(chunk);
    return chunk->manualAnimationRef;
}
const SdfAnimationParams& SdfTextChunkGetAnimationParams(const SdfTextChunk* chunk) {
    IM_ASSERT(chunk);
    return chunk->animationParams;
}
SdfAnimationParams& SdfTextChunkGetAnimationParams(SdfTextChunk* chunk)	{
    IM_ASSERT(chunk);
    return chunk->animationParams;
}

void SdfTextChunkSetGlobalParams(struct SdfTextChunk* chunk,const SdfGlobalParams& params)    {
    IM_ASSERT(chunk);
    chunk->globalParams = params;
}
const SdfGlobalParams& SdfTextChunkGetGlobalParams(const struct SdfTextChunk* chunk) {
    IM_ASSERT(chunk);return chunk->globalParams;
}
SdfGlobalParams& SdfTextChunkGetGlobalParams(struct SdfTextChunk* chunk) {
    IM_ASSERT(chunk);return chunk->globalParams;
}



bool SdfTextChunk::setupUniformValuesAndDrawArrays(SdfShaderProgram* SP,bool shadowPass,const ImVec2& screenSize)	{
    if (animationMode==SDF_AM_NONE || (animationParams.startChar==0 && animationParams.endChar==-1))    {
        // is tmpKeyFrame always good even if animationMode==SDF_AM_NONE ?
        if (tmpKeyFrame.alpha==0.f) return false;

	const GLint startCharSize = (tmpKeyFrame.startChar>globalParams.startChar?tmpKeyFrame.startChar:globalParams.startChar)*6;
	const GLint endCharSize1 = (tmpKeyFrame.endChar<0)?buffer->verts.size():(tmpKeyFrame.endChar*6);
	GLint endCharSize = (globalParams.endChar<0)?buffer->verts.size():(globalParams.endChar*6);
	if (endCharSize>=endCharSize1) endCharSize = endCharSize1;

	if (endCharSize<=startCharSize) return false;

    if (shadowPass)	{
        SP->setUniformOffsetAndScale(ImVec2((tmpKeyFrame.offset.x*screenSize.x)+shadowOffsetInPixels.x*tmpKeyFrame.scale.x
                                            +(screenSize.x*(0.5f-0.5f*tmpKeyFrame.scale.x))  // scaling (not sure this is correct) [can be removed]
                                            ,(tmpKeyFrame.offset.y*screenSize.y)+shadowOffsetInPixels.y*tmpKeyFrame.scale.y
                                            +(screenSize.y*(0.5f-0.5f*tmpKeyFrame.scale.y))  // scaling (not sure this is correct) [can be removed]
                                            ),tmpKeyFrame.scale);
        SP->setUniformAlphaAndShadow(tmpKeyFrame.alpha*0.5f,0.2f);
    }
    else {
        SP->setUniformOffsetAndScale(tmpKeyFrame.offset*screenSize+
                                     (screenSize-screenSize*tmpKeyFrame.scale)*0.5f  // scaling (not sure this is correct) [can be removed]
                                     ,tmpKeyFrame.scale);
        SP->setUniformAlphaAndShadow(tmpKeyFrame.alpha,1.f);
    }

	glDrawArrays(GL_TRIANGLES,startCharSize,endCharSize-startCharSize);
    }
    else {
        if (globalParams.alpha==0.f) return false;
        // We have an animation, that must be limited to a subset of chars.
	const GLint minStartSize = (globalParams.startChar<0)?0:(globalParams.startChar*6);
	const GLint maxEndSize = (globalParams.endChar<0)?buffer->verts.size():(globalParams.endChar*6);
	const int realAnimationParamsStartSize  = (animationParams.startChar*6) > minStartSize ? (animationParams.startChar*6) : minStartSize;
	const int realAnimationParamsEndSize    = (animationParams.endChar<0 || animationParams.endChar*6 > maxEndSize) ? maxEndSize : (animationParams.endChar*6);
	const int realTmpFrameStartSize         = (tmpKeyFrame.startChar*6) > minStartSize ? (tmpKeyFrame.startChar*6) : minStartSize;
	const int realTmpFrameEndSize           = (tmpKeyFrame.endChar<0 || tmpKeyFrame.endChar*6 > maxEndSize) ? maxEndSize : (tmpKeyFrame.endChar*6);

        // 1) draw non-animated edges:
        {
        if (shadowPass)	{
            SP->setUniformOffsetAndScale(ImVec2(globalParams.offset.x*screenSize.x+shadowOffsetInPixels.x*globalParams.scale.x
                                                +(screenSize.x*(0.5f-0.5f*globalParams.scale.x))  // scaling (not sure this is correct) [can be removed]
                                                ,globalParams.offset.y*screenSize.y+shadowOffsetInPixels.y*globalParams.scale.y
                                                +(screenSize.y*(0.5f-0.5f*globalParams.scale.y))  // scaling (not sure this is correct) [can be removed]
                                                ),globalParams.scale);
            SP->setUniformAlphaAndShadow(globalParams.alpha*0.5f,0.2f);
        }
        else {
            SP->setUniformOffsetAndScale(globalParams.offset*screenSize
                                         +(screenSize-screenSize*globalParams.scale)*0.5f  // scaling (not sure this is correct) [can be removed]
                                         ,globalParams.scale);
            SP->setUniformAlphaAndShadow(globalParams.alpha,1.f);
            }
            // First edge:
	    if (realAnimationParamsStartSize>minStartSize) glDrawArrays(GL_TRIANGLES, minStartSize, realAnimationParamsStartSize-minStartSize);
            // Last Edge:
	    if (maxEndSize-realAnimationParamsEndSize>0) glDrawArrays(GL_TRIANGLES, realAnimationParamsEndSize, maxEndSize-realAnimationParamsEndSize);
        }

        // 2) draw animated portion:
	if (realTmpFrameStartSize<=realAnimationParamsEndSize && realTmpFrameEndSize>=realAnimationParamsStartSize)
	{
        if (shadowPass)	{
            SP->setUniformOffsetAndScale(ImVec2((tmpKeyFrame.offset.x*screenSize.x)+shadowOffsetInPixels.x*tmpKeyFrame.scale.x
                                                +(screenSize.x*(0.5f-0.5f*tmpKeyFrame.scale.x))  // scaling (not sure this is correct) [can be removed]
                                                ,(tmpKeyFrame.offset.y*screenSize.y)+shadowOffsetInPixels.y*tmpKeyFrame.scale.y
                                                +(screenSize.y*(0.5f-0.5f*tmpKeyFrame.scale.y))  // scaling (not sure this is correct) [can be removed]
                                                ),tmpKeyFrame.scale);
            SP->setUniformAlphaAndShadow(tmpKeyFrame.alpha*0.5f,0.2f);
        }
        else {
            SP->setUniformOffsetAndScale(tmpKeyFrame.offset*screenSize
                                         +(screenSize-screenSize*tmpKeyFrame.scale)*0.5f  // scaling (not sure this is correct) [can be removed]
                                         ,tmpKeyFrame.scale);
            SP->setUniformAlphaAndShadow(tmpKeyFrame.alpha,1.f);
        }
	    const GLint startAnimationCharIndex = realAnimationParamsStartSize>realTmpFrameStartSize?realAnimationParamsStartSize:realTmpFrameStartSize;
	    const GLint endAnimationCharIndex   = realTmpFrameEndSize<realAnimationParamsEndSize?realTmpFrameEndSize:realAnimationParamsEndSize;

	    glDrawArrays(GL_TRIANGLES, startAnimationCharIndex, endAnimationCharIndex-startAnimationCharIndex);
        }    
    }
    return true;
}

void SdfRender(const ImVec4* pViewportOverride) {
    ImGuiIO& io = ImGui::GetIO();
    static ImVec2 displaySizeLast = io.DisplaySize;
    const ImVec2 displaySize = pViewportOverride ? ImVec2(pViewportOverride->z,pViewportOverride->w) : io.DisplaySize;
    if (displaySize.x==0 || displaySize.y==0) return;
    const bool screenSizeChanged = displaySizeLast.x!=displaySize.x || displaySizeLast.y!=displaySize.y;
    if (screenSizeChanged) {
        displaySizeLast = displaySize;
        for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++) {
            gSdfInit.gSdfTextChunks[i]->dirty = true;
        }
    }

    bool hasRegularFonts=false,hasOutlineFonts=false;float globalTime = ImGui::GetTime();
    for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++) {
        SdfTextChunk* c = gSdfInit.gSdfTextChunks[i];
        if (c->textBits.size()==0 || !c->checkVisibleAndEvalutateAnimationIfNecessary(globalTime)) continue;
        hasRegularFonts|=(c->buffer->type==0 || (c->buffer->type&(SDF_BT_SHADOWED)));
        hasOutlineFonts|=(c->buffer->type&(SDF_BT_OUTLINE));
        if (hasRegularFonts && hasOutlineFonts) break;
    }

    if (!hasRegularFonts && !hasOutlineFonts) return;


    const float fb_x = pViewportOverride ? pViewportOverride->x*io.DisplayFramebufferScale.x : 0.f;
    const float fb_y = pViewportOverride ? pViewportOverride->y*io.DisplayFramebufferScale.y : 0.f;
    const float fb_height = displaySize.y * io.DisplayFramebufferScale.y;   // Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
    const float fb_width = displaySize.x * io.DisplayFramebufferScale.x;
    glViewport((GLint)fb_x, (GLint)fb_y, (GLsizei)fb_width, (GLsizei)fb_height);
    //fprintf(stderr,"%d %d %d %d (%d %d)\n",(GLint)fb_x, (GLint)fb_y, (GLsizei)fb_width, (GLsizei)fb_height,(int)io.DisplaySize.x,(int)io.DisplaySize.y);

    if (hasRegularFonts && !gSdfShaderPrograms[0].program) {
        static bool done = false;
        if (done) return;
        done = true;
        if (!gSdfShaderPrograms[0].loadShaderProgram(false)) return;
    }
    if (hasOutlineFonts && !gSdfShaderPrograms[1].program) {
        static bool done = false;
        if (done) return;
        done = true;
        if (!gSdfShaderPrograms[1].loadShaderProgram(true)) return;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glActiveTexture(GL_TEXTURE0);
    glCullFace(GL_FRONT);       // with this I can leave GL_CULL_FACE as it is
    glDisable(GL_DEPTH_TEST);
    //glEnable(GL_SCISSOR_TEST);    // TO FIX: Does not work well with the y of the glScissor(...) call [when boundsCenter.y changes]


    ImTextureID lastBoundTexture = 0;
    const ImVec2 screenSize = displaySize;   // For scissor test
    const ImVec2 screenOffset = pViewportOverride ? ImVec2(pViewportOverride->x,pViewportOverride->y) : ImVec2(0,0);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    if (hasRegularFonts)    {
        SdfShaderProgram& SP = gSdfShaderPrograms[0];
        glUseProgram(SP.program);
        if (screenSizeChanged) SP.resetUniformOrtho();
        bool isShadow = false;
        for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++) {
            SdfTextChunk* c = gSdfInit.gSdfTextChunks[i];
            if (c->textBits.size()==0 || !c->tmpVisible) continue;
            hasRegularFonts=(c->buffer->type!=SDF_BT_OUTLINE);
            if (!hasRegularFonts) continue;
            isShadow = (c->buffer->type&(SDF_BT_SHADOWED));

            if (lastBoundTexture!=c->charset->fntTexture) {
                lastBoundTexture=c->charset->fntTexture;
                glBindTexture(GL_TEXTURE_2D,(unsigned long)c->charset->fntTexture);
            }

            if (!c->buffer->vbo) glGenBuffers(1,&c->buffer->vbo);
            glBindBuffer(GL_ARRAY_BUFFER, c->buffer->vbo);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SdfVertexBuffer::VertexDeclaration), 0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SdfVertexBuffer::VertexDeclaration),(const void*) (2*sizeof(GLfloat)));
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SdfVertexBuffer::VertexDeclaration), (const void*)(0 + 4*sizeof(GLfloat)));

            if (c->endText(displaySize)) c->buffer->updateBoundVbo();

            glScissor(screenOffset.x+(c->props.boundsCenter.x-c->props.boundsHalfSize.x)*screenSize.x,screenOffset.y+(c->props.boundsCenter.y-c->props.boundsHalfSize.y)*screenSize.y,
            c->props.boundsHalfSize.x*screenSize.x*2.f,c->props.boundsHalfSize.y*screenSize.y*2.f);

            if (isShadow) c->setupUniformValuesAndDrawArrays(&SP,true,screenSize);
            if (!isShadow || (c->buffer->type==SDF_BT_SHADOWED)) c->setupUniformValuesAndDrawArrays(&SP,false,screenSize);

        }
    }
    if (hasOutlineFonts)    {
        SdfShaderProgram& SP = gSdfShaderPrograms[1];
        glUseProgram(SP.program);
        if (screenSizeChanged) SP.resetUniformOrtho();
        for (int i=0,isz=gSdfInit.gSdfTextChunks.size();i<isz;i++) {
            SdfTextChunk* c = gSdfInit.gSdfTextChunks[i];
            if (c->textBits.size()==0 || !c->tmpVisible) continue;
            hasOutlineFonts=(c->buffer->type&(SDF_BT_OUTLINE));
            if (!hasOutlineFonts) continue;

            if (lastBoundTexture!=c->charset->fntTexture) {
                lastBoundTexture=c->charset->fntTexture;
                glBindTexture(GL_TEXTURE_2D,(unsigned long)c->charset->fntTexture);
            }

            if (!c->buffer->vbo) glGenBuffers(1,&c->buffer->vbo);
            glBindBuffer(GL_ARRAY_BUFFER, c->buffer->vbo);

            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(SdfVertexBuffer::VertexDeclaration), 0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(SdfVertexBuffer::VertexDeclaration),(const void*) (2*sizeof(GLfloat)));
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(SdfVertexBuffer::VertexDeclaration), (const void*)(0 + 4*sizeof(GLfloat)));

            if (c->endText(displaySize)) c->buffer->updateBoundVbo();

            glScissor(screenOffset.x+(c->props.boundsCenter.x-c->props.boundsHalfSize.x)*screenSize.x,screenOffset.y+(c->props.boundsCenter.y-c->props.boundsHalfSize.y)*screenSize.y,
            c->props.boundsHalfSize.x*screenSize.x*2.f,c->props.boundsHalfSize.y*screenSize.y*2.f);

            c->setupUniformValuesAndDrawArrays(&SP,false,screenSize);
        }
    }

    glBindTexture(GL_TEXTURE_2D,0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glUseProgram(0);

    glEnable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glCullFace(GL_BACK);
    glDisable(GL_BLEND);

}


#ifndef NO_IMGUISDF_EDIT
inline bool SdfAnimationKeyFrameEdit(SdfAnimationKeyFrame* kf,bool isFirstFrame=false)	{
    IM_ASSERT(kf);
    bool changed = false;
    ImGui::PushID(kf);
    ImGui::PushItemWidth(ImGui::GetWindowWidth()*0.2f);

    changed|=ImGui::DragFloat("Alpha##SdfAlpha",&kf->alpha,0.01f,0.0f,1.f);
    if (!(isFirstFrame && kf->timeInSeconds==0.f)) {
        ImGui::SameLine();
        changed|=ImGui::DragFloat("FrameTimeInSeconds##SdfFrameTimeInSeconds",&kf->timeInSeconds,0.01f,0.0f,60.f);
        if (isFirstFrame && ImGui::IsItemHovered()) ImGui::SetTooltip("%s","You should set it to zero\nfor the first frame.");
    }
    changed|=ImGui::DragFloat2("Offset##SdfKeyFrameOffset",&kf->offset.x,0.01f,-1.0f,1.0f);
    //
    changed|=ImGui::DragFloat2("Scale##SdfKeyFrameScale",&kf->scale.x,0.001f,0.1f,5.0f);

    changed|=ImGui::DragInt("StartChar##SdfStartChar",&kf->startChar,0.01f,0,2000);
    ImGui::SameLine();
    changed|=ImGui::DragInt("EndChar##SdfEndChar",&kf->endChar,0.01f,-1,2000);

    ImGui::PopItemWidth();
    ImGui::PopID();
    return changed;
}
inline bool SdfAnimationEdit(SdfAnimation* an)	{
    IM_ASSERT(an);
    bool changed = false;
    ImGui::PushID(an);

    changed|=ImGui::Checkbox("Looping##SdfAnimationLooping",&an->looping);
    if (!an->looping) {
        ImGui::SameLine();
        changed|=ImGui::Checkbox("Must Hide Text At End##SdfAnimationMustHideTextAtEnd",&an->mustMuteAtEnd);
    }

    char name[65]="";
    int kfri = -1;
    for (int kfi=0,nkf=an->keyFrames.size();kfi<nkf;kfi++)    {
        SdfAnimationKeyFrame& kf = an->keyFrames[kfi];
        ImGui::PushID(kfi);
        sprintf(&name[0],"KeyFrame %.3d", kfi);
        if (ImGui::TreeNode(name)) {
            changed|= SdfAnimationKeyFrameEdit(&kf,kfi==0);
            if (ImGui::SmallButton("Remove##SdfRemoveKeyFrame")) {changed = true;kfri=kfi;}
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
    ImGui::PushItemWidth(ImGui::GetWindowWidth()/10.f);
    ImGui::Separator();
    if (ImGui::Button("Add KeyFrame##SdfAddKeyFrame")) {
        SdfAnimationKeyFrame kf(1.f);
        if (an->keyFrames.size()==0) {kf.timeInSeconds=0.f;}
        else {
            kf = an->keyFrames[an->keyFrames.size()-1];
            if (kf.timeInSeconds<=0.f) kf.timeInSeconds = 1.f;
        }
        an->addKeyFrame(kf);
        changed = true;
    }
    ImGui::PopItemWidth();
    ImGui::PopID();

    if (kfri>=0) an->removeKeyFrameAt(kfri);

    return changed;
}

bool SdfTextChunkEdit(SdfTextChunk* sdfTextChunk, char* buffer, int bufferSize)   {
    IM_ASSERT(sdfTextChunk && buffer && bufferSize>0);

    ImGui::PushID(sdfTextChunk);
    SdfTextChunkProperties& sdfLayoutProps = sdfTextChunk->props;
    unsigned int flags = (unsigned int) sdfTextChunk->buffer->type;

    bool changed = false,changed2=false;
    static bool useMarkups = true;
    changed2|=ImGui::Checkbox("Use markups",&useMarkups);
    changed|=ImGui::CheckboxFlags("Outline##SDF_outline_style",&flags,ImGui::SDF_BT_OUTLINE);ImGui::SameLine();
    changed|=ImGui::CheckboxFlags("Shadowed##SDF_shadowed_style",&flags,ImGui::SDF_BT_SHADOWED);
    if (changed) ImGui::SdfTextChunkSetStyle(sdfTextChunk,(int)flags);

    changed=changed2;changed2=false;
    if (!useMarkups) ImGui::SameLine();
    static bool italic = false;if (!useMarkups) changed|=ImGui::Checkbox("Italic##SDF_italic",&italic);
    static ImVec4 color = sdfTextChunk->buffer->getColorOfVert(0) ? *sdfTextChunk->buffer->getColorOfVert(0) : SdfTextDefaultColor.colorTopLeft;
    if (!useMarkups) {
        ImGui::PushItemWidth(ImGui::GetWindowWidth()/3.f);
        changed|=ImGui::ColorEdit4("Color##SDF_color",&color.x);
        ImGui::PopItemWidth();
    }
    //ImGui::PushItemWidth(ImGui::GetWindowWidth()/6.f);
    //static ImVec2 scaling(1.f,1.f);changed|=ImGui::DragFloat2("Scale##SDF_scale",&scaling.x,0.1f,0.25f,4.f);
    //static float scaling(1.f);changed|=ImGui::DragFloat("Scale##SDF_scale",&scaling,0.1f,0.25f,4.f);
    //ImGui::PopItemWidth();

    ImGui::PushItemWidth(ImGui::GetWindowWidth()/10.f);
    changed2|=ImGui::DragFloat("max num lines##SDF_maxnumlines",&sdfLayoutProps.maxNumTextLines,0.5f,1.0f,100.f);
    ImGui::PopItemWidth();
    ImGui::PushItemWidth(ImGui::GetWindowWidth()/6.f);
    static const char* Halignments[] = {"LEFT","CENTER","RIGHT","JUSTIFY"};
    static const char* Valignments[] = {"TOP","CENTER","BOTTOM"};
    int halign = (int) sdfLayoutProps.halign;if (ImGui::Combo("Halignment##SDF_Halignment",&halign,&Halignments[0],4))   {changed2=true;sdfLayoutProps.halign = (ImGui::SDFHAlignment) halign;}
    ImGui::SameLine();
    int valign = (int) sdfLayoutProps.valign;if (ImGui::Combo("Valignment##SDF_Valignment",&valign,&Valignments[0],3))   {changed2=true;sdfLayoutProps.valign = (ImGui::SDFVAlignment) valign;}
    changed2|=ImGui::DragFloat2("boundsCenter##SDF_boundsCenter",&sdfLayoutProps.boundsCenter.x,0.01f,0.0f,1.0f);
    ImGui::SameLine();
    changed2|=ImGui::DragFloat2("boundsHalfSize##SDF_boundsHalfSize",&sdfLayoutProps.boundsHalfSize.x,0.01f,0.001f,0.5f);
    //changed2|=ImGui::DragFloat("lineHeightOvr##SDF_lineHeightOvr",&sdfLayoutProps.lineHeightOverride,0.01f,0.0f,2.f);
    ImGui::PopItemWidth();

    bool textChanged = false;
    ImGui::PushItemWidth(-1.f);
    changed|=textChanged=ImGui::InputTextMultiline("##SDF Text",buffer,bufferSize,ImVec2(0,0),ImGuiInputTextFlags_AllowTabInput);
    ImGui::PopItemWidth();
    if (changed || changed2) {
        if (changed2) ImGui::SdfTextChunkGetProperties(sdfTextChunk) = sdfLayoutProps;
        ImGui::SdfClearText(sdfTextChunk);
        if (!useMarkups)    {
            ImGui::SdfTextColor textColor(color);
            //ImVec2 scaling2D(scaling,scaling);
            ImGui::SdfAddText(sdfTextChunk,buffer,italic,&textColor,NULL/*&scaling2D*/); // Actually we can append multiple of these calls together
        }
        else ImGui::SdfAddTextWithTags(sdfTextChunk,buffer,NULL);
    }

    // Animations:
    ImGui::PushItemWidth(ImGui::GetWindowWidth()/6.f);
    static const char* AnimationModeNames[SDF_AM_TYPING+1] = {"NONE","MANUAL",
                                                              "FADE_IN","ZOOM_IN","APPEAR_IN","LEFT_IN","RIGHT_IN","TOP_IN","BOTTOM_IN",
                                                              "FADE_OUT","ZOOM_OUT","APPEAR_OUT","LEFT_OUT","RIGHT_OUT","TOP_OUT","BOTTOM_OUT",
                                                              "BLINK","PULSE","TYPING"};
    int animationMode = (int) ImGui::SdfTextChunkGetAnimationMode(sdfTextChunk);
    if (ImGui::Combo("Animation Mode##SDFAnimationMode",&animationMode,&AnimationModeNames[0],sizeof(AnimationModeNames)/sizeof(AnimationModeNames[0])))    {
        ImGui::SdfTextChunkSetAnimationMode(sdfTextChunk,(SDFAnimationMode)animationMode);
        sdfTextChunk->setMute(false);
        sdfTextChunk->animationStartTime = -1.f;
    }
    //ImGui::SameLine();
    bool changed3 = false;
    if (ImGui::TreeNode("Animation Params"))	{
	changed3|=ImGui::DragFloat("ASpeed##SdfAnimationSpeed",&sdfTextChunk->animationParams.speed,0.01f,0.2f,5.f);
	//ImGui::SameLine();
	//if (ImGui::Checkbox("ALoop##SdfAnimationLooping",&sdfTextChunk->animationParams.looping)) {changed3=true;sdfTextChunk->animationStartTime = -1.f;}
	//if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s","Looping affects only\nmanual animations.");
	//ImGui::SameLine();
	bool hov1 = false;
	ImGui::DragInt("AStartChar##SdfAStartChar",&sdfTextChunk->animationParams.startChar,0.01f,0,2000);
	hov1|=ImGui::IsItemHovered();ImGui::SameLine();
	ImGui::DragInt("AEndChar##SdfAEndChar",&sdfTextChunk->animationParams.endChar,0.01f,-1,2000);
	hov1|=ImGui::IsItemHovered();if (hov1) ImGui::SetTooltip("%s","Useful only for partial\nanimations of the text.");
	if (ImGui::SmallButton("Reset##SdfAnimationParams")) {changed3=true;sdfTextChunk->animationParams = SdfAnimationParams();}
	ImGui::Separator();
	ImGui::TreePop();
    }

    if (ImGui::TreeNode("Global Params"))	{
    ImGui::DragFloat("GAlpha##SdfGAlpha",&sdfTextChunk->globalParams.alpha,0.01f,0.0f,1.f);
	ImGui::SameLine();
    ImGui::DragFloat2("GOffset##SdfGKeyFrameOffset",&sdfTextChunk->globalParams.offset.x,0.01f,-1.0f,1.0f);
    ImGui::DragFloat2("GScale##SdfGKeyFrameScale",&sdfTextChunk->globalParams.scale.x,0.001f,0.1f,5.0f);
	//ImGui::SameLine();

	bool hov2 = false;
	ImGui::DragInt("GStartChar##SdfGStartChar",&sdfTextChunk->globalParams.startChar,0.01f,0,2000);
	hov2|=ImGui::IsItemHovered();ImGui::SameLine();
	ImGui::DragInt("GEndChar##SdfGEndChar",&sdfTextChunk->globalParams.endChar,0.01f,-1,2000);
	hov2|=ImGui::IsItemHovered();if (hov2) ImGui::SetTooltip("%s","Useful only for partial\ndisplay of the text.");
	//ImGui::SameLine();
	if (ImGui::SmallButton("Reset##SdfGlobalParams")) {changed3=true;sdfTextChunk->globalParams = SdfGlobalParams();}
	ImGui::Separator();
	ImGui::TreePop();
    }

    if (changed3) {sdfTextChunk->setMute(false);sdfTextChunk->animationStartTime = -1.f;}
    //ImGui::SameLine();
    //ImGui::SameLine();ImGui::DragFloat2("Scale##SdfKeyFrameScale",&sdfTextChunk->animationParams.scale.x,0.01f,0.25f,2.5f);


    ImGui::PopItemWidth();

    //if (animationMode == SDF_AM_MANUAL)	{
    SdfAnimation* optAnimation = sdfTextChunk->manualAnimationRef;
    if (optAnimation) {
        if (ImGui::CollapsingHeader("Manual Animation##SdfManualAnimation"))    {
            if (SdfAnimationEdit(optAnimation)) {
                optAnimation->update();
                //sdfTextChunk->animationMode = SDF_AM_MANUAL;
                sdfTextChunk->animationStartTime = -1.f;
                sdfTextChunk->setMute(false);
	    }
	    ImGui::SameLine();
	    if (ImGui::Button("Restart##SdfManualAnimationRestart")) {
		sdfTextChunk->setMute(false);sdfTextChunk->animationStartTime = -1.f;
		sdfTextChunk->animationMode = SDF_AM_MANUAL;
	    }
        }
    }
/*
    else {
        ImGui::Text("You need to add to the text chunk a new animation to edit it.");
        ImGui::Text("Please see: SdfAddAnimation() and");
        ImGui::Text("SdfTextChunkSetManualAnimation(...).");
    }
*/
//    }



    ImGui::PopID();
    return textChanged;
}



#endif //NO_IMGUISDF_EDIT

} // namespace

