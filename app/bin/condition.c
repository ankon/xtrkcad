/** \file condition.c
 * Condition
 */

/* -*- C -*- ****************************************************************
 *
 *  Created By    : Adam Richards
 *  Created       : Sun Jan 11 13:11:45 2019
 *
 *  Description	 Condition is an implementation of the Logix capability from JMRI
 *  			 A Condition is a set of logical tests
 *  			 They can be ANDed ORed, NOTANDed, NOTORed
 *
 *  			 Note Antecedents not implemented yet
 *
 *  Notes
 *
 *  History
 *
 ****************************************************************************
 *
 *    Copyright (C) 2019 Adam Richards
 *
 *    This program is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program; if not, write to the Free Software
 *    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 ****************************************************************************/

#include "condition.h"
#include "cundo.h"
#include "custom.h"
#include "fileio.h"
#include "i18n.h"
#include "layout.h"
#include "param.h"
#include "track.h"
#include "trackx.h"
#include "utility.h"
#include "messages.h"
#include "common.h"

typedef enum ConditionType_e {COND_NONE = -1, COND_NOT, COND_AND, COND_OR, COND_NOTAND, COND_NOTOR, COND_MIXED} ConditionType_e;

typedef enum ActionFireType_e {FIRE_UNKNOWN = -1, FIRE_CHANGESTOTRUE, FIRE_CHANGESTOFALSE, FIRE_CHANGES } ActionFireType_e;

typedef struct Condition_t Condition_t;

typedef enum stateValues_e {STATE_UNKNOWN, STATE_TRUE, STATE_FALSE} stateValues_e;

/*
 * A Test is a named logical comparison - it is part of a condition
 */
typedef struct {
	char * nameToTest;
	typeControl_e typeToTest;
	char * valueToTest;
	stateValues_e lastStateTest;
	BOOL_T fireTestOnChange;
	BOOL_T inverse;
	Condition_t condition;
	BOOL_T found;
} Test_t, *Test_p;

/*
 * An Action is a call to set State called by a Condition
 */
typedef struct {
	char * performName;
	typeControl_e typeToAction;
	char * stateToAction;
	ActionFireType_e actionFireType;
} Action_t, *Action_p;

typedef Precedent_t;

/*
 * A Precedent stack is a postfix parsing of the infix Precedence string
 */
typedef struct {
	int condIndex;
	int testIndex;
	ConditionType_e conditionType;
} Precedent_t,*Precedent_p;

/*
 * A Condition is the result of a set of Tests - it can have Actions associated with it
 */
typedef struct {
	char * conditionName;
	char * precedentString;
	dynArr_t precedents;           //Other sub-conditions
	ConditionType_e conditionType;
	dynArr_t tests;
	dynArr_t actions;
	stateValues_e lastStateTest;
} Condition_t, *Condition_p;

/*
 * A ConditionGroup represents a set of Conditions
 */
typedef struct {
	char * groupName;
	dynArr_t conditions;
	struct ConditionGroup_t *next;
} ConditionGroup_t, *ConditionGroup_p;

/*
 * An Event Watcher is a set of Tests that listen for a particular EventName and Type
 */
typedef struct {
	char eventName[50];
	typeControl_e watcherType;
	dynArr_t watcherTests;
	BOOL_T found;
} eventWatcher_t, *eventWatcher_p;

/*
 * An Action performer is a set of objects that are called for a particular ActionName and Type
 */
typedef struct {
	char performName[50];
	typeControl_e performType;
	dynArr_t performTracks;
	dynArr_t performStates;
} actionPerformer_t, *actionPerformer_p;

/*
 * A StateName is a state that either an Event will give or an Action will set
 */
typedef struct {
	char name[50];
	BOOL_T found;
} stateName_t, *stateName_p;

static ConditionGroup_p conditionGroupsFirst = NULL;		//List of all Condition Groups
static ConditionGroup_p conditionGroupsLast = NULL;
static dynArr_t eventWatchers[NUM_OF_TESTABLE_TYPES]; 		//Arrays of Events for Tests
static dynArr_t actionPerformers[NUM_OF_TESTABLE_TYPES];	//Arrays of Actions for Conditions

/*
 * Add a Test to the Listening Events Arrays
 */
