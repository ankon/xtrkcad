/*
 * csignal.h
 *
 *  Created on: Jul 27, 2019
 *      Author: richardsa
 */

#ifndef APP_BIN_CSIGNAL_H_
#define APP_BIN_CSIGNAL_H_

extern long SignalDisplay;
extern long SignalSide;
extern double SignalDiagramFactor;

#define SIGNAL_DISPLAY_PLAN (1)
#define SIGNAL_DISPLAY_DIAG (0)
#define SIGNAL_DISPLAY_ELEV (2)

#define SIGNAL_SIDE_LEFT (0)
#define SIGNAL_SIDE_RIGHT (1)


/*
 * A Signaling System is a set of elements that can be used to send instructions to drivers
 * It has a set of HeadTypes which show appearances and a set of MastTypes that integrate those Heads into a SignalMast.
 * There can be several Systems active on one layout - reflecting different heritages or vintages.
 * Each HeadType and mastType is qualified by its system.
 */
typedef struct signalSystem_t {
	int systemsCount;					//Number of Systems installed
	char * systemName[10];				//Names of Systems
	char * notes[10];					//Explanations
	dynArr_t headTypes;					//Types of Heads - Heads to add to Signals - combined
	dynArr_t postTypes;					//Types of Posts - Places to put Heads - combined
	dynArr_t prototypeSignals;          //List of ProtoType Signals - combined
	dynArr_t extraAspectTypes;			//Array of all non-base Aspects Used by all Signals - combined
} signalSystem_t, *signalSystem_p;

static signalSystem_t signalSystem;    //The combined signallingSystems in use

/* These are the normal Aspect Names that JMRI will recognize they are used in XTrackCAD if no Base Aspects are defined */
typedef enum {ASPECT_NONE = -1,
			ASPECT_DANGER,
			ASPECT_PROCEED,
			ASPECT_CAUTION,
			ASPECT_FLASHCAUTION,
			ASPECT_PRELIMINARYCAUTION,
			ASPECT_FLASHPRELIMINARYCAUTION,
			ASPECT_OFF,
			ASPECT_ON,
			ASPECT_CALLON,
			ASPECT_SHUNT,
			ASPECT_WARNING,
			} signalBaseAspects_e;

#define BaseAspectCount ASPECT_WARNING+1

static const char * signalBaseAspectNames[] = {
		"NONE",
		"DANGER",
		"PROCEED",
		"CAUTION",
		"FLASHCAUTION",
		"PRELIMINARYCAUTION",
		"FLASHPRELIMINARYCAUTION",
		"OFF",
		"ON",
		"CALLON",
		"SHUNT",
		"WARNING",
};

/*
 * An AspectType maps an AspectName to a Speed - Note multiple Aspects on a Signal can map onto one baseAspect
 */
typedef struct signalAspectType_t {
	char * aspectName;
	signalBaseAspects_e baseAspect;
} signalAspectType_t, *signalAspectType_p;


/*
 * A Signal Post contains up to 3 drawings (Plan, Elevation and Drawing).  It can also contain an arbitrary number of signals.
 */
typedef struct signalPost_t {
	SCALEINX_T scale;
	dynArr_t drawings[3];
	char * postName;
	int paramFileIndex;
	dynArr_t feet;				//CoOrds
	dynArr_t signals;			//Signals
} signalPost_t, *signalPost_p;

/*
 * A Signal Post Type contains up to 3 drawings (Plan, Elevation and Drawing).
 *
 * It will be scaled when added to a Signal
 *
 */
typedef struct signalPostType_t {
	SCALEINX_T scale;
	dynArr_t drawings[3];
	char * postTypeName;
	int paramFileIndex;
	dynArr_t feet;				//CoOrds
} signalPostType_t, *signalPostType_p;

/*
 * A Map from the Aspects from this Signal to all the Heads and the Appearances that they show
 */

typedef struct headAspectMap_t {
	int aspectMapHeadNumber;					//Which Head is that on this Signal
	char * aspectMapHeadAppearance;				//Appearance name
	int aspectMapHeadAppearanceNumber;		    //Which appearance is that on the head
} headAspectMap_t, *headAspectMap_p;

/*
 * An Aspect is an appearance by a Signal Mast to a driver of what he is allowed to do. It may be shown by using one or more Heads.
 * The Aspect in JMRI communicates a max speed to the "driver".
 */
typedef struct signalAspect_t {
    char * 		aspectName;					//Aspect
    char * 		aspectScript;
    dynArr_t 	headAspectMap;					//Array of all Head settings for this Aspect
    signalAspectType_p aspectType;
} signalAspect_t, *signalAspect_p;


/*
 * A HeadType is a parameter definition from which shows all the Appearances of a Head
 */
typedef struct signalHeadType_t {
	char * headTypeName;
	dynArr_t headSegs;                  //Draw Segments that don't change for head (background, lamp body, etc)
	dynArr_t headAppearances;	 		//All things the head can show
	BOOL_T used;
	SCALEINX_T headScale;
	} signalHeadType_t, *signalHeadType_p;

