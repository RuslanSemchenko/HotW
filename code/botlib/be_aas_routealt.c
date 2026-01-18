/*****************************************************************************
 * name:		be_aas_routealt.c
 *
 * desc:		AAS
 *
 * $Archive: /MissionPack/code/botlib/be_aas_routealt.c $
 *
 *****************************************************************************/

#include "../qcommon/q_shared.h"
#include "l_utils.h"
#include "l_memory.h"
#include "l_log.h"
#include "l_script.h"
#include "l_precomp.h"
#include "l_struct.h"
#include "aasfile.h"
#include "botlib.h"
#include "be_aas.h"
#include "be_aas_funcs.h"
#include "be_interface.h"
#include "be_aas_def.h"

#define ENABLE_ALTROUTING
 //#define ALTROUTE_DEBUG

typedef struct midrangearea_s
{
	int valid;
	unsigned short starttime;
	unsigned short goaltime;
} midrangearea_t;

midrangearea_t* midrangeareas = NULL;
int* clusterareas = NULL;
int numclusterareas = 0;

//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_AltRoutingFloodCluster_r(int areanum)
{
	int i, otherareanum;
	aas_area_t* area;
	aas_face_t* face;

	if (!midrangeareas || !clusterareas) return;

	//add the current area to the areas of the current cluster
	clusterareas[numclusterareas] = areanum;
	numclusterareas++;
	//remove the area from the mid range areas
	midrangeareas[areanum].valid = qfalse;
	//flood to other areas through the faces of this area
	area = &aasworld.areas[areanum];
	for (i = 0; i < area->numfaces; i++)
	{
		face = &aasworld.faces[abs(aasworld.faceindex[area->firstface + i])];
		//get the area at the other side of the face
		if (face->frontarea == areanum) otherareanum = face->backarea;
		else otherareanum = face->frontarea;
		//if there is an area at the other side of this face
		if (!otherareanum) continue;
		//if the other area is not a midrange area
		if (!midrangeareas[otherareanum].valid) continue;
		//
		AAS_AltRoutingFloodCluster_r(otherareanum);
	} //end for
} //end of the function AAS_AltRoutingFloodCluster_r
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
int AAS_AlternativeRouteGoals(vec3_t start, int startareanum, vec3_t goal, int goalareanum, int travelflags,
	aas_altroutegoal_t* altroutegoals, int maxaltroutegoals,
	int type)
{
#ifndef ENABLE_ALTROUTING
	(void)start; (void)startareanum; (void)goal; (void)goalareanum;
	(void)travelflags; (void)altroutegoals; (void)maxaltroutegoals; (void)type;
	return 0;
#else
	int i, j, bestareanum;
	int numaltroutegoals, nummidrangeareas;
	int starttime, goaltime, goaltraveltime;
	float dist, bestdist;
	vec3_t mid, dir;
#ifdef ALTROUTE_DEBUG
	int startmillisecs;

	startmillisecs = Sys_MilliSeconds();
#endif

	// »справление C4100: ѕараметр goal не используетс€ напр€мую
	(void)goal;

	if (!startareanum || !goalareanum || !midrangeareas)
		return 0;

	//travel time towards the goal area
	goaltraveltime = AAS_AreaTravelTimeToGoalArea(startareanum, start, goalareanum, travelflags);

	//clear the midrange areas
	Com_Memset(midrangeareas, 0, aasworld.numareas * sizeof(midrangearea_t));
	numaltroutegoals = 0;
	nummidrangeareas = 0;

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!(type & ALTROUTEGOAL_ALL))
		{
			if (!(type & ALTROUTEGOAL_CLUSTERPORTALS && (aasworld.areasettings[i].contents & AREACONTENTS_CLUSTERPORTAL)))
			{
				if (!(type & ALTROUTEGOAL_VIEWPORTALS && (aasworld.areasettings[i].contents & AREACONTENTS_VIEWPORTAL)))
				{
					continue;
				}
			}
		}

		if (!AAS_AreaReachability(i)) continue;

		starttime = AAS_AreaTravelTimeToGoalArea(startareanum, start, i, travelflags);
		if (!starttime) continue;

		if (starttime > (float)1.1 * goaltraveltime) continue;

		goaltime = AAS_AreaTravelTimeToGoalArea(i, NULL, goalareanum, travelflags);
		if (!goaltime) continue;

		if (goaltime > (float)0.8 * goaltraveltime) continue;

		// »справление C4244: явное приведение к unsigned short
		midrangeareas[i].valid = qtrue;
		midrangeareas[i].starttime = (unsigned short)starttime;
		midrangeareas[i].goaltime = (unsigned short)goaltime;

		Log_Write("%d midrange area %d", nummidrangeareas, i);
		nummidrangeareas++;
	}

	for (i = 1; i < aasworld.numareas; i++)
	{
		if (!midrangeareas[i].valid) continue;

		numclusterareas = 0;
		AAS_AltRoutingFloodCluster_r(i);

		VectorClear(mid);
		for (j = 0; j < numclusterareas; j++)
		{
			VectorAdd(mid, aasworld.areas[clusterareas[j]].center, mid);
		}

		// »справление C4244: ѕриведение результата делени€ к vec_t
		VectorScale(mid, (vec_t)(1.0 / numclusterareas), mid);

		bestdist = 999999.0f;
		bestareanum = 0;
		for (j = 0; j < numclusterareas; j++)
		{
			VectorSubtract(mid, aasworld.areas[clusterareas[j]].center, dir);
			dist = VectorLength(dir);
			if (dist < bestdist)
			{
				bestdist = dist;
				bestareanum = clusterareas[j];
			}
		}

		VectorCopy(aasworld.areas[bestareanum].center, altroutegoals[numaltroutegoals].origin);
		altroutegoals[numaltroutegoals].areanum = bestareanum;

		// »справление C4244: явное приведение
		altroutegoals[numaltroutegoals].starttraveltime = (int)midrangeareas[bestareanum].starttime;
		altroutegoals[numaltroutegoals].goaltraveltime = (int)midrangeareas[bestareanum].goaltime;
		altroutegoals[numaltroutegoals].extratraveltime =
			(midrangeareas[bestareanum].starttime + midrangeareas[bestareanum].goaltime) -
			goaltraveltime;
		numaltroutegoals++;

#ifdef ALTROUTE_DEBUG
		AAS_ShowAreaPolygons(bestareanum, 1, qtrue);
#endif
		if (numaltroutegoals >= maxaltroutegoals) break;
	}