void subscribeEvent(char name[50],typeControl_e type,Test_p t) {
	for(int i=0;i<eventWatchers[type].cnt;i++) {
		eventWatcher_p ew = &DYNARR_N(eventWatcher_t,eventWatchers[type],i);
		if(strncmp(ew->eventName,name,50)==0) {
			DYNARR_APPEND(Test_p,ew->watcherTests,1);
			DYNARR_LAST(Test_p,ew->watcherTests) = t;
			return;
		}
	}
	DYNARR_APPEND(eventWatcher_t,eventWatchers[type],1);
	eventWatcher_p ew = &DYNARR_LAST(eventWatcher_t,eventWatchers[type]);
	ew->eventName[49] = '\0';
	strncpy(ew->eventName,name,49);
	ew->watcherType = type;
	ew->found = FALSE;
	DYNARR_RESET(void*,ew->watcherTests);
	DYNARR_APPEND(void*,ew->watcherTests,1);
	ew->watcherTests[ew->watcherTests.cnt-1] = (void *)t;
}

/*
 * Record that there is at least one Publisher for this Event and the state that is published
 */
void registerStatePublisher(char name[50],typeControl_e type, trk, dynArr_t states) {
	for(int i;i<eventWatchers[type].cnt;i++) {
		eventWatcher_p pw = &eventWatchers[type].ptr[i];
		if(strncmp(pw->eventName,name,50)==0) {
			pw->found = TRUE;
			for (int j=0;j<pw->watcherTests.cnt;j++) {
				Test_p t = &DYNARR_N(Test_t,pw->watcherTests,j);
				for (int k=0;k<states.cnt;k++) {
					if (strncmp(states.ptr[k],t->valueToTest,50)==0) {
						t->found = TRUE;
					}
				}
			}
		}
	}
}

/*
 * Add an Action to the Action Arrays
 */
void addAction(char name[50],typeControl_e type, char state[50]) {
	for(int i=0;i<actionPerformers[type].cnt;i++) {
		actionPerformer_p ap = &actionPerformers[type].ptr[i];
		if (strncmp(ap->performName,name,50)==0) {
			for (int j=0;j<ap->performStates.cnt;j++) {
				if (strncmp((char *)&ap->performStates.ptr[j],state,50) == 0) return;
			}
			DYNARR_APPEND(stateName_t,ap->performStates,1);
			stateName_p sn = &DYNARR_LAST(stateName_t,ap->performStates);
			sn->name[49] = '\0';
			strcpy(sn->name,state,49);
			sn->found = FALSE;
		}
	}
	DYNARR_APPEND(actionPerformer_t,actionPerformers[type],1);
	actionPerformer_p ap = &DYNARR_LAST(actionPerformer_t,actionPerformers[type]);
	ap->performName[49] = '\0';
	strncpy(ap->performName,name,49);
	ap->performType = type;
	DYNARR_APPEND(stateName_t,ap->performStates,1);
	stateName_p sn = &DYNARR_LAST(stateName_t,ap->performStates);
	sn->name[49] = '\0';
	strcpy(sn->name,state,49);
	sn->found = FALSE;
}

/*
 * Note that this track is a receiver for this Action and tick-off States it knows about
 */
void subscribeAction(char name[50],typeControl_e type, track_p trk, dynArr_t actions) {
	for(int i=0;i<actionPerformers[type].cnt;i++) {
		actionPerformer_p ap = &DYNARR_N(actionPerformer_t,actionPerformers[type],i);
		if (strncmp(ap->performName,name,50)==0) {
			BOOL_T found = FALSE;
			for(int j=0;j<ap->performTracks.cnt;j++) {
				if (trk == &DYNARR_N(track_p,ap->performTracks,j)) return;
			}
			DYNARR_APPEND(track_p,ap->performTracks,1);
			DYNARR_LAST(track_p,ap->performTracks) = trk;
			for (int j=0;j<actions.cnt;j++) {
				for (int k=0;k<ap->performStates.cnt;k++) {
					stateName_p sn = &DYNARR_N(stateName_t,ap->performStates,k);
					if (strncmp(sn->name,&actions.ptr[j],50)==0) {
						sn->found = TRUE;
						return;
					}
				}
			}
		}
	}
}

/*
 * Describe a missing link
 */
void Complain(dynArr_t complaints,char * title, char * name, typeControl_e type, char * detail) {
	DYNARR_APPEND(char*,complaints,1);
	char * typename;
	switch (type) {
	case TYPE_CONTROL:
		typename = "Control";
		break;
	case TYPE_TURNOUT:
		typename = "Turnout";
		break;
	case TYPE_BLOCK:
		typename = "Block";
		break;
	case TYPE_SIGNAL:
		typename = "Sensor";
		break;

	}
	char output[150];
	output[149] = '\0';
	if (detail)
		snprintf(output,149,"%s Type:%s Name:%s %s",title,typename,name,detail);
	else
		snprintf(output,149,"%s Type:%s Name:%s",title,typename,name);
	strdup(&complaints.ptr[complaints.cnt-1],output);
}