/*
 * A Head is the smallest controllable unit. It could be as small as one light or arm, or as complex as a matrix display
 * The head is found by its ordinal position on the post.
 */
typedef struct signalHead_t {
	char * headName;					//Often includes Lever #
	coOrd headPos;						//Relative to Post
	char * headTypeName;				//Type Name
	signalHeadType_p headType;				//Pointer to common HeadType definition
	dynArr_t headSegs[3];				//Static segs
	int currentHeadAppearance;			//Index of appearance within HeadType.Appearances
	char * diagramText;					//Internal Label for Matrix/Stencil
	} signalHead_t, *signalHead_p;

/*
 * A Signal Part Record - this links together Posts, Aspects and Groups into a Signal that can be copied to the Layout
 */

typedef enum {SIGNAL_DIAGRAM, SIGNAL_PLAN, SIGNAL_ELEVATION } SIGNAL_LOOK_e;


typedef struct signalPart_t {
		SCALEINX_T scaleInx;
		char * title;
		coOrd orig;
		coOrd size;
		dynArr_t staticSegs[3];
		dynArr_t heads;
		dynArr_t aspects;
		dynArr_t groups;
		int paramFileIndex;
		DIST_T barScale;
		char * contentsLabel;
		dynArr_t currSegs;
		} signalPart_t, * signalPart_p;

/*
 * SignalGroupInstance_p
 * For a group, the set of heads, appearances and conditions
 */
typedef struct {
	int headId;						//Head To Change
	char * appearanceOnName;		//Name of On Appearance
	char * appearanceOffName;		//Name of Off Appearance
	char * conditions;				//Condition that has to be true to set the Appearance On
} signalGroupInstance_t,*signalGroupInstance_p;

/* List of Aspects in which this Group is active */
typedef struct {
	signalAspect_p aspect;			//Aspect
	char * aspectName;				//AspectName
} groupAspectList_t,*groupAspectList_p;

/*signalGroup_p
 * The Group is a way of connecting one Head's Appearances to another Head having an Appearance and some Conditions
 * The typical use is for Matrix, Stencil or Feathers to Indicate which Route has been cleared
 */
typedef struct signalGroup_t {
	char * name;						//Group
	dynArr_t aspects;					//List of Aspects that this group works for
	dynArr_t groupInstances;			//List of conditions and Appearances
	int currInstanceDisplay;			//Just to show Instances
} signalGroup_t, * signalGroup_p;

/*
 * A Signal Appearance is a unique display by one Head
 */
typedef struct HeadAppearance_t {
	char * appearanceName;				//Appearance postName
	dynArr_t appearanceSegs; 			//How to Draw it
	coOrd orig;
	ANGLE_T angle;
} HeadAppearance_t, * HeadAppearance_p;

typedef enum baseSignalIndicators {IND_DIAGRAM,
			IND_UNLIT,
			IND_RED,
			IND_GREEN,
			IND_YELLOW,
			IND_LUNAR,
			IND_FLASHRED,
			IND_FLASHGREEN,
			IND_FLASHYELLOW,
			IND_FLASHLUNAR,
			IND_ON,
			IND_OFF,
			IND_LIT} baseSignalIndicators; //Eight predefined Appearances

/*
 * An IndicatorType maps an IndicatorName to a default Aspect
 */
typedef struct signalIndicatorType_t {
	char * aspectName;
	baseSignalIndicators indicator;
	signalBaseAspects_e aspect;
} signalIndicatorType_t, *signalIndicatorType_p;

void FormatSignalPartTitle(long format,char * title );
BOOL_T ReadSignalPart ( char * line );
BOOL_T ReadSignalPostType (char * line);
signalPostType_p CreateSignalPostType(SCALEINX_T scale, char * name);
BOOL_T WriteSignalSystem(FILE * f);
BOOL_T WriteSignalPostTypes(FILE * f, SCALEINX_T out_scale);
signalPart_p FindSignalDef(SCALEINX_T scale, char * name);
BOOL_T ReadSignalProto (char* line);
void SetSignalHead(track_p sig,int head, char* app);
BOOL_T ResolveSignalTrack ( track_p trk );
BOOL_T SignalIterate( track_p * sig );
void ClearSignals();
void SaveSignals();
void RestoreSignals();
void UpdateSignals();
BOOL_T ReadSignalSystem( char * line);
signalAspectType_p FindBaseAspect(char * name);
signalHeadType_p FindHeadType( char * name, SCALEINX_T scale);
int FindHeadAppearance(signalHeadType_p ht, char * appearanceName);
enum paramFileState GetSignalPartCompatibility(int paramFileIndex, SCALEINX_T scaleIndex);

#define SIG_ITERATE(SIG)		for (SIG=sig_first; SIG!=NULL; SIG=SIG->sig_next) if (!(SIG->deleted))
#endif /* APP_BIN_CSIGNAL_H_ */