#ifdef ALTROUTE_DEBUG
	botimport.Print(PRT_MESSAGE, "alternative route goals in %d msec\n", Sys_MilliSeconds() - startmillisecs);
#endif
	return numaltroutegoals;
#endif
} //end of the function AAS_AlternativeRouteGoals
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_InitAlternativeRouting(void)
{
#ifdef ENABLE_ALTROUTING
	if (midrangeareas) FreeMemory(midrangeareas);
	midrangeareas = (midrangearea_t*)GetMemory(aasworld.numareas * sizeof(midrangearea_t));

	if (clusterareas) FreeMemory(clusterareas);
	clusterareas = (int*)GetMemory(aasworld.numareas * sizeof(int));

	if (midrangeareas) Com_Memset(midrangeareas, 0, aasworld.numareas * sizeof(midrangearea_t));
	if (clusterareas) Com_Memset(clusterareas, 0, aasworld.numareas * sizeof(int));
#endif
} //end of the function AAS_InitAlternativeRouting
//===========================================================================
//
// Parameter:				-
// Returns:					-
// Changes Globals:		-
//===========================================================================
void AAS_ShutdownAlternativeRouting(void)
{
#ifdef ENABLE_ALTROUTING
	if (midrangeareas) FreeMemory(midrangeareas);
	midrangeareas = NULL;
	if (clusterareas) FreeMemory(clusterareas);
	clusterareas = NULL;
	numclusterareas = 0;
#endif
} //end of the function AAS_ShutdownAlternativeRouting