/*
 * See if all Actions have "Correct Do-ers" and all Tests have "Correct Publishers"
 */
BOOL_T TestConditionsComplete(dynArr_t complaints) {
	DYNARR_RESET((char*),complaints);
	BOOL_T rc = TRUE;
	for (int i=0;i<NUM_OF_TESTABLE_TYPES;i++) {
		for (int j=0;j<actionPerformers[type].cnt;j++) {
			actionPerformer_p ap = &actionPerformers[type].ptr[i];
			if (ap->performTracks.cnt == 0) {
				Complain(complaints,"No Sink of Event",ap->performName, ap->performType, NULL);
				//Complain - no Action Performer
				rc = FALSE;
			}
			for (int k=0;k<ap->performStates.cnt;k++) {
				stateName_p sn = &DYNARR_N(stateName_t,ap->performStates,k);
				if (!sn->found) {
					Complain(complaints,"No Sink of State",ap->performName, ap->performType, sn->name);
					//Complain this state is not known
					rc = FALSE;
				}
			}
		}
		for (int j=0;j<eventWatchers[type].cnt;j++) {
			eventWatcher_p ew = &DYNARR_N(eventWatcher_t,eventWatchers,j);
			if (!ew->found) {
				for (int k=0;ew->watcherTests;k++) {
					Test_p t = &DYNARR_N(Test_t,ew->watcherTests,k);
					if (!t->found) {
						//Complain - Producer does not produce State
						Complain(complaints,"Event does not produce State",ew->eventName, ew->watcherType, t->valueToTest);
						rc = FALSE;
					}
				}
				Complain(complaints,"No Source of Event",ew->eventName, ew->watcherType, NULL);
				//Complain - no Event Producer
				rc = FALSE;
			}
		}
	}
	return rc;
}

/*
 * Start over with the Arrays for Listening Events and Arrays
 * We do this every time we begin a new run or to Test for missing Conditions
 */
void rebuildArrays(BOOL_T complain) {
	pubSubParmList_t parm_list;
	for (int i=0;i<NUM_OF_TESTABLE_TYPES;i++) {
		DYNARR_RESET(actionPerformer_t,actionPerformers[i]); 	//Note don't reset sub-arrays - will be reused
	}
	for (int i=0;i<NUM_OF_TESTABLE_TYPES;i++) {
		DYNARR_RESET(eventWatcher_t,eventWatchers[i]);   		//Note don't reset sub-arrays - will be reused
	}
	// First register every name in the Conditions
	ConditionGroup_p cg = conditionGroupsFirst;
	while (cg) {
		for (int j=0;j<cg->conditions.cnt;j++) {
			Condition_p c = &DYNARR_N(Condition_t,cg->conditions,j);
			for (int k=0;k<c->tests.cnt;k++) {
				Test_p t = &c->tests.ptr[k];
				subscribeEvent(t->nameToTest,t->typeToTest,t);
			}
			for (int k=0;k<c->actions.cnt;k++) {
				Action_p a = &c->actions.ptr[k];
				addActionToCondition(a->performName,a->typeToAction,a->stateToAction);
			}
		}
		cg = cg->next;
	}
	track_p trk;
	//Then find all Actions and States for all Objects and wire them to the Arrays, noting if they exist
	TRK_ITERATE(trk) {
		parm_list.command = DESCRIBE_NAMES;
		if (TrackPubSub(trk,&parm_list)) {
			for (int i=0;i<parm_list.names.cnt;i++) {
				pubSubParmList_t sub_parm_list;
				strncpy(sub_parm_list.action,&parm_list.names.ptr[i],50);
				sub_parm_list.command = DESCRIBE_STATES;
				if (TrackPubSub(trk,&sub_parm_list))
					registerStatePublisher(&parm_list.names.ptr[i],parm_list.type,sub_parm_list.states);
				sub_parm_list.command = DESCRIBE_ACTIONS;
				if (TrackPubSub(trk,&sub_parm_list))
					subscribeAction(&parm_list.names.ptr[i],parm_list.type, trk, sub_parm_list.actions);
			}
		}
	}
	dynArr_t temp_states;
	DYNARR_APPEND(stateName_t,temp_states,1);
	stateName_t sn = temp_states.ptr[temp_states.cnt-1];
	sn->name = "TRUE";
	DYNARR_APPEND(stateName_t,temp_states,1);
	sn = temp_states.ptr[temp_states.cnt-1];
	sn.name = "FALSE";
	//Register all Conditions that may be needed
	ConditionGroup_p cg = conditionGroupsFirst;
	while (cg) {
		for (int j=0;j<cg->conditions.cnt;j++) {
			Condition_p c = &cg->conditions.ptr[j];
			snprintf(name,"$s.%s",&cg->groupName,&c->conditionName);
			registerStatePublisher(&c->conditionName,parm_list.type,NULL,&temp_states);
		}
		cg=cg->next;
	}
	if (complain) TestConditionsComplete();
}



/*
 * Use actionPerformer look-aside list to find track(s) to send this typeControl_e to
 */
void fireAction(Action_p act_p, BOOL_T init) {
	pubSubParmList_t parm_list;
	parm_list.command = FIRE_ACTION;
	parm_list.name = &act_p->performName;
	parm_list.action = &act_p->stateToAction;
	parm_list.type = act_p->typeToAction;
	parm_list.suppressPublish = init;
	for (int i=0;i<actionPerformers[act_p->typeToAction].cnt;i++) {
		actionPerformer_p ap = (actionPerformer_p)actionPerformers[act_p->typeToAction].ptr[i];
		if ((strncmp(ap->performName,parm_list.name,50)!=0)) continue;
		for (int j=0;j<ap->performTracks.cnt;j++) {
			track_p trk = (void *)ap->performTracks.ptr[j];
			TrackPubSub(trk,&parm_list);
		}
	}
}

/*
 * Work out new Condition State and fire Actions and publishEvent (if changed)
 */
BOOL_T processCondition(Condition_p cond_p, BOOL_T fire) {
	stateValues_e saveState = cond_p->lastStateTest;
	for (int i=0;i<cond_p->tests.cnt;i++) {
		Test_p test_p = (Test_p)cond_p->tests.ptr[i];
		if (cond_p->conditionType == COND_AND || cond_p->conditionType == COND_NOTAND ) {
			if (test_p->lastStateTest == test_p->inverse?STATE_TRUE:STATE_FALSE) {
				cond_p->lastStateTest = (cond_p->conditionType == COND_AND)?FALSE:TRUE;
				break;
			}
		}
		if (cond_p->conditionType == COND_OR || cond_p->conditionType == COND_NOTOR ) {
			if (test_p->lastStateTest == test_p->inverse?STATE_FALSE:STATE_TRUE) {
				cond_p->lastStateTest = (cond_p->conditionType == COND_OR)?TRUE:FALSE;
				break;
			}
		}
		if (cond_p->conditionType == COND_MIXED) {

		}
	}
	if (saveState == cond_p->lastStateTest) return FALSE;  //No change
	if (!fire) return TRUE;								   //Suppress actions and publish
	for (int i=0; i<cond_p->actions;i++) {
		Action_p action_p = (Action_p)cond_p->actions.ptr[i];
		if ((action_p->actionFireType == FIRE_CHANGESTOTRUE && cond_p->lastStateTest == STATE_TRUE) ||
			(action_p->actionFireType == FIRE_CHANGESTOFALSE && cond_p->lastStateTest == STATE_FALSE) ||
			action_p->actionFireType == FIRE_CHANGES) {
			fireAction(action_p, TRUE);
		}

	}
	//Last - publish change for any Listeners
	char * eventName;
	snprintf(eventName,"%s.%s","Condition",cond_p->conditionName);
	publishEvent(eventName, TYPE_CONTROL, cond_p->lastStateTest);
	return TRUE;
}

/*
 * Process Test - set state according to the Test
 */
BOOL_T processTest(Test_p test, char * publisherName, typeControl_e type, char * newState, BOOL_T percolate) {
	stateValues_e save = test->lastStateTest;
	if (type == test->typeToTest &&
		(strncmp(test->nameToTest,publisherName,50) == 0  )) {
		if (strncmp(test->valueToTest,newState,50) == 0 )
			test->lastStateTest = test->inverse?STATE_FALSE:STATE_TRUE;
		else
			test->lastStateTest = test->inverse?STATE_TRUE:STATE_FALSE;
	}

	if (test->fireTestOnChange && (test->lastStateTest != save) && percolate) {
		processCondition(test->condition, TRUE);
	}
	return (test->lastStateTest != save);
}

/*
 * Find current state - don't percolate
 */
BOOL_T GetState(Test_p test_p) {
	track_p trk;
	TRK_ITERATE(trk) {
		pubSubParmList_t parm_list;
		parm_list.command = DESCRIBE_NAMES;
		TrackPubSub(trk,&parm_list);
		for( int i=0;i<parm_list.names.cnt;i++) {
			char *s = DYNARR_N(char *,parm_list.names,i);
			if (strncmp(s,test_p->nameToTest,50) == 0) {
				parm_list.command = GET_STATE;
				parm_list.name = &test_p->nameToTest;
				TrackPubSub(trk,&parm_list);
				return processTest(test_p,test_p->nameToTest, test_p->typeToTest, parm_list.state, FALSE);
			}
		}
	}
	return NULL;
}

/*
 * First go around just get all initial States - first assume they are "OK" - Then fire all state changes as though they had just happened
 */
void InitializeStates() {
	ConditionGroup_p cg = conditionGroupsFirst;
	while (cg) {
		for (int j=0; j<cg->conditions.cnt; j++) {
			Condition_p cond_p = (Condition_p)cg->conditions.ptr[j];
			for (int k=0; k<cond_p->tests.cnt; k++) {
				Test_p test_p = (Test_p)cond_p->tests.ptr[k];
				if (!GetState(test_p->nameToTest)) {

				}
			}
			processCondition(cond_p,FALSE);  //Now do the Condition (once)
		}
		cg = cg->next;
	}
	ConditionGroup_p cg = conditionGroupsFirst;
	while (cg) {
		for (int j=0; j<cg->conditions.cnt; j++) {
			Condition_p cond_p = (Condition_p)cg->conditions.ptr[j];
			for (int i=0; i<cond_p->actions;i++) {
				Action_p action_p = (Action_p)cond_p->actions.ptr[i];
				if ((action_p->actionFireType == FIRE_CHANGESTOTRUE && cond_p->lastStateTest == STATE_TRUE) ||
					(action_p->actionFireType == FIRE_CHANGESTOFALSE && cond_p->lastStateTest == STATE_FALSE) ||
					action_p->actionFireType == FIRE_CHANGES) {
					fireAction(action_p,FALSE);
				}
			}
		}
		cg = cg->next;
	}
}

static int recurse = 0;

#define RECURSE_LIMIT (50)

/*
 * Called by Publisher to notify listeners (Tests) that a State changed
 */
void publishEvent(char * publisherName, typeControl_e type, char * newState) {
	recurse++;
	if (recurse>RECURSE_LIMIT) {
		//Complain about a possible loop
		recurse--;
		return;
	}
	for (int i=0;i<eventWatchers[type].cnt;i++) {
		eventWatcher_p pw = (eventWatcher_p)eventWatchers[type].ptr[i];
		if ((strncmp(publisherName,pw->eventName,50)==0)) {
			for (int j=0;j<pw->watcherTests.cnt;j++) {
				Test_p test = (Test_p)pw->watcherTests.ptr[j];
				processTest(test,publisherName,type,newState, TRUE);
			}
		}
	}
	recurse--;

}

char * ConditionTypeToString(ConditionType_e ct) {
	switch (ct) {
	case COND_AND:
		return "AND";
	case COND_OR:
		return "OR";
	case COND_NOTOR:
		return "NOT OR";
	case COND_NOTAND:
		return "NOT AND";
	case COND_NOT:
		return "NOT";
	}
	return NULL;
}

ConditionType_e ConvertToConditionType(char * type) {
	if (strncmp(type,"AND",3) == 0) return COND_AND;
	if (strncmp(type,"NOT AND",7) == 0) return COND_NOTAND;
	if (strncmp(type,"OR",2) == 0) return COND_OR;
	if (strncmp(type,"NOT OR",6) == 0) return COND_NOTOR;
	if (strncmp(type,"NOT",3) == 0) return COND_NOT;
	return COND_NONE;
}



char * ControlTypeToString(typeControl_e tc) {
	switch (tc) {
	case TYPE_SIGNAL:
		return "SIGNAL";
	case TYPE_BLOCK:
		return "BLOCK";
	case TYPE_TURNOUT:
		return "TURNOUT";
	case TYPE_CONTROL:
		return "CONTROL";
	case TYPE_SENSOR:
		return "SENSOR";
	}
	return "UNKNOWN";
}

typeControl_e ConvertToControlType(char * type) {
	if (strncmp(type,"SIGNAL",6)==0) return TYPE_SIGNAL;
	if (strncmp(type,"BLOCK",5)==0)  return TYPE_BLOCK;
	if (strncmp(type,"TURNOUT",6)==0) return TYPE_TURNOUT;
	if (strncmp(type,"CONTROL",5)==0)  return TYPE_CONTROL;
	if (strncmp(type,"SENSOR",6)==0) return TYPE_SENSOR;
	return TYPE_UNKNOWN;
}

char * ActionFireTypeToString(ActionFireType_e f) {
	switch(f) {
	case FIRE_CHANGESTOTRUE: return "FIRETRUE";
	case FIRE_CHANGESTOFALSE: return "FIREFALSE";
	}
	return "FIREALL";
}

ActionFireType_e ConvertToActionFireType(char * type) {
	if (strncmp(type,"FIRETRUE",9)) return FIRE_CHANGESTOTRUE;
	if (strncmp(type,"FIREFALSE",9)) return FIRE_CHANGESTOFALSE;
	if (strncmp(type,"FIREALL",7)) return FIRE_CHANGES;
	return FIRE_UNKNOWN;
}

BOOL_T WriteConditions( Condition_p cond_p, FILE * f )
{
    int rc = 0;
    int i = 0;
    ConditionGroup_p cg = conditionGroupsFirst;
    while (cg) {
		rc &= fprintf(f, "CONDITIONGROUP %d \"%s\"\n",
					  i, cg->groupName )>0;
		for (int j = 0; j < cg->conditions.cnt; j++) {
			Condition_p c = cg->conditions.ptr[j];
			rc &= fprintf(f, "\tCONDITION %d %s \"%s\"\n",
							j, ConditionTypeToString(c->conditionType), c->conditionName )>0;
			for (int k=0; k<c->tests.cnt;k++) {
				Test_p t = c->tests.ptr[k];
				rc &= fprintf(f, "\t\tTEST R%d %s IF %s \"%s\" IS %s \"%s\"\n",
							k, t->fireTestOnChange?"FIREONCHANGE":"NOFIRE",
									ControlTypeToString(t->typeToTest), t->nameToTest, t->inverse?"COND_NOT":"",  t->valueToTest )>0;
			}
			for (int k=0; k<c->actions.cnt;k++) {
				Action_p a = c->actions.ptr[k];
				rc &= fprintf(f, "\t\tACTION %d %s SET %s \"%s\" TO \"%s\"\n",
							k, ConvertToActionFireType(a->actionFireType),
									ControlTypeToString(a->typeToAction), a->performName, a->stateToAction )>0;
			}
			rc &= fprintf(f, "\tEND \n")>0;
		}
    	rc &= fprintf( f, "ENDCONDITIONGROUP\n" )>0;
    	cg = cg->next;
    	i++;
    }
    return rc;
}

ConditionGroup_p GetConditionGroupData(track_p trk) {
	return (ConditionGroup_p) GetTrkExtraData(trk);
}

static void DeleteConditionGroup(ConditionGroup_p cg) {
	for(int i=0;i<cg->conditions.cnt;i++) {
		Condition_p c = &DYNARR_N(Condition_t,cg->conditions,i);
		for (int j=0; j<c->tests.cnt;j++) {
			Test_p t = &DYNARR_N(Test_t,c->tests,j);
			MyFree(t->nameToTest);
			MyFree(t->valueToTest);
			MyFree(t);
		}
		DYNARR_FREE(Test_t,c->tests);
		for (int j=0; j<c->actions.cnt;j++) {
			Action_p a = &DYNARR_N(Action_t,c->actions,j);
			MyFree(a->performName);
			MyFree(a->stateToAction);
			MyFree(a);
		}
		DYNARR_FREE(Action_t,c->actions);
	}
	MyFree(cg->groupName);
	DYNARR_FREE(Condition_t,cg->conditions);
	ConditionGroup_p cglist = conditionGroupsFirst, cglast = NULL;
	//Remove from list
	while (cglist) {
		if (cglist == cg) {
			if (cglast) cglast->next = cg->next;
			else conditionGroupsFirst = cg->next;
			break;
		}
		cglast = cglist;
		cglist = cglist->next;

	}
	if (cg == conditionGroupsLast) conditionGroupsLast = cglast;
}

static ConditionGroup_p NewConditionGroup(char * name) {
	ConditionGroup_p cg = MyMalloc(sizeof(ConditionGroup_t));
	cg->groupName = name;
	conditionGroupsLast->next = cg;
	conditionGroupsLast = cg;
	cg->next = NULL;
	return cg;
}

static Condition_p addConditionToGroup(ConditionGroup_p cg,char * name, ConditionType_e type) {
	DYNARR_APPEND(Condition_t,cg->conditions,1);
	Condition_p c = &DYNARR_LAST(Condition_t,cg->conditions);
	c->conditionName = name;
	c->conditionType = type;
	return c;
}

static void addTestToCondition(Condition_p c, char * nameToTest, typeControl_e typeToTest, char * valueToTest, BOOL_T fireTestOnChange, BOOL_T inverse) {
	DYNARR_APPEND(Test_t,c->tests,1);
	Test_p t = &DYNARR_LAST(Test_t,c->tests);
	t->nameToTest = nameToTest;
	t->typeToTest = typeToTest;
	t->valueToTest = valueToTest;
	t->fireTestOnChange = fireTestOnChange;
	t->inverse = inverse;
}

static void addActionToCondition(Condition_p c, char * performName, typeControl_e typeToAction, char * stateToAction, ActionFireType_e actionFireType) {
	DYNARR_APPEND(Action_t,c->actions,1);
	Action_p a = &DYNARR_LAST(Action_t,c->actions);
	a->performName = performName;
	a->typeToAction = typeToAction;
	a->stateToAction = stateToAction;
	a->actionFireType = actionFireType;
}

void ReadConditionGroup(char * line) {
	char * name;
	char * cp = NULL;
	track_p trk;
	int index;
	if ( strncmp ( cp, "CONDITIONGROUP",20) !=0) return;
	if (!GetArgs(line+13,"dq",&index,&name)) {
			return;
	}
	ConditionGroup_p cg = NewConditionGroup(name);
	while ( (cp = GetNextLine()) != NULL ) {
		while (isspace((unsigned char)*cp)) cp++;
		if ( strncmp( cp, "ENDCONDITIONGROUP", 17 ) == 0 ) {
			break;
		}
		if ( *cp == '\n' || *cp == '#' ) {
			continue;
		}
		if ( strncmp( cp, "CONDITION", 9 ) == 0 ) {
			int condIndex;
			char * conditionName;
			char * conditionType;
			if (!GetArgs(cp+9,"dsq",&condIndex,&conditionType,&conditionName)) return;
			Condition_p c = addConditionToGroup(cg,conditionName,ConvertToConditionType(conditionType));
			while ( (cp = GetNextLine()) != NULL ) {
				while (isspace((unsigned char)*cp)) cp++;
				if ( strncmp( cp, "END", 3 ) == 0 ) {
					break;
				}
				if ( *cp == '\n' || *cp == '#' ) {
					continue;
				}
				if ( strncmp( cp, "TEST", 4 ) == 0 ) {
					char *testIndex,*conditionStr,*testName,*notStr,*stateStr,*typeStr,*ifs,*is;
					if (!GetArgs(cp+4,"ssssqssq",&testIndex,&conditionStr,&ifs,&typeStr,&testName,&is,&notStr,stateStr)) return;
					// Ignore index, IF and IS
					BOOL_T fire=FALSE,inverse=FALSE;
					if (strncmp(conditionStr,"FIREONCHANGE",12) == 0) fire = TRUE;
					if (*notStr && strncmp(notStr,"NOT",3)==0) inverse =TRUE;
					typeControl_e ty = ConvertToControlType(typeStr);
					Test_p t = addTestToCondition(c,testName,ty,stateStr,fire,inverse);
				}
				//%d %s SET %s \"%s\" TO \"%s\"\n
				if ( strncmp( cp, "ACTION", 6 ) == 0 ) {
					char *actionIndex,*fireStr,*typeStr,*actionName,*stateStr,*set,*to;
					if (!GetArgs(cp+6,"dsssqsq",&actionIndex,&fireStr,&set,&typeStr,&actionName,&to,stateStr)) return;
					//Ignore index, SET and TO
					Action_p a = addActionToCondition(c, actionName, ConvertToActionType(typeStr), stateStr, ConvertToActionFireType(fireStr));
				}
			}
		}
	}
}


typedef enum {Operation,Value} PushType_e;

void push(dynArr_t stack, PushType_e type,ConditionType_e cond,int r) {
	DYNARR_APPEND(Precedent_t,stack,1);
	Precedent_p p = &DYNARR_LAST(Precedent_t,stack);
	p->conditionType = cond;
	p->testIndex = r;
}

static char * x;
static dynArr_t precedents;

int parseTest(dynArr_t);
int parseOr(dynArr_t);


void removeBlanks() {
	while (x && x[0] == ' ') {
		x++;
	}
}

/*
 * Descending Recursive Parser for Precedent expressions
 *
 * The expressions can contain AND, NAND, OR, NOR, NOT and ()s that use numbered Test terms that start R (R01, etc).
 *
 * So a valid expression could be "NOT R01 AND ( NOT R00 COND_OR R01 NAND R00 ) COND_OR R03"
 */

/*
 * All Test Terms preceded by "R" as in "R01"
 */
int parseTest(dynArr_t s) {
	int rc = 0;
	BOOL_T found_test = FALSE;
	removeBlanks();
	if (*x == 'R') {
		x++;
		if (!*x || *x == ' ' || (*x >='0' && *x <= '9') ) {
			printf("%s/n","Test Number not complete");
			return 1;
		}
		int i=0, r=0;
		while (x[i] && (x[i] != ' ')) {
			if (*x >= '0' && *x <='9') {
				r = 10*r +(*x - '0');
			} else break;
			x++;
		}
		found_test = TRUE;
		push(s,Value,NULL,r);    				//Immediately Push Test Term
	} else {
		removeBlanks();
		if ( *x == "(") {                        //Test for brackets
			x++;
			if (rc &=parseOr(s)) return rc;	    //Recurse inside from top
			removeBlanks();
			if (*x == ")") {
				x++;
			} else {
				printf("%s/n","Missing close bracket");
				return 3;
			}
			found_test = TRUE;
		}
	}
	if (found_test)	return 0;
	printf("%s/n","Missing Test Term");
	return 4;
}

/*
 * This prefix NOT qualifies a term - it isn't part of an infix operator (this is like -1 versus x-1 in arithmetic)
 */
int parseNot(dynArr_t s) {
	int rc = 0;
	removeBlanks();
	if (strncmp(x,"COND_NOT",3) == 0 ) {
		x = x+3;
		if (rc &=parseNot(s)) return rc;		    //Get a term, could have another COND_NOT (like --1)
		push(s,Operation,NOT,-1);					//Immediately Push COND_NOT after terms
	} else {
		if (rc &=parseTest(s)) return rc;           //Otherwise must be a term
	}
	return rc;
}

int parseAnd(dynArr_t s) {
	int rc = 0;

	if (rc &=parseNot(s)) return rc;                //Get a term via unary term

	removeBlanks();
	while (strncmp(x,"COND_AND",3)==0 || strncmp(x,"NOT COND_AND",7)==0) {  //Keep going for ANDs
		BOOL_T found = FALSE;
		BOOL_T inverse = FALSE;
		if (strncmp(x,"COND_AND",3) == 0) {
			x = x+3;
			found = TRUE;
		}
		if (strncmp(x,"COND_NOT COND_AND",7) == 0) {
			x = x+7;
			found = TRUE;
			inverse = TRUE;
		}
		if (found) {
			if (rc &=removeBlanks()) return rc;

			if (rc &=parseNot(s)) return rc;                //Get another term

			push(s,Operation,inverse?COND_NOTAND:COND_AND,-1);		//Push operation after two terms
		}
		removeBlanks();
	}
	return 0;
}

int parseOr(dynArr_t s) {
	int rc = 0;
	if (rc &=parseAnd(s)) return rc;                 //Get a term via higher priority
	removeBlanks();
	while (strncmp(x,"COND_OR",2)==0 || strncmp(x,"COND_NOT COND_OR",6)==0) {
		BOOL_T found = FALSE;
		BOOL_T inverse = FALSE;
		if (strncmp(x,"COND_OR",2) == 0) {
			x = x+2;
			found = TRUE;
		}
		if (strncmp(x,"CONDITION_NOT COND_OR",6) == 0) {
			x = x+6;
			found = TRUE;
			inverse = TRUE;
		}
		if (found) {
			if (rc &=parseAnd(s)) return rc;			    //Get another Term
			push(Operation,inverse?NOTOR:COND_OR,-1);			//Push operation after two terms
		}
		removeBlanks();
	}
	return rc;
}

int parseExpression(char * string)  {
	int rc = 0;
	DYNARR_RESET(Precedent_t,precedents);
	x = string;
	rc = parseOr(precedents);						//Start with lowest priority
	if (rc == 0  && !*x ) {
		printf("%s/n","PostFix Form:");
		for (int i=0;i<precedents.cnt;i++) {
			Precedent_p p = DYNARR_N(Precedent_t,precedents,i);
			if (p->conditionType == COND_NONE) {
				printf("%d",p->testIndex);
			}
			else {
				switch(p->conditionType) {
				case COND_AND:
					printf("%s","COND_AND");
					break;
				case COND_NOTAND:
					printf("%s","NAND");
					break;
				case COND_OR:
					printf("%s","COND_OR");
					break;
				case NOR:
					printf("%s","NOR");
					break;
				case COND_NOT:
					printf("%s","COND_NOT");
					break;
				}
			}
		}
		printf("/n");
		return 0;      			//Clean
	}
	if ((rc == 0) && *x) {
		printf("%s/n","Unexpected characters in expression");
		rc = 8;
	}
	char *mark;
	strndup(mark,string,255);
	int i;
	for (i=0;i<string-x;i++) {
		mark[i] = ' ';
	}
	mark[i] = '^';
	i++;
	while(mark[i]) mark[i] = ' ';
	printf("%s/n",string);
	printf("%s/n",mark);
	return rc;
}





